/*
 * MTA:SA Android - Remote Player
 *
 * Represents a remote player in the game world.
 * Handles position interpolation, CPed management, and sync data.
 *
 * Phase 7e: Player Synchronization
 */

#ifndef CREMOTE_PLAYER_H
#define CREMOTE_PLAYER_H

#include <cstdint>
#include <string>
#include <chrono>
#include <atomic>
#include <algorithm>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "CPedFactory.h"

namespace MTA::Android::Multiplayer
{

#define REMOTE_LOG_TAG "MTA-RemotePlayer"
#define REMOTE_LOGI(...) __android_log_print(ANDROID_LOG_INFO, REMOTE_LOG_TAG, __VA_ARGS__)
#define REMOTE_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, REMOTE_LOG_TAG, __VA_ARGS__)
#define REMOTE_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, REMOTE_LOG_TAG, __VA_ARGS__)
#define REMOTE_LOGW(...) __android_log_print(ANDROID_LOG_WARN, REMOTE_LOG_TAG, __VA_ARGS__)

//=============================================================================
// Constants
//=============================================================================

constexpr float INTERPOLATION_WARP_THRESHOLD = 50.0f;  // Teleport if > 50 units
constexpr int SYNC_TIMEOUT_MS = 15000;                  // Consider stale after 15s (debug to keep players alive)
constexpr int INTERPOLATION_TIME_MS = 100;              // 100ms between syncs

//=============================================================================
// CVector (if not already included)
//=============================================================================

struct CVector3D
{
    float x, y, z;

    CVector3D() : x(0), y(0), z(0) {}
    CVector3D(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    CVector3D operator+(const CVector3D& other) const
    {
        return CVector3D(x + other.x, y + other.y, z + other.z);
    }

    CVector3D operator-(const CVector3D& other) const
    {
        return CVector3D(x - other.x, y - other.y, z - other.z);
    }

    CVector3D operator*(float scalar) const
    {
        return CVector3D(x * scalar, y * scalar, z * scalar);
    }

    float Length() const
    {
        return sqrtf(x * x + y * y + z * z);
    }

    float DistanceTo(const CVector3D& other) const
    {
        return (*this - other).Length();
    }
};

//=============================================================================
// Remote Player Sync Data
//=============================================================================

struct RemoteSyncData
{
    // Position and movement
    CVector3D position;
    CVector3D targetPosition;
    CVector3D velocity;

    // Rotation
    float rotation;
    float targetRotation;

    // Health/Armor
    uint8_t health;
    uint8_t armor;

    // Weapon
    uint8_t weaponSlot;
    uint16_t ammo;

    // Flags
    bool isOnGround;
    bool isInWater;
    bool isDucked;
    bool isInVehicle;

    // Vehicle info (if in vehicle)
    uint32_t vehicleId;
    uint8_t vehicleSeat;

    // Controller/Key state (for animations via CPad hooks)
    uint8_t controllerLeftStickX;   // 0-255, center=128
    uint8_t controllerLeftStickY;   // 0-255, center=128
    uint16_t keyFlags;              // Key bitfield from sync packet

    // Timing
    uint64_t lastSyncTime;
    uint16_t syncTimeContext;

    RemoteSyncData()
        : rotation(0), targetRotation(0)
        , health(100), armor(0)
        , weaponSlot(0), ammo(0)
        , isOnGround(true), isInWater(false)
        , isDucked(false), isInVehicle(false)
        , vehicleId(0), vehicleSeat(0)
        , controllerLeftStickX(128), controllerLeftStickY(128)
        , keyFlags(0)
        , lastSyncTime(0), syncTimeContext(0)
    {}
};

//=============================================================================
// Interpolation State
//=============================================================================

struct InterpolationState
{
    CVector3D startPosition;
    CVector3D targetPosition;
    CVector3D errorOffset;        // Position error to smooth out

    float startRotation;
    float targetRotation;

    uint64_t startTime;
    uint64_t finishTime;
    float lastAlpha;              // Last interpolation progress (0-1)

    bool active;

    InterpolationState()
        : startRotation(0), targetRotation(0)
        , startTime(0), finishTime(0)
        , lastAlpha(0), active(false)
    {}
};

//=============================================================================
// CRemotePlayer Class
//=============================================================================

class CRemotePlayer
{
public:
    CRemotePlayer(uint32_t playerId, const std::string& nickname);
    ~CRemotePlayer();

    //=========================================================================
    // Identification
    //=========================================================================

    uint32_t GetId() const { return m_playerId; }
    const std::string& GetNickname() const { return m_nickname; }
    void SetNickname(const std::string& nickname) { m_nickname = nickname; }

    //=========================================================================
    // CPed Management
    //=========================================================================

    /**
     * Create the game ped for this player
     * @param gameBase Base address of libGTASA.so
     * @param skinId Model ID for the ped (0 = CJ)
     * @return true if ped created successfully
     */
    bool CreatePed(uintptr_t gameBase, uint16_t skinId = 0);

    /**
     * Destroy the game ped
     */
    void DestroyPed();

    /**
     * Check if ped exists (and is ready, not pending)
     */
    bool HasPed() const { return m_pedPtr != 0 && m_pendingSlot < 0; }

    /**
     * Check if ped creation is pending (queued for game thread)
     */
    bool IsPedPending() const { return m_pendingSlot >= 0; }

    /**
     * Get ped pointer (0 if pending or not created)
     */
    uintptr_t GetPedPtr() const { return (m_pendingSlot >= 0) ? 0 : m_pedPtr; }

