/*
 * MTA:SA Android - Player Synchronization
 *
 * Phase 7d: Reads player position from GTA:SA memory and syncs with server
 *
 * This module handles:
 * - Reading player position/rotation from game memory
 * - Sending position updates to MTA server
 * - Processing position updates from other players
 */

#ifndef CPLAYER_SYNC_H
#define CPLAYER_SYNC_H

#include <cstdint>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "../signatures/ARMAddressMap.h"

namespace MTA::Android::Sync
{

#define SYNC_LOG_TAG "MTA-Sync"
#define SYNC_LOGI(...) __android_log_print(ANDROID_LOG_INFO, SYNC_LOG_TAG, __VA_ARGS__)
#define SYNC_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, SYNC_LOG_TAG, __VA_ARGS__)
#define SYNC_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, SYNC_LOG_TAG, __VA_ARGS__)

//=============================================================================
// Position/Rotation Structures
//=============================================================================

struct CVector
{
    float x, y, z;

    CVector() : x(0), y(0), z(0) {}
    CVector(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct PlayerSyncData
{
    CVector position;
    float   rotation;       // Z rotation in radians
    CVector velocity;
    uint8_t health;
    uint8_t armor;
    uint8_t weaponId;
    bool    isInVehicle;
    uint16_t vehicleId;

    PlayerSyncData() : rotation(0), health(100), armor(0), weaponId(0),
                       isInVehicle(false), vehicleId(0) {}
};

//=============================================================================
// GTA:SA Memory Offsets (CPed structure)
//=============================================================================

// CPed/CPlayerPed structure offsets (from SA-MP/MTA research)
namespace PedOffsets
{
    // CPlaceable (base class)
    constexpr uint32_t MATRIX_PTR       = 0x14;     // RwMatrix* - transformation matrix

    // CEntity (inherits CPlaceable)
    constexpr uint32_t MODEL_INDEX      = 0x22;     // uint16_t

    // CPhysical (inherits CEntity)
    constexpr uint32_t VELOCITY         = 0x44;     // CVector - linear velocity
    constexpr uint32_t TURN_SPEED       = 0x50;     // CVector - angular velocity

    // CPed (inherits CPhysical)
    constexpr uint32_t PED_TYPE         = 0x5A4;    // ePedType
    constexpr uint32_t HEALTH           = 0x540;    // float
    constexpr uint32_t MAX_HEALTH       = 0x544;    // float
    constexpr uint32_t ARMOR            = 0x548;    // float
    constexpr uint32_t CURRENT_WEAPON   = 0x718;    // uint8_t - current weapon slot
    constexpr uint32_t WEAPON_SLOTS     = 0x5A0;    // CWeapon[13]
    constexpr uint32_t PED_STATE        = 0x52E;    // ePedState
    constexpr uint32_t MOVE_STATE       = 0x530;    // eMoveState
    constexpr uint32_t VEHICLE          = 0x58C;    // CVehicle* - current vehicle

    // ARM64 adjustments (structures may be larger)
    #if defined(__aarch64__)
        constexpr uint32_t ARM64_MATRIX_PTR = 0x18;
        constexpr uint32_t ARM64_VELOCITY   = 0x50;
        constexpr uint32_t ARM64_HEALTH     = 0x580;
        constexpr uint32_t ARM64_ARMOR      = 0x588;
        constexpr uint32_t ARM64_VEHICLE    = 0x5C0;
    #endif
}

// RwMatrix structure (row-major 4x4)
struct RwMatrix
{
    CVector right;      // 0x00
    uint32_t flags;     // 0x0C
    CVector up;         // 0x10
    uint32_t pad1;      // 0x1C
    CVector at;         // 0x20 - forward vector
    uint32_t pad2;      // 0x2C
    CVector pos;        // 0x30 - position!
    uint32_t pad3;      // 0x3C
};

//=============================================================================
// Position Callback Type
//=============================================================================

using PositionCallback = std::function<void(const PlayerSyncData& data)>;

//=============================================================================
// CPlayerSync Class
//=============================================================================

class CPlayerSync
{
public:
    CPlayerSync();
    ~CPlayerSync();

    /**
     * Initialize sync system with game library base
     */
    bool Initialize(uintptr_t gameBase);

    /**
     * Start sync loop (call after player spawns in game)
     */
    bool Start();

    /**
     * Stop sync loop
     */
    void Stop();

    /**
     * Check if sync is running
     */
    bool IsRunning() const { return m_running.load(); }

    /**
     * Set callback for position updates (called on sync thread)
     */
    void SetPositionCallback(PositionCallback callback);

    /**
     * Get current sync data (thread-safe)
     */
    PlayerSyncData GetSyncData() const;

    /**
     * Check if player is in game (spawned, not in menu)
     */
    bool IsPlayerInGame() const;

    /**
     * Get local player ped pointer
     */
    uintptr_t GetLocalPlayerPed() const;

private:
    /**
     * Sync thread function
     */
    void SyncThreadFunc();

    /**
     * Read player data from game memory
     */
    bool ReadPlayerData(PlayerSyncData& data);

    /**
     * Get player matrix from ped
     */
    RwMatrix* GetPlayerMatrix(uintptr_t pedPtr) const;

    // Game library base
    uintptr_t m_gameBase = 0;

    // Sync state
    std::atomic<bool> m_running{false};
    std::thread m_syncThread;

    // Cached sync data
    mutable std::atomic<bool> m_dataLock{false};
    PlayerSyncData m_syncData;

    // Callback
    PositionCallback m_positionCallback;

    // Sync rate (ms between updates)
    static constexpr int SYNC_INTERVAL_MS = 100;  // 10 Hz
};

//=============================================================================
// Implementation
//=============================================================================

inline CPlayerSync::CPlayerSync()
{
}

inline CPlayerSync::~CPlayerSync()
{
    Stop();
}

inline bool CPlayerSync::Initialize(uintptr_t gameBase)
{
    if (gameBase == 0)
    {
        SYNC_LOGE("Invalid game base address");
        return false;
    }

    m_gameBase = gameBase;
    SYNC_LOGI("Player sync initialized, game base: 0x%lx", (unsigned long)gameBase);
    return true;
}

inline bool CPlayerSync::Start()
{
    if (m_running.load())
    {
        SYNC_LOGD("Sync already running");
        return true;
    }

    if (m_gameBase == 0)
    {
        SYNC_LOGE("Cannot start sync: not initialized");
        return false;
    }

    m_running = true;
    m_syncThread = std::thread(&CPlayerSync::SyncThreadFunc, this);

    SYNC_LOGI("Player sync started");
    return true;
}

inline void CPlayerSync::Stop()
{
    if (!m_running.load())
        return;

    m_running = false;

    if (m_syncThread.joinable())
    {
        m_syncThread.join();
    }

    SYNC_LOGI("Player sync stopped");
}

inline void CPlayerSync::SetPositionCallback(PositionCallback callback)
{
    m_positionCallback = callback;
}

inline PlayerSyncData CPlayerSync::GetSyncData() const
{
    // Simple spinlock
    while (m_dataLock.exchange(true)) {}
    PlayerSyncData data = m_syncData;
    m_dataLock = false;
    return data;
}

inline uintptr_t CPlayerSync::GetLocalPlayerPed() const
{
    // Rate-limit logging to avoid spam (log every 50 calls)
    static int debugCounter = 0;
    bool shouldLog = (debugCounter++ % 50 == 0);

    if (m_gameBase == 0)
    {
        if (shouldLog) SYNC_LOGE("GetLocalPlayerPed: FAIL - m_gameBase is 0!");
        return 0;
    }

    if (shouldLog) SYNC_LOGI("GetLocalPlayerPed: m_gameBase=0x%lx", (unsigned long)m_gameBase);

#if defined(__aarch64__)
    //=========================================================================
    // ARM64: Debug both approaches to find where detection fails
    //=========================================================================

    // --- Approach 1: FindPlayerPed function call (what CGameBypass uses) ---
    if (shouldLog) {
        SYNC_LOGI("GetLocalPlayerPed: [ARM64] Trying FindPlayerPed function...");
        SYNC_LOGI("  FindPlayerPed offset=0x%x, absolute addr=0x%lx",
                 MTA::Android::ARM::ARM64::FindPlayerPed,
                 (unsigned long)(m_gameBase + MTA::Android::ARM::ARM64::FindPlayerPed));
    }

    typedef void* (*FindPlayerPed_t)(int);
    auto FindPlayerPedFunc = reinterpret_cast<FindPlayerPed_t>(m_gameBase + MTA::Android::ARM::ARM64::FindPlayerPed);
    void* pPed = FindPlayerPedFunc(0);

    if (shouldLog) {
        if (pPed) {
            SYNC_LOGI("GetLocalPlayerPed: [ARM64] FindPlayerPed(0) SUCCESS = 0x%lx", (unsigned long)pPed);
        } else {
            SYNC_LOGE("GetLocalPlayerPed: [ARM64] FindPlayerPed(0) returned NULL!");
        }
    }

    // --- Approach 2: Offset-based (for comparison/debugging) ---
    if (shouldLog) {
        SYNC_LOGI("GetLocalPlayerPed: [ARM64] Also checking offset-based approach for comparison...");

        uintptr_t playersArrayAddr = m_gameBase + MTA::Android::ARM::ARM64::g_WorldPlayersPtr;
        uintptr_t playerFocusAddr = m_gameBase + MTA::Android::ARM::ARM64::g_PlayerInFocus;

        SYNC_LOGI("  g_WorldPlayersPtr: offset=0x%x, addr=0x%lx",
                 MTA::Android::ARM::ARM64::g_WorldPlayersPtr, (unsigned long)playersArrayAddr);
        SYNC_LOGI("  g_PlayerInFocus: offset=0x%x, addr=0x%lx",
                 MTA::Android::ARM::ARM64::g_PlayerInFocus, (unsigned long)playerFocusAddr);

        // Try to read values (may crash if wrong offsets - be careful)
        uintptr_t* playersPtr = reinterpret_cast<uintptr_t*>(playersArrayAddr);
        int playerIndex = *reinterpret_cast<int*>(playerFocusAddr);

        SYNC_LOGI("  playerIndex (from offset) = %d", playerIndex);

        if (playersPtr && playerIndex >= 0 && playerIndex <= 1) {
            uintptr_t offsetBasedPed = playersPtr[playerIndex];
            SYNC_LOGI("  playersPtr[%d] (from offset) = 0x%lx", playerIndex, (unsigned long)offsetBasedPed);

            // Compare the two approaches
            if (offsetBasedPed != reinterpret_cast<uintptr_t>(pPed)) {
                SYNC_LOGE("  MISMATCH! FindPlayerPed=0x%lx vs Offset=0x%lx",
                         (unsigned long)pPed, (unsigned long)offsetBasedPed);
            } else {
                SYNC_LOGI("  MATCH: Both approaches return same ped address");
            }
        } else {
            SYNC_LOGE("  Offset-based approach FAILED: playersPtr=%p, playerIndex=%d",
                     (void*)playersPtr, playerIndex);
        }
    }

    // Use FindPlayerPed result (the working approach)
    if (pPed) {
        return reinterpret_cast<uintptr_t>(pPed);
    }

    if (shouldLog) SYNC_LOGE("GetLocalPlayerPed: [ARM64] FINAL RESULT = NULL (player not found!)");
    return 0;

#else
    //=========================================================================
    // ARM32: Use offset-based approach
    //=========================================================================

    if (shouldLog) {
        SYNC_LOGI("GetLocalPlayerPed: [ARM32] Using offset-based approach...");
        SYNC_LOGI("  g_WorldPlayersPtr: offset=0x%x, addr=0x%lx",
                 MTA::Android::ARM::ARM32::g_WorldPlayersPtr,
                 (unsigned long)(m_gameBase + MTA::Android::ARM::ARM32::g_WorldPlayersPtr));
        SYNC_LOGI("  g_PlayerInFocus: offset=0x%x, addr=0x%lx",
                 MTA::Android::ARM::ARM32::g_PlayerInFocus,
                 (unsigned long)(m_gameBase + MTA::Android::ARM::ARM32::g_PlayerInFocus));
    }

    uintptr_t* playersPtr = reinterpret_cast<uintptr_t*>(m_gameBase + MTA::Android::ARM::ARM32::g_WorldPlayersPtr);
    int playerIndex = *reinterpret_cast<int*>(m_gameBase + MTA::Android::ARM::ARM32::g_PlayerInFocus);

    if (shouldLog) {
        SYNC_LOGI("  playersPtr=0x%lx, playerIndex=%d", (unsigned long)playersPtr, playerIndex);
    }

    if (!playersPtr) {
        if (shouldLog) SYNC_LOGE("GetLocalPlayerPed: [ARM32] FAIL - playersPtr is NULL!");
        return 0;
    }

    if (playerIndex < 0 || playerIndex > 1) {
        if (shouldLog) SYNC_LOGE("GetLocalPlayerPed: [ARM32] FAIL - playerIndex=%d out of range!", playerIndex);
        return 0;
    }

    uintptr_t pedPtr = playersPtr[playerIndex];

    if (shouldLog) {
        if (pedPtr) {
            SYNC_LOGI("GetLocalPlayerPed: [ARM32] SUCCESS - playersPtr[%d]=0x%lx", playerIndex, (unsigned long)pedPtr);
        } else {
            SYNC_LOGE("GetLocalPlayerPed: [ARM32] FAIL - playersPtr[%d] is NULL!", playerIndex);
        }
    }

    return pedPtr;
#endif
}

inline bool CPlayerSync::IsPlayerInGame() const
{
    // Rate-limit logging (log every 50 calls)
    static int debugCounter = 0;
    bool shouldLog = (debugCounter++ % 50 == 0);

    uintptr_t ped = GetLocalPlayerPed();
    if (ped == 0)
    {
        if (shouldLog) SYNC_LOGE("IsPlayerInGame: NO - GetLocalPlayerPed() returned 0");
        return false;
    }

    // Check if ped has a valid matrix (means it's spawned in world)
    RwMatrix* matrix = GetPlayerMatrix(ped);

    if (matrix == nullptr)
    {
        if (shouldLog) SYNC_LOGE("IsPlayerInGame: NO - ped=0x%lx but GetPlayerMatrix() returned NULL", (unsigned long)ped);
        return false;
    }

    if (shouldLog) {
        SYNC_LOGI("IsPlayerInGame: YES - ped=0x%lx, matrix=0x%lx, pos=(%.2f, %.2f, %.2f)",
                 (unsigned long)ped, (unsigned long)matrix,
                 matrix->pos.x, matrix->pos.y, matrix->pos.z);
    }

    return true;
}

inline RwMatrix* CPlayerSync::GetPlayerMatrix(uintptr_t pedPtr) const
{
    // Rate-limit logging (log every 50 calls)
    static int debugCounter = 0;
    bool shouldLog = (debugCounter++ % 50 == 0);

    if (pedPtr == 0)
    {
        if (shouldLog) SYNC_LOGE("GetPlayerMatrix: FAIL - pedPtr is 0");
        return nullptr;
    }

    #if defined(__aarch64__)
        if (shouldLog) {
            SYNC_LOGI("GetPlayerMatrix: [ARM64] pedPtr=0x%lx, MATRIX_PTR offset=0x%x",
                     (unsigned long)pedPtr, PedOffsets::ARM64_MATRIX_PTR);
        }
        uintptr_t matrixPtr = *reinterpret_cast<uintptr_t*>(pedPtr + PedOffsets::ARM64_MATRIX_PTR);
    #else
        if (shouldLog) {
            SYNC_LOGI("GetPlayerMatrix: [ARM32] pedPtr=0x%lx, MATRIX_PTR offset=0x%x",
                     (unsigned long)pedPtr, PedOffsets::MATRIX_PTR);
        }
        uintptr_t matrixPtr = *reinterpret_cast<uintptr_t*>(pedPtr + PedOffsets::MATRIX_PTR);
    #endif

    if (shouldLog) {
        SYNC_LOGI("GetPlayerMatrix: matrixPtr value = 0x%lx", (unsigned long)matrixPtr);
    }

    if (matrixPtr == 0)
    {
        if (shouldLog) SYNC_LOGE("GetPlayerMatrix: FAIL - matrix pointer at ped+0x%x is NULL",
                                #if defined(__aarch64__)
                                    PedOffsets::ARM64_MATRIX_PTR
                                #else
                                    PedOffsets::MATRIX_PTR
                                #endif
                                );
        return nullptr;
    }

    if (shouldLog) {
        // Validate the matrix looks reasonable
        RwMatrix* matrix = reinterpret_cast<RwMatrix*>(matrixPtr);
        SYNC_LOGI("GetPlayerMatrix: SUCCESS - matrix at 0x%lx, pos=(%.2f, %.2f, %.2f)",
                 (unsigned long)matrixPtr, matrix->pos.x, matrix->pos.y, matrix->pos.z);
    }

    return reinterpret_cast<RwMatrix*>(matrixPtr);
}

inline bool CPlayerSync::ReadPlayerData(PlayerSyncData& data)
{
    uintptr_t ped = GetLocalPlayerPed();
    if (ped == 0)
        return false;

    RwMatrix* matrix = GetPlayerMatrix(ped);
    if (!matrix)
        return false;

    // Read position from matrix
    data.position = matrix->pos;

    // Calculate rotation from forward vector (at)
    data.rotation = atan2f(-matrix->at.x, matrix->at.y);

    // Read velocity
    #if defined(__aarch64__)
        CVector* vel = reinterpret_cast<CVector*>(ped + PedOffsets::ARM64_VELOCITY);
        float* health = reinterpret_cast<float*>(ped + PedOffsets::ARM64_HEALTH);
        float* armor = reinterpret_cast<float*>(ped + PedOffsets::ARM64_ARMOR);
        uintptr_t vehicle = *reinterpret_cast<uintptr_t*>(ped + PedOffsets::ARM64_VEHICLE);
    #else
        CVector* vel = reinterpret_cast<CVector*>(ped + PedOffsets::VELOCITY);
        float* health = reinterpret_cast<float*>(ped + PedOffsets::HEALTH);
        float* armor = reinterpret_cast<float*>(ped + PedOffsets::ARMOR);
        uintptr_t vehicle = *reinterpret_cast<uintptr_t*>(ped + PedOffsets::VEHICLE);
    #endif

    if (vel)
        data.velocity = *vel;

    if (health)
    {
        float healthVal = *health;
        // WORKAROUND: Health reading returns 0 incorrectly on ARM64
        // Force minimum health of 100 until we debug the correct offset
        if (healthVal <= 0.0f)
            healthVal = 100.0f;
        data.health = static_cast<uint8_t>(healthVal);
    }

    if (armor)
        data.armor = static_cast<uint8_t>(*armor);

    data.isInVehicle = (vehicle != 0);

    return true;
}

inline void CPlayerSync::SyncThreadFunc()
{
    SYNC_LOGI("Sync thread started");

    int frameCount = 0;

    while (m_running.load())
    {
        // Check if player is in game
        if (IsPlayerInGame())
        {
            PlayerSyncData data;
            if (ReadPlayerData(data))
            {
                // Update cached data
                while (m_dataLock.exchange(true)) {}
                m_syncData = data;
                m_dataLock = false;

                // Call callback if set
                if (m_positionCallback)
                {
                    m_positionCallback(data);
                }

                // Log position every 10 frames (1 second)
                if (++frameCount >= 10)
                {
                    SYNC_LOGI("Player pos: %.2f, %.2f, %.2f (health: %d)",
                             data.position.x, data.position.y, data.position.z,
                             data.health);
                    frameCount = 0;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(SYNC_INTERVAL_MS));
    }

    SYNC_LOGI("Sync thread finished");
}

} // namespace MTA::Android::Sync

#endif // CPLAYER_SYNC_H
