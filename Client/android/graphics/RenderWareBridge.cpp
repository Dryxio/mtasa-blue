/*
 * MTA:SA Android - RenderWare to OpenGL ES Bridge Implementation
 *
 * Provides conversion between RenderWare and OpenGL ES resources.
 */

#include "RenderWareBridge.h"
#include "GLESGraphics.h"

#include <GLES3/gl3.h>
#include <cstring>
#include <cmath>

#ifdef __ANDROID__
#include <android/log.h>
#define RW_LOG_TAG "MTA-RWBridge"
#define RW_LOGI(...) __android_log_print(ANDROID_LOG_INFO, RW_LOG_TAG, __VA_ARGS__)
#define RW_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, RW_LOG_TAG, __VA_ARGS__)
#define RW_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, RW_LOG_TAG, __VA_ARGS__)
#else
#define RW_LOGI(...) printf(__VA_ARGS__)
#define RW_LOGE(...) fprintf(stderr, __VA_ARGS__)
#define RW_LOGD(...) printf(__VA_ARGS__)
#endif

namespace MTA::Android::RenderWare
{

//=============================================================================
// Global Instance
//=============================================================================

RenderWareBridge* g_pRWBridge = nullptr;

bool InitializeRWBridge()
{
    if (g_pRWBridge)
        return true;

    g_pRWBridge = new RenderWareBridge();
    if (!g_pRWBridge->Initialize())
    {
        delete g_pRWBridge;
        g_pRWBridge = nullptr;
        return false;
    }

    RW_LOGI("RenderWare bridge initialized");
    return true;
}

void ShutdownRWBridge()
{
    if (g_pRWBridge)
    {
        g_pRWBridge->Shutdown();
        delete g_pRWBridge;
        g_pRWBridge = nullptr;
        RW_LOGI("RenderWare bridge shutdown");
    }
}

//=============================================================================
// RenderWareBridge Implementation
//=============================================================================

RenderWareBridge::RenderWareBridge()
    : m_renderStatesDirty(false)
    , m_im3dActive(false)
    , m_frameCounter(0)
    , m_initialized(false)
{
    // Initialize matrices to identity
    m_viewMatrix.Identity();
    m_projMatrix.Identity();
    m_worldMatrix.Identity();
    m_im3dTransform.Identity();
}

RenderWareBridge::~RenderWareBridge()
{
    Shutdown();
}

bool RenderWareBridge::Initialize()
{
    if (m_initialized)
        return true;

    // Set default render states
    m_renderStates[rwRENDERSTATEZTESTENABLE] = 1;
    m_renderStates[rwRENDERSTATEZWRITEENABLE] = 1;
    m_renderStates[rwRENDERSTATECULLMODE] = rwCULLMODECULLBACK;
    m_renderStates[rwRENDERSTATEVERTEXALPHAENABLE] = 0;
    m_renderStates[rwRENDERSTATESRCBLEND] = rwBLENDSRCALPHA;
    m_renderStates[rwRENDERSTATEDESTBLEND] = rwBLENDINVSRCALPHA;
    m_renderStates[rwRENDERSTATETEXTUREFILTER] = rwFILTERLINEAR;
    m_renderStates[rwRENDERSTATETEXTUREADDRESS] = rwTEXTUREADDRESSWRAP;
    m_renderStates[rwRENDERSTATEFOGENABLE] = 0;
    m_renderStates[rwRENDERSTATEALPHATESTFUNCTION] = 0;
    m_renderStates[rwRENDERSTATEALPHATESTFUNCTIONREF] = 0;

    m_initialized = true;
    return true;
}

void RenderWareBridge::Shutdown()
{
    if (!m_initialized)
        return;

    ClearCache();
    m_renderStates.clear();
    m_im3dVertices.clear();
    m_initialized = false;
}

//=============================================================================
// Raster/Texture Conversion
//=============================================================================

uint32_t RenderWareBridge::GetGLTexture(RwRaster* raster)
{
    if (!raster)
        return 0;

    // Check cache first
    auto it = m_rasterCache.find(raster);
    if (it != m_rasterCache.end())
    {
        if (!it->second.isDirty)
            return it->second.glTextureId;

        // Update dirty texture
        UpdateRasterTexture(raster);
        return it->second.glTextureId;
    }

    // Create new cache entry
    RasterGLCache* cache = CreateRasterCache(raster);
    if (!cache)
        return 0;

    return cache->glTextureId;
}

RasterGLCache* RenderWareBridge::CreateRasterCache(RwRaster* raster)
{
    if (!raster || raster->width <= 0 || raster->height <= 0)
        return nullptr;

    RasterGLCache cache;
    cache.lastUpdateFrame = m_frameCounter;
    cache.isDirty = false;

    // Create GL texture
    GLuint texId;
    glGenTextures(1, &texId);
    if (texId == 0)
    {
        RW_LOGE("Failed to create GL texture for raster");
        return nullptr;
    }

    cache.glTextureId = texId;

    // Upload initial pixel data
    UploadRasterPixels(raster, texId);

    // Store in cache
    m_rasterCache[raster] = std::move(cache);
    return &m_rasterCache[raster];
}

void RenderWareBridge::UploadRasterPixels(RwRaster* raster, uint32_t glTexture)
{
    if (!raster || glTexture == 0)
        return;

    glBindTexture(GL_TEXTURE_2D, glTexture);

    // Get format information
    uint32_t glInternalFormat, glFormat, glType;
    GetGLFormat(raster->format, glInternalFormat, glFormat, glType);

    // Upload pixel data
    if (raster->pixels)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat,
                     raster->width, raster->height, 0,
                     glFormat, glType, raster->pixels);
    }
    else
    {
        // Create empty texture
        glTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat,
                     raster->width, raster->height, 0,
                     glFormat, glType, nullptr);
    }

    // Generate mipmaps if requested
    if (raster->format & rwRASTERFORMATMIPMAP)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    // Set default filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderWareBridge::UpdateRasterTexture(RwRaster* raster)
{
    auto it = m_rasterCache.find(raster);
    if (it == m_rasterCache.end())
        return;

    UploadRasterPixels(raster, it->second.glTextureId);
    it->second.isDirty = false;
    it->second.lastUpdateFrame = m_frameCounter;
}