    /**
     * Try to resolve pending ped - call this from game thread
     * Returns true if ped is now ready
     */
    bool TryResolvePendingPed()
    {
        // Not pending - already resolved or never started
        if (m_pendingSlot < 0)
            return m_pedPtr != 0;

        int slot = m_pendingSlot;
        auto& factory = CPedFactory::GetInstance();
        uintptr_t actualPed = factory.GetPedForSlot(slot);
        if (actualPed != 0)
        {
            m_pedPtr = actualPed;
            m_pendingSlot = -1;  // No longer pending
            REMOTE_LOGI("Player %u: Resolved pending ped in slot %d -> 0x%lx",
                        m_playerId, slot, (unsigned long)m_pedPtr);

            // DEBUG: Check if remote ped is different from local player
            // If they're the same, remote player will mirror local player movement!
            using FindPlayerPedFn = uintptr_t (*)(int);
            auto fnFindPlayerPed = reinterpret_cast<FindPlayerPedFn>(factory.GetGameBase() + 0x4EFAE0);
            uintptr_t localPed = fnFindPlayerPed(0);

            if (m_pedPtr == localPed)
            {
                REMOTE_LOGE("BUG: Remote ped 0x%lx == Local ped! They will move together!",
                            (unsigned long)m_pedPtr);
            }
            else
            {
                REMOTE_LOGI("OK: Remote ped 0x%lx != Local ped 0x%lx",
                            (unsigned long)m_pedPtr, (unsigned long)localPed);
            }

            // Also check matrix pointers
            uintptr_t remoteMatrix = GetPedMatrix();
            uintptr_t localMatrix = 0;
            if (localPed != 0)
            {
                uintptr_t* localMatrixAddr = reinterpret_cast<uintptr_t*>(localPed + 0x18);
                localMatrix = *localMatrixAddr;
            }

            if (remoteMatrix == localMatrix && remoteMatrix != 0)
            {
                REMOTE_LOGE("BUG: Remote matrix 0x%lx == Local matrix! Shared transform!",
                            (unsigned long)remoteMatrix);
            }
            else
            {
                REMOTE_LOGI("OK: Remote matrix 0x%lx != Local matrix 0x%lx",
                            (unsigned long)remoteMatrix, (unsigned long)localMatrix);
            }

            // CRITICAL FIX FOR DEAD POSE:
            // 1. Set invulnerable FIRST to prevent collision damage during spawn
            // 2. Then set health/state
            // 3. Then call SetInitialState
            SetInvulnerable(true);

            // Set ped health, armor, and state to prevent dead/ragdoll state
            // ARM64 offsets: fHealth=0x580, fArmor=0x588, m_nPedState=0x572
            SetPedHealth(100.0f);
            SetPedArmor(0.0f);
            SetPedState(1);  // PEDSTATE_IDLE

            // Check and log intelligence state
            EnsureIntelligence();

            // Call SetInitialState to flush tasks and set idle standing state
            // This may not work if intelligence is NULL, but invulnerability should help
            SetInitialState();

            m_pedCreationTime = GetTimeMs();  // Start state maintenance timer
            REMOTE_LOGI("Player %u: Set INVULNERABLE + health=100, armor=0, state=IDLE, SetInitialState called", m_playerId);

            return true;
        }
        return false;
    }

    //=========================================================================
    // Sync Data
    //=========================================================================

    /**
     * Update sync data from server packet
     * Sets up interpolation from current to target position
     */
    void UpdateSyncData(const RemoteSyncData& data);

    /**
     * Get current sync data
     */
    const RemoteSyncData& GetSyncData() const { return m_syncData; }

    /**
     * Check if sync data is stale (not updated recently)
     */
    bool IsSyncStale() const;

    //=========================================================================
    // Position/Rotation
    //=========================================================================

    /**
     * Get current interpolated position
     */
    CVector3D GetPosition() const;

    /**
     * Get current interpolated rotation
     */
    float GetRotation() const;

    /**
     * Set position directly (teleport, no interpolation)
     */
    void SetPosition(const CVector3D& position);

    /**
     * Set rotation directly
     */
    void SetRotation(float rotation);

    //=========================================================================
    // Per-Frame Update
    //=========================================================================

    /**
     * Called every frame to update interpolation and apply to ped
     * @param currentTime Current time in milliseconds
     */
    void Update(uint64_t currentTime);

    //=========================================================================
    // Spawning
    //=========================================================================

    /**
     * Check if player is spawned (has ped in world)
     */
    bool IsSpawned() const { return m_spawned; }

    /**
     * Spawn player at position
     */
    void Spawn(const CVector3D& position, float rotation, uint16_t skinId);

    /**
     * Despawn player (remove from world)
     */
    void Despawn();

private:
    //=========================================================================
    // Internal Methods
    //=========================================================================

    /**
     * Update interpolation alpha based on time
     */
    void UpdateInterpolation(uint64_t currentTime);

    /**
     * Apply current position/rotation to game ped
     */
    void ApplyToGamePed();

    /**
     * Get ped's matrix pointer for position updates
     */
    uintptr_t GetPedMatrix() const;

    /**
     * Write position to ped matrix
     */
    void WritePedPosition(const CVector3D& position);

    /**
     * Write rotation to ped
     */
    void WritePedRotation(float rotation);

    /**
     * Teleport ped using game's CPed::Teleport function
     * This properly updates all internal game state, unlike direct matrix manipulation
     * ARM64 offset: 0x59DD90
     */
    void TeleportPed(const CVector3D& position);

    /**
     * Set ped health (ARM64: offset 0x580)
     */
    void SetPedHealth(float health);

    /**
     * Set ped armor (ARM64: offset 0x588)
     */
    void SetPedArmor(float armor);

    /**
     * Set ped state (ARM64: offset 0x572)
     * States: 0=NONE, 1=IDLE, 54=DEAD, 55=DIE
     */
    void SetPedState(uint16_t state);

    /**
     * Call CPlayerPed::SetInitialState to flush tasks and set idle state
     * This is the key function that makes the ped stand up instead of being dead
     * ARM64 offset: 0x5C0D50
     */
    void SetInitialState();

    /**
     * Call CPed::SetIdle to set ped to idle animation state
     * ARM64 offset: 0x59DA8C
     */
    void SetIdle();

