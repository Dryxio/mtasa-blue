/*
 * MTA:SA Android - GLSL ES Shaders
 *
 * This file contains GLSL ES 3.0 shaders that replicate MTA's D3D9 rendering.
 * Each shader corresponds to a D3D9 fixed-function pipeline configuration
 * or ID3DXEffect shader used by MTA.
 *
 * Shader Categories:
 *   1. 2D Shaders (UI/HUD rendering)
 *   2. 3D Shaders (world geometry)
 *   3. Material Shaders (textured objects)
 *   4. Post-Processing Shaders
 *   5. Special Effects Shaders
 *
 * D3D9 to GLSL ES Mappings:
 *   D3DTSS_COLOROP D3DTOP_SELECTARG2   → Diffuse only shader
 *   D3DTSS_COLOROP D3DTOP_MODULATE     → Texture * Diffuse shader
 *   D3DTSS_ALPHAOP D3DTOP_MODULATE     → Alpha modulation in fragment
 *   Fixed Function Transform           → MVP matrix in vertex shader
 */

#ifndef GLES_SHADERS_H
#define GLES_SHADERS_H

namespace MTA::Android::Graphics::Shaders
{

//=============================================================================
// SECTION 1: 2D SHADERS (UI/HUD)
//=============================================================================

/**
 * 2D Color Only Shader
 *
 * Replaces D3D9 Fixed Function with:
 *   - D3DTSS_COLOROP = D3DTOP_SELECTARG2
 *   - D3DTSS_COLORARG2 = D3DTA_DIFFUSE
 *
 * Used by: CPrimitiveBatcher, CLine3DBatcher (for 2D lines)
 */
constexpr const char* VS_2D_Color = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;

uniform mat4 uMVP;

out vec4 vColor;

void main()
{
    gl_Position = uMVP * vec4(aPosition, 0.0, 1.0);
    vColor = aColor;
}
)glsl";

constexpr const char* FS_2D_Color = R"glsl(#version 300 es
precision mediump float;

in vec4 vColor;

out vec4 fragColor;

void main()
{
    fragColor = vColor;
}
)glsl";

/**
 * 2D Textured Shader
 *
 * Replaces D3D9 Fixed Function with:
 *   - D3DTSS_COLOROP = D3DTOP_MODULATE
 *   - D3DTSS_COLORARG1 = D3DTA_TEXTURE
 *   - D3DTSS_COLORARG2 = D3DTA_DIFFUSE
 *
 * Used by: CTileBatcher for sprites and UI textures
 */
constexpr const char* VS_2D_Textured = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    gl_Position = uMVP * vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)glsl";

constexpr const char* FS_2D_Textured = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 fragColor;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);
    fragColor = texColor * vColor;
}
)glsl";

/**
 * 2D Textured with Alpha Test
 *
 * Same as above but with alpha discard for sprites with transparency
 */
constexpr const char* FS_2D_TexturedAlphaTest = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;
uniform float uAlphaRef;

out vec4 fragColor;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);
    vec4 finalColor = texColor * vColor;

    if (finalColor.a < uAlphaRef)
        discard;

    fragColor = finalColor;
}
)glsl";

//=============================================================================
// SECTION 2: 3D SHADERS (World Geometry)
//=============================================================================

/**
 * 3D Position + Color Shader
 *
 * Used by: CLine3DBatcher for 3D debug lines
 */
constexpr const char* VS_3D_Color = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;

uniform mat4 uMVP;

out vec4 vColor;

void main()
{
    gl_Position = uMVP * vec4(aPosition, 1.0);
    vColor = aColor;
}
)glsl";

constexpr const char* FS_3D_Color = R"glsl(#version 300 es
precision mediump float;

in vec4 vColor;

out vec4 fragColor;

void main()
{
    fragColor = vColor;
}
)glsl";

/**
 * 3D Textured Shader (No Lighting)
 *
 * Used by: CMaterialLine3DBatcher, basic 3D objects
 * Replaces D3D9:
 *   - D3DTSS_COLOROP = D3DTOP_MODULATE
 *   - D3DRS_LIGHTING = FALSE
 */
constexpr const char* VS_3D_Textured = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    gl_Position = uMVP * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)glsl";

constexpr const char* FS_3D_Textured = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 fragColor;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);
    fragColor = texColor * vColor;
}
)glsl";

/**
 * 3D Textured with Lighting
 *
 * Basic per-vertex lighting (Gouraud shading)
 * Replaces D3D9 with D3DRS_LIGHTING = TRUE
 */
constexpr const char* VS_3D_TexturedLit = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aColor;

uniform mat4 uMVP;
uniform mat4 uWorld;
uniform mat3 uNormalMatrix;