void RenderWareBridge::ReleaseRasterTexture(RwRaster* raster)
{
    auto it = m_rasterCache.find(raster);
    if (it == m_rasterCache.end())
        return;

    if (it->second.glTextureId != 0)
    {
        GLuint texId = it->second.glTextureId;
        glDeleteTextures(1, &texId);
    }

    m_rasterCache.erase(it);
}

void RenderWareBridge::GetGLFormat(uint32_t rwFormat, uint32_t& glInternalFormat,
                                    uint32_t& glFormat, uint32_t& glType)
{
    // Extract format from flags
    uint32_t format = rwFormat & rwRASTERFORMATMASK;

    switch (format)
    {
    case rwRASTERFORMAT8888:
        glInternalFormat = GL_RGBA8;
        glFormat = GL_RGBA;
        glType = GL_UNSIGNED_BYTE;
        break;

    case rwRASTERFORMAT888:
        glInternalFormat = GL_RGB8;
        glFormat = GL_RGB;
        glType = GL_UNSIGNED_BYTE;
        break;

    case rwRASTERFORMAT565:
        glInternalFormat = GL_RGB565;
        glFormat = GL_RGB;
        glType = GL_UNSIGNED_SHORT_5_6_5;
        break;

    case rwRASTERFORMAT1555:
        glInternalFormat = GL_RGB5_A1;
        glFormat = GL_RGBA;
        glType = GL_UNSIGNED_SHORT_5_5_5_1;
        break;

    case rwRASTERFORMAT4444:
        glInternalFormat = GL_RGBA4;
        glFormat = GL_RGBA;
        glType = GL_UNSIGNED_SHORT_4_4_4_4;
        break;

    case rwRASTERFORMATLUM8:
        glInternalFormat = GL_LUMINANCE;
        glFormat = GL_LUMINANCE;
        glType = GL_UNSIGNED_BYTE;
        break;

    default:
        // Default to RGBA
        glInternalFormat = GL_RGBA8;
        glFormat = GL_RGBA;
        glType = GL_UNSIGNED_BYTE;
        break;
    }
}

//=============================================================================
// Geometry Conversion
//=============================================================================

