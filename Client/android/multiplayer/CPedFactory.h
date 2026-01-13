/*
 * MTA:SA Android - Ped Factory
 *
 * Handles creation and destruction of game peds for remote players.
 * Uses GTA:SA game functions to spawn peds in the game world.
 *
 * Phase 7f: Remote Player Rendering
 *
 * SOLUTION: The CWorld::Add crash is caused by the game's CWorld::Players array
 * being limited to 2 players. SetupPlayerPed(2+) crashes because it accesses
 * out-of-bounds memory in CWorld::Players.
 *
 * SA-MP Android solves this by:
 * 1. Creating a custom CWorld::Players array with 1004 entries
 * 2. Patching the game's pointer to use their array
 * 3. Then SetupPlayerPed works for any slot
 *
 * See CWorldPlayers.h for the patch implementation.
 *
 * Based on SA-MP Android's proven approach:
 * Uses player slots (2+) and chains: SetupPlayerPed → DeactivatePlayerPed →
 * FindPlayerPed → ClearSpace → ReactivatePlayerPed → CWorld::Add
 */

#ifndef CPED_FACTORY_H
#define CPED_FACTORY_H

#include <cstdint>
#include <cmath>
#include <array>
#include <mutex>
#include <vector>
#include <functional>

#ifdef __ANDROID__
#include <android/log.h>
#include <sys/mman.h>
#endif

#include "../signatures/ARMAddressMap.h"
#include "CWorldPlayers.h"  // CWorld::Players array patch - CRITICAL for remote player support

namespace MTA::Android::Multiplayer
{

#define PEDFAC_LOG_TAG "MTA-PedFactory"
#define PEDFAC_LOGI(...) __android_log_print(ANDROID_LOG_INFO, PEDFAC_LOG_TAG, __VA_ARGS__)
#define PEDFAC_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, PEDFAC_LOG_TAG, __VA_ARGS__)
#define PEDFAC_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, PEDFAC_LOG_TAG, __VA_ARGS__)
#define PEDFAC_LOGW(...) __android_log_print(ANDROID_LOG_WARN, PEDFAC_LOG_TAG, __VA_ARGS__)

//=============================================================================
// GTA:SA Ped Structures (minimal definitions)
//=============================================================================

// RwMatrix - Used for entity transformations
struct RwMatrix
{
    float right[4];    // 0x00 - Right vector + flags
    float up[4];       // 0x10 - Up vector + pad
    float at[4];       // 0x20 - Forward vector + pad
    float pos[4];      // 0x30 - Position + pad
};

// CPed structure offsets
namespace PedOffset
{
    #if defined(__aarch64__)
        constexpr uint32_t MATRIX_PTR = 0x18;
        constexpr uint32_t MODEL_INDEX = 0x24;
        constexpr uint32_t HEALTH = 0x580;
        constexpr uint32_t ARMOR = 0x588;
        constexpr uint32_t VELOCITY = 0x50;
        constexpr uint32_t PED_TYPE = 0x5E0;
        constexpr uint32_t PED_STATE = 0x572;
    #else
        constexpr uint32_t MATRIX_PTR = 0x14;
        constexpr uint32_t MODEL_INDEX = 0x22;
        constexpr uint32_t HEALTH = 0x540;
        constexpr uint32_t ARMOR = 0x548;
        constexpr uint32_t VELOCITY = 0x44;
        constexpr uint32_t PED_TYPE = 0x5A4;
        constexpr uint32_t PED_STATE = 0x52E;
    #endif
}

//=============================================================================
// ARM64 Game Function Addresses (from nm -D libGTASA.so)
// These are VERIFIED EXPORTED SYMBOLS from GTA:SA Android v2.10 ARM64
//=============================================================================

namespace GameAddr
{
    #if defined(__aarch64__)
        // Player Ped Management (slot-based - may crash for slots 2+)
        constexpr uintptr_t SetupPlayerPed       = 0x5C0FD4;  // CPlayerPed::SetupPlayerPed(int)
        constexpr uintptr_t RemovePlayerPed      = 0x5C10B4;  // CPlayerPed::RemovePlayerPed(int)
        constexpr uintptr_t DeactivatePlayerPed  = 0x5C1140;  // CPlayerPed::DeactivatePlayerPed(int) - from SA-MP
        constexpr uintptr_t ReactivatePlayerPed  = 0x5C1158;  // CPlayerPed::ReactivatePlayerPed(int) - from SA-MP

        // MTA PC Approach - Direct Allocation (bypasses slot system)
        // Found from SA-MP ARM64 symbol dump: DUMP 2.1 (64).txt
        constexpr uintptr_t CPed_OperatorNew     = 0x59576C;  // CPed::operator new(unsigned long) - _ZN4CPednwEm
        constexpr uintptr_t CPlayerPed_Ctor      = 0x5C0BAC;  // CPlayerPed::CPlayerPed(int, bool) - _ZN10CPlayerPedC1Eib
        constexpr uintptr_t CPlayerPedData_Ctor  = 0x4F10CC;  // CPlayerPedData::CPlayerPedData() - _ZN14CPlayerPedDataC1Ev
        constexpr uintptr_t CPlayerPedData_Alloc = 0x4F12A4;  // CPlayerPedData::AllocateData() - used by constructor

        // Finding Player/Ped
        constexpr uintptr_t FindPlayerPed        = 0x4EFAE0;  // FindPlayerPed(int)
        constexpr uintptr_t FindPlayerInfo       = 0x4EFA34;  // FindPlayerInfo(int)

        // Ped Class
        constexpr uintptr_t CPed_SetModelIndex   = 0x59993C;  // CPed::SetModelIndex(uint)
        constexpr uintptr_t CPed_Teleport        = 0x59DD90;  // CPed::Teleport(CVector, uchar)

        // Entity/Placeable
        constexpr uintptr_t CPlaceable_SetMatrix = 0x4EBF5C;  // CPlaceable::SetMatrix(CMatrix&)

        // World Management
        constexpr uintptr_t CWorld_Add           = 0x507518;  // CWorld::Add(CEntity*)
        constexpr uintptr_t CWorld_Remove        = 0x5073A0;  // CWorld::Remove(CEntity*)

        // Population (alternative ped creation)
        constexpr uintptr_t CPopulation_AddPed   = 0x5CEB40;  // CPopulation::AddPed(ePedType, uint, CVector&, bool)

        // Scripts - clear spawn area
        constexpr uintptr_t ClearSpaceForMissionEntity = 0x419BE0;  // CTheScripts::ClearSpaceForMissionEntity

        // Pool access
        constexpr uintptr_t GetPoolPed           = 0x575D0C;  // GetPoolPed(int)
        constexpr uintptr_t CPools_ms_pPedPool   = 0xBC3BB8;  // CPools::ms_pPedPool (data)

        // Streaming
        constexpr uintptr_t CStreaming_RequestModel = 0x4106E4;  // CStreaming::RequestModel(int, int)
        constexpr uintptr_t CStreaming_LoadAllRequestedModels = 0x412EF8;  // CStreaming::LoadAllRequestedModels(bool)

        // Structure sizes (from SA-MP reference VALIDATE_SIZE)
        constexpr size_t SIZEOF_CPLAYERPED = 0x998;  // 2456 bytes (CPlayerPedGta ARM64)
        constexpr size_t SIZEOF_CPLAYERPEDDATA = 0xD8;  // 216 bytes (CPlayerPedData ARM64)