    /**
     * Call CPed::SetPedState via game function (not direct memory write)
     * ARM64 offset: 0x597FF0
     */
    void SetPedStateFunc(uint32_t state);

    /**
     * Call CPed::SetMoveState via game function (moves animations to walk/run)
     */
    void SetMoveStateFunc(uint8_t state);

    /**
     * DEBUG: Dump ped state - read back all important values to understand what's happening
     */
    void DebugDumpPedState(const char* context);

    /**
     * Set ped invulnerability flags (bInvulnerable, bCollisionProof, etc.)
     * This prevents the ped from taking damage/ragdolling when spawned
     * ARM64: Physical flags are in CPhysical at offset ~0x50
     */
    void SetInvulnerable(bool invulnerable);

    /**
     * Create CPedIntelligence for the ped if it doesn't exist
     * This is needed for task system to work properly
     * ARM64: CPedIntelligence::CPedIntelligence(CPed*) at 0x5BC510
     */
    void EnsureIntelligence();

    /**
     * Interpolate angle (handling wrap-around)
     */
    static float InterpolateAngle(float start, float end, float alpha);

    /**
     * Get current time in ms
     */
    static uint64_t GetTimeMs();

private:
    // Identification
    uint32_t m_playerId;
    std::string m_nickname;

    // Game ped
    uintptr_t m_pedPtr;
    uintptr_t m_gameBase;
    int m_pendingSlot;  // Slot number if ped creation is pending (-1 if not pending)
    bool m_spawned;
    uint64_t m_pedCreationTime;  // Time when ped was created (for state maintenance)
    uint64_t m_lastSetInitialStateCall;  // Track SetInitialState calls to avoid calling every frame
    CVector3D m_lastMovePosition;
    uint64_t m_lastMoveTime;
    uint8_t m_lastMoveState;

    // Sync data
    RemoteSyncData m_syncData;