// Lighting uniforms
uniform vec3 uLightDir;
uniform vec4 uLightDiffuse;
uniform vec4 uLightAmbient;
uniform vec4 uMaterialDiffuse;
uniform vec4 uMaterialAmbient;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    gl_Position = uMVP * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;

    // Transform normal to world space
    vec3 worldNormal = normalize(uNormalMatrix * aNormal);

    // Basic directional light calculation
    float NdotL = max(dot(worldNormal, -uLightDir), 0.0);

    // Combine lighting
    vec4 ambient = uLightAmbient * uMaterialAmbient;
    vec4 diffuse = uLightDiffuse * uMaterialDiffuse * NdotL;

    vColor = (ambient + diffuse) * aColor;
    vColor.a = aColor.a;
}
)glsl";

constexpr const char* FS_3D_TexturedLit = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 fragColor;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);
    fragColor = texColor * vColor;
}
)glsl";

//=============================================================================
// SECTION 3: FOG SHADERS
//=============================================================================

/**
 * 3D Textured with Linear Fog
 *
 * Replaces D3D9 fixed function fog (D3DRS_FOGENABLE)
 */
constexpr const char* VS_3D_TexturedFog = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;
uniform mat4 uWorld;
uniform vec3 uCameraPos;
uniform float uFogStart;
uniform float uFogEnd;

out vec2 vTexCoord;
out vec4 vColor;
out float vFogFactor;

void main()
{
    vec4 worldPos = uWorld * vec4(aPosition, 1.0);
    gl_Position = uMVP * vec4(aPosition, 1.0);

    vTexCoord = aTexCoord;
    vColor = aColor;

    // Calculate linear fog
    float dist = length(worldPos.xyz - uCameraPos);
    vFogFactor = clamp((uFogEnd - dist) / (uFogEnd - uFogStart), 0.0, 1.0);
}
)glsl";

constexpr const char* FS_3D_TexturedFog = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;
in float vFogFactor;

uniform sampler2D uTexture;
uniform vec4 uFogColor;

out vec4 fragColor;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);
    vec4 baseColor = texColor * vColor;

    // Apply fog
    fragColor = mix(uFogColor, baseColor, vFogFactor);
    fragColor.a = baseColor.a;
}
)glsl";

//=============================================================================
// SECTION 4: SPECIAL MTA PROJECTION SHADERS
//=============================================================================

/**
 * MTA 2D with 3D-Friendly Projection
 *
 * MTA uses a special projection matrix that allows shaders to manipulate
 * the Z coordinate for effects. This replicates CTileBatcher's projection.
 *
 * The projection is set up with:
 *   - Far plane: 10000
 *   - Near plane: 100
 *   - fAdjustZFactor: 1000
 *
 * This allows 2D elements to have depth for layering/effects.
 */
constexpr const char* VS_MTA_2D = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec4 aPosition;  // x, y, z, rhw (pretransformed)
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

// MTA's 2D projection constants
uniform vec2 uViewportSize;
uniform float uFarPlane;
uniform float uNearPlane;
uniform float uAdjustZ;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    // Replicate MTA's CTileBatcher projection
    float Q = uFarPlane / (uFarPlane - uNearPlane);
    float rcpSizeX = 2.0 / uViewportSize.x * uAdjustZ;
    float rcpSizeY = -2.0 / uViewportSize.y * uAdjustZ;

    vec4 pos;
    pos.x = aPosition.x * rcpSizeX + (-uViewportSize.x / 2.0 - 0.5) * rcpSizeX;
    pos.y = aPosition.y * rcpSizeY + (-uViewportSize.y / 2.0 - 0.5) * rcpSizeY;
    pos.z = aPosition.z * Q + (-Q * uNearPlane);
    pos.w = aPosition.z;  // For proper perspective divide

    gl_Position = pos;
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)glsl";

constexpr const char* FS_MTA_2D = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;
uniform bool uHasTexture;

out vec4 fragColor;

void main()
{
    if (uHasTexture)
    {
        vec4 texColor = texture(uTexture, vTexCoord);
        fragColor = texColor * vColor;
    }
    else
    {
        fragColor = vColor;
    }
}
)glsl";

//=============================================================================
// SECTION 5: POST-PROCESSING SHADERS
//=============================================================================

/**
 * Fullscreen Quad Vertex Shader
 *
 * Used for post-processing effects, renders a fullscreen triangle
 */
constexpr const char* VS_Fullscreen = R"glsl(#version 300 es
precision highp float;

out vec2 vTexCoord;

void main()
{
    // Generate fullscreen triangle from vertex ID
    // Vertex 0: (-1, -1), Vertex 1: (3, -1), Vertex 2: (-1, 3)
    float x = float((gl_VertexID & 1) << 2) - 1.0;
    float y = float((gl_VertexID & 2) << 1) - 1.0;

    gl_Position = vec4(x, y, 0.0, 1.0);
    vTexCoord = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);
}
)glsl";