        // Offsets within CPedGTA (ARM64)
        // Calculated from: CPhysical(0x198) + AudioEntities(0x400) + m_pIntelligence(8)
        constexpr uint32_t PPLAYERDATA_OFFSET = 0x5A0;  // Offset of m_pPlayerData in CPedGTA
    #else
        // ARM32 addresses (from SA-MP reference) - +1 for Thumb mode
        constexpr uintptr_t SetupPlayerPed       = 0x4C39A4 + 1;
        constexpr uintptr_t DeactivatePlayerPed  = 0x4C3AD4 + 1;
        constexpr uintptr_t ReactivatePlayerPed  = 0x4C3AEC + 1;
        constexpr uintptr_t FindPlayerPed        = 0x4025C0 + 1;
        constexpr uintptr_t ClearSpaceForMissionEntity = 0x34DA34 + 1;
        constexpr uintptr_t GetPoolPed           = 0x483DB8 + 1;
        constexpr uintptr_t CPed_SetModelIndex   = 0x49FAD5;
        constexpr uintptr_t CWorld_Add           = 0x407518;
        constexpr uintptr_t CWorld_Remove        = 0x4073A0;
    #endif
}

//=============================================================================
// Player Slot Management (SA-MP approach)
// Slot 0 = local player, slots 2+ = remote players
//=============================================================================

constexpr int PLAYER_PED_SLOTS = 210;    // Max players + buffer
constexpr int FIRST_REMOTE_SLOT = 2;     // Slot 0 is local, 1 reserved

//=============================================================================
// Pending Ped Operation - for deferred execution on main thread
//=============================================================================

enum class PedOperationType
{
    SetModel,
    SetPosition,
    InitializeVisuals,
    AddToWorld,
    CreateInSlot  // NEW: Create ped using slot-based approach (must run on game thread!)
};

struct PendingPedOperation
{
    PedOperationType type;
    uintptr_t pedPtr;
    uint16_t modelId;
    float x, y, z;
    float rotation;
    int slotNum;
    int retryCount;
    uintptr_t* resultPtr;  // For returning created ped pointer
};

//=============================================================================
// CPool<CPed> for managing game peds
// GTA:SA uses pools to manage game entities
//=============================================================================

// Pool slot status flags
constexpr uint8_t POOL_SLOT_FREE = 0x00;
constexpr uint8_t POOL_SLOT_USED = 0x80;

//=============================================================================
// CPedFactory - Creates and manages game peds
// Uses SA-MP Android's proven player slot approach
//=============================================================================

class CPedFactory
{
public:
    /**
     * Get singleton instance
     */
    static CPedFactory& GetInstance()
    {
        static CPedFactory instance;
        return instance;
    }

    /**
     * Initialize with game base address
     */
    bool Initialize(uintptr_t gameBase);

    /**
     * Check if initialized
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * Get game base address
     */
    uintptr_t GetGameBase() const { return m_gameBase; }

    /**
     * Create a ped at the specified position using player slot system
     * @param modelId Skin/model ID (0 = CJ, 1-299 = peds)
     * @param x,y,z Position
     * @param rotation Z rotation in radians
     * @return Ped pointer or 0 on failure
     */
    uintptr_t CreatePed(uint16_t modelId, float x, float y, float z, float rotation);

    /**
     * Create a ped using a specific player slot (public wrapper - queues for game thread)
     * @param slotNum Player slot number (2+ for remote players)
     * @param modelId Skin/model ID
     * @param x,y,z Position
     * @param rotation Z rotation in radians
     * @return Ped pointer or 0 on failure
     */
    uintptr_t CreatePedInSlot(int slotNum, uint16_t modelId, float x, float y, float z, float rotation);

    /**
     * Internal: Create ped in slot - MUST be called from game thread!
     * This is the actual implementation called by ProcessPendingOperations.
     */
    uintptr_t CreatePedInSlotInternal(int slotNum, uint16_t modelId, float x, float y, float z, float rotation);

    /**
     * Create a ped using MTA PC's direct allocation approach
     * This bypasses the problematic slot system that crashes in AllocateData
     * Based on: /Client/game_sa/CPlayerPedSA.cpp lines 32-100
     *
     * @param modelId Skin/model ID
     * @param x,y,z Position
     * @param rotation Z rotation in radians
     * @return Ped pointer or 0 on failure
     */
    uintptr_t CreatePedDirect(uint16_t modelId, float x, float y, float z, float rotation);

    /**
     * Destroy a ped and free its slot
     * @param pedPtr Ped pointer to destroy
     */
    void DestroyPed(uintptr_t pedPtr);

    /**
     * Destroy a ped by its slot number
     */
    void DestroyPedBySlot(int slotNum);

    /**
     * Get the slot number associated with a ped
     */
    int GetPedSlot(uintptr_t pedPtr);

    /**
     * Update ped position
     */
    void SetPedPosition(uintptr_t pedPtr, float x, float y, float z);

    /**
     * Update ped rotation
     */
    void SetPedRotation(uintptr_t pedPtr, float rotation);

    /**
     * Get ped's current position
     */
    bool GetPedPosition(uintptr_t pedPtr, float& x, float& y, float& z);

    /**
     * Set ped health
     */
    void SetPedHealth(uintptr_t pedPtr, float health);

    /**
     * Set ped armor
     */
    void SetPedArmor(uintptr_t pedPtr, float armor);

    /**
     * Set ped model/skin
     */
    void SetPedModel(uintptr_t pedPtr, uint16_t modelId);

    /**
     * Request model to be loaded
     */
    bool RequestModel(uint16_t modelId);

    /**
     * Check if model is loaded
     */
    bool IsModelLoaded(uint16_t modelId);

    /**
     * Load all requested models synchronously
     */
    void LoadAllRequestedModels();

    /**
     * Queue a SetModelIndex operation for deferred execution
     * This is needed because SetModelIndex crashes when called from network thread
     */
    void QueueSetModel(uintptr_t pedPtr, uint16_t modelId);

    /**
     * Process pending ped operations - call from game thread
     * Returns number of operations processed
     */
    int ProcessPendingOperations();

    /**
     * Check if there are pending operations
     */
    bool HasPendingOperations() const;

    /**
     * Get the ped pointer for a slot (0 if not yet created)
     */
    uintptr_t GetPedForSlot(int slotNum) const
    {
        if (slotNum >= 0 && slotNum < PLAYER_PED_SLOTS)
            return m_slotPedPtrs[slotNum];
        return 0;
    }

    /**
     * Get the last assigned slot (for async creation tracking)
     */
    int GetLastAssignedSlot() const { return m_lastAssignedSlot; }

private:
    CPedFactory() : m_initialized(false), m_gameBase(0), m_pedPool(0), m_lastAssignedSlot(-1)
    {
        m_usedSlots.fill(false);
    }

    /**
     * Find the first free player slot
     */
    int FindFreeSlot();

    /**
     * Mark slot as used/free
     */
    void SetSlotUsed(int slotNum, bool used);

    /**
     * Check if slot is used
     */
    bool IsSlotUsed(int slotNum) const;

    /**
     * Find the CPedPool address
     */
    bool FindPedPool();

    /**
     * Get ped's matrix pointer
     */
    RwMatrix* GetPedMatrix(uintptr_t pedPtr);

    //=========================================================================
    // Game Function Typedefs
    //=========================================================================

    // CPlayerPed::SetupPlayerPed(int slotNum)
    using SetupPlayerPedFn = void (*)(int);

    // CPlayerPed::DeactivatePlayerPed(int slotNum)
    using DeactivatePlayerPedFn = void (*)(int);