GeometryGLCache* RenderWareBridge::GetGLGeometry(RpGeometry* geometry)
{
    if (!geometry)
        return nullptr;

    auto it = m_geometryCache.find(geometry);
    if (it != m_geometryCache.end())
    {
        if (!it->second.isDirty)
            return &it->second;

        // Update dirty geometry
        UpdateGeometryBuffers(geometry);
        return &it->second;
    }

    return CreateGeometryCache(geometry);
}

GeometryGLCache* RenderWareBridge::CreateGeometryCache(RpGeometry* geometry)
{
    if (!geometry || geometry->numVertices <= 0)
        return nullptr;

    GeometryGLCache cache;
    cache.numVertices = geometry->numVertices;
    cache.numIndices = geometry->numTriangles * 3;
    cache.lastUpdateFrame = m_frameCounter;
    cache.isDirty = false;

    // Create buffers
    cache.vertexBuffer = std::make_unique<Graphics::GLESVertexBuffer>();
    cache.indexBuffer = std::make_unique<Graphics::GLESIndexBuffer>();

    BuildGeometryBuffers(geometry, &cache);

    m_geometryCache[geometry] = std::move(cache);
    return &m_geometryCache[geometry];
}

void RenderWareBridge::BuildGeometryBuffers(RpGeometry* geometry, GeometryGLCache* cache)
{
    if (!geometry || !cache)
        return;

    // Build interleaved vertex data
    // Format: Position (3f) + Normal (3f) + Color (4ub) + TexCoord (2f)
    const uint32_t vertexSize = sizeof(float) * 3 +  // Position
                                sizeof(float) * 3 +  // Normal
                                sizeof(uint32_t) +   // Color (packed)
                                sizeof(float) * 2;   // TexCoord

    std::vector<uint8_t> vertexData(geometry->numVertices * vertexSize);
    uint8_t* ptr = vertexData.data();

    for (int32_t i = 0; i < geometry->numVertices; i++)
    {
        // Position
        if (geometry->morphTarget.vertices)
        {
            memcpy(ptr, &geometry->morphTarget.vertices[i], sizeof(float) * 3);
        }
        else
        {
            float zero[3] = {0, 0, 0};
            memcpy(ptr, zero, sizeof(float) * 3);
        }
        ptr += sizeof(float) * 3;

        // Normal
        if (geometry->morphTarget.normals)
        {
            memcpy(ptr, &geometry->morphTarget.normals[i], sizeof(float) * 3);
        }
        else
        {
            float up[3] = {0, 1, 0};
            memcpy(ptr, up, sizeof(float) * 3);
        }
        ptr += sizeof(float) * 3;

        // Color
        if (geometry->preLitLum)
        {
            uint32_t color = geometry->preLitLum[i].ToRGBA();
            memcpy(ptr, &color, sizeof(uint32_t));
        }
        else
        {
            uint32_t white = 0xFFFFFFFF;
            memcpy(ptr, &white, sizeof(uint32_t));
        }
        ptr += sizeof(uint32_t);

        // TexCoord
        if (geometry->texCoords[0])
        {
            memcpy(ptr, &geometry->texCoords[0][i], sizeof(float) * 2);
        }
        else
        {
            float zero[2] = {0, 0};
            memcpy(ptr, zero, sizeof(float) * 2);
        }
        ptr += sizeof(float) * 2;
    }

    // Create vertex buffer
    cache->vertexBuffer->Create(vertexData.size(), vertexSize, vertexData.data(), false);

    // Set up vertex attributes
    // Position at offset 0
    cache->vertexBuffer->SetVertexAttrib(0, 3, GL_FLOAT, GL_FALSE, 0);
    // Normal at offset 12
    cache->vertexBuffer->SetVertexAttrib(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3);
    // Color at offset 24
    cache->vertexBuffer->SetVertexAttrib(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(float) * 6);
    // TexCoord at offset 28
    cache->vertexBuffer->SetVertexAttrib(3, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6 + sizeof(uint32_t));

    // Build index data
    if (geometry->triangles && geometry->numTriangles > 0)
    {
        std::vector<uint16_t> indices(geometry->numTriangles * 3);
        for (int32_t i = 0; i < geometry->numTriangles; i++)
        {
            indices[i * 3 + 0] = geometry->triangles[i].verts[0];
            indices[i * 3 + 1] = geometry->triangles[i].verts[1];
            indices[i * 3 + 2] = geometry->triangles[i].verts[2];
        }

        cache->indexBuffer->Create(indices.size(), false, indices.data(), false);
    }
}

