#ifndef render3d_h
#define render3d_h

#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <string>
#include <iostream>
#include <OpenGL/OpenGL.h>
#include <GLUT/GLUT.h>
#include "Eigen/Geometry"

typedef unsigned char uint8;

#define _SafeDeleteArray_(p) {if (p) {delete[] p;}}

typedef Eigen::Matrix<float, 2, 1> Vector2f;
typedef Eigen::Matrix<float, 3, 1> Vector3f;
typedef Eigen::Matrix<float, 4, 1> Vector4f;
typedef Eigen::Matrix<float, 3, 3> Matrix3f;
typedef Eigen::Matrix<float, 4, 4> Matrix4f;
typedef Eigen::Quaternionf Quaternion;

struct Attrib
{
    std::string name;
    int location;
    int size;
    GLenum type;
};

struct Uniform
{
    std::string name;
    int location;
    int size;
    GLenum type;
};

class Program
{
public:
    Program();
    ~Program();

    bool LoadAndCompile(const std::string& vert_file, const std::string& frag_file, const std::string& macros = "");
    GLuint GetGLProgramId() const;
    int GetAttribLocation(const std::string& attrib_name);
    int GetUniformLocation(const std::string& uniform_name);
    void Use();

private:
    GLuint m_gl_program = 0;
    std::map<std::string, Attrib> m_attribs;;
    std::map<std::string, Uniform> m_uniforms;
    std::list<std::string> m_builtin_uniforms; // wvp, vorld, view, projection.. etc
    static std::list<std::string> AVAILABLE_BUILTIN_UNIFORMS;
    friend class Material;
};

enum TextureFormat
{
    RGB,
    RGBA,
};

enum TextureType
{
    TEXTURE_2D,
    TEXTURE_CUBE_COMPACT
};

class Texture
{
public:
    Texture();
    ~Texture();

    int GetWidth() const;
    int GetHeight() const;
    TextureFormat GetFormat() const;
    GLuint GetGlTextureId() const;
    TextureType GetTextureType() const;

private:
    GLuint m_gl_texture = 0;
    int m_width = 0;
    int m_height = 0;
    TextureFormat m_format = RGBA;
    TextureType m_type = TEXTURE_2D;
    friend class Renderer;
};

class MaterialParam;
class SubMesh;
class Material
{
public:
    Material(SubMesh* submesh, Program* program);
    ~Material();
    
    void Apply();
    void SetName(const std::string& name);
    std::string GetName() const;
    Program* GetProgram() const;
    void SetFloatParam(const std::string& name, float value);
    void SetMatrix4fParam(const std::string& name, Matrix4f matrix);
    void SetTextureParam(const std::string& name, Texture* texture);
    void SetSHParam(const std::string& name, const float* sh_params);
    bool IsTranslucent() const;
    void SetTranslucent(bool translucent);

private:
    void ResetIdleTextureUnit();
    void UpdateBuiltinUniforms();

private:
    SubMesh* m_submesh = nullptr;
    Program* m_program = nullptr;
    std::map<std::string, MaterialParam*> m_params;
    bool m_translucent = false;
    int m_idle_texture_unit = 0;
    friend class TextureMaterialParam;
};

class MaterialParam
{
public:
    MaterialParam(Material* material, const std::string& name);
    virtual ~MaterialParam() {}
    virtual void Apply() = 0;

protected:
    std::string m_name;
    Material* m_material;
};

class FloatMaterialParam : public MaterialParam
{
public:
    FloatMaterialParam(Material* material, const std::string& name, float value);
    virtual void Apply() override;

private:
    float m_value;
    friend class Material;
};

class Matrix4fMaterialParam : public MaterialParam
{
public:
    Matrix4fMaterialParam (Material* material, const std::string& name, Matrix4f matrix);
    virtual void Apply() override;
private:
    Matrix4f m_matrix;
    friend class Material;
};

class TextureMaterialParam : public MaterialParam
{
public:
    TextureMaterialParam(Material* material, const std::string& name, Texture* texture);
    virtual void Apply() override;

private:
    Texture* m_texture;
    friend class Material;
};

class SHMaterialParam : public MaterialParam
{
public:
    SHMaterialParam(Material* material, const std::string& name, const float* sh_params);
    virtual void Apply() override;
    
private:
    const float* m_sh_params = nullptr;
    friend class Material;
};

// 默认为RGBA
struct PngImage
{
    unsigned width = 0;
    unsigned height = 0;
    int bytesPerPixel = 4;
    std::vector<unsigned char> pixels;
    
    bool LoadFromFile(const std::string& file_path);
};

struct ObjTri
{
    int v0,t0,n0,v1,t1,n1,v2,t2,n2;
};

class Mesh;
class SubMesh;
class ObjMeshParser
{
public:
    ObjMeshParser(Mesh* mesh, char* data, int data_size);

    // 加载一个obj模型, 返回它每个子mesh使用的material名称.
    std::vector<std::string> Parse();

private:
    SubMesh* GenerateSubMesh(std::vector<Vector3f> *positions, std::vector<Vector2f> *texcoords, std::vector<Vector3f> *normals, std::vector<ObjTri> *triangles);

private:
    Mesh* m_mesh = nullptr;
    int m_curr = 0;
    char* m_data = nullptr;
    int m_data_size = 0;
};

class SubMesh
{
public:
    SubMesh(Mesh* mesh);
    ~SubMesh();
    
    Mesh* GetMesh() const;
    Material* GetMaterial() const;
    int GetVertexCount() const;

    void Render();

private:
    int m_vertex_count = 0;
    Vector3f* m_positions = nullptr;
    Vector2f* m_texcoords = nullptr;
    Vector3f* m_normals = nullptr;

    GLuint m_vao = 0;