    // CPlayerPed::ReactivatePlayerPed(int slotNum)
    using ReactivatePlayerPedFn = void (*)(int);

    // CPlayerPed::RemovePlayerPed(int slotNum)
    using RemovePlayerPedFn = void (*)(int);

    // FindPlayerPed(int slotNum) -> CPed*
    using FindPlayerPedFn = uintptr_t (*)(int);

    // CWorld::Add(CEntity*)
    using CWorldAddFn = void (*)(uintptr_t);

    // CWorld::Remove(CEntity*)
    using CWorldRemoveFn = void (*)(uintptr_t);

    // CPed::SetModelIndex(uint modelId)
    using CPedSetModelIndexFn = void (*)(uintptr_t, uint32_t);

    // CTheScripts::ClearSpaceForMissionEntity(CVector& pos, CEntity* entity)
    using ClearSpaceFn = void (*)(float*, uintptr_t);

    // CStreaming::RequestModel(int modelId, int flags)
    using RequestModelFn = void (*)(int, int);

    // CStreaming::LoadAllRequestedModels(bool bOnlyPriority)
    using LoadAllModelsFn = void (*)(bool);

    // MTA PC-style direct allocation functions (bypasses slot system)
    // CPed::operator new(unsigned long size) -> void*
    using CPedOperatorNewFn = void* (*)(size_t);

    // CPlayerPed::CPlayerPed(int pedType, bool notLocal)
    // In ARM64: X0=this, W1=pedType, W2=notLocal
    using CPlayerPedCtorFn = void (*)(void*, int, bool);

    // CPlayerPedData::CPlayerPedData()
    using CPlayerPedDataCtorFn = void (*)(void*);

    //=========================================================================
    // Member Variables
    //=========================================================================

    // Game base address
    uintptr_t m_gameBase;

    // CPedPool address
    uintptr_t m_pedPool;

    // Initialization state
    bool m_initialized;

    // Player slot tracking (true = in use)
    std::array<bool, PLAYER_PED_SLOTS> m_usedSlots;

    // Slot to ped pointer mapping
    std::array<uintptr_t, PLAYER_PED_SLOTS> m_slotPedPtrs;

    // Mutex for thread safety
    mutable std::mutex m_mutex;

    // Pending operations queue (processed from game thread)
    std::vector<PendingPedOperation> m_pendingOperations;
    mutable std::mutex m_pendingMutex;

    // Track last assigned slot for async creation
    int m_lastAssignedSlot;
};

//=============================================================================
// Implementation
//=============================================================================

inline bool CPedFactory::Initialize(uintptr_t gameBase)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized)
        return true;

    if (gameBase == 0)
    {
        PEDFAC_LOGE("Invalid game base");
        return false;
    }

    m_gameBase = gameBase;

    // Initialize slot tracking
    m_usedSlots.fill(false);
    m_slotPedPtrs.fill(0);

    // Slots 0-1 are reserved for local player
    m_usedSlots[0] = true;
    m_usedSlots[1] = true;

    // Find ped pool (optional, for debugging)
    FindPedPool();

    //=========================================================================
    // CRITICAL: Apply CWorld::Players patch to enable remote players
    //
    // Without this patch, SetupPlayerPed(slot>=2) crashes because the game's
    // internal CWorld::Players array only supports 2 players.
    //
    // SA-MP Android uses this same approach (patches.cpp:145)
    //=========================================================================
    PEDFAC_LOGI("=== Applying CWorld::Players Patch ===");
    auto& worldPlayers = CWorldPlayers::GetInstance();
    if (!worldPlayers.Initialize(gameBase))
    {
        PEDFAC_LOGW("CWorld::Players patch FAILED - will use direct allocation fallback");
        PEDFAC_LOGW("Remote players may not be visible in game world!");
    }
    else
    {
        PEDFAC_LOGI("CWorld::Players patch SUCCESS - slot-based ped creation enabled!");

        // Disable CGame::InitialiseWhenRestarting to prevent our patch from being overwritten
        // This is SA-MP's approach: write a RET instruction at the function start
        // In multiplayer, respawning is controlled by the server, not the game
        if (worldPlayers.DisableGameRestart())
        {
            PEDFAC_LOGI("Game restart DISABLED - patch will persist!");
        }
        else
        {
            PEDFAC_LOGW("Failed to disable game restart - using periodic verification fallback");
        }
    }

    m_initialized = true;

    #if defined(__aarch64__)
        PEDFAC_LOGI("ARM64 Ped factory initialized");
        PEDFAC_LOGI("  Game base: 0x%lx", (unsigned long)gameBase);
        PEDFAC_LOGI("  CWorld::Players patched: %s", worldPlayers.IsPatched() ? "YES" : "NO");
        PEDFAC_LOGI("  SetupPlayerPed: 0x%lx", (unsigned long)(gameBase + GameAddr::SetupPlayerPed));
        PEDFAC_LOGI("  FindPlayerPed: 0x%lx", (unsigned long)(gameBase + GameAddr::FindPlayerPed));
        PEDFAC_LOGI("  CWorld::Add: 0x%lx", (unsigned long)(gameBase + GameAddr::CWorld_Add));
        PEDFAC_LOGI("  sizeof(CPlayerPed): 0x%lx", (unsigned long)GameAddr::SIZEOF_CPLAYERPED);
    #else
        PEDFAC_LOGI("ARM32 Ped factory initialized, game base: 0x%lx", (unsigned long)gameBase);
    #endif

    return true;
}

inline int CPedFactory::FindFreeSlot()
{
    for (int i = FIRST_REMOTE_SLOT; i < PLAYER_PED_SLOTS; ++i)
    {
        if (!m_usedSlots[i])
            return i;
    }
    return -1;
}

inline void CPedFactory::SetSlotUsed(int slotNum, bool used)
{
    if (slotNum >= 0 && slotNum < PLAYER_PED_SLOTS)
    {
        m_usedSlots[slotNum] = used;
    }
}

inline bool CPedFactory::IsSlotUsed(int slotNum) const
{
    if (slotNum >= 0 && slotNum < PLAYER_PED_SLOTS)
        return m_usedSlots[slotNum];
    return true;
}

inline bool CPedFactory::FindPedPool()
{
    #if defined(__aarch64__)
        m_pedPool = m_gameBase + GameAddr::CPools_ms_pPedPool;
        PEDFAC_LOGD("PedPool address: 0x%lx", (unsigned long)m_pedPool);
    #else
        m_pedPool = 0;
    #endif
    return m_pedPool != 0;
}

inline RwMatrix* CPedFactory::GetPedMatrix(uintptr_t pedPtr)
{
    if (pedPtr == 0)
        return nullptr;

    uintptr_t* matrixPtrAddr = reinterpret_cast<uintptr_t*>(pedPtr + PedOffset::MATRIX_PTR);
    uintptr_t matrixPtr = *matrixPtrAddr;

    if (matrixPtr == 0)
        return nullptr;

    return reinterpret_cast<RwMatrix*>(matrixPtr);
}

//=============================================================================
// CreatePed - Main entry point (finds free slot automatically)
//
// CRITICAL: Game functions like SetupPlayerPed are NOT thread-safe!
// They must be called from the game's main thread, not the network thread.
//
// STRATEGY:
// 1. If CWorld::Players patch is applied → Queue slot-based creation for game thread
// 2. Otherwise → Fall back to direct allocation (deferred CWorld::Add)
//=============================================================================