void RenderWareBridge::UpdateGeometryBuffers(RpGeometry* geometry)
{
    auto it = m_geometryCache.find(geometry);
    if (it == m_geometryCache.end())
        return;

    BuildGeometryBuffers(geometry, &it->second);
    it->second.isDirty = false;
    it->second.lastUpdateFrame = m_frameCounter;
}

void RenderWareBridge::ReleaseGeometry(RpGeometry* geometry)
{
    auto it = m_geometryCache.find(geometry);
    if (it != m_geometryCache.end())
    {
        m_geometryCache.erase(it);
    }
}

//=============================================================================
// Matrix Conversion
//=============================================================================

void RenderWareBridge::ConvertMatrix(const RwMatrix& rwMatrix, Graphics::Matrix4x4& glMatrix)
{
    // RenderWare uses row-major, OpenGL uses column-major
    // RwMatrix: right, up, at, pos (3x3 rotation + translation)
    glMatrix.m[0]  = rwMatrix.right.x;
    glMatrix.m[1]  = rwMatrix.right.y;
    glMatrix.m[2]  = rwMatrix.right.z;
    glMatrix.m[3]  = 0.0f;

    glMatrix.m[4]  = rwMatrix.up.x;
    glMatrix.m[5]  = rwMatrix.up.y;
    glMatrix.m[6]  = rwMatrix.up.z;
    glMatrix.m[7]  = 0.0f;

    glMatrix.m[8]  = rwMatrix.at.x;
    glMatrix.m[9]  = rwMatrix.at.y;
    glMatrix.m[10] = rwMatrix.at.z;
    glMatrix.m[11] = 0.0f;

    glMatrix.m[12] = rwMatrix.pos.x;
    glMatrix.m[13] = rwMatrix.pos.y;
    glMatrix.m[14] = rwMatrix.pos.z;
    glMatrix.m[15] = 1.0f;
}

void RenderWareBridge::ConvertMatrix(const Graphics::Matrix4x4& glMatrix, RwMatrix& rwMatrix)
{
    rwMatrix.right.x = glMatrix.m[0];
    rwMatrix.right.y = glMatrix.m[1];
    rwMatrix.right.z = glMatrix.m[2];
    rwMatrix.flags   = 0;

    rwMatrix.up.x    = glMatrix.m[4];
    rwMatrix.up.y    = glMatrix.m[5];
    rwMatrix.up.z    = glMatrix.m[6];
    rwMatrix.pad1    = 0;

    rwMatrix.at.x    = glMatrix.m[8];
    rwMatrix.at.y    = glMatrix.m[9];
    rwMatrix.at.z    = glMatrix.m[10];
    rwMatrix.pad2    = 0;

    rwMatrix.pos.x   = glMatrix.m[12];
    rwMatrix.pos.y   = glMatrix.m[13];
    rwMatrix.pos.z   = glMatrix.m[14];
    rwMatrix.pad3    = 0;
}

//=============================================================================
// Render State Translation
//=============================================================================

void RenderWareBridge::SetRenderState(RwRenderState state, uint32_t value)
{
    auto it = m_renderStates.find(state);
    if (it != m_renderStates.end() && it->second == value)
        return;  // No change

    m_renderStates[state] = value;
    m_renderStatesDirty = true;
}

uint32_t RenderWareBridge::GetRenderState(RwRenderState state)
{
    auto it = m_renderStates.find(state);
    return (it != m_renderStates.end()) ? it->second : 0;
}