/**
 * Simple Copy/Blit Shader
 */
constexpr const char* FS_Copy = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;

uniform sampler2D uTexture;

out vec4 fragColor;

void main()
{
    fragColor = texture(uTexture, vTexCoord);
}
)glsl";

/**
 * Grayscale Post-Process
 */
constexpr const char* FS_Grayscale = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform float uIntensity;

out vec4 fragColor;

void main()
{
    vec4 color = texture(uTexture, vTexCoord);
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    fragColor = vec4(mix(color.rgb, vec3(gray), uIntensity), color.a);
}
)glsl";

/**
 * Gaussian Blur (Single Pass - Horizontal or Vertical)
 */
constexpr const char* FS_GaussianBlur = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform vec2 uDirection;  // (1/width, 0) for horizontal, (0, 1/height) for vertical
uniform float uBlurSize;

out vec4 fragColor;

void main()
{
    vec4 sum = vec4(0.0);

    // 9-tap Gaussian kernel
    sum += texture(uTexture, vTexCoord - 4.0 * uDirection * uBlurSize) * 0.0162162162;
    sum += texture(uTexture, vTexCoord - 3.0 * uDirection * uBlurSize) * 0.0540540541;
    sum += texture(uTexture, vTexCoord - 2.0 * uDirection * uBlurSize) * 0.1216216216;
    sum += texture(uTexture, vTexCoord - 1.0 * uDirection * uBlurSize) * 0.1945945946;
    sum += texture(uTexture, vTexCoord) * 0.2270270270;
    sum += texture(uTexture, vTexCoord + 1.0 * uDirection * uBlurSize) * 0.1945945946;
    sum += texture(uTexture, vTexCoord + 2.0 * uDirection * uBlurSize) * 0.1216216216;
    sum += texture(uTexture, vTexCoord + 3.0 * uDirection * uBlurSize) * 0.0540540541;
    sum += texture(uTexture, vTexCoord + 4.0 * uDirection * uBlurSize) * 0.0162162162;

    fragColor = sum;
}
)glsl";

//=============================================================================
// SECTION 6: RENDERWARE COMPATIBILITY SHADERS
//=============================================================================

/**
 * RenderWare-style Vertex Shader
 *
 * Handles the typical RenderWare vertex format used by GTA:SA:
 *   - Position (float3)
 *   - Normal (float3)
 *   - Color (ubyte4)
 *   - TexCoord (float2)
 *
 * This shader is designed to work with GTA:SA's native mesh data.
 */
constexpr const char* VS_RenderWare = R"glsl(#version 300 es
precision highp float;

// RenderWare vertex attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;      // Pre-lit vertex color
layout(location = 3) in vec2 aTexCoord;

// Transform matrices
uniform mat4 uWorld;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

// Ambient light (GTA:SA uses pre-baked lighting + ambient)
uniform vec4 uAmbientColor;

out vec2 vTexCoord;
out vec4 vColor;
out vec3 vWorldNormal;
out vec3 vWorldPos;

void main()
{
    vec4 worldPos = uWorld * vec4(aPosition, 1.0);
    gl_Position = uProjection * uView * worldPos;

    vTexCoord = aTexCoord;
    vWorldPos = worldPos.xyz;
    vWorldNormal = uNormalMatrix * aNormal;

    // Combine pre-lit color with ambient
    vColor = aColor * uAmbientColor;
}
)glsl";

constexpr const char* FS_RenderWare = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;
in vec3 vWorldNormal;
in vec3 vWorldPos;

uniform sampler2D uTexture;
uniform bool uAlphaTest;
uniform float uAlphaRef;

out vec4 fragColor;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);
    vec4 finalColor = texColor * vColor;

    // Alpha test (common in GTA:SA for vegetation, fences, etc.)
    if (uAlphaTest && finalColor.a < uAlphaRef)
        discard;

    fragColor = finalColor;
}
)glsl";

/**
 * RenderWare Night Vertex Colors
 *
 * GTA:SA uses dual vertex colors for day/night interpolation.
 * This shader handles that case.
 */
constexpr const char* VS_RenderWareDayNight = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aDayColor;
layout(location = 3) in vec4 aNightColor;
layout(location = 4) in vec2 aTexCoord;

uniform mat4 uWorld;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uDayNightBalance;  // 0.0 = day, 1.0 = night

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    vec4 worldPos = uWorld * vec4(aPosition, 1.0);
    gl_Position = uProjection * uView * worldPos;

    vTexCoord = aTexCoord;
    vColor = mix(aDayColor, aNightColor, uDayNightBalance);
}
)glsl";