inline uintptr_t CPedFactory::CreatePed(uint16_t modelId, float x, float y, float z, float rotation)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
    {
        PEDFAC_LOGE("CreatePed: Factory not initialized");
        return 0;
    }

    PEDFAC_LOGI("CreatePed: Creating ped model %d at (%.1f, %.1f, %.1f)",
                modelId, x, y, z);

    // Check if CWorld::Players patch is applied
    auto& worldPlayers = CWorldPlayers::GetInstance();
    if (worldPlayers.IsPatched())
    {
        // PREFERRED: Use slot-based approach (SA-MP Android method)
        // CRITICAL: Must be executed on game thread, not network thread!
        int slotNum = worldPlayers.FindFreeSlot();
        if (slotNum >= 2)
        {
            PEDFAC_LOGI("CreatePed: Queuing slot-based creation (slot %d) for game thread", slotNum);
            worldPlayers.SetSlotUsed(slotNum, true);

            // Queue for game thread execution
            {
                std::lock_guard<std::mutex> pendingLock(m_pendingMutex);
                PendingPedOperation op;
                op.type = PedOperationType::CreateInSlot;
                op.slotNum = slotNum;
                op.modelId = modelId;
                op.x = x;
                op.y = y;
                op.z = z;
                op.rotation = rotation;
                op.pedPtr = 0;
                op.retryCount = 0;
                op.resultPtr = nullptr;  // Result will be stored in m_slotPedPtrs
                m_pendingOperations.push_back(op);
            }

            // Mark slot as pending (will be updated when actually created)
            m_usedSlots[slotNum] = true;
            m_slotPedPtrs[slotNum] = 0;  // Will be set after creation
            m_lastAssignedSlot = slotNum;  // Track for GetPedForSlot lookup

            // IMPORTANT: Return the slot number with a high bit set to mark as "pending"
            // This allows caller to track which slot to check later
            // The high bit (0x80000000) indicates this is a slot number, not a ped pointer
            uintptr_t pendingMarker = 0x80000000 | static_cast<uintptr_t>(slotNum);
            PEDFAC_LOGI("CreatePed: Queued for slot %d - returning pending marker 0x%lx",
                        slotNum, (unsigned long)pendingMarker);
            return pendingMarker;
        }
        else
        {
            PEDFAC_LOGW("CreatePed: No free slots available, falling back to direct allocation");
        }
    }
    else
    {
        PEDFAC_LOGW("CreatePed: CWorld::Players not patched, using direct allocation fallback");
    }

    // FALLBACK: Use direct allocation (may crash in CWorld::Add)
    return CreatePedDirect(modelId, x, y, z, rotation);
}

//=============================================================================
// CreatePedInSlot - Public wrapper that queues for game thread
//=============================================================================

inline uintptr_t CPedFactory::CreatePedInSlot(int slotNum, uint16_t modelId, float x, float y, float z, float rotation)
{
    // Queue for game thread execution (thread-safe)
    PEDFAC_LOGI("CreatePedInSlot: Queuing slot %d for game thread", slotNum);

    std::lock_guard<std::mutex> lock(m_pendingMutex);
    PendingPedOperation op;
    op.type = PedOperationType::CreateInSlot;
    op.slotNum = slotNum;
    op.modelId = modelId;
    op.x = x;
    op.y = y;
    op.z = z;
    op.rotation = rotation;
    op.pedPtr = 0;
    op.retryCount = 0;
    op.resultPtr = nullptr;
    m_pendingOperations.push_back(op);

    // Return slot as temp ID - actual ped created on game thread
    return static_cast<uintptr_t>(slotNum);
}

//=============================================================================
// CreatePedInSlotInternal - SA-MP Android's proven approach
// MUST BE CALLED FROM GAME THREAD!
// Based on: samp-android-reference/app/src/main/cpp/samp/game/playerped.cpp
//=============================================================================