    // Interpolation
    InterpolationState m_interp;
    CVector3D m_currentPosition;
    float m_currentRotation;
};

//=============================================================================
// Implementation
//=============================================================================

inline uint64_t CRemotePlayer::GetTimeMs()
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

inline CRemotePlayer::CRemotePlayer(uint32_t playerId, const std::string& nickname)
    : m_playerId(playerId)
    , m_nickname(nickname)
    , m_pedPtr(0)
    , m_gameBase(0)
    , m_pendingSlot(-1)  // -1 = not pending
    , m_spawned(false)
    , m_pedCreationTime(0)
    , m_lastSetInitialStateCall(0)
    , m_lastMovePosition()
    , m_lastMoveTime(0)
    , m_lastMoveState(0xFF)
    , m_currentRotation(0)
{
    REMOTE_LOGI("Created remote player %u: %s", playerId, nickname.c_str());
}

inline CRemotePlayer::~CRemotePlayer()
{
    DestroyPed();
    REMOTE_LOGI("Destroyed remote player %u: %s", m_playerId, m_nickname.c_str());
}

inline bool CRemotePlayer::CreatePed(uintptr_t gameBase, uint16_t skinId)
{
    // Check if we already have a valid ped (not pending)
    if (m_pedPtr != 0 && m_pendingSlot < 0)
    {
        REMOTE_LOGD("Ped already exists for player %u", m_playerId);
        return true;
    }

    // Check if we have a pending slot - try to get the actual ped pointer
    if (m_pendingSlot >= 0)
    {
        auto& factory = CPedFactory::GetInstance();
        uintptr_t actualPed = factory.GetPedForSlot(m_pendingSlot);
        if (actualPed != 0)
        {
            m_pedPtr = actualPed;
            REMOTE_LOGI("Player %u: Ped ready in slot %d, ptr=0x%lx",
                        m_playerId, m_pendingSlot, (unsigned long)m_pedPtr);
            m_pendingSlot = -1;  // No longer pending
            return true;
        }
        // Still pending - ped not yet created on game thread
        REMOTE_LOGD("Player %u: Still waiting for ped in slot %d", m_playerId, m_pendingSlot);
        return false;
    }

    m_gameBase = gameBase;

    // Use the ped factory to create the ped
    auto& factory = CPedFactory::GetInstance();
    if (!factory.IsInitialized())
    {
        factory.Initialize(gameBase);
    }

    // Create ped at current position - may return pending marker (0x80000000 | slot)
    uintptr_t result = factory.CreatePed(
        skinId,
        m_currentPosition.x,
        m_currentPosition.y,
        m_currentPosition.z,
        m_currentRotation
    );

    // Check if result is a pending marker (high bit set AND low bits are a valid slot number)
    // Real ARM64 pointers also have high bits set but low bits are large addresses, not slot numbers
    constexpr int MAX_VALID_SLOT = 1024;  // Reasonable max for player slots
    uint32_t lowBits = static_cast<uint32_t>(result & 0x7FFFFFFF);
    bool isPendingMarker = (result & 0x80000000) && (lowBits >= 2) && (lowBits < MAX_VALID_SLOT);

    if (isPendingMarker)
    {
        // Pending marker - ped will be created on game thread
        int slot = static_cast<int>(lowBits);
        m_pendingSlot = slot;  // Store slot number separately
        m_pedPtr = 0;  // Clear ped pointer until resolved
        REMOTE_LOGI("Player %u: Ped queued for game thread (slot %d)", m_playerId, slot);
        return false;  // Not ready yet
    }
    else if (result != 0)
    {
        // Direct ped pointer (includes ARM64 pointers with high bits set)
        m_pedPtr = result;
        m_pendingSlot = -1;  // Not pending
        REMOTE_LOGI("Created ped for player %u: ptr=0x%lx",
                    m_playerId, (unsigned long)m_pedPtr);

        // CRITICAL FIX FOR DEAD POSE:
        // Set invulnerable FIRST to prevent collision damage during spawn
        SetInvulnerable(true);

        // Set health and armor to prevent dead/ragdoll state
        SetPedHealth(100.0f);
        SetPedArmor(0.0f);
        SetPedState(1);  // PEDSTATE_IDLE

        // Check and log intelligence state
        EnsureIntelligence();

        // Call SetInitialState to flush tasks and set idle standing state
        SetInitialState();

        m_pedCreationTime = GetTimeMs();  // Start state maintenance timer
        REMOTE_LOGI("Player %u: Set INVULNERABLE + health=100, armor=0, state=IDLE, SetInitialState called", m_playerId);

        return true;
    }
    else
    {
        REMOTE_LOGD("CreatePed for player %u (skin %d) - failed",
                    m_playerId, skinId);
        return false;
    }
}

inline void CRemotePlayer::DestroyPed()
{
    if (m_pedPtr == 0)
        return;

    // TODO: Call game function to destroy ped
    // CWorld::Remove(pPed) and delete

    m_pedPtr = 0;
    m_spawned = false;
    REMOTE_LOGI("Destroyed ped for player %u", m_playerId);
}

inline void CRemotePlayer::UpdateSyncData(const RemoteSyncData& data)
{
    uint64_t now = GetTimeMs();

    RemoteSyncData adjusted = data;

    // Check if this is newer data
    if (adjusted.syncTimeContext < m_syncData.syncTimeContext &&
        m_syncData.syncTimeContext - adjusted.syncTimeContext < 128)
    {
        // Old packet, ignore
        return;
    }

    // Ignore invalid positions (0,0,0) which can cause oscillation
    if (adjusted.position.x == 0.0f && adjusted.position.y == 0.0f && adjusted.position.z == 0.0f)
    {
        REMOTE_LOGD("Player %u: Ignoring invalid (0,0,0) position in sync data", m_playerId);
        return;
    }

    // Calculate distance to new position
    float distance = m_currentPosition.DistanceTo(adjusted.position);

    // If distance is huge, teleport instead of interpolate
    if (distance > INTERPOLATION_WARP_THRESHOLD)
    {
        REMOTE_LOGI("Player %u warped %.1f units - using CPed::Teleport", m_playerId, distance);
        m_currentPosition = adjusted.position;
        m_currentRotation = adjusted.rotation;
        m_interp.active = false;

        // CRITICAL: Use CPed::Teleport instead of direct matrix manipulation
        // This properly updates all internal game state (collision, world sectors, etc.)
        if (HasPed())
        {
            TeleportPed(adjusted.position);
            WritePedRotation(adjusted.rotation);
        }
    }
    else
    {
        // Set up interpolation
        m_interp.startPosition = m_currentPosition;
        m_interp.targetPosition = adjusted.position;
        m_interp.startRotation = m_currentRotation;
        m_interp.targetRotation = adjusted.rotation;
        m_interp.startTime = now;
        m_interp.finishTime = now + INTERPOLATION_TIME_MS;
        m_interp.lastAlpha = 0.0f;
        m_interp.active = true;
    }

    // Update sync data
    m_syncData = adjusted;
    m_syncData.lastSyncTime = now;
}

inline bool CRemotePlayer::IsSyncStale() const
{
    uint64_t now = GetTimeMs();
    return (now - m_syncData.lastSyncTime) > SYNC_TIMEOUT_MS;
}

inline CVector3D CRemotePlayer::GetPosition() const
{
    return m_currentPosition;
}

inline float CRemotePlayer::GetRotation() const
{
    return m_currentRotation;
}

inline void CRemotePlayer::SetPosition(const CVector3D& position)
{
    m_currentPosition = position;
    m_interp.active = false;

    // Only write to ped if it exists and is not pending creation
    // Use TeleportPed for proper game state updates
    if (HasPed())
    {
        TeleportPed(position);
    }
}

inline void CRemotePlayer::SetRotation(float rotation)
{
    m_currentRotation = rotation;

    // IMPORTANT: Use HasPed() to check for both existence AND pending marker
    if (HasPed())
    {
        WritePedRotation(rotation);
    }
}

inline void CRemotePlayer::Update(uint64_t currentTime)
{
    // Try to resolve pending ped (if ped was queued for game thread)
    if (IsPedPending())
    {
        TryResolvePendingPed();
    }

    // Update interpolation
    UpdateInterpolation(currentTime);

    // Apply to game ped (only if ped is ready)
    if (HasPed())
    {
        ApplyToGamePed();

        // State maintenance: Keep setting health/state for first 5 seconds after ped creation
        // The game may overwrite our initial settings during ped initialization
        constexpr uint64_t STATE_MAINTENANCE_DURATION_MS = 5000;
        constexpr uint64_t SETINITIALSTATE_DURATION_MS = 2000;  // Call state functions for first 2 seconds
        constexpr uint64_t SETINITIALSTATE_INTERVAL_MS = 500;   // Every 500ms (less spam for debug)

        if (m_pedCreationTime > 0 && (currentTime - m_pedCreationTime) < STATE_MAINTENANCE_DURATION_MS)
        {
            // Call game functions repeatedly for first 2 seconds to ensure ped is in idle state
            uint64_t timeSinceCreation = currentTime - m_pedCreationTime;
            if (timeSinceCreation < SETINITIALSTATE_DURATION_MS)
            {
                // Call every 500ms (not every frame to avoid overhead)
                if ((timeSinceCreation / SETINITIALSTATE_INTERVAL_MS) != m_lastSetInitialStateCall)
                {
                    m_lastSetInitialStateCall = timeSinceCreation / SETINITIALSTATE_INTERVAL_MS;

                    // DEBUG: Dump state BEFORE we try to fix it
                    DebugDumpPedState("BEFORE_FIX");

                    // Try to fix the ped state:
                    // CRITICAL: Set invulnerable to prevent damage/ragdoll
                    SetInvulnerable(true);
                    SetPedHealth(100.0f);
                    SetPedArmor(0.0f);
                    SetPedState(1);  // PEDSTATE_IDLE (direct memory write)
                    SetInitialState();
                    SetPedStateFunc(1);  // PEDSTATE_IDLE via game function
                    SetIdle();

                    // DEBUG: Dump state AFTER we tried to fix it
                    DebugDumpPedState("AFTER_FIX");
                }
            }
        }
    }
}

inline void CRemotePlayer::UpdateInterpolation(uint64_t currentTime)
{
    if (!m_interp.active)
        return;

    // Calculate interpolation progress (alpha)
    float alpha = 0.0f;
    if (m_interp.finishTime > m_interp.startTime)
    {
        alpha = static_cast<float>(currentTime - m_interp.startTime) /
                static_cast<float>(m_interp.finishTime - m_interp.startTime);
    }

    // Clamp alpha
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    // Interpolate position (linear)
    CVector3D delta = m_interp.targetPosition - m_interp.startPosition;
    m_currentPosition = m_interp.startPosition + delta * alpha;

    // Interpolate rotation (with angle wrapping)
    m_currentRotation = InterpolateAngle(
        m_interp.startRotation,
        m_interp.targetRotation,
        alpha
    );

    m_interp.lastAlpha = alpha;

    // Interpolation complete
    if (alpha >= 1.0f)
    {
        m_interp.active = false;
    }
}

inline void CRemotePlayer::ApplyToGamePed()
{
    if (m_pedPtr == 0)
        return;

    WritePedPosition(m_currentPosition);
    WritePedRotation(m_currentRotation);

    const uint64_t now = GetTimeMs();
    if (m_lastMoveTime != 0)
    {
        const float dt = static_cast<float>(now - m_lastMoveTime) / 1000.0f;
        if (dt > 0.0f)
        {
            const float distance = m_currentPosition.DistanceTo(m_lastMovePosition);
            uint8_t moveState = 1;  // PEDMOVE_STILL

            if (distance < INTERPOLATION_WARP_THRESHOLD)
            {
                const float speed = distance / dt;
                if (speed > 0.1f)
                {
                    moveState = (speed < 2.5f) ? 4 : 6;  // WALK or RUN
                }
            }

            if (moveState != m_lastMoveState)
            {
                SetMoveStateFunc(moveState);
                m_lastMoveState = moveState;
            }
        }
    }

    m_lastMovePosition = m_currentPosition;
    m_lastMoveTime = now;
}

inline void CRemotePlayer::Spawn(const CVector3D& position, float rotation, uint16_t skinId)
{
    REMOTE_LOGI("Spawning player %u at (%.1f, %.1f, %.1f) skin=%d",
                m_playerId, position.x, position.y, position.z, skinId);

    // Set initial position
    m_currentPosition = position;
    m_currentRotation = rotation;
    m_syncData.position = position;
    m_syncData.rotation = rotation;

    // Get game base from PedFactory if not set
    if (m_gameBase == 0)
    {
        auto& factory = CPedFactory::GetInstance();
        if (factory.IsInitialized())
        {
            m_gameBase = factory.GetGameBase();
            REMOTE_LOGD("Got game base from PedFactory: 0x%lx", (unsigned long)m_gameBase);
        }
    }

    // Create ped if we have game base
    if (m_gameBase != 0)
    {
        if (CreatePed(m_gameBase, skinId))
        {
            REMOTE_LOGI("Player %u ped created successfully at 0x%lx",
                        m_playerId, (unsigned long)m_pedPtr);
            m_spawned = true;  // Only mark as spawned if ped was actually created
        }
        else
        {
            REMOTE_LOGW("Player %u ped creation failed - will retry on next sync", m_playerId);
            m_spawned = false;  // Keep as not spawned so deferred spawn can retry
        }
    }
    else
    {
        REMOTE_LOGW("Player %u spawn: No game base available - will retry", m_playerId);
        m_spawned = false;  // Keep as not spawned so deferred spawn can retry
    }
}

inline void CRemotePlayer::Despawn()
{
    DestroyPed();
    m_spawned = false;
}

inline uintptr_t CRemotePlayer::GetPedMatrix() const
{
    // Check for zero or pending state
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return 0;

    // Matrix pointer offset (ARM64)
    #if defined(__aarch64__)
        constexpr uint32_t MATRIX_OFFSET = 0x18;
    #else
        constexpr uint32_t MATRIX_OFFSET = 0x14;
    #endif

    uintptr_t* matrixPtrAddr = reinterpret_cast<uintptr_t*>(m_pedPtr + MATRIX_OFFSET);
    return *matrixPtrAddr;
}

inline void CRemotePlayer::WritePedPosition(const CVector3D& position)
{
    uintptr_t matrix = GetPedMatrix();
    if (matrix == 0)
        return;

    // Position is at offset 0x30 in RwMatrix
    float* posX = reinterpret_cast<float*>(matrix + 0x30);
    float* posY = reinterpret_cast<float*>(matrix + 0x34);
    float* posZ = reinterpret_cast<float*>(matrix + 0x38);

    *posX = position.x;
    *posY = position.y;
    *posZ = position.z;
}

inline void CRemotePlayer::WritePedRotation(float rotation)
{
    uintptr_t matrix = GetPedMatrix();
    if (matrix == 0)
        return;

    // Update forward vector (at) based on rotation
    // Forward vector is at offset 0x20 in RwMatrix
    float* atX = reinterpret_cast<float*>(matrix + 0x20);
    float* atY = reinterpret_cast<float*>(matrix + 0x24);

    *atX = -sinf(rotation);
    *atY = cosf(rotation);

    // Keep CPlaceable heading in sync with the matrix.
#if defined(__aarch64__)
    constexpr uint32_t HEADING_OFFSET = 0x14;
#else
    constexpr uint32_t HEADING_OFFSET = 0x10;
#endif
    float* heading = reinterpret_cast<float*>(m_pedPtr + HEADING_OFFSET);
    *heading = rotation;
}

inline void CRemotePlayer::TeleportPed(const CVector3D& position)
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