    Mesh* m_mesh = nullptr;
    
    // 注意! 这里的material实际是program + programParams组成的!
    // 它由submesh管理, 会跟着被delete.
    // Program是共享的, 存在于Renderer的m_program_cache.
    // 注意2: 当material改变时, m_vao需要跟着变, 因为attrib_loc是根据当前material里的program查询得来的
    Material* m_material = nullptr;
    friend class ObjMeshParser;
    friend class Renderer;
};

class Renderer;
class Mesh
{
public:
    Mesh(Renderer* renderer);
    ~Mesh();

    Renderer* GetRenderer() const;
    void SetPosition(const Vector3f& position);
    void SetRotation(const Quaternion& rotation);
    void SetScale(const Vector3f& scale);
    Vector3f GetPosition() const;
    Quaternion GetRotation() const;
    Vector3f GetScale() const;

    void SetTransform(const Matrix4f& Matrix4f);
    Matrix4f GetTransform();
    
    void RenderOpaqueSubMeshes();
    void RenderTranslucentSubMeshes();

private:
    Renderer* m_renderer;
    std::vector<SubMesh*> m_submeshes;

    Vector3f m_position = Vector3f::Zero();
    Quaternion m_rotation   ;
    Vector3f m_scale = Vector3f::Ones();

    // submesh共享的Matrix4f. 在绘制前由当前的pos, rot, scale更新
    // 暂不支持mesh层级节点, 因此没区分local & world, 默认是world
    Matrix4f m_transform;
    bool m_transform_dirty = true;

    friend class Renderer;
    friend class ObjMeshParser;
};

class Renderer;
class Camera
{
public:
    Camera(Renderer* renderer);
    ~Camera();
    
    Vector3f GetPosition() const;
    void SetPosition(Vector3f position);
    Quaternion GetRotation() const;
    void SetRotation(Quaternion rotation);
    
    Matrix4f GetViewMatrix();
    void SetViewMatrix(Matrix4f matrix);
    Matrix4f GetProjectionMatrix() const;
    void  SetProjectionMatrix(Matrix4f matrix);
    Matrix4f GetViewProjectionMatrix();
    
    void MakeOrthographic(float width, float height, float ratio, float near, float far);
    void MakePerspective(float fov, float ratio, float near, float far);
    void MakePNPProjection(float width, float height, float fx, float fy, float near, float far);

private:
    Renderer* m_renderer;
    Vector3f m_position = Vector3f::Ones();
    Quaternion m_rotation;
    bool m_flip_y = false; // 是否垂直翻转? (mediapipe默认的cvPixelBufferRef纹理是倒置的)
    
    Matrix4f m_view_matrix;
    bool m_view_matrix_dirty = true;
    // 这个直接就设置了, 所以不需要dirty flaag
    Matrix4f m_projection_matrix;
    Matrix4f m_view_projection_matrix;
    bool m_view_projection_matrix_dirty = true;
    
    friend class TryonCalculator; //for test..
};

class Renderer
{
public:
    Renderer(int screen_width, int screen_height);
    ~Renderer();

    // 绘制已经加到列表里的mesh
    void BeginRender();
    void RenderBackground(GLuint background_texture_id);
    void RenderMeshes();
    void EndRender();
    
    Camera* GetCamera() const;
    int GetScreenWidth() const;
    int GetScreenHeight() const;
    Texture* GetDiffuseEnvTexture() const;
    Texture* GetSpecularEnvTexture() const;
    Texture* GetIblDFGTexture() const;
    Texture* GetIblIrradianceTexture() const;
    Texture* GetIblSkyboxTexture() const;
    const float* GetSHParams() const;
    
    Program* LoadProgram(const std::string& vert_file, const std::string& frag_file, const std::string& macros = "");
    Texture* LoadTexture(const std::string& texture, bool* out_translucent_flag = nullptr, bool generate_mipmap = false, TextureType texture_type = TEXTURE_2D);
    
    void LoadSHTextures(const std::string& sh_texture_name);
    
    // 加入到Renderer的Model会在Renderer->Render()里自动被渲染.
    // 也可以不加入, 独立用 model->RenderOpaque(), RenderTranslucent()绘制.
    // 可以用来绘制一些特殊对象
    void AddMesh(Mesh* mesh);
    void RemoveMesh(Mesh* mesh);

    // 加载好模型和贴图和Material, 并设置好对应的material params
    Mesh* CreatePBRMesh(const std::string& mesh_file_path);
    Mesh* CreateGlassesMesh(const std::string& mesh_file_path);
    Mesh* CreateOccluderMesh(const std::string& mesh_file_path);
    GLuint GetStandaloneColorTextureId() const;

private:
    std::list<Mesh*> m_mesh_list;
    Camera* m_camera;
    std::map<std::string, Program*> m_program_cache;
    std::map<std::string, Texture*> m_texture_cache;
    Texture* m_diffuse_env_texture;
    Texture* m_specular_env_texture;
    Texture* m_ibl_dfg_texture;
    Texture* m_ibl_irradiance_texture;
    Texture* m_ibl_skybox_texture;
    float m_sh_params[9 * 3];
    int m_screen_width;
    int m_screen_height;
    
    bool m_use_standalone_fbo = false;
    GLuint m_standalone_fbo = 0;
    GLuint m_standalone_color_texture = 0;
    GLuint m_standalone_depth_buffer = 0;
    
    bool m_use_msaa = false;
    int m_msaa_samples = 4;
    GLuint m_msaa_fbo = 0;
    GLuint m_msaa_color_buffer = 0;
    GLuint m_msaa_depth_buffer = 0;
    
    GLuint m_background_program = 0;
    GLint m_background_uniform_loc = -1;
};

#endif /* model_hpp */