inline uintptr_t CPedFactory::CreatePedInSlotInternal(int slotNum, uint16_t modelId, float x, float y, float z, float rotation)
{
    if (!m_initialized || m_gameBase == 0)
    {
        PEDFAC_LOGE("CreatePedInSlot: Not initialized");
        return 0;
    }

    if (slotNum < FIRST_REMOTE_SLOT || slotNum >= PLAYER_PED_SLOTS)
    {
        PEDFAC_LOGE("CreatePedInSlot: Invalid slot %d", slotNum);
        return 0;
    }

    if (m_usedSlots[slotNum])
    {
        PEDFAC_LOGW("CreatePedInSlot: Slot %d already in use", slotNum);
        // Return existing ped if we have it
        if (m_slotPedPtrs[slotNum] != 0)
            return m_slotPedPtrs[slotNum];
    }

    PEDFAC_LOGI("CreatePedInSlot: Creating ped in slot %d, model %d", slotNum, modelId);

    #if defined(__aarch64__)
    //=========================================================================
    // ARM64 Implementation - SA-MP's proven sequence
    //=========================================================================

    PEDFAC_LOGI("=== CreatePedInSlot START: slot=%d model=%d pos=(%.1f,%.1f,%.1f) ===",
                slotNum, modelId, x, y, z);

    // VERIFY: Check that CWorld::Players patch is still in place
    auto& worldPlayers = CWorldPlayers::GetInstance();
    uintptr_t playersPointerAddr = m_gameBase + 0x84E7A8;
    void* currentPlayersPtr = *reinterpret_cast<void**>(playersPointerAddr);
    void* ourPlayersPtr = worldPlayers.GetPlayersArray();

    if (currentPlayersPtr != ourPlayersPtr)
    {
        PEDFAC_LOGE("=== CRITICAL: CWorld::Players patch was OVERWRITTEN! ===");
        PEDFAC_LOGE("  Expected: 0x%lx, Actual: 0x%lx",
                    (unsigned long)ourPlayersPtr, (unsigned long)currentPlayersPtr);
        PEDFAC_LOGE("  Re-applying patch...");
        worldPlayers.Initialize(m_gameBase);
    }
    else
    {
        PEDFAC_LOGI("  VERIFIED: CWorld::Players patch is in place");
    }

    // Step 1: Setup player ped in this slot
    // This allocates a CPlayerPed in the game's player array
    auto fnSetupPlayerPed = reinterpret_cast<SetupPlayerPedFn>(m_gameBase + GameAddr::SetupPlayerPed);
    PEDFAC_LOGI("  Step 1: SetupPlayerPed(%d) @ 0x%lx - CALLING...",
                slotNum, (unsigned long)(m_gameBase + GameAddr::SetupPlayerPed));
    fnSetupPlayerPed(slotNum);
    PEDFAC_LOGI("  Step 1: SetupPlayerPed(%d) - DONE", slotNum);

    // DEBUG: Dump slot contents to verify SetupPlayerPed created a unique ped
    worldPlayers.DebugDumpSlots(slotNum + 1);

    // Step 2: Deactivate the ped (removes from world temporarily)
    // This allows us to reposition without physics issues
    auto fnDeactivatePlayerPed = reinterpret_cast<DeactivatePlayerPedFn>(m_gameBase + GameAddr::DeactivatePlayerPed);
    PEDFAC_LOGI("  Step 2: DeactivatePlayerPed(%d) @ 0x%lx - CALLING...",
                slotNum, (unsigned long)(m_gameBase + GameAddr::DeactivatePlayerPed));
    fnDeactivatePlayerPed(slotNum);
    PEDFAC_LOGI("  Step 2: DeactivatePlayerPed(%d) - DONE", slotNum);

    // Step 3: Get the ped pointer using FindPlayerPed
    auto fnFindPlayerPed = reinterpret_cast<FindPlayerPedFn>(m_gameBase + GameAddr::FindPlayerPed);
    PEDFAC_LOGI("  Step 3: FindPlayerPed(%d) @ 0x%lx - CALLING...",
                slotNum, (unsigned long)(m_gameBase + GameAddr::FindPlayerPed));
    uintptr_t pedPtr = fnFindPlayerPed(slotNum);
    PEDFAC_LOGI("  Step 3: FindPlayerPed(%d) = 0x%lx", slotNum, (unsigned long)pedPtr);

    if (pedPtr == 0)
    {
        PEDFAC_LOGE("CreatePedInSlot: FAILED - FindPlayerPed returned NULL for slot %d", slotNum);
        return 0;
    }

    // CRITICAL DEBUG: Check if this ped is the same as local player (slot 0)
    // If so, we have a bug - remote player will mirror local player!
    uintptr_t localPed = fnFindPlayerPed(0);
    if (pedPtr == localPed)
    {
        PEDFAC_LOGE("=== BUG DETECTED ===");
        PEDFAC_LOGE("FindPlayerPed(%d) returned SAME ped as FindPlayerPed(0)!", slotNum);
        PEDFAC_LOGE("Remote ped 0x%lx == Local ped 0x%lx", (unsigned long)pedPtr, (unsigned long)localPed);
        PEDFAC_LOGE("This means slot %d is aliased to slot 0 - CWorld::Players patch may have failed!", slotNum);
        PEDFAC_LOGE("==================");
        // Continue anyway - let's see what happens
    }
    else
    {
        PEDFAC_LOGI("  OK: Slot %d ped (0x%lx) differs from slot 0 ped (0x%lx)",
                    slotNum, (unsigned long)pedPtr, (unsigned long)localPed);
    }

    // Step 4: Clear space at spawn location
    // Removes nearby entities that might interfere with spawn
    float posVector[3] = { x, y, z };
    auto fnClearSpace = reinterpret_cast<ClearSpaceFn>(m_gameBase + GameAddr::ClearSpaceForMissionEntity);
    PEDFAC_LOGI("  Step 4: ClearSpaceForMissionEntity @ 0x%lx - CALLING...",
                (unsigned long)(m_gameBase + GameAddr::ClearSpaceForMissionEntity));
    fnClearSpace(posVector, pedPtr);
    PEDFAC_LOGI("  Step 4: ClearSpaceForMissionEntity - DONE");

    // Step 5: Reactivate the ped (adds back to world)
    auto fnReactivatePlayerPed = reinterpret_cast<ReactivatePlayerPedFn>(m_gameBase + GameAddr::ReactivatePlayerPed);
    PEDFAC_LOGI("  Step 5: ReactivatePlayerPed(%d) @ 0x%lx - CALLING...",
                slotNum, (unsigned long)(m_gameBase + GameAddr::ReactivatePlayerPed));
    fnReactivatePlayerPed(slotNum);
    PEDFAC_LOGI("  Step 5: ReactivatePlayerPed(%d) - DONE", slotNum);

    // Step 6: Add to world explicitly
    auto fnWorldAdd = reinterpret_cast<CWorldAddFn>(m_gameBase + GameAddr::CWorld_Add);
    PEDFAC_LOGI("  Step 6: CWorld::Add(0x%lx) @ 0x%lx - CALLING...",
                (unsigned long)pedPtr, (unsigned long)(m_gameBase + GameAddr::CWorld_Add));
    fnWorldAdd(pedPtr);
    PEDFAC_LOGI("  Step 6: CWorld::Add - DONE");

    // Step 7a: Check if ped has RpClump already (debug)
    // m_pRwClump is at offset 0x20 in CEntityGTA (after CPlaceable)
    constexpr uint32_t RWCLUMP_OFFSET = 0x20;
    uintptr_t* pRwClump = reinterpret_cast<uintptr_t*>(pedPtr + RWCLUMP_OFFSET);
    PEDFAC_LOGI("  Step 7a: Before SetModelIndex, m_pRwClump = 0x%lx", (unsigned long)*pRwClump);

    // Step 7b: Call CPed::SetModelIndex to ensure model is set
    constexpr uint32_t CPed_SetModelIndex_Offset = 0x595998;
    typedef void (*CPedSetModelIndexFn)(uintptr_t pedPtr, uint32_t modelId);
    auto fnSetModelIndex = reinterpret_cast<CPedSetModelIndexFn>(m_gameBase + CPed_SetModelIndex_Offset);

    PEDFAC_LOGI("  Step 7b: Calling CPed::SetModelIndex(0) @ 0x%lx...",
                (unsigned long)(m_gameBase + CPed_SetModelIndex_Offset));
    fnSetModelIndex(pedPtr, 0);  // Always use model 0 (CJ)
    PEDFAC_LOGI("  Step 7b: CPed::SetModelIndex - DONE, m_pRwClump = 0x%lx", (unsigned long)*pRwClump);

    // Step 7c: If RpClump is still NULL, try CEntity::CreateRwObject
    if (*pRwClump == 0)
    {
        PEDFAC_LOGW("  Step 7c: RpClump is NULL! Trying CEntity::CreateRwObject...");
        constexpr uint32_t CEntity_CreateRwObject_Offset = 0x4CB298;
        typedef void (*CreateRwObjectFn)(uintptr_t entityPtr);
        auto fnCreateRwObject = reinterpret_cast<CreateRwObjectFn>(m_gameBase + CEntity_CreateRwObject_Offset);
        fnCreateRwObject(pedPtr);
        PEDFAC_LOGI("  Step 7c: After CreateRwObject, m_pRwClump = 0x%lx", (unsigned long)*pRwClump);
    }
    else
    {
        PEDFAC_LOGI("  Step 7c: RpClump exists, skipping CreateRwObject");
    }

    // Step 7d: SET VISIBILITY FLAGS - CRITICAL FOR RENDERING!
    // Entity flags are at offset 0x28 from entity base (after CPlaceable + RwObject pointer)
    // m_bIsVisible is bit 7 of the first flags byte
    // m_bDontStream is bit 19 (byte 2, bit 3)
    // m_bStreamingDontDelete is bit 10 (byte 1, bit 2)
    constexpr uint32_t ENTITY_FLAGS_OFFSET = 0x28;
    uint32_t* pFlags = reinterpret_cast<uint32_t*>(pedPtr + ENTITY_FLAGS_OFFSET);
    uint32_t oldFlags = *pFlags;

    // Set m_bIsVisible (bit 7) = true
    *pFlags |= (1 << 7);

    // Set m_bStreamingDontDelete (bit 10) = true - prevent deletion
    *pFlags |= (1 << 10);

    // Clear m_bRemoveFromWorld (bit 11) = false - don't remove
    *pFlags &= ~(1 << 11);

    // Clear m_bDontStream (bit 19) = false - allow normal streaming
    *pFlags &= ~(1 << 19);

    PEDFAC_LOGI("  Step 7d: Set visibility flags: 0x%08x -> 0x%08x (m_bIsVisible=1)", oldFlags, *pFlags);

    // Step 8: Set position via matrix
    RwMatrix* matrix = GetPedMatrix(pedPtr);
    if (matrix)
    {
        matrix->pos[0] = x;
        matrix->pos[1] = y;
        matrix->pos[2] = z + 0.15f;  // Slight offset to avoid ground clipping

        // Set rotation (forward vector)
        float sinR = sinf(rotation);
        float cosR = cosf(rotation);
        matrix->at[0] = -sinR;
        matrix->at[1] = cosR;
        matrix->right[0] = cosR;
        matrix->right[1] = sinR;

        PEDFAC_LOGI("  Step 8: Set matrix position (%.1f, %.1f, %.1f) rot=%.2f - DONE", x, y, z, rotation);
    }
    else
    {
        PEDFAC_LOGW("  Step 8: WARNING - GetPedMatrix returned NULL!");
    }

    // Step 9: Set health, armor, and ped state to make ped alive and standing
    // This is critical - without this, ped spawns in dead/ragdoll state!
    float* healthPtr = reinterpret_cast<float*>(pedPtr + PedOffset::HEALTH);
    float* armorPtr = reinterpret_cast<float*>(pedPtr + PedOffset::ARMOR);
    *healthPtr = 100.0f;
    *armorPtr = 0.0f;
    PEDFAC_LOGI("  Step 9a: Set health=100.0, armor=0.0 - DONE");

    // Set ped state to IDLE (1) - prevents dead/ragdoll state
    // PEDSTATE values: 0=NONE, 1=IDLE, 54=DEAD, 55=DIE
    uint16_t* pedStatePtr = reinterpret_cast<uint16_t*>(pedPtr + PedOffset::PED_STATE);
    *pedStatePtr = 1;  // PEDSTATE_IDLE
    PEDFAC_LOGI("  Step 9b: Set ped state to IDLE (1) - DONE");

    // Mark slot as used
    m_usedSlots[slotNum] = true;
    m_slotPedPtrs[slotNum] = pedPtr;

    PEDFAC_LOGI("=== CreatePedInSlot SUCCESS: ped=0x%lx slot=%d ===", (unsigned long)pedPtr, slotNum);

    return pedPtr;

    #else
    //=========================================================================
    // ARM32 Implementation - Similar approach with Thumb addresses
    //=========================================================================
    PEDFAC_LOGW("ARM32 ped creation not yet implemented");
    return 0;
    #endif
}

