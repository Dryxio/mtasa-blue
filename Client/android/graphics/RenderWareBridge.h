/*
 * MTA:SA Android - RenderWare to OpenGL ES Bridge
 *
 * This module provides conversion between RenderWare structures used by
 * GTA:SA and the OpenGL ES backend used by MTA Android.
 *
 * Key conversions:
 *   RwRaster     -> GLESTexture (texture data)
 *   RpGeometry   -> VBO/IBO (mesh data)
 *   RwMatrix     -> Matrix4x4 (transforms)
 *   RwCamera     -> View/Projection matrices
 *   RpMaterial   -> Material uniforms
 *
 * The bridge intercepts RenderWare render calls and redirects them
 * through the OpenGL ES backend while maintaining compatibility with
 * GTA:SA's native rendering pipeline.
 */

#ifndef RENDERWARE_BRIDGE_H
#define RENDERWARE_BRIDGE_H

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

// Include GLESGraphics.h for Matrix4x4 definition
#include "GLESGraphics.h"

// Forward declarations
namespace MTA::Android::Graphics
{
    class GLESTexture;
    class GLESShader;
    class GLESVertexBuffer;
    class GLESIndexBuffer;
}

namespace MTA::Android::RenderWare
{

//=============================================================================
// RenderWare Structure Definitions (from GTA:SA Android)
// These mirror the structures in libGTASA.so
//=============================================================================

// Basic types
struct RwV2d { float x, y; };
struct RwV3d { float x, y, z; };
struct RwV4d { float x, y, z, w; };

struct RwColor
{
    uint8_t r, g, b, a;

    uint32_t ToARGB() const { return (a << 24) | (r << 16) | (g << 8) | b; }
    uint32_t ToRGBA() const { return (r << 24) | (g << 16) | (b << 8) | a; }

    static RwColor FromARGB(uint32_t argb)
    {
        RwColor c;
        c.a = (argb >> 24) & 0xFF;
        c.r = (argb >> 16) & 0xFF;
        c.g = (argb >> 8) & 0xFF;
        c.b = argb & 0xFF;
        return c;
    }
};

struct RwColorFloat
{
    float r, g, b, a;

    RwColor ToRwColor() const
    {
        RwColor c;
        c.r = static_cast<uint8_t>(r * 255.0f);
        c.g = static_cast<uint8_t>(g * 255.0f);
        c.b = static_cast<uint8_t>(b * 255.0f);
        c.a = static_cast<uint8_t>(a * 255.0f);
        return c;
    }
};

// RenderWare matrix (column-major like OpenGL)
struct RwMatrix
{
    RwV3d    right;     // X axis
    uint32_t flags;
    RwV3d    up;        // Y axis
    uint32_t pad1;
    RwV3d    at;        // Z axis (forward)
    uint32_t pad2;
    RwV3d    pos;       // Position
    uint32_t pad3;
};

// RenderWare raster (texture data container)
struct RwRaster
{
    RwRaster*      parent;
    uint8_t*       pixels;           // CPU pixel data
    uint8_t*       palette;          // For palettized textures
    int32_t        width;
    int32_t        height;
    int32_t        depth;            // Bits per pixel
    int32_t        numLevels;        // Mipmap levels
    int16_t        u, v;             // Sub-raster offset
    uint8_t        type;
    uint8_t        flags;
    uint8_t        privateFlags;
    uint8_t        format;
    uint8_t*       origPixels;
    int32_t        origWidth;
    int32_t        origHeight;
    int32_t        origDepth;
    void*          renderResource;   // Platform-specific (GL texture ID on Android)
};

// Raster format flags
enum RwRasterFormat
{
    rwRASTERFORMATDEFAULT = 0x0000,
    rwRASTERFORMAT1555    = 0x0100,
    rwRASTERFORMAT565     = 0x0200,
    rwRASTERFORMAT4444    = 0x0300,
    rwRASTERFORMATLUM8    = 0x0400,
    rwRASTERFORMAT8888    = 0x0500,
    rwRASTERFORMAT888     = 0x0600,
    rwRASTERFORMAT16      = 0x0700,
    rwRASTERFORMAT24      = 0x0800,
    rwRASTERFORMAT32      = 0x0900,
    rwRASTERFORMAT555     = 0x0A00,
    rwRASTERFORMATMASK    = 0x0F00,
    rwRASTERFORMATPAL8    = 0x2000,
    rwRASTERFORMATPAL4    = 0x4000,
    rwRASTERFORMATMIPMAP  = 0x8000,
    rwRASTERFORMATAUTOMIPMAP = 0x1000,
};

// RenderWare texture
struct RwTexture
{
    RwRaster*  raster;
    void*      txd;              // Parent texture dictionary
    void*      TXDList[2];       // Linked list pointers
    char       name[32];
    char       mask[32];
    uint32_t   flags;
    int32_t    refs;
};

// RenderWare geometry vertex data
struct RwTexCoords
{
    float u, v;
};

struct RpTriangle
{
    uint16_t verts[3];
    uint16_t materialId;
};

// Material for geometry
struct RpMaterial
{
    RwTexture* texture;
    RwColor    color;
    void*      render;
    struct {
        float ambient;
        float specular;
        float diffuse;
    } lighting;
    int16_t    refs;
    int16_t    id;
};

// Geometry container
struct RpGeometry
{
    void*      object;
    uint32_t   flags;
    uint16_t   lockedSinceLastInst;
    int16_t    refs;

