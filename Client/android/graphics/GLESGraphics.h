/*
 * MTA:SA Android - OpenGL ES Graphics Backend
 *
 * This replaces CGraphics (D3D9) with an OpenGL ES 3.0 implementation.
 * Provides equivalent functionality for MTA's rendering system on Android.
 *
 * Architecture:
 *   D3D9 (Windows)     →  OpenGL ES 3.0 (Android)
 *   IDirect3DDevice9   →  EGL Context + GL State
 *   ID3DXEffect        →  GLSL Shader Programs
 *   IDirect3DTexture9  →  GL Textures
 *   Vertex/Index Buffers → VBOs/VAOs
 */

#ifndef GLES_GRAPHICS_H
#define GLES_GRAPHICS_H

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <EGL/egl.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#ifdef __ANDROID__
#include <android/log.h>
#define GL_LOG_TAG "MTA-GLES"
#define GL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, GL_LOG_TAG, __VA_ARGS__)
#define GL_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, GL_LOG_TAG, __VA_ARGS__)
#define GL_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, GL_LOG_TAG, __VA_ARGS__)
#else
#define GL_LOGI(...) printf(__VA_ARGS__)
#define GL_LOGE(...) fprintf(stderr, __VA_ARGS__)
#define GL_LOGD(...) printf(__VA_ARGS__)
#endif

namespace MTA::Android::Graphics
{
    //=========================================================================
    // Forward Declarations
    //=========================================================================

    class GLESTexture;
    class GLESShader;
    class GLESRenderTarget;
    class GLESVertexBuffer;
    class GLESIndexBuffer;

    //=========================================================================
    // Enumerations (matching D3D9 equivalents)
    //=========================================================================

    enum class BlendMode
    {
        NONE,           // No blending
        BLEND,          // Standard alpha blend
        ADD,            // Additive blend
        MODULATE_ADD,   // Modulate + Add
        OVERWRITE       // No blend, direct overwrite
    };

    enum class PrimitiveType
    {
        POINT_LIST,
        LINE_LIST,
        LINE_STRIP,
        TRIANGLE_LIST,
        TRIANGLE_STRIP,
        TRIANGLE_FAN
    };

    enum class CullMode
    {
        NONE,
        CW,     // Clockwise
        CCW     // Counter-clockwise
    };

    enum class TextureFormat
    {
        UNKNOWN,
        RGBA8,
        RGB8,
        RGB565,
        RGBA4,
        RGBA5551,
        ALPHA8,
        LUMINANCE8,
        LUMINANCE_ALPHA,
        // Compressed formats
        ETC1,
        ETC2_RGB,
        ETC2_RGBA,
        // Depth formats
        DEPTH16,
        DEPTH24,
        DEPTH24_STENCIL8
    };

    //=========================================================================
    // Vertex Structures
    //=========================================================================

    struct Vertex2D
    {
        float x, y;
        float u, v;
        uint32_t color;  // RGBA packed
    };

    struct Vertex3D
    {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
        uint32_t color;
    };

    struct VertexPositionColor
    {
        float x, y, z;
        uint32_t color;
    };

    struct VertexPositionTexture
    {
        float x, y, z;
        float u, v;
    };

    struct VertexPositionColorTexture
    {
        float x, y, z;
        uint32_t color;
        float u, v;
    };

    //=========================================================================
    // Matrix Utilities
    //=========================================================================

    struct Matrix4x4
    {
        float m[16];

        Matrix4x4() { Identity(); }

        void Identity()
        {
            for (int i = 0; i < 16; i++) m[i] = 0.0f;
            m[0] = m[5] = m[10] = m[15] = 1.0f;
        }

        void Ortho(float left, float right, float bottom, float top, float near, float far)
        {
            Identity();
            m[0] = 2.0f / (right - left);
            m[5] = 2.0f / (top - bottom);
            m[10] = -2.0f / (far - near);
            m[12] = -(right + left) / (right - left);
            m[13] = -(top + bottom) / (top - bottom);
            m[14] = -(far + near) / (far - near);
        }

        void Perspective(float fovy, float aspect, float near, float far)
        {
            Identity();
            float f = 1.0f / tanf(fovy * 0.5f);
            m[0] = f / aspect;
            m[5] = f;
            m[10] = (far + near) / (near - far);
            m[11] = -1.0f;
            m[14] = (2.0f * far * near) / (near - far);
            m[15] = 0.0f;
        }