    if (m_gameBase == 0)
    {
        auto& factory = CPedFactory::GetInstance();
        if (factory.IsInitialized())
        {
            m_gameBase = factory.GetGameBase();
        }
        else
        {
            REMOTE_LOGE("TeleportPed: No game base available");
            return;
        }
    }

#if defined(__aarch64__)
    // CPed::Teleport(CVector, bool) - ARM64 offset 0x59DD90
    // This function properly teleports the ped, updating all internal game state:
    // - Updates matrix position
    // - Updates collision position
    // - Removes from world sectors and re-adds at new position
    // - Updates any attached entities
    //
    // Signature: void CPed::Teleport(CVector position, bool resetRotation)
    // Parameters:
    //   x0 = this (ped pointer)
    //   x1-x3 = CVector position (passed as 3 floats on ARM64)
    //   w4 = bool resetRotation (we pass false to keep current rotation)

    typedef void (*TeleportFn)(void* ped, float x, float y, float z, bool resetRotation);
    TeleportFn fnTeleport = reinterpret_cast<TeleportFn>(m_gameBase + 0x59DD90);

    REMOTE_LOGI("Player %u: Calling CPed::Teleport(%.1f, %.1f, %.1f)",
                m_playerId, position.x, position.y, position.z);

    fnTeleport(reinterpret_cast<void*>(m_pedPtr), position.x, position.y, position.z, false);