    int32_t    numTriangles;
    int32_t    numVertices;
    int32_t    numMorphTargets;
    int32_t    numTexCoordSets;

    struct {
        RpMaterial** materials;
        int32_t      numMaterials;
        int32_t      space;
    } matList;

    RpTriangle*   triangles;
    RwColor*      preLitLum;         // Pre-lit vertex colors
    RwTexCoords*  texCoords[8];      // Up to 8 UV sets
    void*         mesh;
    void*         resEntry;

    struct {
        void*    geometry;
        void*    boundingSphere;
        RwV3d*   vertices;
        RwV3d*   normals;
    } morphTarget;
};

// Im3D vertex for immediate mode rendering
struct RwIm3DVertex
{
    RwV3d    position;
    RwV3d    normal;
    uint32_t color;      // RGBA packed
    float    u, v;
};

// Im2D vertex for 2D rendering
struct RwIm2DVertex
{
    float    x, y, z;
    float    rhw;        // Reciprocal homogeneous W (for pre-transformed verts)
    uint32_t color;
    float    u, v;
};

//=============================================================================
// RenderWare Render State Enumerations
//=============================================================================

enum RwRenderState
{
    rwRENDERSTATETEXTURERASTER = 1,
    rwRENDERSTATETEXTUREADDRESS,
    rwRENDERSTATETEXTUREADDRESSU,
    rwRENDERSTATETEXTUREADDRESSV,
    rwRENDERSTATETEXTUREPERSPECTIVE,
    rwRENDERSTATEZTESTENABLE,
    rwRENDERSTATESHADEMODE,
    rwRENDERSTATEZWRITEENABLE,
    rwRENDERSTATETEXTUREFILTER,
    rwRENDERSTATESRCBLEND,
    rwRENDERSTATEDESTBLEND,
    rwRENDERSTATEVERTEXALPHAENABLE,
    rwRENDERSTATEBORDERCOLOR,
    rwRENDERSTATEFOGENABLE,
    rwRENDERSTATEFOGCOLOR,
    rwRENDERSTATEFOGTYPE,
    rwRENDERSTATEFOGDENSITY,
    rwRENDERSTATECULLMODE,
    rwRENDERSTATESTENCILENABLE,
    rwRENDERSTATESTENCILFAIL,
    rwRENDERSTATESTENCILZFAIL,
    rwRENDERSTATESTENCILPASS,
    rwRENDERSTATESTENCILFUNCTION,
    rwRENDERSTATESTENCILFUNCTIONREF,
    rwRENDERSTATESTENCILFUNCTIONMASK,
    rwRENDERSTATESTENCILFUNCTIONWRITEMASK,
    rwRENDERSTATEALPHATESTFUNCTION,
    rwRENDERSTATEALPHATESTFUNCTIONREF,
};

enum RwBlendFunction
{
    rwBLENDZERO = 1,
    rwBLENDONE,
    rwBLENDSRCCOLOR,
    rwBLENDINVSRCCOLOR,
    rwBLENDSRCALPHA,
    rwBLENDINVSRCALPHA,
    rwBLENDDESTALPHA,
    rwBLENDINVDESTALPHA,
    rwBLENDDESTCOLOR,
    rwBLENDINVDESTCOLOR,
    rwBLENDSRCALPHASAT,
};

enum RwTextureFilterMode
{
    rwFILTERNEAREST = 1,
    rwFILTERLINEAR,
    rwFILTERMIPNEAREST,
    rwFILTERMIPLINEAR,
    rwFILTERLINEARMIPNEAREST,
    rwFILTERLINEARMIPLINEAR,
};

enum RwTextureAddressMode
{
    rwTEXTUREADDRESSWRAP = 1,
    rwTEXTUREADDRESSMIRROR,
    rwTEXTUREADDRESSCLAMP,
    rwTEXTUREADDRESSBORDER,
};

enum RwCullMode
{
    rwCULLMODECULLNONE = 1,
    rwCULLMODECULLBACK,
    rwCULLMODECULLFRONT,
};

enum RwPrimitiveType
{
    rwPRIMTYPEPOINTLIST = 1,
    rwPRIMTYPELINELIST,
    rwPRIMTYPELINESTRIP,
    rwPRIMTYPETRILIST,
    rwPRIMTYPETRISTRIP,
    rwPRIMTYPETRIFAN,
};

//=============================================================================
// Cached GL Resources
//=============================================================================

/**
 * Cached OpenGL ES texture for a RwRaster
 */
struct RasterGLCache
{
    uint32_t                                     glTextureId;
    std::unique_ptr<Graphics::GLESTexture>       texture;
    uint32_t                                     lastUpdateFrame;
    bool                                         isDirty;
};

/**
 * Cached OpenGL ES mesh data for RpGeometry
 */
struct GeometryGLCache
{
    std::unique_ptr<Graphics::GLESVertexBuffer>  vertexBuffer;
    std::unique_ptr<Graphics::GLESIndexBuffer>   indexBuffer;
    uint32_t                                     numVertices;
    uint32_t                                     numIndices;
    uint32_t                                     lastUpdateFrame;
    bool                                         isDirty;
};

//=============================================================================
// RenderWareBridge Class
//=============================================================================

/**
 * Bridge between RenderWare and OpenGL ES
 *
 * This class manages the conversion of RenderWare resources to OpenGL ES
 * equivalents and provides render state translation.
 */
class RenderWareBridge
{
public:
    RenderWareBridge();
    ~RenderWareBridge();

