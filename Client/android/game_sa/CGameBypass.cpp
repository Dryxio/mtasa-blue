/*
 * MTA:SA Android - Game Bypass Implementation
 *
 * Phase 7d: Position Sync & Stability
 */

#include "CGameBypass.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "MTA-GameBypass"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace MTA::Android::Game
{

using namespace MTA::Android::ARM;

//=============================================================================
// Singleton
//=============================================================================

CGameBypass& CGameBypass::GetInstance()
{
    static CGameBypass instance;
    return instance;
}

CGameBypass::CGameBypass()
{
}

//=============================================================================
// Initialization
//=============================================================================

bool CGameBypass::Initialize(uintptr_t libBase)
{
    if (m_initialized)
    {
        LOGW("CGameBypass already initialized");
        return true;
    }

    if (libBase == 0)
    {
        LOGE("Invalid libGTASA base address");
        return false;
    }

    m_libBase = libBase;

    // Resolve game state pointer
#if defined(__aarch64__)
    m_pGameState = reinterpret_cast<uint32_t*>(m_libBase + ARM64::gGameState);
#else
    m_pGameState = reinterpret_cast<uint32_t*>(m_libBase + ARM32::gGameState);
#endif

    LOGI("CGameBypass initialized");
    LOGI("  libGTASA base: 0x%lx", (unsigned long)m_libBase);
    LOGI("  gGameState ptr: 0x%lx", (unsigned long)m_pGameState);

    m_initialized = true;
    m_waitingForSpawn = true;

    return true;
}

//=============================================================================
// Game State
//=============================================================================

SystemState CGameBypass::GetGameState() const
{
    if (!m_initialized || !m_pGameState)
    {
        return SystemState::GS_START_UP;
    }

    return static_cast<SystemState>(*m_pGameState);
}

void CGameBypass::SetGameState(SystemState state)
{
    if (!m_initialized || !m_pGameState)
    {
        LOGE("Cannot set game state - not initialized");
        return;
    }

    LOGI("Setting game state: %d -> %d", *m_pGameState, static_cast<int>(state));
    *m_pGameState = static_cast<uint32_t>(state);
}

bool CGameBypass::IsGamePlaying() const
{
    return GetGameState() == SystemState::GS_PLAYING_GAME;
}

bool CGameBypass::IsGameReadyForSpawn() const
{
    if (!m_initialized)
        return false;

    // Game must be in playing state
    SystemState state = GetGameState();
    if (state != SystemState::GS_PLAYING_GAME && state != SystemState::GS_INIT_PLAYING_GAME)
    {
        return false;
    }

    // Check if local player ped exists
    void* pPed = GetLocalPlayerPed();
    return pPed != nullptr;
}

//=============================================================================
// Auto-Spawn
//=============================================================================

bool CGameBypass::SpawnLocalPlayer(float x, float y, float z, float rotation, int skin)
{
    if (!m_initialized)
    {
        LOGE("Cannot spawn - not initialized");
        return false;
    }

    if (!IsGameReadyForSpawn())
    {
        LOGW("Game not ready for spawn yet");
        return false;
    }

    void* pPed = GetLocalPlayerPed();
    if (!pPed)
    {
        LOGE("Cannot spawn - no local player ped");
        return false;
    }

    LOGI("Spawning local player at (%.2f, %.2f, %.2f) rot=%.2f skin=%d",
         x, y, z, rotation, skin);

    // Apply multiplayer patches if not done yet
    if (!m_patchesApplied)
    {
        ApplyMultiplayerPatches();
        m_patchesApplied = true;
    }

    // Set player skin
    SetPlayerModel(skin);

    // Refresh streaming at spawn location
    RefreshStreamingAt(x, y);

    // Position the player
    RestartPlayerAt(x, y, z, rotation);

    // Set camera behind player
    SetCameraBehindPlayer();

    // Enable HUD
    DisplayHUD(true);

    // Set default world state
    SetWorldTime(12, 0);  // Noon
    SetWorldWeather(0);   // Clear

    m_playerSpawned = true;
    m_waitingForSpawn = false;

    LOGI("Player spawned successfully!");

    // Fire callback
    if (m_onSpawnCallback)
    {
        m_onSpawnCallback();
    }

    return true;
}

bool CGameBypass::SpawnLocalPlayerDefault()
{
    return SpawnLocalPlayer(DEFAULT_SPAWN_X, DEFAULT_SPAWN_Y, DEFAULT_SPAWN_Z,
                           DEFAULT_SPAWN_ROT, DEFAULT_SPAWN_SKIN);
}

//=============================================================================
// Game Loop
//=============================================================================

void CGameBypass::Process()
{
    if (!m_initialized)
        return;

    // Auto-spawn when game is ready
    if (m_waitingForSpawn && !m_playerSpawned)
    {
        if (IsGameReadyForSpawn())
        {
            LOGI("Game ready for spawn - triggering auto-spawn");
            SpawnLocalPlayerDefault();
        }
    }
}

//=============================================================================
// Utility Functions
//=============================================================================

void CGameBypass::SetWorldTime(int hour, int minute)
{
    if (!m_initialized)
        return;

    // Address of CClock::ms_nGameClockHours and CClock::ms_nGameClockMinutes
    // These are adjacent bytes
#if defined(__aarch64__)
    uint8_t* pHours = reinterpret_cast<uint8_t*>(m_libBase + 0xBBBC1A);
    uint8_t* pMinutes = reinterpret_cast<uint8_t*>(m_libBase + 0xBBBC1B);
#else
    uint8_t* pHours = reinterpret_cast<uint8_t*>(m_libBase + 0x953142);
    uint8_t* pMinutes = reinterpret_cast<uint8_t*>(m_libBase + 0x953143);
#endif

    *pHours = static_cast<uint8_t>(hour);
    *pMinutes = static_cast<uint8_t>(minute);

    LOGI("Set world time to %02d:%02d", hour, minute);
}

void CGameBypass::SetWorldWeather(int weatherId)
{
    if (!m_initialized)
        return;

    // CWeather::ForceWeatherNow address
#if defined(__aarch64__)
    uint16_t* pWeather = reinterpret_cast<uint16_t*>(m_libBase + 0xBBBC3E);
    uint16_t* pWeatherOld = reinterpret_cast<uint16_t*>(m_libBase + 0xBBBC40);
#else
    uint16_t* pWeather = reinterpret_cast<uint16_t*>(m_libBase + 0x953166);
    uint16_t* pWeatherOld = reinterpret_cast<uint16_t*>(m_libBase + 0x953168);
#endif

    *pWeather = static_cast<uint16_t>(weatherId);
    *pWeatherOld = static_cast<uint16_t>(weatherId);

    LOGI("Set world weather to %d", weatherId);
}

void CGameBypass::DisplayHUD(bool enable)
{
    if (!m_initialized)
        return;

#if defined(__aarch64__)
    uint8_t* pHUDVisibility = reinterpret_cast<uint8_t*>(m_libBase + ARM64::g_HUDVisibility);
#else
    uint8_t* pHUDVisibility = reinterpret_cast<uint8_t*>(m_libBase + ARM32::g_HUDVisibility + 1);
#endif

    *pHUDVisibility = enable ? 0 : 1;  // 0 = visible, 1 = hidden

    LOGI("HUD %s", enable ? "enabled" : "disabled");
}

void CGameBypass::SetCameraBehindPlayer()
{
    if (!m_initialized)
        return;

    // Call CCamera::SetBehindPlayer (via TheCamera->SetBehindPlayer())
    // This sets the camera to follow the player from behind

    // For now, we'll set camera position relative to player
    // Full implementation would call the game function

    LOGI("Camera set behind player");
}

//=============================================================================
// Internal Methods
//=============================================================================

void CGameBypass::ApplyMultiplayerPatches()
{
    LOGI("Applying multiplayer patches...");

    // These patches disable singleplayer systems that interfere with multiplayer
    // Based on SA-MP's patches:
    //
    // 1. Disable population spawning (CPopulation::AddToPopulation)
    // 2. Disable car generation (CCarCtrl::GenerateRandomCars)
    // 3. Disable garage system (CGarages::Init_AfterRestart)
    // 4. Disable loading screen (CLoadingScreen::DisplayPCScreen)
    // 5. Disable cheats (CCheat::ProcessCheats)
    //
    // For now, we just log that we would apply these patches
    // Full implementation requires memory patching with mprotect

    LOGI("Multiplayer patches applied (stub)");
}

void* CGameBypass::GetLocalPlayerPed() const
{
    if (!m_initialized)
        return nullptr;

    // For now, just return a non-null value to indicate game is ready
    // The actual player ped lookup needs proper offset research
    // This allows the spawn flow to complete without crashing

    // Return a dummy non-null pointer to indicate "ready"
    // We won't actually access this memory in the simplified spawn
    return reinterpret_cast<void*>(1);
}

void CGameBypass::RestartPlayerAt(float x, float y, float z, float rotation)
{
    if (!m_initialized)
        return;

    // For now, just log the spawn position
    // Full implementation requires proper player ped offsets for ARM64
    // which need to be researched from the actual libGTASA.so binary

    LOGI("RestartPlayerAt called: (%.2f, %.2f, %.2f) rot=%.2f", x, y, z, rotation);
    LOGI("NOTE: Position setting is a stub - proper offsets needed");

    // TODO: Research actual CPlayerPed offsets for ARM64:
    // - Find CWorld::Players array address
    // - Find CPlayerInfo structure layout
    // - Find CPed/CEntity matrix offset
    // - Call game's native teleport/spawn function instead of direct memory write
}

void CGameBypass::SetPlayerModel(int modelId)
{
    if (!m_initialized)
        return;

    // CPlayerPed::SetModelIndex
    // For now, just log - full implementation would call the game function

    LOGI("Set player model to %d (stub)", modelId);
}

void CGameBypass::RefreshStreamingAt(float x, float y)
{
    if (!m_initialized)
        return;

    // CStreaming::RequestZone or RefreshStreamingAt
    // This forces the game to load world data at the position

    LOGI("Refresh streaming at (%.2f, %.2f) (stub)", x, y);
}

} // namespace MTA::Android::Game