    REMOTE_LOGI("Player %u: Teleport completed", m_playerId);
#endif
}

inline void CRemotePlayer::SetPedHealth(float health)
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

#if defined(__aarch64__)
    // ARM64: fHealth is at offset 0x580 in CPed
    constexpr size_t HEALTH_OFFSET = 0x580;
    float* healthPtr = reinterpret_cast<float*>(m_pedPtr + HEALTH_OFFSET);
    *healthPtr = health;
#endif
}

inline void CRemotePlayer::SetPedArmor(float armor)
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

#if defined(__aarch64__)
    // ARM64: fArmor is at offset 0x588 in CPed
    constexpr size_t ARMOR_OFFSET = 0x588;
    float* armorPtr = reinterpret_cast<float*>(m_pedPtr + ARMOR_OFFSET);
    *armorPtr = armor;
#endif
}

inline void CRemotePlayer::SetPedState(uint16_t state)
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

#if defined(__aarch64__)
    // ARM64: m_nPedState is at offset 0x572 in CPed
    // States: 0=PEDSTATE_NONE, 1=PEDSTATE_IDLE, 54=PEDSTATE_DEAD, 55=PEDSTATE_DIE
    constexpr size_t PED_STATE_OFFSET = 0x572;
    uint16_t* statePtr = reinterpret_cast<uint16_t*>(m_pedPtr + PED_STATE_OFFSET);
    *statePtr = state;
#endif
}

inline void CRemotePlayer::SetInitialState()
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

    if (m_gameBase == 0)
    {
        auto& factory = CPedFactory::GetInstance();
        if (factory.IsInitialized())
        {
            m_gameBase = factory.GetGameBase();
        }
        else
        {
            REMOTE_LOGE("SetInitialState: No game base available");
            return;
        }
    }

#if defined(__aarch64__)
    // CPlayerPed::SetInitialState - ARM64 offset 0x5C0D50
    // This function:
    // 1. Flushes all current tasks via m_pIntelligence->m_TaskMgr.FlushImmediately()
    // 2. Sets ped to TASK_PRIMARY_DEFAULT (idle/standing state)
    // 3. Applies idle animation automatically
    //
    // Signature: void CPlayerPed::SetInitialState(void)
    // It's a member function, so 'this' (ped pointer) is passed in x0
    typedef void (*SetInitialStateFn)(void* playerPed);
    SetInitialStateFn fnSetInitialState = reinterpret_cast<SetInitialStateFn>(m_gameBase + 0x5C0D50);

    REMOTE_LOGI("Player %u: Calling SetInitialState(0x%lx) at 0x%lx",
                m_playerId, (unsigned long)m_pedPtr, (unsigned long)(m_gameBase + 0x5C0D50));

    fnSetInitialState(reinterpret_cast<void*>(m_pedPtr));

    REMOTE_LOGI("Player %u: SetInitialState completed - ped should now be standing", m_playerId);
#endif
}

inline void CRemotePlayer::SetIdle()
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

    if (m_gameBase == 0)
    {
        auto& factory = CPedFactory::GetInstance();
        if (factory.IsInitialized())
        {
            m_gameBase = factory.GetGameBase();
        }
        else
        {
            REMOTE_LOGE("SetIdle: No game base available");
            return;
        }
    }

#if defined(__aarch64__)
    // CPed::SetIdle - ARM64 offset 0x59DA8C
    // Sets the ped to idle animation state
    // Signature: void CPed::SetIdle(void)
    typedef void (*SetIdleFn)(void* ped);
    SetIdleFn fnSetIdle = reinterpret_cast<SetIdleFn>(m_gameBase + 0x59DA8C);

    REMOTE_LOGI("Player %u: Calling SetIdle(0x%lx)", m_playerId, (unsigned long)m_pedPtr);
    fnSetIdle(reinterpret_cast<void*>(m_pedPtr));
#endif
}

inline void CRemotePlayer::SetPedStateFunc(uint32_t state)
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

    if (m_gameBase == 0)
    {
        auto& factory = CPedFactory::GetInstance();
        if (factory.IsInitialized())
        {
            m_gameBase = factory.GetGameBase();
        }
        else
        {
            REMOTE_LOGE("SetPedStateFunc: No game base available");
            return;
        }
    }