void RenderWareBridge::ApplyRenderStates()
{
    if (!m_renderStatesDirty)
        return;

    // Z Test
    if (GetRenderState(rwRENDERSTATEZTESTENABLE))
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    // Z Write
    glDepthMask(GetRenderState(rwRENDERSTATEZWRITEENABLE) ? GL_TRUE : GL_FALSE);

    // Cull Mode
    uint32_t cullMode = GetRenderState(rwRENDERSTATECULLMODE);
    switch (cullMode)
    {
    case rwCULLMODECULLNONE:
        glDisable(GL_CULL_FACE);
        break;
    case rwCULLMODECULLBACK:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case rwCULLMODECULLFRONT:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    }

    // Alpha Blending
    if (GetRenderState(rwRENDERSTATEVERTEXALPHAENABLE))
    {
        glEnable(GL_BLEND);
        uint32_t srcBlend = GetGLBlendFunc(static_cast<RwBlendFunction>(
            GetRenderState(rwRENDERSTATESRCBLEND)));
        uint32_t dstBlend = GetGLBlendFunc(static_cast<RwBlendFunction>(
            GetRenderState(rwRENDERSTATEDESTBLEND)));
        glBlendFunc(srcBlend, dstBlend);
    }
    else
    {
        glDisable(GL_BLEND);
    }

    m_renderStatesDirty = false;
}

//=============================================================================
// Immediate Mode Rendering
//=============================================================================

void RenderWareBridge::Im3DRenderOpen(const RwMatrix* transform)
{
    m_im3dVertices.clear();

    if (transform)
        ConvertMatrix(*transform, m_im3dTransform);
    else
        m_im3dTransform.Identity();

    m_im3dActive = true;
}

void RenderWareBridge::Im3DSubmitVertices(const RwIm3DVertex* vertices, int32_t numVertices)
{
    if (!m_im3dActive || !vertices || numVertices <= 0)
        return;

    m_im3dVertices.insert(m_im3dVertices.end(), vertices, vertices + numVertices);
}

void RenderWareBridge::Im3DRenderPrimitive(RwPrimitiveType primType)
{
    if (!m_im3dActive || m_im3dVertices.empty())
        return;

    // Apply render states
    ApplyRenderStates();

    // Get GL primitive type
    GLenum glPrimType = GetGLPrimitiveType(primType);

    // TODO: Use GLESGraphics to render the vertices
    // This requires setting up a VBO with the Im3D vertex data

    RW_LOGD("Im3DRenderPrimitive: %d vertices, type %d", (int)m_im3dVertices.size(), (int)primType);
}

void RenderWareBridge::Im3DRenderIndexedPrimitive(RwPrimitiveType primType,
                                                   const uint16_t* indices, int32_t numIndices)
{
    if (!m_im3dActive || m_im3dVertices.empty() || !indices || numIndices <= 0)
        return;

    // Apply render states
    ApplyRenderStates();

    GLenum glPrimType = GetGLPrimitiveType(primType);

    // TODO: Use GLESGraphics to render indexed vertices

    RW_LOGD("Im3DRenderIndexedPrimitive: %d vertices, %d indices, type %d",
            (int)m_im3dVertices.size(), numIndices, (int)primType);
}

void RenderWareBridge::Im3DRenderClose()
{
    m_im3dVertices.clear();
    m_im3dActive = false;
}

void RenderWareBridge::Im2DRenderPrimitive(RwPrimitiveType primType,
                                            const RwIm2DVertex* vertices, int32_t numVertices)
{
    if (!vertices || numVertices <= 0)
        return;

    // Apply render states
    ApplyRenderStates();

    GLenum glPrimType = GetGLPrimitiveType(primType);

    // TODO: Use GLESGraphics for 2D rendering

    RW_LOGD("Im2DRenderPrimitive: %d vertices, type %d", numVertices, (int)primType);
}

void RenderWareBridge::Im2DRenderIndexedPrimitive(RwPrimitiveType primType,
                                                    const RwIm2DVertex* vertices, int32_t numVertices,
                                                    const uint16_t* indices, int32_t numIndices)
{
    if (!vertices || numVertices <= 0 || !indices || numIndices <= 0)
        return;

    // Apply render states
    ApplyRenderStates();

    GLenum glPrimType = GetGLPrimitiveType(primType);

    // TODO: Use GLESGraphics for indexed 2D rendering

    RW_LOGD("Im2DRenderIndexedPrimitive: %d vertices, %d indices, type %d",
            numVertices, numIndices, (int)primType);
}

//=============================================================================
// Utility Functions
//=============================================================================

void RenderWareBridge::ClearCache()
{
    // Release all raster textures
    for (auto& pair : m_rasterCache)
    {
        if (pair.second.glTextureId != 0)
        {
            GLuint texId = pair.second.glTextureId;
            glDeleteTextures(1, &texId);
        }
    }
    m_rasterCache.clear();

    // Release all geometry buffers (handled by unique_ptr destructors)
    m_geometryCache.clear();

    RW_LOGI("RenderWare bridge cache cleared");
}

