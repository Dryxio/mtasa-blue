/*
 * MTA:SA Android - Game Bypass Module
 *
 * CGameBypass: Handles bypassing singleplayer menus and auto-spawning
 * the player into the multiplayer world.
 *
 * This module:
 * - Detects when the game has finished loading
 * - Bypasses the main menu and loading screens
 * - Auto-spawns the local player at a position
 * - Sets up the camera and world state for multiplayer
 *
 * Phase 7d: Position Sync & Stability
 */

#ifndef CGAME_BYPASS_H
#define CGAME_BYPASS_H

#include <cstdint>
#include <functional>
#include "../signatures/ARMAddressMap.h"

namespace MTA::Android::Game
{

//=============================================================================
// SystemState Enum (from GTA:SA)
//=============================================================================

enum class SystemState : uint32_t
{
    GS_START_UP = 0,
    GS_INIT_LOGO_MPEG = 1,
    GS_LOGO_MPEG = 2,
    GS_INIT_INTRO_MPEG = 3,
    GS_INTRO_MPEG = 4,
    GS_INIT_ONCE = 5,
    GS_INIT_FRONTEND = 6,
    GS_FRONTEND = 7,
    GS_INIT_PLAYING_GAME = 8,
    GS_PLAYING_GAME = 9
};

//=============================================================================
// CGameBypass Class
//=============================================================================

class CGameBypass
{
public:
    //-------------------------------------------------------------------------
    // Singleton access
    //-------------------------------------------------------------------------
    static CGameBypass& GetInstance();

    //-------------------------------------------------------------------------
    // Initialization
    //-------------------------------------------------------------------------

    /**
     * Initialize the game bypass system
     * Call this after libGTASA is loaded
     * @param libBase Base address of libGTASA.so
     * @return true if initialization succeeded
     */
    bool Initialize(uintptr_t libBase);

    /**
     * Check if bypass is initialized
     */
    bool IsInitialized() const { return m_initialized; }

    //-------------------------------------------------------------------------
    // Game State
    //-------------------------------------------------------------------------

    /**
     * Get the current game state
     */
    SystemState GetGameState() const;

    /**
     * Set the game state (use with caution!)
     */
    void SetGameState(SystemState state);

    /**
     * Check if the game is in playing state
     */
    bool IsGamePlaying() const;

    /**
     * Check if game has loaded enough to spawn player
     */
    bool IsGameReadyForSpawn() const;

    //-------------------------------------------------------------------------
    // Auto-Spawn
    //-------------------------------------------------------------------------

    /**
     * Spawn the local player at a position
     * @param x, y, z World coordinates
     * @param rotation Heading rotation in degrees
     * @param skin Model ID for the player skin (default: CJ = 0)
     * @return true if spawn succeeded
     */
    bool SpawnLocalPlayer(float x, float y, float z, float rotation, int skin = 0);

    /**
     * Spawn at the default MTA spawn position (Grove Street)
     */
    bool SpawnLocalPlayerDefault();

    /**
     * Check if local player is spawned
     */
    bool IsLocalPlayerSpawned() const { return m_playerSpawned; }

    //-------------------------------------------------------------------------
    // Game Loop Hook
    //-------------------------------------------------------------------------

    /**
     * Process function - call every frame
     * Handles auto-spawn when game is ready
     */
    void Process();

    /**
     * Set callback for when player spawns
     */
    void SetOnSpawnCallback(std::function<void()> callback) { m_onSpawnCallback = callback; }

    //-------------------------------------------------------------------------
    // Utility Functions
    //-------------------------------------------------------------------------

    /**
     * Set world time
     */
    void SetWorldTime(int hour, int minute);

    /**
     * Set world weather
     */
    void SetWorldWeather(int weatherId);

    /**
     * Enable/disable HUD
     */
    void DisplayHUD(bool enable);

    /**
     * Set camera behind player
     */
    void SetCameraBehindPlayer();

private:
    CGameBypass();
    ~CGameBypass() = default;
    CGameBypass(const CGameBypass&) = delete;
    CGameBypass& operator=(const CGameBypass&) = delete;

    //-------------------------------------------------------------------------
    // Internal Methods
    //-------------------------------------------------------------------------

    /**
     * Apply multiplayer patches to disable singleplayer systems
     */
    void ApplyMultiplayerPatches();

    /**
     * Get pointer to the local player ped
     */
    void* GetLocalPlayerPed() const;

    /**
     * Call RestartIfWastedAt on the player
     */
    void RestartPlayerAt(float x, float y, float z, float rotation);

    /**
     * Set player model
     */
    void SetPlayerModel(int modelId);

    /**
     * Refresh world streaming at position
     */
    void RefreshStreamingAt(float x, float y);

    //-------------------------------------------------------------------------
    // Member Variables
    //-------------------------------------------------------------------------

    uintptr_t m_libBase = 0;
    bool m_initialized = false;
    bool m_playerSpawned = false;
    bool m_patchesApplied = false;
    bool m_waitingForSpawn = false;

    std::function<void()> m_onSpawnCallback;

    // Pointers to game data (resolved at initialization)
    uint32_t* m_pGameState = nullptr;

    // Default spawn position (Grove Street)
    static constexpr float DEFAULT_SPAWN_X = 2488.562f;
    static constexpr float DEFAULT_SPAWN_Y = -1666.864f;
    static constexpr float DEFAULT_SPAWN_Z = 12.8757f;
    static constexpr float DEFAULT_SPAWN_ROT = 0.0f;
    static constexpr int DEFAULT_SPAWN_SKIN = 0;  // CJ
};

} // namespace MTA::Android::Game

#endif // CGAME_BYPASS_H