#if defined(__aarch64__)
    // CPed::SetPedState - ARM64 offset 0x597FF0
    // Sets the ped state using the game's function (handles side effects)
    // Signature: void CPed::SetPedState(ePedState state)
    typedef void (*SetPedStateFn)(void* ped, uint32_t state);
    SetPedStateFn fnSetPedState = reinterpret_cast<SetPedStateFn>(m_gameBase + 0x597FF0);

    fnSetPedState(reinterpret_cast<void*>(m_pedPtr), state);
#endif
}

inline void CRemotePlayer::SetMoveStateFunc(uint8_t state)
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

    if (m_gameBase == 0)
    {
        auto& factory = CPedFactory::GetInstance();
        if (factory.IsInitialized())
        {
            m_gameBase = factory.GetGameBase();
        }
        else
        {
            REMOTE_LOGE("SetMoveStateFunc: No game base available");
            return;
        }
    }

#if defined(__aarch64__)
    typedef void (*SetMoveStateFn)(void* ped, uint8_t state);
    SetMoveStateFn fnSetMoveState = reinterpret_cast<SetMoveStateFn>(
        m_gameBase + MTA::Android::ARM::ARM64::CPed_SetMoveState);
    fnSetMoveState(reinterpret_cast<void*>(m_pedPtr), state);
#elif defined(__arm__)
    typedef void (*SetMoveStateFn)(void* ped, uint8_t state);
    SetMoveStateFn fnSetMoveState = reinterpret_cast<SetMoveStateFn>(
        m_gameBase + MTA::Android::ARM::ARM32::CPed_SetMoveState);
    fnSetMoveState(reinterpret_cast<void*>(m_pedPtr), state);
#endif
}

inline void CRemotePlayer::DebugDumpPedState(const char* context)
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

#if defined(__aarch64__)
    // Dump raw memory at key offsets to understand the actual structure
    // ARM64 structure sizes from SA-MP:
    // - CPhysical: 0x198 bytes
    // - After CPhysical: audio entities, then CPed members

    // Raw memory dump at potential intelligence locations
    uintptr_t val_538 = *reinterpret_cast<uintptr_t*>(m_pedPtr + 0x538);
    uintptr_t val_540 = *reinterpret_cast<uintptr_t*>(m_pedPtr + 0x540);
    uintptr_t val_598 = *reinterpret_cast<uintptr_t*>(m_pedPtr + 0x598);
    uintptr_t val_5A0 = *reinterpret_cast<uintptr_t*>(m_pedPtr + 0x5A0);
    uintptr_t val_5A8 = *reinterpret_cast<uintptr_t*>(m_pedPtr + 0x5A8);

    // Read vtable to verify this is a valid ped
    uintptr_t vtable = *reinterpret_cast<uintptr_t*>(m_pedPtr);

    // Read m_pMatrix at 0x18
    uintptr_t matrix = *reinterpret_cast<uintptr_t*>(m_pedPtr + 0x18);

    // Read RpClump at 0x20
    uintptr_t rpClump = *reinterpret_cast<uintptr_t*>(m_pedPtr + 0x20);

    // Try different possible offsets for ped state (search around expected location)
    // From SA-MP: m_nPedState comes after m_pIntelligence, m_pPlayerData, m_nCreatedBy
    uint32_t word_5AC = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x5AC);
    uint32_t word_5B0 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x5B0);
    uint32_t word_5B4 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x5B4);

    // Health should be a float - search for 100.0 pattern
    float health_580 = *reinterpret_cast<float*>(m_pedPtr + 0x580);
    float health_5C0 = *reinterpret_cast<float*>(m_pedPtr + 0x5C0);
    float health_5D0 = *reinterpret_cast<float*>(m_pedPtr + 0x5D0);

    REMOTE_LOGW("=== RAW PED DUMP [%s] Player %u ===", context, m_playerId);
    REMOTE_LOGW("  Ped: 0x%lx, VTable: 0x%lx", (unsigned long)m_pedPtr, (unsigned long)vtable);
    REMOTE_LOGW("  Matrix@0x18: 0x%lx, RpClump@0x20: 0x%lx", (unsigned long)matrix, (unsigned long)rpClump);
    REMOTE_LOGW("  @0x538: 0x%lx (SA-MP 2.10 m_pIntelligence?)", (unsigned long)val_538);
    REMOTE_LOGW("  @0x540: 0x%lx (SA-MP 2.10 m_pPlayerData?)", (unsigned long)val_540);
    REMOTE_LOGW("  @0x598: 0x%lx (m_pIntelligence alt)", (unsigned long)val_598);
    REMOTE_LOGW("  @0x5A0: 0x%lx (m_pPlayerData alt)", (unsigned long)val_5A0);
    REMOTE_LOGW("  @0x5A8: 0x%lx (m_pIntelligence alt2)", (unsigned long)val_5A8);
    REMOTE_LOGW("  @0x5AC: 0x%08X, @0x5B0: 0x%08X, @0x5B4: 0x%08X", word_5AC, word_5B0, word_5B4);
    REMOTE_LOGW("  health@0x580: %.2f, @0x5C0: %.2f, @0x5D0: %.2f", health_580, health_5C0, health_5D0);

    // Dump ped flags area
    uint32_t flags_46C = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x46C);
    uint32_t flags_470 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x470);
    uint32_t flags_570 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x570);
    uint32_t flags_574 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x574);
    REMOTE_LOGW("  flags@0x46C: 0x%08X, @0x470: 0x%08X", flags_46C, flags_470);
    REMOTE_LOGW("  state@0x570: 0x%08X, @0x574: 0x%08X", flags_570, flags_574);

    // Dump CPhysical flags at various possible ARM64 offsets
    uint32_t phys_40 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x40);
    uint32_t phys_44 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x44);
    uint32_t phys_48 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x48);
    uint32_t phys_50 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x50);
    uint32_t phys_54 = *reinterpret_cast<uint32_t*>(m_pedPtr + 0x54);
    REMOTE_LOGW("  phys@0x40: 0x%08X, @0x44: 0x%08X, @0x48: 0x%08X", phys_40, phys_44, phys_48);
    REMOTE_LOGW("  phys@0x50: 0x%08X, @0x54: 0x%08X", phys_50, phys_54);
    REMOTE_LOGW("========================================");