//=============================================================================
// CreatePedDirect - MTA PC's Direct Allocation Approach
// Based on: /Client/game_sa/CPlayerPedSA.cpp lines 32-100
// This bypasses the slot system that crashes in AllocateData
//=============================================================================

inline uintptr_t CPedFactory::CreatePedDirect(uint16_t modelId, float x, float y, float z, float rotation)
{
    if (!m_initialized || m_gameBase == 0)
    {
        PEDFAC_LOGE("CreatePedDirect: Not initialized");
        return 0;
    }

    PEDFAC_LOGI("=== CreatePedDirect START: model=%d pos=(%.1f,%.1f,%.1f) ===",
                modelId, x, y, z);

    #if defined(__aarch64__)
    //=========================================================================
    // ARM64 Implementation - MTA PC's Direct Allocation Method
    //=========================================================================

    // Step 1: Allocate memory with CPed::operator new
    auto fnCPedOperatorNew = reinterpret_cast<CPedOperatorNewFn>(m_gameBase + GameAddr::CPed_OperatorNew);
    PEDFAC_LOGI("  Step 1: CPed::operator new(%zu) @ 0x%lx - CALLING...",
                GameAddr::SIZEOF_CPLAYERPED, (unsigned long)(m_gameBase + GameAddr::CPed_OperatorNew));

    void* pedMemory = fnCPedOperatorNew(GameAddr::SIZEOF_CPLAYERPED);
    if (pedMemory == nullptr)
    {
        PEDFAC_LOGE("CreatePedDirect: FAILED - CPed::operator new returned NULL");
        return 0;
    }
    PEDFAC_LOGI("  Step 1: CPed::operator new = 0x%lx - SUCCESS", (unsigned long)pedMemory);

    // Step 2: Call CPlayerPed constructor with (1, false)
    // Parameters: pedType=1 (player), notLocal=false (behaves like AI ped)
    auto fnCPlayerPedCtor = reinterpret_cast<CPlayerPedCtorFn>(m_gameBase + GameAddr::CPlayerPed_Ctor);
    PEDFAC_LOGI("  Step 2: CPlayerPed::CPlayerPed(1, false) @ 0x%lx - CALLING...",
                (unsigned long)(m_gameBase + GameAddr::CPlayerPed_Ctor));

    fnCPlayerPedCtor(pedMemory, 1, false);  // pedType=1, notLocal=false
    PEDFAC_LOGI("  Step 2: CPlayerPed::CPlayerPed - DONE");

    uintptr_t pedPtr = reinterpret_cast<uintptr_t>(pedMemory);

    // Step 3: Get local player's pPlayerData to copy from
    auto fnFindPlayerPed = reinterpret_cast<FindPlayerPedFn>(m_gameBase + GameAddr::FindPlayerPed);
    uintptr_t localPlayerPed = fnFindPlayerPed(0);
    PEDFAC_LOGI("  Step 3: Local player ped = 0x%lx", (unsigned long)localPlayerPed);

    // Step 4: Create and setup CPlayerPedData
    // Allocate manually using standard C++ new
    void* playerDataMemory = malloc(GameAddr::SIZEOF_CPLAYERPEDDATA);
    if (playerDataMemory != nullptr)
    {
        PEDFAC_LOGI("  Step 4: Allocated CPlayerPedData @ 0x%lx (%zu bytes)",
                    (unsigned long)playerDataMemory, GameAddr::SIZEOF_CPLAYERPEDDATA);

        // Initialize with CPlayerPedData constructor
        auto fnCPlayerPedDataCtor = reinterpret_cast<CPlayerPedDataCtorFn>(m_gameBase + GameAddr::CPlayerPedData_Ctor);
        fnCPlayerPedDataCtor(playerDataMemory);
        PEDFAC_LOGI("  Step 4: CPlayerPedData constructor called");

        // Copy from local player's pPlayerData if available
        if (localPlayerPed != 0)
        {
            // Get local player's pPlayerData pointer
            uintptr_t* localPDataPtrAddr = reinterpret_cast<uintptr_t*>(localPlayerPed + GameAddr::PPLAYERDATA_OFFSET);
            uintptr_t localPData = *localPDataPtrAddr;

            if (localPData != 0)
            {
                // Copy the data
                memcpy(playerDataMemory, reinterpret_cast<void*>(localPData), GameAddr::SIZEOF_CPLAYERPEDDATA);
                PEDFAC_LOGI("  Step 4: Copied from local player's pPlayerData @ 0x%lx", (unsigned long)localPData);
            }
        }

        // Step 5: Assign pPlayerData to our new ped
        uintptr_t* pedPDataPtrAddr = reinterpret_cast<uintptr_t*>(pedPtr + GameAddr::PPLAYERDATA_OFFSET);
        *pedPDataPtrAddr = reinterpret_cast<uintptr_t>(playerDataMemory);
        PEDFAC_LOGI("  Step 5: Assigned pPlayerData to new ped");
    }
    else
    {
        PEDFAC_LOGW("  Step 4: WARNING - Failed to allocate CPlayerPedData");
    }

    // Step 6: Set model index directly (SA-MP approach)
    // SetModelIndex crashes both from network thread AND game thread
    // Just set m_nModelIndex directly - the model should already be loaded (CJ = model 0)
    uint16_t* pModelIndex = reinterpret_cast<uint16_t*>(pedPtr + PedOffset::MODEL_INDEX);
    *pModelIndex = modelId;
    PEDFAC_LOGI("  Step 6: Set m_nModelIndex directly to %d (no deferred call)", modelId);

    // DISABLED: Deferred SetModelIndex crashes with SIGABRT
    // The direct m_nModelIndex assignment should be enough for model 0 (CJ)

    // Step 7: Set streaming flags to prevent the ped from being streamed out
    // MTA PC sets: bStreamingDontDelete = true, bDontStream = true
    // These are at offset 0x36 (flags byte) in CEntity on ARM64
    // Looking at CEntitySAInterface, the flags are bitfields starting around offset 0x34-0x38
    // For now, we'll rely on the model being set correctly

    // Step 8: Clear space at spawn location
    // DISABLED: ClearSpaceForMissionEntity may be corrupting memory, causing CWorld::Add to crash
    // float posVector[3] = { x, y, z };
    // auto fnClearSpace = reinterpret_cast<ClearSpaceFn>(m_gameBase + GameAddr::ClearSpaceForMissionEntity);
    // fnClearSpace(posVector, pedPtr);
    PEDFAC_LOGI("  Step 8: ClearSpaceForMissionEntity - SKIPPED (testing crash fix)");

    // Step 9: Queue CWorld::Add for game thread execution
    // CWorld::Add crashes when called from network thread - queue it for game thread
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        PendingPedOperation op;
        op.type = PedOperationType::AddToWorld;
        op.pedPtr = pedPtr;
        op.retryCount = 0;
        m_pendingOperations.push_back(op);
        PEDFAC_LOGI("  Step 9: Queued CWorld::Add for deferred execution from game thread");
    }

    // Step 10: Set position via matrix
    RwMatrix* matrix = GetPedMatrix(pedPtr);
    if (matrix)
    {
        matrix->pos[0] = x;
        matrix->pos[1] = y;
        matrix->pos[2] = z + 0.15f;  // Slight offset to avoid ground clipping

        // Set rotation (forward vector)
        float sinR = sinf(rotation);
        float cosR = cosf(rotation);
        matrix->at[0] = -sinR;
        matrix->at[1] = cosR;
        matrix->right[0] = cosR;
        matrix->right[1] = sinR;

        PEDFAC_LOGI("  Step 10: Set matrix position (%.1f, %.1f, %.1f) rot=%.2f - DONE", x, y, z, rotation);
    }

    // Step 11: Set health and armor so ped is alive!
    float* healthPtr = reinterpret_cast<float*>(pedPtr + PedOffset::HEALTH);
    float* armorPtr = reinterpret_cast<float*>(pedPtr + PedOffset::ARMOR);
    *healthPtr = 100.0f;
    *armorPtr = 0.0f;
    PEDFAC_LOGI("  Step 11: Set health=100.0, armor=0.0 - DONE");

    // Step 12: Set ped state to idle (not dead)
    uint16_t* pedStatePtr = reinterpret_cast<uint16_t*>(pedPtr + PedOffset::PED_STATE);
    *pedStatePtr = 1;  // PEDSTATE_IDLE
    PEDFAC_LOGI("  Step 12: Set ped state to IDLE (1) - DONE");

    // Track in our slot system (use first free slot for tracking)
    int slotNum = FindFreeSlot();
    if (slotNum >= 0)
    {
        m_usedSlots[slotNum] = true;
        m_slotPedPtrs[slotNum] = pedPtr;
        PEDFAC_LOGI("  Tracking in slot %d", slotNum);
    }

    PEDFAC_LOGI("=== CreatePedDirect SUCCESS: ped=0x%lx ===", (unsigned long)pedPtr);
    return pedPtr;

    #else
    PEDFAC_LOGW("CreatePedDirect: ARM32 not yet implemented");
    return 0;
    #endif
}

