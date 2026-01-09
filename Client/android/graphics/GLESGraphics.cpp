/*
 * MTA:SA Android - OpenGL ES Graphics Implementation
 */

#include "GLESGraphics.h"
#include <cmath>
#include <cstring>

namespace MTA::Android::Graphics
{
    //=========================================================================
    // Global Instance
    //=========================================================================

    GLESGraphics* g_pGraphics = nullptr;

    bool InitializeGraphics(EGLDisplay display, EGLSurface surface, EGLContext context)
    {
        if (g_pGraphics)
        {
            GL_LOGE("Graphics already initialized");
            return false;
        }

        g_pGraphics = new GLESGraphics();
        if (!g_pGraphics->Initialize(display, surface, context))
        {
            delete g_pGraphics;
            g_pGraphics = nullptr;
            return false;
        }

        return true;
    }

    void ShutdownGraphics()
    {
        if (g_pGraphics)
        {
            g_pGraphics->Shutdown();
            delete g_pGraphics;
            g_pGraphics = nullptr;
        }
    }

    //=========================================================================
    // Built-in Shader Sources
    //=========================================================================

    static const char* VERTEX_SHADER_COLOR = R"(#version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec4 aColor;

        uniform mat4 uMVP;

        out vec4 vColor;

        void main()
        {
            gl_Position = uMVP * vec4(aPosition, 1.0);
            vColor = aColor;
        }
    )";

    static const char* FRAGMENT_SHADER_COLOR = R"(#version 300 es
        precision mediump float;

        in vec4 vColor;
        out vec4 fragColor;

        void main()
        {
            fragColor = vColor;
        }
    )";

    static const char* VERTEX_SHADER_TEXTURE = R"(#version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec2 aTexCoord;

        uniform mat4 uMVP;

        out vec2 vTexCoord;

        void main()
        {
            gl_Position = uMVP * vec4(aPosition, 1.0);
            vTexCoord = aTexCoord;
        }
    )";

    static const char* FRAGMENT_SHADER_TEXTURE = R"(#version 300 es
        precision mediump float;

        in vec2 vTexCoord;
        out vec4 fragColor;

        uniform sampler2D uTexture;

        void main()
        {
            fragColor = texture(uTexture, vTexCoord);
        }
    )";

    static const char* VERTEX_SHADER_TEXTURE_COLOR = R"(#version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec4 aColor;
        layout(location = 2) in vec2 aTexCoord;

        uniform mat4 uMVP;

        out vec4 vColor;
        out vec2 vTexCoord;

        void main()
        {
            gl_Position = uMVP * vec4(aPosition, 1.0);
            vColor = aColor;
            vTexCoord = aTexCoord;
        }
    )";

    static const char* FRAGMENT_SHADER_TEXTURE_COLOR = R"(#version 300 es
        precision mediump float;

        in vec4 vColor;
        in vec2 vTexCoord;
        out vec4 fragColor;

        uniform sampler2D uTexture;

        void main()
        {
            fragColor = texture(uTexture, vTexCoord) * vColor;
        }
    )";

    //=========================================================================
    // GLESTexture Implementation
    //=========================================================================

    bool GLESTexture::Create(uint32_t width, uint32_t height, TextureFormat format, const void* data)
    {
        Destroy();

        m_width = width;
        m_height = height;
        m_format = format;

        glGenTextures(1, &m_textureId);
        glBindTexture(GL_TEXTURE_2D, m_textureId);

        // Set default parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLenum glFormat, glType, glInternalFormat;
        switch (format)
        {
            case TextureFormat::RGBA8:
                glInternalFormat = GL_RGBA8;
                glFormat = GL_RGBA;
                glType = GL_UNSIGNED_BYTE;
                break;
            case TextureFormat::RGB8:
                glInternalFormat = GL_RGB8;
                glFormat = GL_RGB;
                glType = GL_UNSIGNED_BYTE;
                break;
            case TextureFormat::RGB565:
                glInternalFormat = GL_RGB565;
                glFormat = GL_RGB;
                glType = GL_UNSIGNED_SHORT_5_6_5;
                break;
            case TextureFormat::ALPHA8:
                glInternalFormat = GL_R8;
                glFormat = GL_RED;
                glType = GL_UNSIGNED_BYTE;
                break;
            default:
                glInternalFormat = GL_RGBA8;
                glFormat = GL_RGBA;
                glType = GL_UNSIGNED_BYTE;
                break;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat, width, height, 0, glFormat, glType, data);

        glBindTexture(GL_TEXTURE_2D, 0);

        return !GLESGraphics::CheckGLError("GLESTexture::Create");
    }

    void GLESTexture::Destroy()
    {
        if (m_textureId)
        {
            glDeleteTextures(1, &m_textureId);
            m_textureId = 0;
        }
    }

    void GLESTexture::Bind(uint32_t unit) const
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, m_textureId);
    }

    void GLESTexture::Unbind(uint32_t unit) const
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    bool GLESTexture::UpdateData(const void* data, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glBindTexture(GL_TEXTURE_2D, m_textureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);
        return !GLESGraphics::CheckGLError("GLESTexture::UpdateData");
    }

    //=========================================================================
    // GLESShader Implementation
    //=========================================================================

    bool GLESShader::CreateFromSource(const char* vertexSource, const char* fragmentSource)
    {
        Destroy();

        m_vertexId = CompileShader(GL_VERTEX_SHADER, vertexSource);
        if (!m_vertexId) return false;

        m_fragmentId = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (!m_fragmentId)
        {
            glDeleteShader(m_vertexId);
            m_vertexId = 0;
            return false;
        }

        m_programId = glCreateProgram();
        glAttachShader(m_programId, m_vertexId);
        glAttachShader(m_programId, m_fragmentId);

        if (!LinkProgram())
        {
            Destroy();
            return false;
        }

        return true;
    }

    GLuint GLESShader::CompileShader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            GL_LOGE("Shader compilation failed: %s", infoLog);
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    bool GLESShader::LinkProgram()
    {
        glLinkProgram(m_programId);

        GLint success;
        glGetProgramiv(m_programId, GL_LINK_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetProgramInfoLog(m_programId, 512, nullptr, infoLog);
            GL_LOGE("Shader linking failed: %s", infoLog);
            return false;
        }

        return true;
    }

    void GLESShader::Destroy()
    {
        if (m_programId)
        {
            glDeleteProgram(m_programId);
            m_programId = 0;
        }
        if (m_vertexId)
        {
            glDeleteShader(m_vertexId);
            m_vertexId = 0;
        }
        if (m_fragmentId)
        {
            glDeleteShader(m_fragmentId);
            m_fragmentId = 0;
        }
        m_uniformCache.clear();
    }

    void GLESShader::Bind() const
    {
        glUseProgram(m_programId);
    }

    void GLESShader::Unbind() const
    {
        glUseProgram(0);
    }

    GLint GLESShader::GetUniformLocation(const char* name) const
    {
        auto it = m_uniformCache.find(name);
        if (it != m_uniformCache.end())
            return it->second;

        GLint location = glGetUniformLocation(m_programId, name);
        m_uniformCache[name] = location;
        return location;
    }

    GLint GLESShader::GetAttribLocation(const char* name) const
    {
        return glGetAttribLocation(m_programId, name);
    }

    void GLESShader::SetUniform1i(const char* name, int value)
    {
        glUniform1i(GetUniformLocation(name), value);
    }

    void GLESShader::SetUniform1f(const char* name, float value)
    {
        glUniform1f(GetUniformLocation(name), value);
    }

    void GLESShader::SetUniform2f(const char* name, float x, float y)
    {
        glUniform2f(GetUniformLocation(name), x, y);
    }

    void GLESShader::SetUniform3f(const char* name, float x, float y, float z)
    {
        glUniform3f(GetUniformLocation(name), x, y, z);
    }

    void GLESShader::SetUniform4f(const char* name, float x, float y, float z, float w)
    {
        glUniform4f(GetUniformLocation(name), x, y, z, w);
    }

    void GLESShader::SetUniformMatrix4fv(const char* name, const float* matrix)
    {
        glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, matrix);
    }

    void GLESShader::SetUniformColor(const char* name, uint32_t color)
    {
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        glUniform4f(GetUniformLocation(name), r, g, b, a);
    }

    //=========================================================================
    // GLESRenderTarget Implementation
    //=========================================================================

    bool GLESRenderTarget::Create(uint32_t width, uint32_t height, bool hasDepth)
    {
        Destroy();

        m_width = width;
        m_height = height;

        // Create framebuffer
        glGenFramebuffers(1, &m_fboId);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboId);

        // Create color texture
        m_colorTexture = std::make_unique<GLESTexture>();
        if (!m_colorTexture->Create(width, height, TextureFormat::RGBA8, nullptr))
        {
            Destroy();
            return false;
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               m_colorTexture->GetId(), 0);

        // Create depth buffer if needed
        if (hasDepth)
        {
            glGenRenderbuffers(1, &m_depthRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER, m_depthRbo);
        }

        // Check framebuffer completeness
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            GL_LOGE("Framebuffer incomplete: 0x%x", status);
            Destroy();
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void GLESRenderTarget::Destroy()
    {
        if (m_fboId)
        {
            glDeleteFramebuffers(1, &m_fboId);
            m_fboId = 0;
        }
        if (m_depthRbo)
        {
            glDeleteRenderbuffers(1, &m_depthRbo);
            m_depthRbo = 0;
        }
        m_colorTexture.reset();
    }

    void GLESRenderTarget::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboId);
        glViewport(0, 0, m_width, m_height);
    }

    void GLESRenderTarget::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    //=========================================================================
    // GLESVertexBuffer Implementation
    //=========================================================================

    bool GLESVertexBuffer::Create(uint32_t size, uint32_t stride, const void* data, bool dynamic)
    {
        Destroy();

        m_size = size;
        m_stride = stride;
        m_dynamic = dynamic;

        glGenVertexArrays(1, &m_vaoId);
        glGenBuffers(1, &m_vboId);

        glBindVertexArray(m_vaoId);
        glBindBuffer(GL_ARRAY_BUFFER, m_vboId);
        glBufferData(GL_ARRAY_BUFFER, size, data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        return !GLESGraphics::CheckGLError("GLESVertexBuffer::Create");
    }

    void GLESVertexBuffer::Destroy()
    {
        if (m_vboId)
        {
            glDeleteBuffers(1, &m_vboId);
            m_vboId = 0;
        }
        if (m_vaoId)
        {
            glDeleteVertexArrays(1, &m_vaoId);
            m_vaoId = 0;
        }
    }

    bool GLESVertexBuffer::UpdateData(const void* data, uint32_t offset, uint32_t size)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_vboId);
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return !GLESGraphics::CheckGLError("GLESVertexBuffer::UpdateData");
    }

    void GLESVertexBuffer::Bind() const
    {
        glBindVertexArray(m_vaoId);
    }

    void GLESVertexBuffer::Unbind() const
    {
        glBindVertexArray(0);
    }

    void GLESVertexBuffer::SetVertexAttrib(GLuint index, GLint size, GLenum type, GLboolean normalized, uint32_t offset)
    {
        glBindVertexArray(m_vaoId);
        glBindBuffer(GL_ARRAY_BUFFER, m_vboId);
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(index, size, type, normalized, m_stride, (void*)(uintptr_t)offset);
        glBindVertexArray(0);
    }

    //=========================================================================
    // GLESIndexBuffer Implementation
    //=========================================================================

    bool GLESIndexBuffer::Create(uint32_t count, bool use32Bit, const void* data, bool dynamic)
    {
        Destroy();

        m_count = count;
        m_is32Bit = use32Bit;
        m_dynamic = dynamic;

        glGenBuffers(1, &m_iboId);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_iboId);

        uint32_t size = count * (use32Bit ? 4 : 2);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        return !GLESGraphics::CheckGLError("GLESIndexBuffer::Create");
    }

    void GLESIndexBuffer::Destroy()
    {
        if (m_iboId)
        {
            glDeleteBuffers(1, &m_iboId);
            m_iboId = 0;
        }
    }

    void GLESIndexBuffer::Bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_iboId);
    }

    void GLESIndexBuffer::Unbind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    //=========================================================================
    // GLESGraphics Implementation
    //=========================================================================

    GLESGraphics::GLESGraphics()
        : m_eglDisplay(EGL_NO_DISPLAY)
        , m_eglSurface(EGL_NO_SURFACE)
        , m_eglContext(EGL_NO_CONTEXT)
        , m_screenWidth(0)
        , m_screenHeight(0)
        , m_initialized(false)
        , m_currentBlendMode(BlendMode::NONE)
        , m_currentCullMode(CullMode::NONE)
        , m_depthTestEnabled(false)
        , m_depthWriteEnabled(true)
        , m_mvpDirty(true)
        , m_currentShader(nullptr)
        , m_currentRenderTarget(nullptr)
    {
    }

    GLESGraphics::~GLESGraphics()
    {
        Shutdown();
    }

    bool GLESGraphics::Initialize(EGLDisplay display, EGLSurface surface, EGLContext context)
    {
        m_eglDisplay = display;
        m_eglSurface = surface;
        m_eglContext = context;

        // Query surface dimensions
        EGLint width, height;
        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);
        m_screenWidth = width;
        m_screenHeight = height;

        GL_LOGI("Initializing OpenGL ES Graphics");
        GL_LOGI("  Screen: %dx%d", m_screenWidth, m_screenHeight);
        GL_LOGI("  GL_VENDOR: %s", glGetString(GL_VENDOR));
        GL_LOGI("  GL_RENDERER: %s", glGetString(GL_RENDERER));
        GL_LOGI("  GL_VERSION: %s", glGetString(GL_VERSION));

        // Create built-in shaders
        CreateBuiltInShaders();

        // Create dynamic vertex buffer for immediate mode drawing
        m_dynamicVB = std::make_unique<GLESVertexBuffer>();
        m_dynamicVB->Create(64 * 1024, sizeof(VertexPositionColorTexture), nullptr, true);

        // Setup default vertex attributes for dynamic VB
        m_dynamicVB->SetVertexAttrib(0, 3, GL_FLOAT, GL_FALSE, 0);  // Position
        m_dynamicVB->SetVertexAttrib(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 12);  // Color
        m_dynamicVB->SetVertexAttrib(2, 2, GL_FLOAT, GL_FALSE, 16);  // TexCoord

        // Set default state
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        m_initialized = true;
        GL_LOGI("OpenGL ES Graphics initialized successfully");
        return true;
    }

    void GLESGraphics::Shutdown()
    {
        if (!m_initialized) return;

        DestroyBuiltInShaders();
        m_dynamicVB.reset();

        m_initialized = false;
        GL_LOGI("OpenGL ES Graphics shut down");
    }

    void GLESGraphics::CreateBuiltInShaders()
    {
        m_shaderColor = std::make_unique<GLESShader>();
        m_shaderColor->CreateFromSource(VERTEX_SHADER_COLOR, FRAGMENT_SHADER_COLOR);

        m_shaderTexture = std::make_unique<GLESShader>();
        m_shaderTexture->CreateFromSource(VERTEX_SHADER_TEXTURE, FRAGMENT_SHADER_TEXTURE);

        m_shaderTextureColor = std::make_unique<GLESShader>();
        m_shaderTextureColor->CreateFromSource(VERTEX_SHADER_TEXTURE_COLOR, FRAGMENT_SHADER_TEXTURE_COLOR);
    }

    void GLESGraphics::DestroyBuiltInShaders()
    {
        m_shaderColor.reset();
        m_shaderTexture.reset();
        m_shaderTextureColor.reset();
    }

    void GLESGraphics::BeginFrame()
    {
        // Update MVP if needed
        if (m_mvpDirty)
        {
            Matrix4x4 vp = Matrix4x4::Multiply(m_viewMatrix, m_projectionMatrix);
            m_mvpMatrix = Matrix4x4::Multiply(m_worldMatrix, vp);
            m_mvpDirty = false;
        }
    }

    void GLESGraphics::EndFrame()
    {
        eglSwapBuffers(m_eglDisplay, m_eglSurface);
    }

    void GLESGraphics::SetBlendMode(BlendMode mode)
    {
        if (m_currentBlendMode == mode) return;
        m_currentBlendMode = mode;

        switch (mode)
        {
            case BlendMode::NONE:
                glDisable(GL_BLEND);
                break;
            case BlendMode::BLEND:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BlendMode::ADD:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
            case BlendMode::MODULATE_ADD:
                glEnable(GL_BLEND);
                glBlendFunc(GL_DST_COLOR, GL_ONE);
                break;
            case BlendMode::OVERWRITE:
                glDisable(GL_BLEND);
                break;
        }
    }

    void GLESGraphics::SetCullMode(CullMode mode)
    {
        if (m_currentCullMode == mode) return;
        m_currentCullMode = mode;

        switch (mode)
        {
            case CullMode::NONE:
                glDisable(GL_CULL_FACE);
                break;
            case CullMode::CW:
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CCW);
                glCullFace(GL_BACK);
                break;
            case CullMode::CCW:
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CW);
                glCullFace(GL_BACK);
                break;
        }
    }

    void GLESGraphics::SetDepthTest(bool enable)
    {
        if (m_depthTestEnabled == enable) return;
        m_depthTestEnabled = enable;

        if (enable)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }

    void GLESGraphics::SetDepthWrite(bool enable)
    {
        if (m_depthWriteEnabled == enable) return;
        m_depthWriteEnabled = enable;
        glDepthMask(enable ? GL_TRUE : GL_FALSE);
    }

    void GLESGraphics::SetScissorTest(bool enable, int x, int y, int width, int height)
    {
        if (enable)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(x, m_screenHeight - y - height, width, height);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    void GLESGraphics::SetViewport(int x, int y, int width, int height)
    {
        glViewport(x, y, width, height);
    }

    void GLESGraphics::Clear(bool color, bool depth, bool stencil, uint32_t clearColor)
    {
        GLbitfield mask = 0;

        if (color)
        {
            float r = ((clearColor >> 16) & 0xFF) / 255.0f;
            float g = ((clearColor >> 8) & 0xFF) / 255.0f;
            float b = (clearColor & 0xFF) / 255.0f;
            float a = ((clearColor >> 24) & 0xFF) / 255.0f;
            glClearColor(r, g, b, a);
            mask |= GL_COLOR_BUFFER_BIT;
        }

        if (depth)
        {
            glClearDepthf(1.0f);
            mask |= GL_DEPTH_BUFFER_BIT;
        }

        if (stencil)
        {
            glClearStencil(0);
            mask |= GL_STENCIL_BUFFER_BIT;
        }

        if (mask)
            glClear(mask);
    }

    void GLESGraphics::SetWorldMatrix(const Matrix4x4& matrix)
    {
        m_worldMatrix = matrix;
        m_mvpDirty = true;
    }

    void GLESGraphics::SetViewMatrix(const Matrix4x4& matrix)
    {
        m_viewMatrix = matrix;
        m_mvpDirty = true;
    }

    void GLESGraphics::SetProjectionMatrix(const Matrix4x4& matrix)
    {
        m_projectionMatrix = matrix;
        m_mvpDirty = true;
    }

    void GLESGraphics::SetOrthoProjection(float width, float height)
    {
        m_projectionMatrix.Ortho(0, width, height, 0, -1, 1);
        m_mvpDirty = true;
    }

    void GLESGraphics::SetPerspectiveProjection(float fovy, float aspect, float near, float far)
    {
        m_projectionMatrix.Perspective(fovy, aspect, near, far);
        m_mvpDirty = true;
    }

    void GLESGraphics::DrawRect(float x, float y, float width, float height, uint32_t color)
    {
        VertexPositionColorTexture vertices[6];

        // Convert color from ARGB to RGBA
        uint32_t rgba = ((color & 0xFF) << 24) | ((color & 0xFF00) << 8) |
                        ((color & 0xFF0000) >> 8) | ((color >> 24) & 0xFF);

        // Triangle 1
        vertices[0] = {x, y, 0, rgba, 0, 0};
        vertices[1] = {x + width, y, 0, rgba, 1, 0};
        vertices[2] = {x + width, y + height, 0, rgba, 1, 1};

        // Triangle 2
        vertices[3] = {x, y, 0, rgba, 0, 0};
        vertices[4] = {x + width, y + height, 0, rgba, 1, 1};
        vertices[5] = {x, y + height, 0, rgba, 0, 1};

        m_dynamicVB->UpdateData(vertices, 0, sizeof(vertices));

        m_shaderColor->Bind();
        if (m_mvpDirty)
        {
            Matrix4x4 vp = Matrix4x4::Multiply(m_viewMatrix, m_projectionMatrix);
            m_mvpMatrix = Matrix4x4::Multiply(m_worldMatrix, vp);
            m_mvpDirty = false;
        }
        m_shaderColor->SetUniformMatrix4fv("uMVP", m_mvpMatrix.m);

        m_dynamicVB->Bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        m_dynamicVB->Unbind();
    }

    void GLESGraphics::DrawTexture(GLESTexture* texture, float x, float y, float width, float height,
                                    float u1, float v1, float u2, float v2, uint32_t color)
    {
        if (!texture) return;

        VertexPositionColorTexture vertices[6];

        uint32_t rgba = ((color & 0xFF) << 24) | ((color & 0xFF00) << 8) |
                        ((color & 0xFF0000) >> 8) | ((color >> 24) & 0xFF);

        vertices[0] = {x, y, 0, rgba, u1, v1};
        vertices[1] = {x + width, y, 0, rgba, u2, v1};
        vertices[2] = {x + width, y + height, 0, rgba, u2, v2};
        vertices[3] = {x, y, 0, rgba, u1, v1};
        vertices[4] = {x + width, y + height, 0, rgba, u2, v2};
        vertices[5] = {x, y + height, 0, rgba, u1, v2};

        m_dynamicVB->UpdateData(vertices, 0, sizeof(vertices));

        texture->Bind(0);
        m_shaderTextureColor->Bind();
        m_shaderTextureColor->SetUniformMatrix4fv("uMVP", m_mvpMatrix.m);
        m_shaderTextureColor->SetUniform1i("uTexture", 0);

        m_dynamicVB->Bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        m_dynamicVB->Unbind();

        texture->Unbind(0);
    }

    void GLESGraphics::DrawLine2D(float x1, float y1, float x2, float y2, uint32_t color, float width)
    {
        VertexPositionColorTexture vertices[2];

        uint32_t rgba = ((color & 0xFF) << 24) | ((color & 0xFF00) << 8) |
                        ((color & 0xFF0000) >> 8) | ((color >> 24) & 0xFF);

        vertices[0] = {x1, y1, 0, rgba, 0, 0};
        vertices[1] = {x2, y2, 0, rgba, 0, 0};

        m_dynamicVB->UpdateData(vertices, 0, sizeof(vertices));

        glLineWidth(width);

        m_shaderColor->Bind();
        m_shaderColor->SetUniformMatrix4fv("uMVP", m_mvpMatrix.m);

        m_dynamicVB->Bind();
        glDrawArrays(GL_LINES, 0, 2);
        m_dynamicVB->Unbind();
    }

    void GLESGraphics::SetRenderTarget(GLESRenderTarget* target)
    {
        if (target)
        {
            target->Bind();
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, m_screenWidth, m_screenHeight);
        }
        m_currentRenderTarget = target;
    }

    bool GLESGraphics::CheckGLError(const char* operation)
    {
        GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            const char* errorStr;
            switch (error)
            {
                case GL_INVALID_ENUM: errorStr = "GL_INVALID_ENUM"; break;
                case GL_INVALID_VALUE: errorStr = "GL_INVALID_VALUE"; break;
                case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
                case GL_OUT_OF_MEMORY: errorStr = "GL_OUT_OF_MEMORY"; break;
                case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
                default: errorStr = "Unknown"; break;
            }
            GL_LOGE("GL Error in %s: %s (0x%x)", operation, errorStr, error);
            return true;
        }
        return false;
    }

} // namespace MTA::Android::Graphics
