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
    if (m_gameBase == 0)
        return 0;

    // Get player array and focus index
    #if defined(__aarch64__)
        uintptr_t* playersPtr = reinterpret_cast<uintptr_t*>(m_gameBase + ARM::ARM64::g_WorldPlayersPtr);
        int playerIndex = *reinterpret_cast<int*>(m_gameBase + ARM::ARM64::g_PlayerInFocus);
    #else
        uintptr_t* playersPtr = reinterpret_cast<uintptr_t*>(m_gameBase + ARM::ARM32::g_WorldPlayersPtr);
        int playerIndex = *reinterpret_cast<int*>(m_gameBase + ARM::ARM32::g_PlayerInFocus);
    #endif

    if (!playersPtr || playerIndex < 0 || playerIndex > 1)
        return 0;

    return playersPtr[playerIndex];
}

inline bool CPlayerSync::IsPlayerInGame() const
{
    uintptr_t ped = GetLocalPlayerPed();
    if (ped == 0)
        return false;

    // Check if ped has a valid matrix (means it's spawned in world)
    RwMatrix* matrix = GetPlayerMatrix(ped);
    return (matrix != nullptr);
}

inline RwMatrix* CPlayerSync::GetPlayerMatrix(uintptr_t pedPtr) const
{
    if (pedPtr == 0)
        return nullptr;

    #if defined(__aarch64__)
        uintptr_t matrixPtr = *reinterpret_cast<uintptr_t*>(pedPtr + PedOffsets::ARM64_MATRIX_PTR);
    #else
        uintptr_t matrixPtr = *reinterpret_cast<uintptr_t*>(pedPtr + PedOffsets::MATRIX_PTR);
    #endif

    if (matrixPtr == 0)
        return nullptr;

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
        data.health = static_cast<uint8_t>(*health);

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
