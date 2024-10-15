#include "render3d.h"

namespace render3d
{
    bool IsNewlineChar(char c)
    {
        return c == '\n' || c == '\r';
    }
    
    std::string ReadTextFile(const std::string& text_file)
    {
        auto statusOr = PathToResourceAsFile(text_file);
        if (statusOr.status() != render3d::OkStatus())
        {
            return "";
        }
        std::string abs_model_file = statusOr.ValueOrDie();
        FILE* file = fopen(abs_model_file.c_str(), "r");
        if (file == nullptr)
        {
            return "";
        }
        
        // read it to a memory chunk
        fseek(file, 0, SEEK_END);
        int file_size = ftell(file);

        fseek(file, 0, SEEK_SET);
        char* content = new char[file_size + 1];
        fread(content, file_size, 1, file);
        fclose(file);
        content[file_size] = 0;
        std::string res(content);
        delete []content;
        content = nullptr;
        return res;
    }
    
    // 这里内部new了一块内存, 需在外部用完后释放
    unsigned char* AllocateBinaryFileBuffer(const std::string& shader_file, size_t &buf_size)
    {
        std::string abs_model_file = PathToResourceAsFile(shader_file).ValueOrDie();
        FILE* file = fopen(abs_model_file.c_str(), "r");
        // read it to a memory chunk
        fseek(file, 0, SEEK_END);
        int file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        char* content = new char[file_size];
        fread(content, file_size, 1, file);
        fclose(file);
        buf_size = file_size;
        return (unsigned char*)content;
    }
    
    // shader内置的会被系统自动更新的uniform.
    std::list<std::string>& Program::GetAvailableBuiltinUniforms()
    {
        static std::list<std::string> AVAILABLE_BUILTIN_UNIFORMS =
        {
            "matWorld",
            "matView",
            "matProjection",
            "matWorldView",
            "matViewProjection",
            "matWVP",
            "diffuseEnvMap",
            "specularEnvMap",
            "iblBrdfLutMap",
            "iblDiffuseEnvMap",
            "iblSpecularEnvMap",
            // 需要什么自行添加实现
        };
        return AVAILABLE_BUILTIN_UNIFORMS;
    };
    
    Program::Program()
    {
    }
    
    Program::~Program()
    {
        if (m_gl_program > 0)
        {
            glDeleteProgram(m_gl_program);
            m_gl_program = 0;
        }
    }

    bool Program::LoadAndCompile(const std::string& vert_file, const std::string& frag_file, const std::string& macros)
    {
        GLuint vert_shader = 0;
        GLuint frag_shader = 0;
        GLint ok = GL_TRUE;
        
        GLuint program = glCreateProgram();
        if (program == 0) {
            return false;
        }
        
        std::string shader_prefix = macros + "\n";
        std::string vert_src = ReadTextFile(vert_file);
        std::string full_vert_src = shader_prefix + vert_src;
        ok = ok && GlhCompileShader(GL_VERTEX_SHADER, full_vert_src.c_str(), &vert_shader);
            
        std::string frag_src = ReadTextFile(frag_file);
        std::string full_frag_src = shader_prefix + frag_src;
        ok = ok && GlhCompileShader(GL_FRAGMENT_SHADER, full_frag_src.c_str(), &frag_shader);
        
        if (ok) {
            glAttachShader(program, vert_shader);
            glAttachShader(program, frag_shader);
            
            GLint status;
            glLinkProgram(program);
            glGetProgramiv(program, GL_LINK_STATUS, &status);
            if (!status)
            {
                GLchar    buff[1024];
                GLsizei     length;
                glGetProgramInfoLog(program, 1024, &length, buff);
                printf("length:%i\nlog:'%s'\n", length, buff);
                return false;
            }
        }
        
        if (vert_shader) glDeleteShader(vert_shader);
        if (frag_shader) glDeleteShader(frag_shader);
        
        if (!ok) {
            glDeleteProgram(program);
            program = 0;
            return false;
        }

        // extract all available uniforms and attribs
        char buf[1024];
        int active_attribs = 0;
        glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &active_attribs);
        for (int i=0; i<active_attribs; i++)
        {
            int name_len = 0;
            Attrib attrib;
            glGetActiveAttrib(program, i, 1024, &name_len, &attrib.size, &attrib.type, buf);
            buf[name_len] = 0;
            attrib.name = buf;
            attrib.location = glGetAttribLocation(program, buf);
            m_attribs[attrib.name] = attrib;
        }