uint32_t RenderWareBridge::GetGLPrimitiveType(RwPrimitiveType rwPrimType)
{
    switch (rwPrimType)
    {
    case rwPRIMTYPEPOINTLIST:  return GL_POINTS;
    case rwPRIMTYPELINELIST:   return GL_LINES;
    case rwPRIMTYPELINESTRIP:  return GL_LINE_STRIP;
    case rwPRIMTYPETRILIST:    return GL_TRIANGLES;
    case rwPRIMTYPETRISTRIP:   return GL_TRIANGLE_STRIP;
    case rwPRIMTYPETRIFAN:     return GL_TRIANGLE_FAN;
    default:                   return GL_TRIANGLES;
    }
}

uint32_t RenderWareBridge::GetGLBlendFunc(RwBlendFunction rwBlend)
{
    switch (rwBlend)
    {
    case rwBLENDZERO:          return GL_ZERO;
    case rwBLENDONE:           return GL_ONE;
    case rwBLENDSRCCOLOR:      return GL_SRC_COLOR;
    case rwBLENDINVSRCCOLOR:   return GL_ONE_MINUS_SRC_COLOR;
    case rwBLENDSRCALPHA:      return GL_SRC_ALPHA;
    case rwBLENDINVSRCALPHA:   return GL_ONE_MINUS_SRC_ALPHA;
    case rwBLENDDESTALPHA:     return GL_DST_ALPHA;
    case rwBLENDINVDESTALPHA:  return GL_ONE_MINUS_DST_ALPHA;
    case rwBLENDDESTCOLOR:     return GL_DST_COLOR;
    case rwBLENDINVDESTCOLOR:  return GL_ONE_MINUS_DST_COLOR;
    case rwBLENDSRCALPHASAT:   return GL_SRC_ALPHA_SATURATE;
    default:                   return GL_ONE;
    }
}

uint32_t RenderWareBridge::GetGLTextureFilter(RwTextureFilterMode rwFilter, bool minFilter)
{
    if (minFilter)
    {
        switch (rwFilter)
        {
        case rwFILTERNEAREST:           return GL_NEAREST;
        case rwFILTERLINEAR:            return GL_LINEAR;
        case rwFILTERMIPNEAREST:        return GL_NEAREST_MIPMAP_NEAREST;
        case rwFILTERMIPLINEAR:         return GL_NEAREST_MIPMAP_LINEAR;
        case rwFILTERLINEARMIPNEAREST:  return GL_LINEAR_MIPMAP_NEAREST;
        case rwFILTERLINEARMIPLINEAR:   return GL_LINEAR_MIPMAP_LINEAR;
        default:                        return GL_LINEAR;
        }
    }
    else
    {
        // Mag filter doesn't support mipmapping
        switch (rwFilter)
        {
        case rwFILTERNEAREST:
        case rwFILTERMIPNEAREST:
            return GL_NEAREST;
        default:
            return GL_LINEAR;
        }
    }
}

uint32_t RenderWareBridge::GetGLTextureWrap(RwTextureAddressMode rwAddress)
{
    switch (rwAddress)
    {
    case rwTEXTUREADDRESSWRAP:   return GL_REPEAT;
    case rwTEXTUREADDRESSMIRROR: return GL_MIRRORED_REPEAT;
    case rwTEXTUREADDRESSCLAMP:  return GL_CLAMP_TO_EDGE;
    case rwTEXTUREADDRESSBORDER: return GL_CLAMP_TO_EDGE;  // GL ES doesn't have border
    default:                     return GL_REPEAT;
    }
}

//=============================================================================
// Hook Installation
//=============================================================================

bool InstallRenderWareBridgeHooks()
{
    // This will be implemented in the hooks module
    // It needs to intercept RenderWare render calls like:
    // - RwRenderStateSet/Get
    // - RwIm2DRenderPrimitive
    // - RwIm3DRenderOpen/Close/etc
    // - RpAtomicRender
    // - RpClumpRender

    RW_LOGI("RenderWare bridge hooks installation placeholder");

    // TODO: Install actual hooks using ARMHookInstaller

    return true;
}

} // namespace MTA::Android::RenderWare