#endif
}

inline void CRemotePlayer::SetInvulnerable(bool invulnerable)
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

#if defined(__aarch64__)
    // SA-MP Android's approach: set specific ped flags to prevent death/damage appearance
    // From SA-MP's CPedGTA.h structure analysis:
    //
    // m_nPedFlags is 16 bytes (0x10) at offset 0x5E0 in CPedGTA (ARM64)
    // Calculated from: m_pIntelligence(0x598) + m_pPlayerData(8) + m_nCreatedBy(4) +
    //                  m_nPedState(4) + m_nMoveState(4) + m_storedCollPoly(40) +
    //                  m_distTravelledSinceLastHeightCheck(4) = 0x5E0
    //
    // Flag bytes within m_nPedFlags:
    //   Byte 6: contains bNeverEverTargetThisPed (bit 4 = 0x10)
    //   Byte 9: contains bDoesntDropWeaponsWhenDead (bit 1 = 0x02)
    //
    // SA-MP sets these flags in playerped.cpp line 89-92:
    //   m_pPed->bDoesntDropWeaponsWhenDead = true;
    //   m_pPed->bNeverEverTargetThisPed = true;

    constexpr uint32_t PED_FLAGS_OFFSET = 0x5E0;

    // bNeverEverTargetThisPed: byte 6 of m_nPedFlags, bit 4 (0x10)
    uint8_t* flagsByte6 = reinterpret_cast<uint8_t*>(m_pedPtr + PED_FLAGS_OFFSET + 6);
    // bDoesntDropWeaponsWhenDead: byte 9 of m_nPedFlags, bit 1 (0x02)
    uint8_t* flagsByte9 = reinterpret_cast<uint8_t*>(m_pedPtr + PED_FLAGS_OFFSET + 9);

    if (invulnerable)
    {
        *flagsByte6 |= 0x10;  // bNeverEverTargetThisPed = true
        *flagsByte9 |= 0x02;  // bDoesntDropWeaponsWhenDead = true
        REMOTE_LOGI("Player %u: Set SA-MP ped flags (bNeverEverTargetThisPed, bDoesntDropWeaponsWhenDead)", m_playerId);

        // Also dump the flag bytes for debugging
        uint8_t byte6Val = *flagsByte6;
        uint8_t byte9Val = *flagsByte9;
        REMOTE_LOGD("  flagsByte6@0x5E6=0x%02X, flagsByte9@0x5E9=0x%02X", byte6Val, byte9Val);
    }
    else
    {
        *flagsByte6 &= ~0x10;  // bNeverEverTargetThisPed = false
        *flagsByte9 &= ~0x02;  // bDoesntDropWeaponsWhenDead = false
        REMOTE_LOGI("Player %u: Cleared SA-MP ped flags", m_playerId);
    }
#endif
}

inline void CRemotePlayer::EnsureIntelligence()
{
    if (m_pedPtr == 0 || m_pendingSlot >= 0)
        return;

    if (m_gameBase == 0)
    {
        auto& factory = CPedFactory::GetInstance();
        if (factory.IsInitialized())
        {
            m_gameBase = factory.GetGameBase();
        }
        else
        {
            REMOTE_LOGE("EnsureIntelligence: No game base available");
            return;
        }
    }

#if defined(__aarch64__)
    // Check if m_pIntelligence exists at any known offset (SA-MP 2.10 vs newer layouts).
    struct Candidate
    {
        uintptr_t offset;
        const char* label;
    };
    const Candidate candidates[] = {
        {0x538, "0x538"},
        {0x598, "0x598"},
        {0x5A8, "0x5A8"},
    };

    for (const auto& candidate : candidates)
    {
        uintptr_t value = *reinterpret_cast<uintptr_t*>(m_pedPtr + candidate.offset);
        if (value != 0)
        {
            REMOTE_LOGD("Player %u: Intelligence already exists at %s -> 0x%lx",
                        m_playerId,
                        candidate.label,
                        (unsigned long)value);
            return;
        }
    }

    // CPedIntelligence::CPedIntelligence(CPed* ped) - ARM64 offset 0x5BC510
    // First we need to allocate memory for CPedIntelligence, then call constructor
    //
    // From SA-MP ARM64:
    //   SIZEOF_CPedIntelligence = ~0x1C0 bytes (estimate)
    //   Constructor: _ZN16CPedIntelligenceC1EP4CPed at 0x5BC510
    //
    // However, this is complex and may crash if not done correctly.
    // For now, let's just log that intelligence is NULL and rely on other methods.

    REMOTE_LOGW("Player %u: m_pIntelligence is NULL at all known offsets (0x538/0x598/0x5A8)", m_playerId);
    REMOTE_LOGW("  This means SetInitialState may not function correctly");
    REMOTE_LOGW("  Using invulnerability flags instead to prevent dead pose");
#endif
}

inline float CRemotePlayer::InterpolateAngle(float start, float end, float alpha)
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI;

    // Normalize angles to -PI to PI
    while (start < -PI) start += TWO_PI;
    while (start > PI) start -= TWO_PI;
    while (end < -PI) end += TWO_PI;
    while (end > PI) end -= TWO_PI;

    // Find shortest path
    float diff = end - start;
    if (diff > PI) diff -= TWO_PI;
    if (diff < -PI) diff += TWO_PI;

    // Interpolate
    float result = start + diff * alpha;

    // Normalize result
    while (result < -PI) result += TWO_PI;
    while (result > PI) result -= TWO_PI;

    return result;
}

} // namespace MTA::Android::Multiplayer

#endif // CREMOTE_PLAYER_H