//=============================================================================
// SECTION 7: WATER SHADER (GTA:SA style)
//=============================================================================

constexpr const char* VS_Water = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 uMVP;
uniform float uTime;
uniform float uWaveHeight;
uniform float uWaveFrequency;

out vec2 vTexCoord;
out vec2 vTexCoord2;
out float vHeight;

void main()
{
    vec3 pos = aPosition;

    // Simple wave animation
    float wave1 = sin(pos.x * uWaveFrequency + uTime) * uWaveHeight;
    float wave2 = sin(pos.y * uWaveFrequency * 0.7 + uTime * 1.3) * uWaveHeight;
    pos.z += wave1 + wave2;

    gl_Position = uMVP * vec4(pos, 1.0);

    // Two texture coordinates for water layers
    vTexCoord = aTexCoord + vec2(uTime * 0.02, uTime * 0.01);
    vTexCoord2 = aTexCoord * 2.0 + vec2(uTime * -0.01, uTime * 0.015);
    vHeight = pos.z;
}
)glsl";

constexpr const char* FS_Water = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec2 vTexCoord2;
in float vHeight;

uniform sampler2D uTexture;
uniform vec4 uWaterColor;
uniform float uTransparency;

out vec4 fragColor;

void main()
{
    vec4 tex1 = texture(uTexture, vTexCoord);
    vec4 tex2 = texture(uTexture, vTexCoord2);

    // Blend two water texture layers
    vec4 waterTex = mix(tex1, tex2, 0.5);

    // Apply water color tint
    vec4 finalColor = waterTex * uWaterColor;
    finalColor.a = uTransparency;

    fragColor = finalColor;
}
)glsl";

//=============================================================================
// SECTION 8: FONT/TEXT SHADER
//=============================================================================

/**
 * Font Rendering Shader
 *
 * For rendering bitmap fonts with alpha channel.
 * The texture contains grayscale font glyphs.
 */
constexpr const char* VS_Font = R"glsl(#version 300 es
precision highp float;

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    gl_Position = uMVP * vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)glsl";

constexpr const char* FS_Font = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 fragColor;

void main()
{
    float alpha = texture(uTexture, vTexCoord).a;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)glsl";

/**
 * SDF Font Rendering Shader
 *
 * For high-quality scalable text using Signed Distance Fields.
 */
constexpr const char* FS_FontSDF = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;
uniform float uSmoothing;  // Typically 0.25 / (spread * scale)

out vec4 fragColor;

void main()
{
    float distance = texture(uTexture, vTexCoord).a;
    float alpha = smoothstep(0.5 - uSmoothing, 0.5 + uSmoothing, distance);
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)glsl";

//=============================================================================
// Shader Program Info Structure
//=============================================================================

struct ShaderProgramInfo
{
    const char* name;
    const char* vertexSource;
    const char* fragmentSource;
};

/**
 * Complete list of all shader programs for easy iteration during initialization.
 */
constexpr ShaderProgramInfo g_ShaderPrograms[] = {
    // 2D Shaders
    {"2D_Color",              VS_2D_Color,           FS_2D_Color},
    {"2D_Textured",           VS_2D_Textured,        FS_2D_Textured},
    {"2D_TexturedAlphaTest",  VS_2D_Textured,        FS_2D_TexturedAlphaTest},

    // 3D Shaders
    {"3D_Color",              VS_3D_Color,           FS_3D_Color},
    {"3D_Textured",           VS_3D_Textured,        FS_3D_Textured},
    {"3D_TexturedLit",        VS_3D_TexturedLit,     FS_3D_TexturedLit},
    {"3D_TexturedFog",        VS_3D_TexturedFog,     FS_3D_TexturedFog},

    // MTA Special
    {"MTA_2D",                VS_MTA_2D,             FS_MTA_2D},

    // Post-Processing
    {"PP_Copy",               VS_Fullscreen,         FS_Copy},
    {"PP_Grayscale",          VS_Fullscreen,         FS_Grayscale},
    {"PP_GaussianBlur",       VS_Fullscreen,         FS_GaussianBlur},

    // RenderWare
    {"RW_Basic",              VS_RenderWare,         FS_RenderWare},
    {"RW_DayNight",           VS_RenderWareDayNight, FS_RenderWare},

    // Special Effects
    {"Water",                 VS_Water,              FS_Water},

    // Font/Text
    {"Font",                  VS_Font,               FS_Font},
    {"FontSDF",               VS_Font,               FS_FontSDF},
};

constexpr size_t g_NumShaderPrograms = sizeof(g_ShaderPrograms) / sizeof(g_ShaderPrograms[0]);

} // namespace MTA::Android::Graphics::Shaders

#endif // GLES_SHADERS_H