        int active_uniforms = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &active_uniforms);
        for (int i=0; i<active_uniforms; i++)
        {
            int name_len = 0;
            Uniform uniform;
            glGetActiveUniform(program, i, 1024, &name_len, &uniform.size, &uniform.type, buf);
            buf[name_len] = 0;
            uniform.name = buf;
            uniform.location = glGetUniformLocation(program, buf);
            m_uniforms[uniform.name] = uniform;
            
            // 是否是builtin uniform?
            auto& all_builtin_uniforms = GetAvailableBuiltinUniforms();
            if (std::find(all_builtin_uniforms.begin(), all_builtin_uniforms.end(), uniform.name) != all_builtin_uniforms.end())
            {
                m_builtin_uniforms.push_back(uniform.name);
            }
        }

        m_gl_program = program;
        return true;
    }

    GLuint Program::GetGLProgramId() const
    {
        return m_gl_program;
    }

    int Program::GetAttribLocation(const std::string& attrib_name)
    {
        auto iter = m_attribs.find(attrib_name);
        if (iter != m_attribs.end())
        {
            return iter->second.location;
        }
        else
        {
            //VLOG("ERROR: Invalid attrib name: %s.", attrib_name.c_str);
            return -1;
        }
    }

    int Program::GetUniformLocation(const std::string& uniform_name)
    {
        auto iter = m_uniforms.find(uniform_name);
        if (iter != m_uniforms.end())
        {
            return iter->second.location;
        }
        else
        {
            //VLOG("ERROR: Invalid attrib name: %s.", uniform_name.c_str);
            return -1;
        }
    }
    
    void Program::Use()
    {
        glUseProgram(m_gl_program);
    }

    Texture::Texture()
    {
    }

    Texture::~Texture()
    {
        if (m_gl_texture > 0)
        {
            glDeleteTextures(1, &m_gl_texture);
            m_gl_texture = 0;
        }
    }

    int Texture::GetWidth() const
    {
        return m_width;
    }

    int Texture::GetHeight() const
    {
        return m_height;
    }

    TextureType Texture::GetType() const
    {
        return m_type;
    }

    TextureFormat Texture::GetFormat() const
    {
        return m_format;
    }

    GLuint Texture::GetGlTextureId() const
    {
        return m_gl_texture;
    }

    Material::Material(SubMesh* submesh, Program* program)
    : m_submesh(submesh), m_program(program)
    {
    }
    
    Material::~Material()
    {
        // delete all params
        for (auto iter = m_params.begin(); iter != m_params.end(); iter++)
        {
            delete iter->second;
        }
        m_params.clear();
    }
    
    void Material::UpdateBuiltinUniforms()
    {
        // 简单起见, 没有使用key-value方式hash查找handler
        // 不过bultin量不多,应该不是什么问题. 后续有时间可稍优化下
        for (auto builtin_uniform : m_program->m_builtin_uniforms)
        {
            if (builtin_uniform == "matWorld")
            {
                auto matWorld = this->m_submesh->GetMesh()->GetTransform();
                SetMatrix4fParam(builtin_uniform, matWorld);
            }
            else if (builtin_uniform == "matView")
            {
                auto matView = this->m_submesh->GetMesh()->GetRenderer()->GetCamera()->GetViewMatrix();
                SetMatrix4fParam(builtin_uniform, matView);
            }
            else if (builtin_uniform == "matProjection")
            {
                auto matProjection = this->m_submesh->GetMesh()->GetRenderer()->GetCamera()->GetProjectionMatrix();
                SetMatrix4fParam(builtin_uniform, matProjection);
            }
            else if (builtin_uniform == "matWorldView")
            {
                auto matWorld = this->m_submesh->GetMesh()->GetTransform();
                auto matView = this->m_submesh->GetMesh()->GetRenderer()->GetCamera()->GetViewMatrix();
                SetMatrix4fParam(builtin_uniform, matView * matWorld);
            }
            else if (builtin_uniform == "matViewProjection")
            {
                auto matViewProjection = this->m_submesh->GetMesh()->GetRenderer()->GetCamera()->GetViewProjectionMatrix();
                SetMatrix4fParam(builtin_uniform, matViewProjection);
            }
            else if (builtin_uniform == "matWVP")
            {
                auto matWorld = this->m_submesh->GetMesh()->GetTransform();
                auto matViewProjection = this->m_submesh->GetMesh()->GetRenderer()->GetCamera()->GetViewProjectionMatrix();
                SetMatrix4fParam(builtin_uniform, matViewProjection * matWorld);
            }
            else if (builtin_uniform == "diffuseEnvMap")
            {
                SetTextureParam(builtin_uniform, m_submesh->GetMesh()->GetRenderer()->GetDiffuseEnvTexture());
            }
            else if (builtin_uniform == "specularEnvMap")
            {
                SetTextureParam(builtin_uniform, m_submesh->GetMesh()->GetRenderer()->GetSpecularEnvTexture());
            }
            else if (builtin_uniform == "iblBrdfLutMap")
            {
                SetTextureParam(builtin_uniform, m_submesh->GetMesh()->GetRenderer()->GetIblBrdfLutTexture());
            }
            else if (builtin_uniform == "iblDiffuseEnvMap")
            {
                SetTextureParam(builtin_uniform, m_submesh->GetMesh()->GetRenderer()->GetIblDiffuseEnvTexture());
            }
            else if (builtin_uniform == "iblSpecularEnvMap")
            {
                SetTextureParam(builtin_uniform, m_submesh->GetMesh()->GetRenderer()->GetIblSpecularEnvTexture());
            }
        }
    }

    void Material::Apply()
    {
        // reset idle stage unit
        this->ResetIdleTextureUnit();

        // use program and apply params
        if (m_program != nullptr)
        {
            m_program->Use();
        }
        
        // update system builtin uniforms
        this->UpdateBuiltinUniforms();
        
        // apply all uniforms
        for (auto iter = m_params.begin(); iter != m_params.end(); iter++)
        {
            if (iter->second != nullptr)
            {
                iter->second->Apply();
            }
        }
    }

    void Material::ResetIdleTextureUnit()
    {
        m_idle_texture_unit = 0;
    }

    Program* Material::GetProgram() const
    {
        return m_program;
    }

    void Material::SetFloatParam(const std::string& name, float value)
    {
        auto iter = this->m_params.find(name);
        if (iter != m_params.end())
        {
            static_cast<FloatMaterialParam*>(iter->second)->m_value = value;
        }
        else
        {
            auto materialParam = new FloatMaterialParam(this, name, value);
            m_params[name] = materialParam;
        }
    }
    
    void Material::SetMatrix4fParam(const std::string& name, Matrix4f matrix)
    {
        auto iter = this->m_params.find(name);
        if (iter != m_params.end())
        {
            static_cast<Matrix4fMaterialParam*>(iter->second)->m_matrix = matrix;
        }
        else
        {
            auto materialParam = new Matrix4fMaterialParam(this, name, matrix);
            m_params[name] = materialParam;
        }
    }

    void Material::SetTextureParam(const std::string& name, Texture* texture)
    {
        auto iter = this->m_params.find(name);
        if (iter != m_params.end())
        {
            static_cast<TextureMaterialParam*>(iter->second)->m_texture = texture;
        }
        else
        {
            auto materialParam = new TextureMaterialParam(this, name, texture);
            m_params[name] = materialParam;
        }
    }
    
    bool Material::IsTranslucent() const
    {
        return m_translucent;
    }
    
    void Material::SetTranslucent(bool translucent)
    {
        m_translucent = translucent;
    }

    MaterialParam::MaterialParam(Material* material, const std::string& name)
    : m_material(material), m_name(name)
    {
    }

    FloatMaterialParam::FloatMaterialParam(Material* material, const std::string& name, float value)
    : MaterialParam(material, name), m_value(value)
    {
    }

    void FloatMaterialParam::Apply()
    {
        glUniform1f(m_material->GetProgram()->GetUniformLocation(m_name), m_value);
    }
    
    Matrix4fMaterialParam::Matrix4fMaterialParam(Material* material, const std::string& name, Matrix4f matrix)
    : MaterialParam(material, name), m_matrix(matrix)
    {
    }
    
    void Matrix4fMaterialParam::Apply()
    {
        int uniform_loc = m_material->GetProgram()->GetUniformLocation(m_name);
        glUniformMatrix4fv(uniform_loc, 1, GL_FALSE, (float*)&m_matrix);
    }

    TextureMaterialParam::TextureMaterialParam(Material* material, const std::string& name, Texture* texture)
    : MaterialParam(material, name), m_texture(texture)
    {
    }

    void TextureMaterialParam::Apply()
    {
        if (m_texture == nullptr)
            return;
        
        glActiveTexture(GL_TEXTURE0 + m_material->m_idle_texture_unit);
        glBindTexture(m_texture->GetType() == TEXTURE_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP, m_texture->GetGlTextureId());
        glUniform1i(m_material->GetProgram()->GetUniformLocation(m_name), m_material->m_idle_texture_unit);
        m_material->m_idle_texture_unit++;
    }


    ObjMeshParser::ObjMeshParser(Mesh* mesh, char* data, int data_size, bool export_triangles)
    : m_mesh(mesh), m_data(data), m_data_size(data_size), m_export_triangles(export_triangles)
    {
    }

    SubMesh* ObjMeshParser::GenerateSubMesh(std::vector<Vector3f> *positions, std::vector<Vector2f> *texcoords, std::vector<Vector3f> *normals, std::vector<ObjTri> *triangles)
    {
        SubMesh* submesh = new SubMesh(m_mesh);
        int vertex_count = triangles->size() * 3;
        submesh->m_vertex_count = vertex_count;
        submesh->m_positions = new Vector3f[vertex_count];
        
        if (texcoords->size() > 0)
        {
            submesh->m_texcoords = new Vector2f[vertex_count];
        }
        
        if (normals->size() > 0)
        {
            submesh->m_normals = new Vector3f[vertex_count];
        }
        
        if (m_export_triangles)
        {
            // save a copy to m_triangles
            submesh->m_ori_positions = *positions;
            submesh->m_ori_triangles = *triangles;
        }
        
        // obj的triangle索引是从1开始的!!! 被坑了好久~
        // 先统一转为从0开始的索引
        for (auto iter=triangles->begin(); iter!=triangles->end(); iter++)
        {
            iter->v0 -= 1;
            iter->v1 -= 1;
            iter->v2 -= 1;
            iter->t0 -= 1;
            iter->t1 -= 1;
            iter->t2 -= 1;
            iter->n0 -= 1;
            iter->n1 -= 1;
            iter->n2 -= 1;
        }
        
        for (int i=0; i<triangles->size(); i++)
        {
            ObjTri tri = (*triangles)[i];
            submesh->m_positions[i*3 + 0] = (*positions)[tri.v0];
            submesh->m_positions[i*3 + 1] = (*positions)[tri.v1];
            submesh->m_positions[i*3 + 2] = (*positions)[tri.v2];
            
            submesh->m_texcoords[i*3 + 0] = (*texcoords)[tri.t0];
            submesh->m_texcoords[i*3 + 1] = (*texcoords)[tri.t1];
            submesh->m_texcoords[i*3 + 2] = (*texcoords)[tri.t2];
            
            submesh->m_normals[i*3 + 0] = (*normals)[tri.n0];
            submesh->m_normals[i*3 + 1] = (*normals)[tri.n1];
            submesh->m_normals[i*3 + 2] = (*normals)[tri.n2];
        }
        
        return submesh;
    }

    std::vector<std::string> ObjMeshParser::Parse(bool* succ)
    {
        std::vector<Vector3f> positions; // x y z
        std::vector<Vector2f> texcoords; // x y
        std::vector<Vector3f> normals; // x y z
        std::vector<ObjTri> triangles; // v0/t0/n0 v1/t1/n1 v2/t2/n2
        bool hasNegIndex = false;

        std::vector<int> lines;
        int line_start_idx = 0;
        lines.push_back(0);

        for (int i=0; i<m_data_size; i++)
        {
            if (IsNewlineChar(m_data[i]))
            {
                m_data[i] = 0;
                if (i + 1 < m_data_size)
                    lines.push_back(i+1);
            }
            else if (m_data[i] == '/')
            {
                m_data[i] = ' '; // 方便sscanf
            }
        }

        std::vector<std::string> submesh_material_names;
        char temp[256];
        for (int i=0; i<lines.size(); i++)
        {
            char* ptr = m_data + lines[i];
            if (*ptr == 'v')
            {
                // maybe v, vt, vn
                if (*(ptr+1) == ' ')
                {
                    float x, y, z;
                    sscanf(ptr, "%s %f %f %f", temp, &x, &y, &z);
                    positions.push_back(Vector3f(x, y, z));
                }
                else if (*(ptr+1) == 't')
                {
                    float x, y;
                    sscanf(ptr, "%s %f %f", temp, &x, &y);
                    texcoords.push_back(Vector2f(x, y));
                }
                else if (*(ptr+1) == 'n')
                {
                    float x, y, z;
                    sscanf(ptr, "%s %f %f %f", temp, &x, &y, &z);
                    normals.push_back(Vector3f(x, y, z));
                }
            }
            else if (*ptr == 'f' && *(ptr+1) == ' ')
            {
                ObjTri tri;
                
                if (texcoords.size() > 0 && normals.size() > 0)
                {
                    sscanf(ptr, "%s %d %d %d %d %d %d %d %d %d", temp, &tri.v0, &tri.t0, &tri.n0, &tri.v1, &tri.t1, &tri.n1, &tri.v2, &tri.t2, &tri.n2);
                    if (tri.v0 < 0 || tri.v1 < 0 || tri.t0 < 0 || tri.t1 < 0 || tri.t2 < 0 || tri.n0 < 0 || tri.n1 < 0 || tri.n2 < 0) {
                        hasNegIndex = true;
                    }
                }
                else if (texcoords.size() > 0 && normals.size() == 0)
                {
                    sscanf(ptr, "%s %d %d %d %d %d %d", temp, &tri.v0, &tri.t0, &tri.v1, &tri.t1, &tri.v2, &tri.t2);
                    if (tri.v0 < 0 || tri.v1 < 0 || tri.v2 < 0 || tri.t0 < 0 || tri.t1 < 0 || tri.t2 < 0) {
                        hasNegIndex = true;
                    }
                }
                else if (texcoords.size() == 0 && normals.size() > 0)
                {
                    sscanf(ptr, "%s %d %d %d %d %d %d", temp, &tri.v0, &tri.n0, &tri.v1, &tri.n1, &tri.v2, &tri.n2);
                    if (tri.v0 < 0 || tri.v1 < 0 || tri.v2 < 0 || tri.t0 < 0 || tri.t1 < 0 || tri.t2 < 0) {
                        hasNegIndex = true;
                    }
                }
                else
                {
                    sscanf(ptr, "%s %d %d %d", temp, &tri.v0, &tri.v1, &tri.v2);
                    if (tri.v0 < 0 || tri.v1 < 0 || tri.v2 < 0) {
                        hasNegIndex = true;
                    }
                }
                if (hasNegIndex) {
                    break;
                }
                triangles.push_back(tri);
            }
            else if (*ptr == 'u' && *(ptr+1)=='s' && *(ptr+2) == 'e' && *(ptr+3) == 'm' && *(ptr+4) == 't' && *(ptr+5) == 'l')
            {
                // usemtl
                
                // 是否是新块? 不是的话则创建
                if (triangles.size() > 0)
                {
                    // 说明不止一个子模型. 上一个模型读完了, 可以创建了
                    SubMesh* submesh = this->GenerateSubMesh(&positions, &texcoords, &normals, &triangles);
                    this->m_mesh->m_submeshes.push_back(submesh);
                    
                    // 清空, 读下一个
                    triangles.clear();
                }
                
                
                // 记录材质名称
                // 已经没有换行符, 前面统一改为0了
                submesh_material_names.push_back(ptr+7);
            }
        }
        
        if (hasNegIndex) {
            submesh_material_names.clear();
            if (succ) {
                *succ = false;
            }
            return submesh_material_names;
        }
        
        // 创建最后一个submesh
        if (!hasNegIndex) {
            SubMesh* submesh = this->GenerateSubMesh(&positions, &texcoords, &normals, &triangles);
            this->m_mesh->m_submeshes.push_back(submesh);
        }
        
        if (succ) {
            *succ = true;
        }
        return submesh_material_names;
    }

    SubMesh::SubMesh(Mesh* mesh)
    : m_mesh(mesh)
    {
    }

    SubMesh::~SubMesh()
    {
        _SafeDeleteArray_(m_positions);
        _SafeDeleteArray_(m_texcoords);
        _SafeDeleteArray_(m_normals);
        
        if (m_vao > 0)
        {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
        
        if (m_vbo_position > 0)
        {
            glDeleteBuffers(1, &m_vbo_position);
            m_vbo_position = 0;
        }
        
        if (m_vbo_texcoords > 0)
        {
            glDeleteBuffers(1, &m_vbo_texcoords);
            m_vbo_texcoords = 0;
        }
        
        if (m_vbo_normals > 0)
        {
            glDeleteBuffers(1, &m_vbo_normals);
            m_vbo_normals = 0;
        }
        
        if (m_material)
        {
            delete m_material;
            m_material = nullptr;
        }
    }
    
    Mesh* SubMesh::GetMesh() const
    {
        return m_mesh;
    }

    void SubMesh::Render()
    {
        if (this->m_material == nullptr)
        {
            //VLOG("error: failed to render submesh. no material found.")
            return;
        }

        // apply material
        this->m_material->Apply();
        
        // bind vao
        if (m_vao == 0 || m_dymc)
        {
            // create vao
            if (!m_dymc)
            {
                glGenVertexArrays(1, &m_vao);
                glBindVertexArray(m_vao);
            }
            
            auto drawFlag = m_dymc ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
            if (this->m_positions != NULL &&  this->m_vbo_position <= 0)
            {
                glGenBuffers(1, &m_vbo_position);
                glBindBuffer(GL_ARRAY_BUFFER, m_vbo_position);
                glBufferData(GL_ARRAY_BUFFER, sizeof(Vector3f) * m_vertex_count, m_positions, drawFlag);
                _SafeDeleteArray_(m_positions);
            }
            
            int position_attrib_location = this->m_material->GetProgram()->GetAttribLocation("a_position");
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo_position);
            glVertexAttribPointer(position_attrib_location, 3, GL_FLOAT, false, 0, nullptr);
            glEnableVertexAttribArray(position_attrib_location);
            
            
            if (this->m_texcoords != NULL && this->m_vbo_texcoords <= 0)
            {
                glGenBuffers(1, &m_vbo_texcoords);
                glBindBuffer(GL_ARRAY_BUFFER, m_vbo_texcoords);
                glBufferData(GL_ARRAY_BUFFER, sizeof(Vector2f) * m_vertex_count, m_texcoords, GL_STATIC_DRAW);
                _SafeDeleteArray_(m_texcoords);
            }
            
            int texcoord_attrib_location = this->m_material->GetProgram()->GetAttribLocation("a_texcoord");
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo_texcoords);
            glVertexAttribPointer(texcoord_attrib_location, 2, GL_FLOAT, false, 0, nullptr);
            glEnableVertexAttribArray(texcoord_attrib_location);
            
            if (this->m_normals != NULL && this->m_vbo_normals <= 0)
            {
                glGenBuffers(1, &m_vbo_normals);
                glBindBuffer(GL_ARRAY_BUFFER, m_vbo_normals);
                glBufferData(GL_ARRAY_BUFFER, sizeof(Vector3f) * m_vertex_count, m_normals, GL_STATIC_DRAW);
                _SafeDeleteArray_(m_normals);
            }
            
            int normal_attrib_location = this->m_material->GetProgram()->GetAttribLocation("a_normal");
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo_normals);
            glVertexAttribPointer(normal_attrib_location, 3, GL_FLOAT, false, 0, nullptr);
            glEnableVertexAttribArray(normal_attrib_location);
        }
        else
        {
            glBindVertexArray(m_vao);
        }
        
        // do rendering
        glDrawArrays(GL_TRIANGLES, 0, m_vertex_count);
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        if (this->m_positions != nullptr)
        {
            int position_attrib_location = this->m_material->GetProgram()->GetAttribLocation("a_position");
            glDisableVertexAttribArray(position_attrib_location);
        }
        
        if (this->m_texcoords != nullptr)
        {
            int texcoord_attrib_location = this->m_material->GetProgram()->GetAttribLocation("a_texcoord");
            glDisableVertexAttribArray(texcoord_attrib_location);
        }
        
        if (this->m_normals != nullptr)
        {
            int normal_attrib_location = this->m_material->GetProgram()->GetAttribLocation("a_normal");
            glDisableVertexAttribArray(normal_attrib_location);
        }
    }

    int SubMesh::GetVertexCount() const
    {
        return m_vertex_count;
    }

    std::vector<Vector3f> SubMesh::GetOriPositionData()
    {
        return m_ori_positions;
    }
    
    std::vector<ObjTri> SubMesh::GetOriTriangleData()
    {
        return m_ori_triangles;
    }
    
    Material* SubMesh::GetMaterial() const
    {
        return m_material;
    }

    void SubMesh::MarkDymc(bool dymc)
    {
        m_dymc = dymc;
    }

    void SubMesh::UpdatePositions(Vector3f *positions)
    {
        if (m_dymc && m_vbo_position > 0)
        {
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo_position);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vector3f) * m_vertex_count, positions);
        }
    }
    
    Mesh::Mesh(Renderer* renderer)
    : m_renderer(renderer)
    {
    }
    
    Mesh::~Mesh()
    {
        for (auto& submesh : m_submeshes)
        {
            delete submesh;
        }
        m_submeshes.clear();
        
        for (auto& texture_path : m_associated_textures)
        {
            auto iter = m_renderer->m_texture_cache.find(texture_path);
            if (iter != m_renderer->m_texture_cache.end())
            {
                delete iter->second.texture;
                m_renderer->m_texture_cache.erase(iter);
            }
        }
    }
    
    Renderer* Mesh::GetRenderer() const
    {
        return m_renderer;
    }

    void Mesh::RenderOpaqueSubMeshes()
    {
        for (auto& submesh : m_submeshes)
        {
            auto material = submesh->GetMaterial();
            if (material && material->IsTranslucent() == false)
                submesh->Render();
        }
    }

    void Mesh::RenderTranslucentSubMeshes()
    {
        for (auto& submesh : m_submeshes)
        {
            auto material = submesh->GetMaterial();
            if (material && material->IsTranslucent() == true)
                submesh->Render();
        }
    }

    void Mesh::replaceTexture(Texture* new_tex) {
        for (int i = 0; i < m_submeshes.size(); ++i) {
            auto submesh = m_submeshes[i];
            submesh->m_material->SetTextureParam("baseMap", new_tex);
        }
    }

    void Mesh::SetPosition(const Vector3f& position)
    {
        this->m_position = position;
        this->m_transform_dirty = true;
    }

    void Mesh::SetRotation(const Eigen::Quaternionf& rotation)
    {
        this->m_rotation = rotation;
        this->m_transform_dirty = true;
    }

    void Mesh::SetScale(const Vector3f& scale)
    {
        this->m_scale = scale;
        this->m_transform_dirty = true;
    }

    Vector3f Mesh::GetPosition() const
    {
        return this->m_position;
    }

    Eigen::Quaternionf Mesh::GetRotation() const
    {
        return this->m_rotation;
    }

    Vector3f Mesh::GetScale() const
    {
        return this->m_scale;
    }

    SubMesh* Mesh::GetSubMesh(int index) const
    {
        if (index >= 0 && index < m_submeshes.size())
            return m_submeshes[index];
        return nullptr;
    }

    void Mesh::SetTransform(const Matrix4f& Matrix4f)
    {
        this->m_transform = Matrix4f;
        this->m_transform_dirty = false;
    }

    Matrix4f Mesh::GetTransform()
    {
        if (m_transform_dirty)
        {
            Eigen::Translation3f translation(m_position);
            Eigen::AlignedScaling3f scale(m_scale);
            m_transform = (translation * m_rotation * scale).matrix();
            m_transform_dirty = false;
        }
        
        return this->m_transform;
    }
    
    Camera::Camera(Renderer* renderer)
    : m_renderer(renderer)
    {
        // 必须设置初始位置, 同时间接初始化view matrix
        // eigen的矩阵是没有初始值的, 会是随机的数
        SetPosition(Vector3f(0, 0, 1000));
        SetRotation(Quaternion(0, 0, 0, 0));
        
        int width = m_renderer->GetScreenWidth();
        int height = m_renderer->GetScreenHeight();
        //MakeOrthographic(width, height, (float)width / (float)height, 0.1f, 5000.0f);
        MakePerspective(60, (float)width/(float)height, 0.1f, 5000.0f);
    }
    
    Camera::~Camera()
    {

    }
    
    Vector3f Camera::GetPosition() const
    {
        return m_position;
    }
    
    void Camera::SetPosition(Vector3f position)
    {
        m_position = position;
        m_view_matrix_dirty = true;
        m_view_projection_matrix_dirty = true;
    }
    
    Quaternion Camera::GetRotation() const
    {
        return m_rotation;
    }
    
    void Camera::SetRotation(Quaternion rotation)
    {
        m_rotation = rotation;
        m_view_matrix_dirty = true;
        m_view_projection_matrix_dirty = true;
    }
    
    Matrix4f Camera::GetViewMatrix()
    {
        if (!m_view_matrix_dirty)
            return m_view_matrix;
        else
        {
            Eigen::Translation3f translation(m_position);
            Matrix4f camera_transform = (translation * m_rotation).matrix();
            m_view_matrix = camera_transform.inverse();
            m_view_matrix_dirty = false;
            return m_view_matrix;
        }
    }
    
    void Camera::SetViewMatrix(Matrix4f matrix)
    {
        m_view_matrix = matrix;
        m_view_matrix_dirty = false;
        m_view_projection_matrix_dirty = true;
    }
    
    Matrix4f Camera::GetProjectionMatrix() const
    {
        return m_projection_matrix;
    }
    
    void Camera::SetProjectionMatrix(Matrix4f matrix)
    {
        m_projection_matrix = matrix;
        m_view_projection_matrix_dirty = true;
    }
    
    Matrix4f Camera::GetViewProjectionMatrix()
    {
        if (!m_view_projection_matrix_dirty)
            return m_view_projection_matrix;
        else
        {
            GetViewMatrix();  // manually update if it's dirty
            m_view_projection_matrix =  m_projection_matrix * m_view_matrix;
            return m_view_projection_matrix;
        }
    }
    
    void Camera::MakeOrthographic(float width, float height, float ratio, float near, float far)
    {
        float half_width = width * 0.5f;
        float half_height = height * 0.5f;
        float left = -half_width;
        float right = half_width;
        float bottom = -half_height;
        float top = half_height;
        
        if (m_flip_y)
        {
            bottom = half_height;
            top = -half_height;
            left = half_width;
            right = -half_width;
        }
        
        memset(&m_projection_matrix, 0, sizeof(m_projection_matrix));
        float* dst = (float*)&m_projection_matrix;
        dst[0] = 2 / (right - left);
        dst[5] = 2 / (top - bottom);
        dst[12] = (left + right) / (left - right);
        dst[10] = 1 / (near - far);
        dst[13] = (top + bottom) / (bottom - top);
        dst[14] = near / (near - far);
        dst[15] = 1;
        
        m_view_projection_matrix_dirty = true;
    }
    
    void Camera::MakePerspective(float fov, float ratio, float near, float far)
    {
        float f_n = 1.0f / (far - near);
        float theta = fov * 3.141592657 / 360.0f;
        float divisor = tan(theta);
        float factor = 1.0f / divisor;
        
        if (m_flip_y)
        {
            factor = -factor;
        }
        
//        memset(&m_projection_matrix, 0, sizeof(m_projection_matrix));
        m_projection_matrix.setZero();
        float* dst = (float*)&m_projection_matrix;
        dst[0] = (1.0f / ratio) * factor;
        dst[5] = factor;
        dst[10] = (-(far + near)) * f_n;
        dst[11] = -1.0f;
        dst[14] = -2.0f * far * near * f_n;
        
        m_view_projection_matrix_dirty = true;
    }

    void Camera::MakePNPProjection(float width, float height, float fx, float fy, float near, float far)
    {
        float factor = m_flip_y ? -1.0f : 1.0f;
//        memset(&m_projection_matrix, 0, sizeof(m_projection_matrix));
        m_projection_matrix.setZero();
        float* dst = (float*)&m_projection_matrix;
#ifdef __ANDROID__
        dst[0] = 2.0f *  fx / width;
#else
        dst[0] = 2.0f *  fx / width * factor;
#endif
        dst[5] = 2.0f * fy / height * factor;
        dst[10] = -(far + near) / (far - near);
        dst[11] = -1.0f;
        dst[14] = -2.0f * far * near / (far - near);
        
        m_view_projection_matrix_dirty = true;
    }

    
    Renderer::Renderer(int screen_width, int screen_height, std::string resource_dir)
    : m_screen_width(screen_width), m_screen_height(screen_height), m_resource_dir(resource_dir)
    {
        // 创建FBO
        glGenFramebuffers(1, &m_standalone_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_standalone_fbo);
        
        glGenTextures(1, &m_standalone_color_texture);
        glBindTexture(GL_TEXTURE_2D, m_standalone_color_texture);
        
        uint* colors = new uint[screen_width * screen_height];
        memset(colors, 0, sizeof(uint) * screen_width * screen_height);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors);
        delete[] colors;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_standalone_color_texture, 0);
        
        glGenBuffers(1, &m_standalone_depth_buffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_standalone_depth_buffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, screen_width, screen_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_standalone_depth_buffer);
        
        if (m_use_msaa)
        {
            glGenFramebuffers(1, &m_msaa_fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, m_msaa_fbo);
            
            glGenBuffers(1, &m_msaa_color_buffer);
            glBindRenderbuffer(GL_RENDERBUFFER, m_msaa_color_buffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaa_samples, GL_RGBA8, screen_width, screen_height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msaa_color_buffer);
            
            glGenBuffers(1, &m_msaa_depth_buffer);
            glBindRenderbuffer(GL_RENDERBUFFER, m_msaa_depth_buffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaa_samples, GL_DEPTH_COMPONENT16, screen_width, screen_height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_msaa_depth_buffer);
        }
        
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
          VLOG(2) << "error: standalone framebuffer is not completed: " << status;
        }
        
        // 创建相机
        m_camera = new Camera(this);
    }
    
    Renderer::~Renderer()
    {
        for (auto iter=m_program_cache.begin(); iter!=m_program_cache.end(); ++iter)
        {
            delete iter->second;
        }
        m_program_cache.clear();
        
        for (auto iter=m_texture_cache.begin(); iter!=m_texture_cache.end(); ++iter)
        {
            delete iter->second.texture;
        }
        m_texture_cache.clear();
        
        if (m_camera != nullptr)
        {
            delete m_camera;
            m_camera = nullptr;
        }
        
        // remove all render meshes
        m_mesh_list.clear();
        
        // clear gl resources
        if (m_standalone_fbo > 0)
        {
            glDeleteFramebuffers(1, &m_standalone_fbo);
        }
        
        if (m_standalone_color_texture > 0)
        {
            glDeleteTextures(1, &m_standalone_color_texture);
        }
        
        if (m_standalone_depth_buffer > 0)
        {
            glDeleteRenderbuffers(1, &m_standalone_depth_buffer);
        }
        
        if (m_use_msaa)
        {
            if (m_msaa_fbo)
            {
                glDeleteFramebuffers(1, &m_msaa_fbo);
            }
            
            if (m_msaa_color_buffer)
            {
                glDeleteRenderbuffers(1, &m_msaa_color_buffer);
            }
            
            if (m_msaa_depth_buffer)
            {
                glDeleteRenderbuffers(1, &m_msaa_depth_buffer);
            }
        }
        
        if (m_background_program > 0)
        {
            glDeleteProgram(m_background_program);
            m_background_program = 0;
            m_background_uniform_loc = -1;
        }
    }

    GLuint Renderer::GetStandaloneColorTextureId() const
    {
        return m_standalone_color_texture;
    }

    void Renderer::BeginRenderNoClear() {
        if (m_standalone_fbo > 0)
        {
            if (m_use_msaa && m_msaa_fbo > 0)
            {
                //glEnable(GL_MULTISAMPLE);
                glBindFramebuffer(GL_FRAMEBUFFER, m_msaa_fbo);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msaa_color_buffer);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_msaa_depth_buffer);
            }
            else
            {
                glBindFramebuffer(GL_FRAMEBUFFER, m_standalone_fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_standalone_color_texture, 0);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_standalone_depth_buffer);
            }
        }
    }
    void Renderer::BeginRender()
    {
        if (m_standalone_fbo > 0)
        {
            if (m_use_msaa && m_msaa_fbo > 0)
            {
                //glEnable(GL_MULTISAMPLE);
                glBindFramebuffer(GL_FRAMEBUFFER, m_msaa_fbo);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msaa_color_buffer);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_msaa_depth_buffer);
            }
            else
            {
                glBindFramebuffer(GL_FRAMEBUFFER, m_standalone_fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_standalone_color_texture, 0);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_standalone_depth_buffer);
            }
            
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClearDepthf(1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
    }

    enum { ATTRIB_VERTEX, ATTRIB_TEXTURE_POSITION, NUM_ATTRIBUTES };
    void Renderer::RenderBackground(GLuint background_texture_id)
    {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        if (m_background_program == 0)
        {
            // Load vertex and fragment shaders
            const GLint attr_location[2] = {
                ATTRIB_VERTEX,
                ATTRIB_TEXTURE_POSITION,
            };
            const GLchar* attr_name[2] = {
                "position",
                "texture_coordinate",
            };
                
            // shader program
            GlhCreateProgram(kBasicVertexShader, kBasicTexturedFragmentShader, 2,
                             (const GLchar**)&attr_name[0], attr_location, &m_background_program);
            m_background_uniform_loc = glGetUniformLocation(m_background_program, "video_frame");
        }
        
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, background_texture_id);
        
        // render background
        {
            static const GLfloat square_vertices[] = {
                -1.0f, -1.0f,  // bottom left
                1.0f,  -1.0f,  // bottom right
                -1.0f, 1.0f,   // top left
                1.0f,  1.0f,   // top right
            };
            static const GLfloat texture_vertices[] = {
                0.0f, 0.0f,  // bottom left
                1.0f, 0.0f,  // bottom right
                0.0f, 1.0f,  // top left
                1.0f, 1.0f,  // top right
            };
            
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            
            // program
            glUseProgram(m_background_program);
            glUniform1i(m_background_uniform_loc, 0);
            
            // vertex storage
            GLuint vbo[2];
            glGenBuffers(2, vbo);
            GLuint vao;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            
            // vbo 0
            glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
            glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(GLfloat), square_vertices,
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(ATTRIB_VERTEX);
            glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, nullptr);
            
            // vbo 1
            glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
            glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(GLfloat), texture_vertices,
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(ATTRIB_TEXTURE_POSITION);
            glVertexAttribPointer(ATTRIB_TEXTURE_POSITION, 2, GL_FLOAT, 0, 0, nullptr);
            
            // draw
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            
            // cleanup
            glDisableVertexAttribArray(ATTRIB_VERTEX);
            glDisableVertexAttribArray(ATTRIB_TEXTURE_POSITION);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(2, vbo);
        }
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void Renderer::RenderMeshes()
    {
        // 绘制不透明物体
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        for (auto& mesh : m_mesh_list)
        {
            mesh->RenderOpaqueSubMeshes();
        }

        // 绘制半透明物体
        // 一般需要对半透明物体从后往前排序. 这里先简化, 后续补上.
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        for (auto& mesh : m_mesh_list)
        {
            mesh->RenderTranslucentSubMeshes();
        }
    }

    void Renderer::EndRender()
    {
        // 恢复到默认状态
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        
        if (m_use_msaa && m_msaa_fbo > 0)
        {
            glFlush();
            
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaa_fbo);
            glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msaa_color_buffer);
            glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_msaa_depth_buffer);
            
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_standalone_fbo);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_standalone_color_texture, 0);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_standalone_depth_buffer);
            
            glBlitFramebuffer(0, 0, m_screen_width, m_screen_height, 0, 0, m_screen_width, m_screen_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            
            glBindFramebuffer(GL_FRAMEBUFFER, m_standalone_fbo);
        }
        
        glFlush();
    }
    
    int Renderer::GetScreenWidth() const
    {
        return m_screen_width;
    }
    
    int Renderer::GetScreenHeight() const
    {
        return m_screen_height;
    }
    
    Camera* Renderer::GetCamera() const
    {
        return m_camera;
    }

    Program* Renderer::LoadProgram(const std::string& vert_file, const std::string& frag_file, const std::string& macros)
    {
        auto program_key = vert_file + frag_file + macros;
        auto iter = m_program_cache.find(program_key);
        if (iter != m_program_cache.end())
        {
            return iter->second;
        }

        Program* program = new Program();
        if (program->LoadAndCompile(vert_file, frag_file, macros))
        {
            m_program_cache[program_key] = program;
            return program;
        }
        else
        {
            delete program;
            return nullptr;
        }
    }

    Texture* Renderer::LoadTexture(const std::string& texture_file, bool* out_translucent_flag, bool generate_mipmap)
    {
        auto iter = m_texture_cache.find(texture_file);
        if (iter != m_texture_cache.end())
        {
            if (out_translucent_flag != nullptr)
            {
                *out_translucent_flag = iter->second.translucent;
            }
            return iter->second.texture;
        }
        
        ///TODO:image_frame可能为空,需要判空
        // load png from file
        ImageFrame* image_frame = getImageFrameFromPath(texture_file, ImageFormat_SRGBA);
        if (image_frame == nullptr)
        {
            return nullptr;
        }

        Texture* texture = new Texture();
        texture->m_width = image_frame->Width();
        texture->m_height = image_frame->Height();
        texture->m_format = RGBA;
        glGenTextures(1, &texture->m_gl_texture);
        glBindTexture(GL_TEXTURE_2D, texture->m_gl_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->m_width, texture->m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_frame->PixelData());
        
        if (generate_mipmap)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generate_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        
        if (out_translucent_flag != nullptr)
        {
            *out_translucent_flag = false;
        }
        
        // extra steps: check if it has alpha less than 255
        if ((image_frame->Format() == ImageFormat_SRGBA || image_frame->Format() == ImageFormat_SBGRA))
        {
            
            const uint8* pixels = image_frame->PixelData();
            for (int row=0; row<image_frame->Height(); row++)
            {
                for (int col=0; col<image_frame->Width(); col++)
                {
                    int idx = row * image_frame->WidthStep() + col * 4;
                    
                    if (out_translucent_flag != nullptr)
                    {
                        uint8 alpha = pixels[idx + 3];
                        if (alpha > 0 && alpha < 255)
                        {
                            *out_translucent_flag = true;
                            break;
                        }
                    }
                }
                
                if (out_translucent_flag != nullptr && *out_translucent_flag == true)
                {
                    break;
                }
            }
        }
        
        // check it
        delete image_frame;
    
        TextureInfo ti;
        ti.texture = texture;
        ti.translucent = out_translucent_flag ? *out_translucent_flag : false;
        m_texture_cache[texture_file] = ti;
        return texture;
    }

    void Renderer::FillCubeTextureFaces(Texture* texture, const std::string& cube_texture_file, bool load_mipmap_chain, int mip_level, int* out_face_size)
    {
        const std::string faces[6] = {
            "right",
            "left",
            "top",
            "bottom",
            "back",
            "front",
        };
        
        char face_png[512];
        
        for (int i=0; i<6; i++)
        {
            sprintf(face_png, "%s_%s_%d.png", cube_texture_file.c_str(), faces[i].c_str(), mip_level);
            ImageFrame* image_frame = getImageFrameFromPath(face_png, ImageFormat_SRGBA);
            *out_face_size = image_frame->Width();
            GLenum target_face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
            glTexImage2D(target_face, mip_level, GL_RGBA, image_frame->Width(), image_frame->Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image_frame->PixelData());
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, load_mipmap_chain ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            printf("xyk loading face png:%s\n", face_png);
            delete image_frame;
        }
    }

    Texture* Renderer::LoadCubeTexture(const std::string& cube_texture_file, bool load_mipmap_chain)
    {
        auto iter = m_texture_cache.find(cube_texture_file);
        if (iter != m_texture_cache.end())
        {
            return iter->second.texture;
        }
        
        Texture* texture = new Texture();
        texture->m_format = RGBA;
        texture->m_type = TEXTURE_CUBE;
        
        glGenTextures(1, &texture->m_gl_texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture->m_gl_texture);
        
        int face_size = 0;
        FillCubeTextureFaces(texture, cube_texture_file, load_mipmap_chain, 0, &face_size);
        texture->m_width = face_size;
        texture->m_height = face_size;
        
        if (load_mipmap_chain && face_size > 2)
        {
            int mip_level = 1;
            int size = texture->m_width / 2;
            while (size != 0)
            {
                int temp;
                FillCubeTextureFaces(texture, cube_texture_file, load_mipmap_chain, mip_level, &temp);
                size = size / 2;
                mip_level += 1;
            }
        }
        
        TextureInfo ti;
        ti.texture = texture;
        ti.translucent = false;
        m_texture_cache[cube_texture_file] = ti;
        return texture;
    }


    Mesh* Renderer::CreatePBRMesh(const std::string& mesh_file_path, const char* mirrorPath)
    {
        char temp[256];
        memcpy(temp, mesh_file_path.c_str(), mesh_file_path.length() - 4);
        temp[mesh_file_path.length()-4] = 0;
        std::string mesh_file_path_without_ext = temp;
        std::string bk_mesh_file_path_without_ext = mesh_file_path_without_ext;
        if (mirrorPath) {
            int div_pos = mesh_file_path_without_ext.rfind("/");
            std::string obj_name = mesh_file_path_without_ext.substr(div_pos);
            std::string dir = mesh_file_path_without_ext.substr(0, div_pos);
            mesh_file_path_without_ext = dir + "/" + mirrorPath + obj_name;
        }

        // load mesh
        std::string text = ReadTextFile(mesh_file_path);
        if (text.length() <= 0)
        {
            return nullptr;
        }
        
        Mesh* mesh = new Mesh(this);
        ObjMeshParser parser(mesh, (char*)text.c_str(), (int)text.length());
        bool parse_succ = false;
        std::vector<std::string> submesh_material_names = parser.Parse(&parse_succ);
        if (!parse_succ) {
            delete mesh;
            return nullptr;
        }

        if (submesh_material_names.size() == 0)
        {
            // obj只有一个material时可能不指定usemtl. 只好从 xxx.mtl 里读newmtl属性了
            std::string mtl_text = mesh_file_path_without_ext + ".mtl";
            int idx = 0;
            while (idx < mtl_text.length())
            {
                if (mtl_text[idx] == 'n' && mtl_text[idx+1] == 'e' && mtl_text[idx+2] == 'w'
                && mtl_text[idx+3] == 'm' && mtl_text[idx+4] == 't' && mtl_text[idx+5] == 'l')
                {
                    int start_idx = idx + 7;
                    int end_idx = start_idx;
                    while (end_idx < mtl_text.length() && !IsNewlineChar(mtl_text[end_idx]))
                    {
                        end_idx++;
                    }
                        
                    mtl_text[end_idx] = 0;
                    submesh_material_names.push_back(mtl_text.c_str() + start_idx);
                    break;
                }
                idx++;
            }

            // 仍然找不到. 无法知道贴图名称
            if (submesh_material_names.empty() == true)
            {
                // VLOG("error: no newmtl found in xxx.mtl")
            }
            
            return mesh;
        }

        assert(submesh_material_names.size() == mesh->m_submeshes.size() && "obj usemtl's count not equal to submesh count");
        
        std::string vert_shader = "/shaders/pbr_kh.vert";
        std::string frag_shader = "/shaders/pbr_kh.frag";
        
//        std::string mat_desc_file = mesh_file_path_without_ext + ".material";
//        std::string mat_desc_content = ReadTextFile(mat_desc_file);
//        if (mat_desc_content.size() > 1)
//        {
//            char token[128];
//            sscanf(mat_desc_content.c_str(), "%s", token);
//            std::string shader_name(token);
//            vert_shader = std::string("/shaders/") + shader_name + ".vert";
//            frag_shader = std::string("/shaders/") + shader_name + ".frag";
//        }

        // load pbr material for submeshes
        for (int i=0; i<submesh_material_names.size(); i++)
        {
            auto submesh_material_name = submesh_material_names[i];
            std::string path_prefix = mesh_file_path_without_ext + "_" + submesh_material_name + "_";
            std::string base_tex_path = path_prefix + "Base.png";
            std::string rma_tex_path = path_prefix + "RMA.png";
            std::string normal_tex_path = path_prefix + "Normal.png";
            std::string emissive_tex_path = path_prefix + "Emissive.png";
            
            bool is_translucent = false;
            Texture* base_tex = LoadTexture(base_tex_path, &is_translucent, true);
            Texture* rma_tex = LoadTexture(rma_tex_path, nullptr, true);
            Texture* normal_tex = LoadTexture(normal_tex_path, nullptr, true);
            Texture* emissive_tex = LoadTexture(emissive_tex_path, nullptr, true);
            // if error, load from base path
            if (!base_tex && mirrorPath) {
                base_tex_path = bk_mesh_file_path_without_ext + "_" + submesh_material_name + "_Base.png";
                base_tex = LoadTexture(base_tex_path, &is_translucent, true);
            }
            if (!rma_tex && mirrorPath) {
                rma_tex_path = bk_mesh_file_path_without_ext + "_" + submesh_material_name + "_RMA.png";
                rma_tex = LoadTexture(rma_tex_path, nullptr, true);
            }
            if (!normal_tex && mirrorPath) {
                normal_tex_path = bk_mesh_file_path_without_ext + "_" + submesh_material_name + "_Normal.png";
                normal_tex = LoadTexture(normal_tex_path, nullptr, true);
            }
            if (!emissive_tex && mirrorPath) {
                emissive_tex_path = bk_mesh_file_path_without_ext + "_" + submesh_material_name + "_Emissive.png";
                emissive_tex = LoadTexture(emissive_tex_path, nullptr, true);
            }
            
            mesh->m_associated_textures.emplace(base_tex_path);
            mesh->m_associated_textures.emplace(rma_tex_path);

            std::string macros = "";
            if (normal_tex != nullptr)
            {
                macros += "#define USE_NORMAL_MAP\n";
                mesh->m_associated_textures.emplace(normal_tex_path);
            }
            
            if (emissive_tex != nullptr)
            {
                macros += "#define USE_EMISSIVE_MAP\n";
                mesh->m_associated_textures.emplace(emissive_tex_path);
            }
            
            Program* program = LoadProgram(CONCAT_RESOURCE_PATH(m_resource_dir, vert_shader.c_str()), CONCAT_RESOURCE_PATH(m_resource_dir, frag_shader.c_str()), macros);
            
            if (program != nullptr)
            {
                auto submesh = mesh->m_submeshes[i];
                submesh->m_material = new Material(submesh, program);
                submesh->m_material->SetTextureParam("baseMap", base_tex);
                submesh->m_material->SetTextureParam("rmaMap", rma_tex);
                submesh->m_material->SetTranslucent(is_translucent);
                
                if (normal_tex != nullptr)
                {
                    submesh->m_material->SetTextureParam("normalMap", normal_tex);
                }
                
                if (emissive_tex != nullptr)
                {
                    submesh->m_material->SetTextureParam("emissiveMap", emissive_tex);
                }
            }
        }
        
        return mesh;
    }

    Mesh* Renderer::CreateScanMesh(const std::string& mesh_file_path, bool export_triangles)
    {
        char temp[256];
        memcpy(temp, mesh_file_path.c_str(), mesh_file_path.length() - 4);
        temp[mesh_file_path.length()-4] = 0;
        std::string mesh_file_path_without_ext = temp;

        // load mesh
        std::string text = ReadTextFile(mesh_file_path);
        if (text.length() <= 0)
        {
            return nullptr;
        }
        
        Mesh* mesh = new Mesh(this);
        ObjMeshParser parser(mesh, (char*)text.c_str(), (int)text.length(), export_triangles);
        bool succ = false;
        std::vector<std::string> submesh_material_names = parser.Parse(&succ);
        if (!succ) {
            delete mesh;
            return nullptr;
        }

        // load pbr material for submeshes
        for (int i=0; i<submesh_material_names.size(); i++)
        {
            auto submesh_material_name = submesh_material_names[i];
            std::string path_prefix = mesh_file_path_without_ext + "_" + submesh_material_name + "_";
            std::string base_tex_path = path_prefix + "Base.jpg";
            
            bool is_translucent = false;
            Texture* base_tex = LoadTexture(base_tex_path, &is_translucent);
            
            Program* program = LoadProgram(CONCAT_RESOURCE_PATH(m_resource_dir, "/shaders/scan.vert"), CONCAT_RESOURCE_PATH(m_resource_dir, "/shaders/scan.frag"), "");
            auto submesh = mesh->m_submeshes[i];
            submesh->m_material = new Material(submesh, program);
            submesh->m_material->SetTextureParam("baseMap", base_tex);
            submesh->m_material->SetTranslucent(is_translucent);
        }
        
        return mesh;
    }

    Mesh* Renderer::createUnlitMesh(const std::string& mesh_file_path)
    {
        char temp[256];
        memcpy(temp, mesh_file_path.c_str(), mesh_file_path.length() - 4);
        temp[mesh_file_path.length()-4] = 0;
        std::string mesh_file_path_without_ext = temp;

        // load mesh
        std::string text = ReadTextFile(mesh_file_path);
        if (text.length() <= 0)
        {
            return nullptr;
        }
        
        Mesh* mesh = new Mesh(this);
        ObjMeshParser parser(mesh, (char*)text.c_str(), (int)text.length());
        bool succ = false;
        std::vector<std::string> submesh_material_names = parser.Parse(&succ);
        if (!succ) {
            delete mesh;
            return nullptr;
        }

        // load pbr material for submeshes
        for (int i=0; i<submesh_material_names.size(); i++)
        {
            auto submesh_material_name = submesh_material_names[i];
            std::string path_prefix = mesh_file_path_without_ext + "_" + submesh_material_name + "_";
            std::string base_tex_path = path_prefix + "Base.png";
            
            bool is_translucent = false;
            Texture* base_tex = LoadTexture(base_tex_path, &is_translucent);
            
            Program* program = LoadProgram(CONCAT_RESOURCE_PATH(m_resource_dir, "/shaders/unlit.vert"), CONCAT_RESOURCE_PATH(m_resource_dir, "/shaders/unlit.frag"), "");
            auto submesh = mesh->m_submeshes[i];
            submesh->m_material = new Material(submesh, program);
            submesh->m_material->SetTextureParam("baseMap", base_tex);
            submesh->m_material->SetTranslucent(is_translucent);
        }
        
        return mesh;
    }

    Mesh* Renderer::CreateDepthMesh(const std::string& mesh_file_path)
    {
        char temp[256];
        memcpy(temp, mesh_file_path.c_str(), mesh_file_path.length() - 4);
        temp[mesh_file_path.length()-4] = 0;
        std::string mesh_file_path_without_ext = temp;

        // load mesh
        std::string text = ReadTextFile(mesh_file_path);
        if (text.length() <= 0)
        {
            return nullptr;
        }
        
        Mesh* mesh = new Mesh(this);
        ObjMeshParser parser(mesh, (char*)text.c_str(), (int)text.length());
        bool succ = false;
        std::vector<std::string> submesh_material_names = parser.Parse(&succ);
        if (!succ) {
            delete mesh;
            return nullptr;
        }

        // load pbr material for submeshes
        for (int i=0; i<submesh_material_names.size(); i++)
        {
            bool is_translucent = false;
            Texture* base_tex = LoadTexture(CONCAT_RESOURCE_PATH(m_resource_dir, "/textures/uv_0.jpg"), &is_translucent);
            
            Program* program = LoadProgram(CONCAT_RESOURCE_PATH(m_resource_dir, "/shaders/depth_mask.vert"), CONCAT_RESOURCE_PATH(m_resource_dir, "/shaders/depth_mask.frag"), "");
            auto submesh = mesh->m_submeshes[i];
            submesh->m_material = new Material(submesh, program);
            submesh->m_material->SetTextureParam("baseMap", base_tex);
            submesh->m_material->SetTranslucent(is_translucent);
        }
        
        return mesh;
    }

    Mesh* Renderer::CreateOccluderMesh(const std::string& mesh_file_path)
    {
        char temp[256];
        memcpy(temp, mesh_file_path.c_str(), mesh_file_path.length() - 4);
        temp[mesh_file_path.length()-4] = 0;
        std::string mesh_file_path_without_ext = temp;

        // load mesh
        std::string text = ReadTextFile(mesh_file_path);
        if (text.length() <= 0)
        {
            return nullptr;
        }
        
        Mesh* mesh = new Mesh(this);
        bool succ = false;
        ObjMeshParser parser(mesh, (char*)text.c_str(), (int)text.length());
        std::vector<std::string> submesh_material_names = parser.Parse(&succ);
        if (!succ) {
            delete mesh;
            return nullptr;
        }
        m_mesh_list.push_back(mesh);

        // load pbr material for submeshes
        for (int i=0; i<mesh->m_submeshes.size(); i++)
        {
            Program* program = LoadProgram(CONCAT_RESOURCE_PATH(m_resource_dir, "/shaders/occluder.vert"), CONCAT_RESOURCE_PATH(m_resource_dir, "/shaders/occluder.frag"), "");
            auto submesh = mesh->m_submeshes[i];
            submesh->m_material = new Material(submesh, program);
        }
        
        return mesh;
    }

    void Renderer::AddMesh(Mesh* mesh)
    {
        if (mesh == nullptr)
            return;
        
        if (std::find(this->m_mesh_list.begin(), this->m_mesh_list.end(), mesh) == this->m_mesh_list.end())
        {
            this->m_mesh_list.push_back(mesh);
        }
    }

    void Renderer::RemoveMesh(Mesh *mesh)
    {
        if (mesh == nullptr)
            return;
        
        auto iter = std::find(this->m_mesh_list.begin(), this->m_mesh_list.end(), mesh);
        if (iter != this->m_mesh_list.end())
        {
            this->m_mesh_list.erase(iter);
        }
    }
    
    Texture* Renderer::GetDiffuseEnvTexture()
    {
        if (m_diffuse_env_texture == nullptr)
        {
            m_diffuse_env_texture = LoadTexture(CONCAT_RESOURCE_PATH(m_resource_dir, "/textures/diffuse.png"), nullptr, false);
        }
        return m_diffuse_env_texture;
    }

    Texture* Renderer::GetSpecularEnvTexture()
    {
        if (m_specular_env_texture == nullptr)
        {
            m_specular_env_texture = LoadTexture(CONCAT_RESOURCE_PATH(m_resource_dir, "/textures/environment.png"), nullptr, true);
        }
        return m_specular_env_texture;
    }

    Texture* Renderer::GetIblBrdfLutTexture()
    {
        if (m_ibl_brdf_lut_texture == nullptr)
        {
            m_ibl_brdf_lut_texture = LoadTexture(CONCAT_RESOURCE_PATH(m_resource_dir, "/textures/brdfLUT.png"), nullptr, false);
        }
        return m_ibl_brdf_lut_texture;
    }

    Texture* Renderer::GetIblDiffuseEnvTexture()
    {
        if (m_ibl_diffuse_env_texture == nullptr)
        {
            m_ibl_diffuse_env_texture = LoadCubeTexture(CONCAT_RESOURCE_PATH(m_resource_dir, "/textures/papermill/diffuse/diffuse"));
        }
        return m_ibl_diffuse_env_texture;
    }

    Texture* Renderer::GetIblSpecularEnvTexture()
    {
        if (m_ibl_specular_env_texture == nullptr)
        {
            m_ibl_specular_env_texture = LoadCubeTexture(CONCAT_RESOURCE_PATH(m_resource_dir, "/textures/papermill/specular/specular"), true);
        }
        return m_ibl_specular_env_texture;
    }
}