//=============================================================================
// DestroyPed - Remove ped from world and free slot
//=============================================================================

inline void CPedFactory::DestroyPed(uintptr_t pedPtr)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (pedPtr == 0)
        return;

    // Find which slot this ped belongs to
    int slotNum = GetPedSlot(pedPtr);
    if (slotNum >= FIRST_REMOTE_SLOT)
    {
        DestroyPedBySlot(slotNum);
    }
    else
    {
        PEDFAC_LOGW("DestroyPed: Ped 0x%lx not in our slot tracking", (unsigned long)pedPtr);
    }
}

inline void CPedFactory::DestroyPedBySlot(int slotNum)
{
    if (slotNum < FIRST_REMOTE_SLOT || slotNum >= PLAYER_PED_SLOTS)
        return;

    if (!m_usedSlots[slotNum])
        return;

    PEDFAC_LOGI("DestroyPedBySlot: Destroying ped in slot %d", slotNum);

    #if defined(__aarch64__)
    uintptr_t pedPtr = m_slotPedPtrs[slotNum];

    // Remove from world
    if (pedPtr != 0)
    {
        auto fnWorldRemove = reinterpret_cast<CWorldRemoveFn>(m_gameBase + GameAddr::CWorld_Remove);
        fnWorldRemove(pedPtr);
    }

    // Remove player ped from slot
    auto fnRemovePlayerPed = reinterpret_cast<RemovePlayerPedFn>(m_gameBase + GameAddr::RemovePlayerPed);
    fnRemovePlayerPed(slotNum);
    #endif

    // Free the slot
    m_usedSlots[slotNum] = false;
    m_slotPedPtrs[slotNum] = 0;

    PEDFAC_LOGI("DestroyPedBySlot: Slot %d freed", slotNum);
}

inline int CPedFactory::GetPedSlot(uintptr_t pedPtr)
{
    for (int i = FIRST_REMOTE_SLOT; i < PLAYER_PED_SLOTS; ++i)
    {
        if (m_slotPedPtrs[i] == pedPtr)
            return i;
    }
    return -1;
}

inline void CPedFactory::SetPedPosition(uintptr_t pedPtr, float x, float y, float z)
{
    RwMatrix* matrix = GetPedMatrix(pedPtr);
    if (!matrix)
        return;

    // Update position in matrix
    matrix->pos[0] = x;
    matrix->pos[1] = y;
    matrix->pos[2] = z;
}

inline void CPedFactory::SetPedRotation(uintptr_t pedPtr, float rotation)
{
    RwMatrix* matrix = GetPedMatrix(pedPtr);
    if (!matrix)
        return;

    // Calculate forward vector from rotation
    float sinR = sinf(rotation);
    float cosR = cosf(rotation);

    // Update forward (at) vector
    matrix->at[0] = -sinR;  // X component
    matrix->at[1] = cosR;   // Y component
    // Z component stays as is

    // Update right vector (perpendicular to forward)
    matrix->right[0] = cosR;
    matrix->right[1] = sinR;
}

inline bool CPedFactory::GetPedPosition(uintptr_t pedPtr, float& x, float& y, float& z)
{
    RwMatrix* matrix = GetPedMatrix(pedPtr);
    if (!matrix)
        return false;

    x = matrix->pos[0];
    y = matrix->pos[1];
    z = matrix->pos[2];

    return true;
}

inline void CPedFactory::SetPedHealth(uintptr_t pedPtr, float health)
{
    if (pedPtr == 0)
        return;

    float* healthPtr = reinterpret_cast<float*>(pedPtr + PedOffset::HEALTH);
    *healthPtr = health;
}

inline void CPedFactory::SetPedArmor(uintptr_t pedPtr, float armor)
{
    if (pedPtr == 0)
        return;

    float* armorPtr = reinterpret_cast<float*>(pedPtr + PedOffset::ARMOR);
    *armorPtr = armor;
}

//=============================================================================
// SetPedModel - Change ped skin/model
//=============================================================================