    // Initialization
    bool Initialize();
    void Shutdown();

    //=========================================================================
    // Raster/Texture Conversion
    //=========================================================================

    /**
     * Get or create GL texture from RwRaster
     * Creates a new GL texture if one doesn't exist, or returns cached version.
     * @param raster RenderWare raster to convert
     * @return OpenGL texture ID, or 0 on failure
     */
    uint32_t GetGLTexture(RwRaster* raster);

    /**
     * Update GL texture from RwRaster data
     * Call when raster pixels have been modified.
     */
    void UpdateRasterTexture(RwRaster* raster);

    /**
     * Release cached GL texture for raster
     */
    void ReleaseRasterTexture(RwRaster* raster);

    /**
     * Convert RwRaster format to GL format
     */
    static void GetGLFormat(uint32_t rwFormat, uint32_t& glInternalFormat,
                            uint32_t& glFormat, uint32_t& glType);

    //=========================================================================
    // Geometry Conversion
    //=========================================================================

    /**
     * Get or create GL buffers from RpGeometry
     * @param geometry RenderWare geometry
     * @return Pointer to cached geometry, or nullptr on failure
     */
    GeometryGLCache* GetGLGeometry(RpGeometry* geometry);

    /**
     * Update GL buffers from geometry data
     */
    void UpdateGeometryBuffers(RpGeometry* geometry);

    /**
     * Release cached GL buffers for geometry
     */
    void ReleaseGeometry(RpGeometry* geometry);

    //=========================================================================
    // Matrix Conversion
    //=========================================================================

    /**
     * Convert RwMatrix to OpenGL-compatible Matrix4x4
     */
    static void ConvertMatrix(const RwMatrix& rwMatrix, Graphics::Matrix4x4& glMatrix);

    /**
     * Convert Matrix4x4 to RwMatrix
     */
    static void ConvertMatrix(const Graphics::Matrix4x4& glMatrix, RwMatrix& rwMatrix);

    //=========================================================================
    // Render State Translation
    //=========================================================================

    /**
     * Set render state (translates RW state to GL state)
     */
    void SetRenderState(RwRenderState state, uint32_t value);

    /**
     * Get current render state value
     */
    uint32_t GetRenderState(RwRenderState state);

    /**
     * Apply all pending render state changes to GL
     */
    void ApplyRenderStates();

    //=========================================================================
    // Immediate Mode Rendering (Im2D/Im3D)
    //=========================================================================