        static Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b)
        {
            Matrix4x4 result;
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    result.m[i * 4 + j] = 0;
                    for (int k = 0; k < 4; k++)
                    {
                        result.m[i * 4 + j] += a.m[i * 4 + k] * b.m[k * 4 + j];
                    }
                }
            }
            return result;
        }
    };

    //=========================================================================
    // GLESTexture - OpenGL ES Texture Wrapper
    //=========================================================================

    class GLESTexture
    {
    public:
        GLESTexture() : m_textureId(0), m_width(0), m_height(0), m_format(TextureFormat::RGBA8) {}
        ~GLESTexture() { Destroy(); }

        bool Create(uint32_t width, uint32_t height, TextureFormat format, const void* data = nullptr);
        bool CreateFromFile(const char* path);
        void Destroy();

        void Bind(uint32_t unit = 0) const;
        void Unbind(uint32_t unit = 0) const;

        bool UpdateData(const void* data, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

        GLuint GetId() const { return m_textureId; }
        uint32_t GetWidth() const { return m_width; }
        uint32_t GetHeight() const { return m_height; }
        TextureFormat GetFormat() const { return m_format; }

    private:
        GLuint m_textureId;
        uint32_t m_width;
        uint32_t m_height;
        TextureFormat m_format;
    };

    //=========================================================================
    // GLESShader - OpenGL ES Shader Program
    //=========================================================================

    class GLESShader
    {
    public:
        GLESShader() : m_programId(0), m_vertexId(0), m_fragmentId(0) {}
        ~GLESShader() { Destroy(); }

        bool CreateFromSource(const char* vertexSource, const char* fragmentSource);
        bool CreateFromFiles(const char* vertexPath, const char* fragmentPath);
        void Destroy();

        void Bind() const;
        void Unbind() const;

        // Uniform setters
        void SetUniform1i(const char* name, int value);
        void SetUniform1f(const char* name, float value);
        void SetUniform2f(const char* name, float x, float y);
        void SetUniform3f(const char* name, float x, float y, float z);
        void SetUniform4f(const char* name, float x, float y, float z, float w);
        void SetUniformMatrix4fv(const char* name, const float* matrix);
        void SetUniformColor(const char* name, uint32_t color);

        GLuint GetId() const { return m_programId; }
        GLint GetUniformLocation(const char* name) const;
        GLint GetAttribLocation(const char* name) const;

    private:
        GLuint CompileShader(GLenum type, const char* source);
        bool LinkProgram();

        GLuint m_programId;
        GLuint m_vertexId;
        GLuint m_fragmentId;
        mutable std::unordered_map<std::string, GLint> m_uniformCache;
    };

    //=========================================================================
    // GLESRenderTarget - Framebuffer Object
    //=========================================================================

    class GLESRenderTarget
    {
    public:
        GLESRenderTarget() : m_fboId(0), m_colorTexture(nullptr), m_depthRbo(0),
                             m_width(0), m_height(0) {}
        ~GLESRenderTarget() { Destroy(); }

        bool Create(uint32_t width, uint32_t height, bool hasDepth = true);
        void Destroy();

        void Bind();
        void Unbind();

        GLESTexture* GetColorTexture() { return m_colorTexture.get(); }
        uint32_t GetWidth() const { return m_width; }
        uint32_t GetHeight() const { return m_height; }

    private:
        GLuint m_fboId;
        std::unique_ptr<GLESTexture> m_colorTexture;
        GLuint m_depthRbo;
        uint32_t m_width;
        uint32_t m_height;
    };

    //=========================================================================
    // GLESVertexBuffer - Vertex Buffer Object
    //=========================================================================

    class GLESVertexBuffer
    {
    public:
        GLESVertexBuffer() : m_vboId(0), m_vaoId(0), m_size(0), m_stride(0) {}
        ~GLESVertexBuffer() { Destroy(); }

        bool Create(uint32_t size, uint32_t stride, const void* data = nullptr, bool dynamic = false);
        void Destroy();

        bool UpdateData(const void* data, uint32_t offset, uint32_t size);
        void Bind() const;
        void Unbind() const;

        GLuint GetVBO() const { return m_vboId; }
        GLuint GetVAO() const { return m_vaoId; }
        uint32_t GetStride() const { return m_stride; }

        // Vertex attribute setup
        void SetVertexAttrib(GLuint index, GLint size, GLenum type, GLboolean normalized, uint32_t offset);

    private:
        GLuint m_vboId;
        GLuint m_vaoId;
        uint32_t m_size;
        uint32_t m_stride;
        bool m_dynamic;
    };

    //=========================================================================
    // GLESIndexBuffer - Index Buffer Object
    //=========================================================================

    class GLESIndexBuffer
    {
    public:
        GLESIndexBuffer() : m_iboId(0), m_count(0), m_is32Bit(false) {}
        ~GLESIndexBuffer() { Destroy(); }

        bool Create(uint32_t count, bool use32Bit, const void* data = nullptr, bool dynamic = false);
        void Destroy();

        bool UpdateData(const void* data, uint32_t offset, uint32_t count);
        void Bind() const;
        void Unbind() const;

        GLuint GetId() const { return m_iboId; }
        uint32_t GetCount() const { return m_count; }
        GLenum GetType() const { return m_is32Bit ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT; }

    private:
        GLuint m_iboId;
        uint32_t m_count;
        bool m_is32Bit;
        bool m_dynamic;
    };

    //=========================================================================
    // GLESGraphics - Main Graphics Class
    //=========================================================================

    class GLESGraphics
    {
    public:
        GLESGraphics();
        ~GLESGraphics();

        // Initialization
        bool Initialize(EGLDisplay display, EGLSurface surface, EGLContext context);
        void Shutdown();

        // Frame management
        void BeginFrame();
        void EndFrame();

        // State management
        void SetBlendMode(BlendMode mode);
        void SetCullMode(CullMode mode);
        void SetDepthTest(bool enable);
        void SetDepthWrite(bool enable);
        void SetScissorTest(bool enable, int x = 0, int y = 0, int width = 0, int height = 0);
        void SetViewport(int x, int y, int width, int height);

        // Clearing
        void Clear(bool color, bool depth, bool stencil, uint32_t clearColor = 0xFF000000);

        // Matrix management
        void SetWorldMatrix(const Matrix4x4& matrix);
        void SetViewMatrix(const Matrix4x4& matrix);
        void SetProjectionMatrix(const Matrix4x4& matrix);
        void SetOrthoProjection(float width, float height);
        void SetPerspectiveProjection(float fovy, float aspect, float near, float far);

        // Drawing primitives
        void DrawPrimitive(PrimitiveType type, uint32_t startVertex, uint32_t primitiveCount);
        void DrawIndexedPrimitive(PrimitiveType type, uint32_t startIndex, uint32_t primitiveCount);

        // 2D Drawing (UI/HUD)
        void DrawRect(float x, float y, float width, float height, uint32_t color);
        void DrawRectOutline(float x, float y, float width, float height, uint32_t color, float lineWidth = 1.0f);
        void DrawTexture(GLESTexture* texture, float x, float y, float width, float height,
                         float u1 = 0, float v1 = 0, float u2 = 1, float v2 = 1, uint32_t color = 0xFFFFFFFF);
        void DrawLine2D(float x1, float y1, float x2, float y2, uint32_t color, float width = 1.0f);

        // 3D Drawing
        void DrawLine3D(float x1, float y1, float z1, float x2, float y2, float z2, uint32_t color);

        // Texture management
        GLESTexture* CreateTexture(uint32_t width, uint32_t height, TextureFormat format, const void* data = nullptr);
        void DestroyTexture(GLESTexture* texture);

        // Shader management
        GLESShader* CreateShader(const char* vertexSource, const char* fragmentSource);
        void DestroyShader(GLESShader* shader);
        void BindShader(GLESShader* shader);

        // Render target management
        GLESRenderTarget* CreateRenderTarget(uint32_t width, uint32_t height, bool hasDepth = true);
        void DestroyRenderTarget(GLESRenderTarget* target);
        void SetRenderTarget(GLESRenderTarget* target);

        // Getters
        uint32_t GetScreenWidth() const { return m_screenWidth; }
        uint32_t GetScreenHeight() const { return m_screenHeight; }
        bool IsInitialized() const { return m_initialized; }

        // Error checking
        static bool CheckGLError(const char* operation);

    private:
        // Built-in shaders
        void CreateBuiltInShaders();
        void DestroyBuiltInShaders();

        // Internal drawing helpers
        void SetupVertexAttribs2D();
        void SetupVertexAttribs3D();

        // EGL context
        EGLDisplay m_eglDisplay;
        EGLSurface m_eglSurface;
        EGLContext m_eglContext;

        // Screen dimensions
        uint32_t m_screenWidth;
        uint32_t m_screenHeight;

        // State
        bool m_initialized;
        BlendMode m_currentBlendMode;
        CullMode m_currentCullMode;
        bool m_depthTestEnabled;
        bool m_depthWriteEnabled;

        // Matrices
        Matrix4x4 m_worldMatrix;
        Matrix4x4 m_viewMatrix;
        Matrix4x4 m_projectionMatrix;
        Matrix4x4 m_mvpMatrix;
        bool m_mvpDirty;

        // Built-in shaders
        std::unique_ptr<GLESShader> m_shaderColor;
        std::unique_ptr<GLESShader> m_shaderTexture;
        std::unique_ptr<GLESShader> m_shaderTextureColor;
        GLESShader* m_currentShader;

        // Dynamic vertex buffer for immediate mode drawing
        std::unique_ptr<GLESVertexBuffer> m_dynamicVB;
        std::vector<uint8_t> m_dynamicVertexData;

        // Current render target
        GLESRenderTarget* m_currentRenderTarget;
    };

    //=========================================================================
    // Global Graphics Instance
    //=========================================================================

    extern GLESGraphics* g_pGraphics;

    // Initialize/shutdown helpers
    bool InitializeGraphics(EGLDisplay display, EGLSurface surface, EGLContext context);
    void ShutdownGraphics();

} // namespace MTA::Android::Graphics

#endif // GLES_GRAPHICS_H