inline void CPedFactory::SetPedModel(uintptr_t pedPtr, uint16_t modelId)
{
    if (pedPtr == 0 || !m_initialized)
        return;

    #if defined(__aarch64__)
    // First request the model to be loaded
    RequestModel(modelId);
    LoadAllRequestedModels();

    // Call CPed::SetModelIndex
    // Note: This is a member function, so we need to call it correctly
    // The function signature is: void CPed::SetModelIndex(uint modelId)
    // On ARM64, 'this' goes in X0, modelId in W1
    using SetModelFn = void (*)(uintptr_t pedPtr, uint32_t modelId);
    auto fnSetModel = reinterpret_cast<SetModelFn>(m_gameBase + GameAddr::CPed_SetModelIndex);
    fnSetModel(pedPtr, modelId);

    PEDFAC_LOGD("SetPedModel: Set ped 0x%lx to model %d", (unsigned long)pedPtr, modelId);
    #else
    PEDFAC_LOGW("SetPedModel: ARM32 not implemented");
    #endif
}

//=============================================================================
// Model Streaming
//=============================================================================

inline bool CPedFactory::RequestModel(uint16_t modelId)
{
    if (!m_initialized || m_gameBase == 0)
        return false;

    #if defined(__aarch64__)
    // CStreaming::RequestModel(modelId, flags)
    // flags: 2 = PRIORITY_REQUEST
    auto fnRequestModel = reinterpret_cast<RequestModelFn>(m_gameBase + GameAddr::CStreaming_RequestModel);
    fnRequestModel(static_cast<int>(modelId), 2);
    PEDFAC_LOGD("RequestModel: Requested model %d", modelId);
    return true;
    #else
    PEDFAC_LOGW("RequestModel: ARM32 not implemented");
    return false;
    #endif
}

inline bool CPedFactory::IsModelLoaded(uint16_t modelId)
{
    // For now, assume models are loaded after LoadAllRequestedModels
    // A proper implementation would check the model info flags
    return true;
}

inline void CPedFactory::LoadAllRequestedModels()
{
    if (!m_initialized || m_gameBase == 0)
        return;

    #if defined(__aarch64__)
    // CStreaming::LoadAllRequestedModels(bOnlyPriority)
    auto fnLoadAll = reinterpret_cast<LoadAllModelsFn>(m_gameBase + GameAddr::CStreaming_LoadAllRequestedModels);
    fnLoadAll(false);
    PEDFAC_LOGD("LoadAllRequestedModels: Loading...");
    #endif
}

//=============================================================================
// QueueSetModel - Queue a SetModelIndex operation for deferred execution
//=============================================================================

inline void CPedFactory::QueueSetModel(uintptr_t pedPtr, uint16_t modelId)
{
    std::lock_guard<std::mutex> lock(m_pendingMutex);

    PendingPedOperation op;
    op.type = PedOperationType::SetModel;
    op.pedPtr = pedPtr;
    op.modelId = modelId;
    op.retryCount = 0;

    m_pendingOperations.push_back(op);

    PEDFAC_LOGI("Queued SetModel operation: ped=0x%lx model=%d (queue size: %zu)",
                (unsigned long)pedPtr, modelId, m_pendingOperations.size());
}

//=============================================================================
// ProcessPendingOperations - Process queued operations from game thread
// Returns number of operations successfully processed
//=============================================================================

inline int CPedFactory::ProcessPendingOperations()
{
    if (!m_initialized)
        return 0;

    std::lock_guard<std::mutex> lock(m_pendingMutex);

    if (m_pendingOperations.empty())
        return 0;

    int processed = 0;
    std::vector<PendingPedOperation> retryQueue;

    for (auto& op : m_pendingOperations)
    {
        //=====================================================================
        // CreateInSlot - MUST run on game thread for thread safety!
        //=====================================================================
        if (op.type == PedOperationType::CreateInSlot)
        {
            PEDFAC_LOGI("=== Processing CreateInSlot on GAME THREAD: slot=%d model=%d ===",
                        op.slotNum, op.modelId);

            #if defined(__aarch64__)
            // This is the SA-MP sequence, now running safely on game thread
            uintptr_t pedPtr = CreatePedInSlotInternal(op.slotNum, op.modelId,
                                                        op.x, op.y, op.z, op.rotation);
            if (pedPtr != 0)
            {
                // Update the slot with the actual ped pointer
                if (op.slotNum >= 0 && op.slotNum < PLAYER_PED_SLOTS)
                {
                    m_slotPedPtrs[op.slotNum] = pedPtr;
                }
                PEDFAC_LOGI("CreateInSlot SUCCESS on game thread: slot=%d ped=0x%lx",
                            op.slotNum, (unsigned long)pedPtr);
                processed++;
            }
            else
            {
                PEDFAC_LOGE("CreateInSlot FAILED on game thread: slot=%d", op.slotNum);
                // Free the slot since creation failed
                auto& worldPlayers = CWorldPlayers::GetInstance();
                worldPlayers.SetSlotUsed(op.slotNum, false);
                m_usedSlots[op.slotNum] = false;
            }
            #endif
            continue;
        }

        // Skip null ped operations (except CreateInSlot which was handled above)
        if (op.pedPtr == 0)
        {
            PEDFAC_LOGW("Skipping operation: ped is NULL");
            continue;
        }

        if (op.type == PedOperationType::AddToWorld)
        {
            PEDFAC_LOGI("Processing AddToWorld: ped=0x%lx (attempt %d)",
                        (unsigned long)op.pedPtr, op.retryCount + 1);

            #if defined(__aarch64__)
            auto fnWorldAdd = reinterpret_cast<CWorldAddFn>(m_gameBase + GameAddr::CWorld_Add);
            PEDFAC_LOGI("  Calling CWorld::Add from game thread...");
            fnWorldAdd(op.pedPtr);
            PEDFAC_LOGI("  CWorld::Add completed!");
            processed++;
            #endif
        }
        else if (op.type == PedOperationType::SetModel)
        {
            PEDFAC_LOGI("Processing SetModel: ped=0x%lx model=%d (attempt %d)",
                        (unsigned long)op.pedPtr, op.modelId, op.retryCount + 1);

            #if defined(__aarch64__)
            // First request the model to be loaded
            RequestModel(op.modelId);
            LoadAllRequestedModels();

            // Set m_nModelIndex directly (like SA-MP does)
            uint16_t* pModelIndex = reinterpret_cast<uint16_t*>(op.pedPtr + PedOffset::MODEL_INDEX);
            *pModelIndex = op.modelId;
            PEDFAC_LOGI("  Set m_nModelIndex directly to %d", op.modelId);

            // Call CPed::SetModelIndex
            using SetModelFn = void (*)(uintptr_t pedPtr, uint32_t modelId);
            auto fnSetModel = reinterpret_cast<SetModelFn>(m_gameBase + GameAddr::CPed_SetModelIndex);
            PEDFAC_LOGI("  Calling CPed::SetModelIndex...");
            fnSetModel(op.pedPtr, op.modelId);
            PEDFAC_LOGI("  CPed::SetModelIndex completed!");

            processed++;
            #endif
        }
    }

    // Clear processed operations
    m_pendingOperations.clear();

    if (processed > 0)
    {
        PEDFAC_LOGI("Processed %d pending ped operations on game thread", processed);
    }

    return processed;
}

//=============================================================================
// HasPendingOperations - Check if there are queued operations
//=============================================================================

inline bool CPedFactory::HasPendingOperations() const
{
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    return !m_pendingOperations.empty();
}

} // namespace MTA::Android::Multiplayer

#endif // CPED_FACTORY_H