    /**
     * Begin Im3D rendering
     */
    void Im3DRenderOpen(const RwMatrix* transform);

    /**
     * Submit Im3D vertices
     */
    void Im3DSubmitVertices(const RwIm3DVertex* vertices, int32_t numVertices);

    /**
     * Render Im3D primitives
     */
    void Im3DRenderPrimitive(RwPrimitiveType primType);

    /**
     * Render Im3D indexed primitives
     */
    void Im3DRenderIndexedPrimitive(RwPrimitiveType primType,
                                     const uint16_t* indices, int32_t numIndices);

    /**
     * End Im3D rendering
     */
    void Im3DRenderClose();

    /**
     * Render Im2D vertices directly
     */
    void Im2DRenderPrimitive(RwPrimitiveType primType,
                              const RwIm2DVertex* vertices, int32_t numVertices);

    /**
     * Render Im2D indexed primitives
     */
    void Im2DRenderIndexedPrimitive(RwPrimitiveType primType,
                                      const RwIm2DVertex* vertices, int32_t numVertices,
                                      const uint16_t* indices, int32_t numIndices);

    //=========================================================================
    // Camera Setup
    //=========================================================================

    /**
     * Set up GL matrices from RwCamera
     */
    void SetupCameraMatrices(void* camera);

    /**
     * Get current view matrix
     */
    const Graphics::Matrix4x4& GetViewMatrix() const { return m_viewMatrix; }

    /**
     * Get current projection matrix
     */
    const Graphics::Matrix4x4& GetProjectionMatrix() const { return m_projMatrix; }

    //=========================================================================
    // Utility
    //=========================================================================

    /**
     * Clear all cached resources
     */
    void ClearCache();

    /**
     * Get frame counter for cache invalidation
     */
    uint32_t GetCurrentFrame() const { return m_frameCounter; }

    /**
     * Increment frame counter (call once per frame)
     */
    void NextFrame() { m_frameCounter++; }

    /**
     * Convert RW primitive type to GL primitive type
     */
    static uint32_t GetGLPrimitiveType(RwPrimitiveType rwPrimType);

    /**
     * Convert RW blend function to GL blend function
     */
    static uint32_t GetGLBlendFunc(RwBlendFunction rwBlend);

    /**
     * Convert RW texture filter to GL filter
     */
    static uint32_t GetGLTextureFilter(RwTextureFilterMode rwFilter, bool minFilter);

    /**
     * Convert RW texture address mode to GL wrap mode
     */
    static uint32_t GetGLTextureWrap(RwTextureAddressMode rwAddress);

private:
    // Resource caches
    std::unordered_map<RwRaster*, RasterGLCache>       m_rasterCache;
    std::unordered_map<RpGeometry*, GeometryGLCache>   m_geometryCache;

    // Render state cache
    std::unordered_map<RwRenderState, uint32_t>        m_renderStates;
    bool                                               m_renderStatesDirty;

    // Current matrices
    Graphics::Matrix4x4                                m_viewMatrix;
    Graphics::Matrix4x4                                m_projMatrix;
    Graphics::Matrix4x4                                m_worldMatrix;

    // Im3D state
    std::vector<RwIm3DVertex>                          m_im3dVertices;
    Graphics::Matrix4x4                                m_im3dTransform;
    bool                                               m_im3dActive;

    // Frame counter for cache management
    uint32_t                                           m_frameCounter;

    // Initialization state
    bool                                               m_initialized;

    // Internal helpers
    RasterGLCache* CreateRasterCache(RwRaster* raster);
    GeometryGLCache* CreateGeometryCache(RpGeometry* geometry);
    void UploadRasterPixels(RwRaster* raster, uint32_t glTexture);
    void BuildGeometryBuffers(RpGeometry* geometry, GeometryGLCache* cache);
};

//=============================================================================
// Global Bridge Instance
//=============================================================================

extern RenderWareBridge* g_pRWBridge;

/**
 * Initialize the RenderWare bridge
 * Call after OpenGL ES context is created
 */
bool InitializeRWBridge();

/**
 * Shutdown the RenderWare bridge
 * Call before destroying OpenGL ES context
 */
void ShutdownRWBridge();

//=============================================================================
// Hook Installation
//=============================================================================

/**
 * Install hooks to intercept RenderWare render calls
 * This redirects GTA:SA's rendering through the bridge.
 */
bool InstallRenderWareBridgeHooks();

} // namespace MTA::Android::RenderWare

#endif // RENDERWARE_BRIDGE_H
