/*
 * MTA:SA Android - CWorld Players Array Patch
 *
 * This module patches the game's CWorld::Players array to support remote players.
 *
 * The game's original CWorld::Players array only supports 2 players (local + 1).
 * SetupPlayerPed(slotNum) crashes for slots >= 2 because it accesses out-of-bounds.
 *
 * This patch replaces the game's pointer with a larger array (1004 players).
 * This is the same approach used by SA-MP Android.
 *
 * Based on: samp-android-reference/app/src/main/cpp/samp/game/patches.cpp
 *           samp-android-reference/app/src/main/cpp/samp/game/World.h
 *
 * Phase 7f: Remote Player Rendering
 */

#ifndef CWORLD_PLAYERS_H
#define CWORLD_PLAYERS_H

#include <cstdint>
#include <cstring>
#include <array>

#ifdef __ANDROID__
#include <android/log.h>
#include <sys/mman.h>
#include <unistd.h>
#endif


namespace MTA::Android::Multiplayer
{

#define CWORLDP_LOG_TAG "MTA-CWorldPlayers"
#define CWORLDP_LOGI(...) __android_log_print(ANDROID_LOG_INFO, CWORLDP_LOG_TAG, __VA_ARGS__)
#define CWORLDP_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, CWORLDP_LOG_TAG, __VA_ARGS__)
#define CWORLDP_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, CWORLDP_LOG_TAG, __VA_ARGS__)
#define CWORLDP_LOGW(...) __android_log_print(ANDROID_LOG_WARN, CWORLDP_LOG_TAG, __VA_ARGS__)

//=============================================================================
// Constants
//=============================================================================

constexpr int MAX_PLAYERS = 1004;  // SA-MP uses 1004 player slots

//=============================================================================
// CPlayerPedData structure
// Based on: samp-android-reference/app/src/main/cpp/samp/game/PlayerPedData.h
//=============================================================================

struct CVector2D
{
    float x, y;
};

struct CVector
{
    float x, y, z;
};

// Forward declarations
struct CEntityGTA;

#if defined(__aarch64__)
// ARM64 structure - 0xD8 bytes (216 bytes)
struct CPlayerPedData
{
    uintptr_t*      m_pWanted;                          // 0x00
    uintptr_t*      m_pPedClothesDesc;                  // 0x08
    uintptr_t*      m_pArrestingCop;                    // 0x10
    CVector2D       m_vecFightMovement;                 // 0x18
    float           m_fMoveBlendRatio;                  // 0x20
    float           m_fTimeCanRun;                      // 0x24
    float           m_fMoveSpeed;                       // 0x28
    uint8_t         m_nChosenWeapon;                    // 0x2C
    uint8_t         m_nCarDangerCounter;                // 0x2D
    uint8_t         pad0[2];                            // 0x2E - padding
    uint32_t        m_nStandStillTimer;                 // 0x30
    uint32_t        m_nHitAnimDelayTimer;               // 0x34
    float           m_fAttackButtonCounter;             // 0x38
    void*           m_pDangerCar;                       // 0x40
    uint32_t        m_nPlayerFlags;                     // 0x48
    uint32_t        m_nPlayerGroup;                     // 0x4C
    uint32_t        m_nAdrenalineEndTime;               // 0x50
    uint8_t         m_nDrunkenness;                     // 0x54
    uint8_t         m_nFadeDrunkenness;                 // 0x55
    uint8_t         m_nDrugLevel;                       // 0x56
    uint8_t         m_nScriptLimitToGangSize;           // 0x57
    float           m_fBreath;                          // 0x58
    uint32_t        m_nMeleeWeaponAnimReferenced;       // 0x5C
    uint32_t        m_nMeleeWeaponAnimReferencedExtra;  // 0x60
    float           m_fFPSMoveHeading;                  // 0x64
    float           m_fLookPitch;                       // 0x68
    float           m_fSkateBoardSpeed;                 // 0x6C
    float           m_fSkateBoardLean;                  // 0x70
    void*           m_pSpecialAtomic;                   // 0x78
    float           m_fGunSpinSpeed;                    // 0x80
    float           m_fGunSpinAngle;                    // 0x84
    uint32_t        m_nLastTimeFiring;                  // 0x88
    uint32_t        m_nTargetBone;                      // 0x8C
    CVector         m_vecTargetBoneOffset;              // 0x90
    uint32_t        m_nBusFaresCollected;               // 0x9C
    bool            m_bPlayerSprintDisabled;            // 0xA0
    bool            m_bDontAllowWeaponChange;           // 0xA1
    bool            m_bForceInteriorLighting;           // 0xA2
    uint8_t         pad1;                               // 0xA3
    uint16_t        m_nPadDownPressedInMilliseconds;    // 0xA4
    uint16_t        m_nPadUpPressedInMilliseconds;      // 0xA6
    uint8_t         m_nWetness;                         // 0xA8
    bool            m_bPlayersGangActive;               // 0xA9
    uint8_t         m_nWaterCoverPerc;                  // 0xAA
    uint8_t         pad2;                               // 0xAB
    float           m_fWaterHeight;                     // 0xAC
    uint32_t        m_nFireHSMissilePressedTime;        // 0xB0
    void*           m_LastHSMissileTarget;              // 0xB8
    uint32_t        m_nModelIndexOfLastBuildingShot;    // 0xC0
    uint32_t        m_nLastHSMissileLOSTime;            // 0xC4 - 31 bits + 1 bit bLastHSMissileLOS
    void*           m_pCurrentProstitutePed;            // 0xC8
    void*           m_pLastProstituteShagged;           // 0xD0
};
static_assert(sizeof(CPlayerPedData) == 0xD8, "CPlayerPedData ARM64 size mismatch");

#else
// ARM32 structure - ~0xAC bytes (172 bytes)
// NOTE: ARM32 support not yet implemented, sizes are approximate
struct CPlayerPedData
{
    uintptr_t*      m_pWanted;
    uintptr_t*      m_pPedClothesDesc;
    uintptr_t*      m_pArrestingCop;
    CVector2D       m_vecFightMovement;
    float           m_fMoveBlendRatio;
    float           m_fTimeCanRun;
    float           m_fMoveSpeed;
    uint8_t         m_nChosenWeapon;
    uint8_t         m_nCarDangerCounter;
    uint32_t        m_nStandStillTimer;
    uint32_t        m_nHitAnimDelayTimer;
    float           m_fAttackButtonCounter;
    void*           m_pDangerCar;
    uint32_t        m_nPlayerFlags;
    uint32_t        m_nPlayerGroup;
    uint32_t        m_nAdrenalineEndTime;
    uint8_t         m_nDrunkenness;
    uint8_t         m_nFadeDrunkenness;
    uint8_t         m_nDrugLevel;
    uint8_t         m_nScriptLimitToGangSize;
    float           m_fBreath;
    uint32_t        m_nMeleeWeaponAnimReferenced;
    uint32_t        m_nMeleeWeaponAnimReferencedExtra;
    float           m_fFPSMoveHeading;
    float           m_fLookPitch;
    float           m_fSkateBoardSpeed;
    float           m_fSkateBoardLean;
    void*           m_pSpecialAtomic;
    float           m_fGunSpinSpeed;
    float           m_fGunSpinAngle;
    uint32_t        m_nLastTimeFiring;
    uint32_t        m_nTargetBone;
    CVector         m_vecTargetBoneOffset;
    uint32_t        m_nBusFaresCollected;
    bool            m_bPlayerSprintDisabled;
    bool            m_bDontAllowWeaponChange;
    bool            m_bForceInteriorLighting;
    uint16_t        m_nPadDownPressedInMilliseconds;
    uint16_t        m_nPadUpPressedInMilliseconds;
    uint8_t         m_nWetness;
    bool            m_bPlayersGangActive;
    uint8_t         m_nWaterCoverPerc;
    float           m_fWaterHeight;
    uint32_t        m_nFireHSMissilePressedTime;
    void*           m_LastHSMissileTarget;
    uint32_t        m_nModelIndexOfLastBuildingShot;
    uint32_t        m_nLastHSMissileLOSTime;
    void*           m_pCurrentProstitutePed;
    void*           m_pLastProstituteShagged;
};
// ARM32 static_assert disabled until ARM32 support implemented
// static_assert(sizeof(CPlayerPedData) == 0xAC, "CPlayerPedData ARM32 size mismatch");
#endif

//=============================================================================
// CPlayerInfoGta structure
// Based on: samp-android-reference/app/src/main/cpp/samp/game/CPlayerInfoGta.h
//=============================================================================

// Forward declaration for vehicle
struct CVehicleGTA;
struct CPlayerPedGta;

enum ePlayerState : uint8_t
{
    PLAYERSTATE_PLAYING,
    PLAYERSTATE_HAS_DIED,
    PLAYERSTATE_HAS_BEEN_ARRESTED,
    PLAYERSTATE_FAILED_MISSION,
    PLAYERSTATE_LEFT_GAME
};

// RwTexture forward declaration
struct RwTexture;

#if defined(__aarch64__)
// ARM64 structure - 0x1D8 bytes (472 bytes)
struct CPlayerInfoGta
{
    CPlayerPedGta*  m_pPed;                             // 0x00 - Pointer to player ped
    CPlayerPedData  PlayerPedData;                      // 0x08 - Embedded player data (0xD8 bytes)
    CVehicleGTA*    pRemoteVehicle;                     // 0xE0
    CVehicleGTA*    pSpecCar;                           // 0xE8
    int32_t         Score;                              // 0xF0
    int32_t         DisplayScore;                       // 0xF4
    int32_t         CollectablesPickedUp;               // 0xF8
    int32_t         TotalNumCollectables;               // 0xFC
    uint32_t        nLastBumpPlayerCarTimer;            // 0x100
    uint32_t        TaxiTimer;                          // 0x104
    uint32_t        vehicle_time_counter;               // 0x108
    bool            bTaxiTimerScore;                    // 0x10C
    bool            m_bTryingToExitCar;                 // 0x10D
    uint8_t         pad0[2];                            // 0x10E
    CVehicleGTA*    pLastTargetVehicle;                 // 0x110
    ePlayerState    PlayerState;                        // 0x118
    bool            bAfterRemoteVehicleExplosion;       // 0x119
    bool            bCreateRemoteVehicleExplosion;      // 0x11A
    bool            bFadeAfterRemoteVehicleExplosion;   // 0x11B
    uint32_t        TimeOfRemoteVehicleExplosion;       // 0x11C
    uint32_t        LastTimeEnergyLost;                 // 0x120
    uint32_t        LastTimeArmourLost;                 // 0x124
    uint32_t        LastTimeBigGunFired;                // 0x128
    uint32_t        TimesUpsideDownInARow;              // 0x12C
    uint32_t        TimesStuckInARow;                   // 0x130
    uint32_t        nCarTwoWheelCounter;                // 0x134
    float           fCarTwoWheelDist;                   // 0x138
    uint32_t        nCarLess3WheelCounter;              // 0x13C
    uint32_t        nBikeRearWheelCounter;              // 0x140
    float           fBikeRearWheelDist;                 // 0x144
    uint32_t        nBikeFrontWheelCounter;             // 0x148
    float           fBikeFrontWheelDist;                // 0x14C
    uint32_t        nTempBufferCounter;                 // 0x150
    uint32_t        nBestCarTwoWheelsTimeMs;            // 0x154
    float           fBestCarTwoWheelsDistM;             // 0x158
    uint32_t        nBestBikeWheelieTimeMs;             // 0x15C
    float           fBestBikeWheelieDistM;              // 0x160
    uint32_t        nBestBikeStoppieTimeMs;             // 0x164
    float           fBestBikeStoppieDistM;              // 0x168
    uint32_t        CarDensityForCurrentZone;           // 0x16C
    float           RoadDensityAroundPlayer;            // 0x170
    uint32_t        TimeOfLastCarExplosionCaused;       // 0x174
    int32_t         ExplosionMultiplier;                // 0x178
    int32_t         HavocCaused;                        // 0x17C
    int32_t         TimeLastEaten;                      // 0x180
    float           CurrentChaseValue;                  // 0x184
    bool            DoesNotGetTired;                    // 0x188
    bool            FastReload;                         // 0x189
    bool            FireProof;                          // 0x18A
    uint8_t         MaxHealth;                          // 0x18B
    uint8_t         MaxArmour;                          // 0x18C
    bool            bGetOutOfJailFree;                  // 0x18D
    bool            bFreeHealthCare;                    // 0x18E
    bool            bCanDoDriveBy;                      // 0x18F
    uint8_t         m_nBustedAudioStatus;               // 0x190
    uint8_t         pad1;                               // 0x191
    int16_t         m_nLastBustMessageNumber;           // 0x192
    uint8_t         CrossHair[0xC];                     // 0x194
    bool            bGetOject;                          // 0x1A0
    uint8_t         pad2[7];                            // 0x1A1 - align
    char            m_skinName[32];                     // 0x1A8
    RwTexture*      m_pSkinTexture;                     // 0x1C8
    bool            m_bParachuteReferenced;             // 0x1D0
    uint8_t         pad3[3];                            // 0x1D1
    uint32_t        m_nRequireParachuteTimer;           // 0x1D4
};
static_assert(sizeof(CPlayerInfoGta) == 0x1D8, "CPlayerInfoGta ARM64 size mismatch");

#else
// ARM32 structure - 0x194 bytes (404 bytes)
struct CPlayerInfoGta
{
    CPlayerPedGta*  m_pPed;
    CPlayerPedData  PlayerPedData;
    CVehicleGTA*    pRemoteVehicle;
    CVehicleGTA*    pSpecCar;
    int32_t         Score;
    int32_t         DisplayScore;
    int32_t         CollectablesPickedUp;
    int32_t         TotalNumCollectables;
    uint32_t        nLastBumpPlayerCarTimer;
    uint32_t        TaxiTimer;
    uint32_t        vehicle_time_counter;
    bool            bTaxiTimerScore;
    bool            m_bTryingToExitCar;
    uint8_t         pad0[2];
    CVehicleGTA*    pLastTargetVehicle;
    ePlayerState    PlayerState;
    bool            bAfterRemoteVehicleExplosion;
    bool            bCreateRemoteVehicleExplosion;
    bool            bFadeAfterRemoteVehicleExplosion;
    uint32_t        TimeOfRemoteVehicleExplosion;
    uint32_t        LastTimeEnergyLost;
    uint32_t        LastTimeArmourLost;
    uint32_t        LastTimeBigGunFired;
    uint32_t        TimesUpsideDownInARow;
    uint32_t        TimesStuckInARow;
    uint32_t        nCarTwoWheelCounter;
    float           fCarTwoWheelDist;
    uint32_t        nCarLess3WheelCounter;
    uint32_t        nBikeRearWheelCounter;
    float           fBikeRearWheelDist;
    uint32_t        nBikeFrontWheelCounter;
    float           fBikeFrontWheelDist;
    uint32_t        nTempBufferCounter;
    uint32_t        nBestCarTwoWheelsTimeMs;
    float           fBestCarTwoWheelsDistM;
    uint32_t        nBestBikeWheelieTimeMs;
    float           fBestBikeWheelieDistM;
    uint32_t        nBestBikeStoppieTimeMs;
    float           fBestBikeStoppieDistM;
    uint32_t        CarDensityForCurrentZone;
    float           RoadDensityAroundPlayer;
    uint32_t        TimeOfLastCarExplosionCaused;
    int32_t         ExplosionMultiplier;
    int32_t         HavocCaused;
    int32_t         TimeLastEaten;
    float           CurrentChaseValue;
    bool            DoesNotGetTired;
    bool            FastReload;
    bool            FireProof;
    uint8_t         MaxHealth;
    uint8_t         MaxArmour;
    bool            bGetOutOfJailFree;
    bool            bFreeHealthCare;
    bool            bCanDoDriveBy;
    uint8_t         m_nBustedAudioStatus;
    int16_t         m_nLastBustMessageNumber;
    uint8_t         CrossHair[0xC];
    bool            bGetOject;
    char            m_skinName[32];
    RwTexture*      m_pSkinTexture;
    bool            m_bParachuteReferenced;
    uint32_t        m_nRequireParachuteTimer;
};
// ARM32 static_assert disabled until ARM32 support implemented
// static_assert(sizeof(CPlayerInfoGta) == 0x194, "CPlayerInfoGta ARM32 size mismatch");
#endif

//=============================================================================
// CWorldPlayers - Manages the extended Players array
//=============================================================================

class CWorldPlayers
{
public:
    /**
     * Get singleton instance
     */
    static CWorldPlayers& GetInstance()
    {
        static CWorldPlayers instance;
        return instance;
    }

    /**
     * Initialize and apply the patch
     * Must be called BEFORE any remote players are created
     *
     * @param gameBase Base address of libGTASA.so
     * @return true if patch was applied successfully
     */
    bool Initialize(uintptr_t gameBase);

    /**
     * Check if the patch has been applied
     */
    bool IsPatched() const { return m_patched; }

    /**
     * Verify the patch is still in place and re-apply if needed.
     * Call this periodically (e.g., in game loop) because the game's
     * DoGameRestart/CGame::InitialiseWhenRestarting can overwrite our patch!
     *
     * @return true if patch is valid, false if it was missing and re-applied
     */
    bool VerifyAndRepairPatch()
    {
        if (!m_patched || m_gameBase == 0)
            return false;

#if defined(__aarch64__)
        uintptr_t playersPointerAddr = m_gameBase + 0x84E7A8;

        // Read current value
        void* currentPtr = *reinterpret_cast<void**>(playersPointerAddr);

        // Check if our patch is still in place
        if (currentPtr != m_Players)
        {
            CWORLDP_LOGW("=== CWorld::Players PATCH WAS OVERWRITTEN! ===");
            CWORLDP_LOGW("  Expected: 0x%lx, Found: 0x%lx",
                         (unsigned long)m_Players, (unsigned long)currentPtr);
            CWORLDP_LOGW("  Re-applying patch (game probably restarted)...");

            // Re-apply the patch
            if (WritePointer(playersPointerAddr, m_Players))
            {
                CWORLDP_LOGI("  Patch RE-APPLIED successfully!");

                // Also copy fresh local player data if available
                if (currentPtr != nullptr)
                {
                    CPlayerInfoGta* gamePlayersArray = reinterpret_cast<CPlayerInfoGta*>(currentPtr);
                    memcpy(&m_Players[0], &gamePlayersArray[0], sizeof(CPlayerInfoGta));
                    CWORLDP_LOGI("  Copied fresh local player data from game");
                }

                return false;  // Patch was repaired
            }
            else
            {
                CWORLDP_LOGE("  FAILED to re-apply patch!");
                return false;
            }
        }

        return true;  // Patch is valid
#else
        return true;
#endif
    }

    /**
     * Get the game base address
     */
    uintptr_t GetGameBase() const { return m_gameBase; }

    /**
     * Get pointer to our Players array
     */
    CPlayerInfoGta* GetPlayersArray() { return m_Players; }

    /**
     * Get pointer to PlayerInFocus
     */
    int* GetPlayerInFocusPtr() { return &m_PlayerInFocus; }

    /**
     * Force re-apply the patch immediately.
     * Call this from hooks that intercept game restart/init functions.
     */
    void ForceReapplyPatch()
    {
        if (m_gameBase == 0)
            return;

#if defined(__aarch64__)
        uintptr_t playersPointerAddr = m_gameBase + 0x84E7A8;

        // Read what the game just set
        void* gamePtr = *reinterpret_cast<void**>(playersPointerAddr);

        // If it's different from ours, copy fresh data and re-apply
        if (gamePtr != m_Players && gamePtr != nullptr)
        {
            CWORLDP_LOGI("ForceReapplyPatch: Game set Players to 0x%lx, copying data and re-patching",
                         (unsigned long)gamePtr);

            // Copy the local player data the game just initialized
            CPlayerInfoGta* gamePlayersArray = reinterpret_cast<CPlayerInfoGta*>(gamePtr);
            memcpy(&m_Players[0], &gamePlayersArray[0], sizeof(CPlayerInfoGta));

            // Re-apply our patch
            WritePointer(playersPointerAddr, m_Players);
            CWORLDP_LOGI("ForceReapplyPatch: Patch re-applied!");
        }
        else if (gamePtr == nullptr)
        {
            // Game set it to NULL, just apply our array
            CWORLDP_LOGW("ForceReapplyPatch: Game set Players to NULL, applying our array");
            WritePointer(playersPointerAddr, m_Players);
        }
#endif
    }

    /**
     * Find a free player slot (starting from slot 2)
     */
    int FindFreeSlot() const;

    /**
     * Mark slot as used/free
     */
    void SetSlotUsed(int slot, bool used);

    /**
     * Check if slot is in use
     */
    bool IsSlotUsed(int slot) const;

    /**
     * Debug: Dump player array info to logs
     * Call this after SetupPlayerPed to verify slots are unique
     */
    void DebugDumpSlots(int maxSlot = 5) const
    {
        CWORLDP_LOGI("=== CWorld::Players Slot Debug ===");
        for (int i = 0; i <= maxSlot && i < MAX_PLAYERS; ++i)
        {
            CPlayerPedGta* ped = m_Players[i].m_pPed;
            CWORLDP_LOGI("  Slot %d: m_pPed = 0x%lx", i, (unsigned long)ped);
        }

        // Check for aliasing (same ped in multiple slots)
        for (int i = 0; i <= maxSlot && i < MAX_PLAYERS; ++i)
        {
            for (int j = i + 1; j <= maxSlot && j < MAX_PLAYERS; ++j)
            {
                if (m_Players[i].m_pPed != nullptr &&
                    m_Players[i].m_pPed == m_Players[j].m_pPed)
                {
                    CWORLDP_LOGE("  BUG: Slot %d and %d have SAME ped 0x%lx!",
                                 i, j, (unsigned long)m_Players[i].m_pPed);
                }
            }
        }
        CWORLDP_LOGI("=================================");
    }

    /**
     * Copy local player data from original array to our array
     * Call this after the game has initialized the local player
     */
    bool CopyLocalPlayerData();

    /**
     * Disable the game restart function to prevent our patch from being overwritten.
     * This writes a RET instruction at the start of CGame::InitialiseWhenRestarting.
     * In multiplayer, respawning is controlled by the server, not the game.
     * Must be called after Initialize().
     */
    bool DisableGameRestart();

    /**
     * Install hooks to re-apply patch during game restarts.
     * This hooks CPools::Load which is called during CGame::InitialiseWhenRestarting.
     * Must be called after Initialize().
     * NOTE: This approach crashes due to PC-relative instructions. Use DisableGameRestart() instead.
     */
    bool InstallRestartHooks();

private:
    CWorldPlayers();
    ~CWorldPlayers() = default;

    /**
     * Make a memory region writable for patching
     */
    bool MakeWritable(uintptr_t address, size_t size);

    /**
     * Write a pointer value to memory
     */
    bool WritePointer(uintptr_t address, void* value);

    //=========================================================================
    // Member Variables
    //=========================================================================

    // Our extended Players array (replaces game's limited array)
    CPlayerInfoGta m_Players[MAX_PLAYERS];

    // Our PlayerInFocus variable (which player camera follows)
    int m_PlayerInFocus;

    // Slot usage tracking
    bool m_usedSlots[MAX_PLAYERS];

    // Game base address
    uintptr_t m_gameBase;

    // Whether patch has been applied
    bool m_patched;

    // Original Players pointer (saved for restoration)
    uintptr_t m_originalPlayersPtr;
    int m_originalPlayerInFocus;
};

//=============================================================================
// Implementation
//=============================================================================

inline CWorldPlayers::CWorldPlayers()
    : m_gameBase(0)
    , m_patched(false)
    , m_PlayerInFocus(0)
    , m_originalPlayersPtr(0)
    , m_originalPlayerInFocus(0)
{
    // Zero-initialize players array
    memset(m_Players, 0, sizeof(m_Players));

    // Zero-initialize slot tracking
    memset(m_usedSlots, 0, sizeof(m_usedSlots));

    // Slots 0-1 are reserved (local player)
    m_usedSlots[0] = true;
    m_usedSlots[1] = true;
}

inline bool CWorldPlayers::MakeWritable(uintptr_t address, size_t size)
{
#ifdef __ANDROID__
    // Align to page boundary
    uintptr_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t alignedAddress = address & ~(pageSize - 1);
    size_t totalSize = size + (address - alignedAddress);

    // Make region writable
    if (mprotect(reinterpret_cast<void*>(alignedAddress), totalSize,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
    {
        CWORLDP_LOGE("mprotect failed for address 0x%lx", (unsigned long)address);
        return false;
    }

    return true;
#else
    return false;
#endif
}

inline bool CWorldPlayers::WritePointer(uintptr_t address, void* value)
{
    if (!MakeWritable(address, sizeof(void*)))
        return false;

    // Write the pointer
    *reinterpret_cast<void**>(address) = value;

    // Flush cache (important on ARM)
#ifdef __ANDROID__
    __builtin___clear_cache(
        reinterpret_cast<char*>(address),
        reinterpret_cast<char*>(address + sizeof(void*))
    );
#endif

    return true;
}

inline bool CWorldPlayers::Initialize(uintptr_t gameBase)
{
    if (m_patched)
    {
        CWORLDP_LOGI("Already patched");
        return true;
    }

    if (gameBase == 0)
    {
        CWORLDP_LOGE("Invalid game base address");
        return false;
    }

    m_gameBase = gameBase;

#if defined(__aarch64__)
    // ARM64 addresses from SA-MP reference
    // CWorld::Players pointer location: 0x84E7A8
    // CWorld::PlayerInFocus location: 0x8516D8

    uintptr_t playersPointerAddr = gameBase + 0x84E7A8;
    uintptr_t playerInFocusAddr = gameBase + 0x8516D8;

    CWORLDP_LOGI("=== CWorldPlayers::Initialize ===");
    CWORLDP_LOGI("  Game base: 0x%lx", (unsigned long)gameBase);
    CWORLDP_LOGI("  Players pointer addr: 0x%lx", (unsigned long)playersPointerAddr);
    CWORLDP_LOGI("  PlayerInFocus addr: 0x%lx", (unsigned long)playerInFocusAddr);
    CWORLDP_LOGI("  Our Players array: 0x%lx", (unsigned long)m_Players);
    CWORLDP_LOGI("  Our PlayerInFocus: 0x%lx", (unsigned long)&m_PlayerInFocus);
    CWORLDP_LOGI("  sizeof(CPlayerInfoGta): 0x%lx (%zu bytes)",
                 (unsigned long)sizeof(CPlayerInfoGta), sizeof(CPlayerInfoGta));

    // Save original values
    m_originalPlayersPtr = *reinterpret_cast<uintptr_t*>(playersPointerAddr);
    m_originalPlayerInFocus = *reinterpret_cast<int*>(playerInFocusAddr);

    CWORLDP_LOGI("  Original Players ptr: 0x%lx", (unsigned long)m_originalPlayersPtr);
    CWORLDP_LOGI("  Original PlayerInFocus: %d", m_originalPlayerInFocus);

    // Copy local player data from original array if it exists
    if (m_originalPlayersPtr != 0)
    {
        CPlayerInfoGta* originalPlayers = reinterpret_cast<CPlayerInfoGta*>(m_originalPlayersPtr);

        // Copy slot 0 (local player)
        memcpy(&m_Players[0], &originalPlayers[0], sizeof(CPlayerInfoGta));
        CWORLDP_LOGI("  Copied local player data from original array");

        if (m_Players[0].m_pPed != nullptr)
        {
            CWORLDP_LOGI("  Local player ped: 0x%lx", (unsigned long)m_Players[0].m_pPed);
        }
    }

    // Patch 1: Replace CWorld::Players pointer
    CWORLDP_LOGI("  Patching CWorld::Players...");
    if (!WritePointer(playersPointerAddr, m_Players))
    {
        CWORLDP_LOGE("Failed to patch CWorld::Players");
        return false;
    }
    CWORLDP_LOGI("  CWorld::Players patched successfully");

    // NOTE: We do NOT patch CWorld::PlayerInFocus
    // PlayerInFocus is an integer (0 = local player), not a pointer.
    // The game uses it to determine which player the camera follows.
    // Patching it would break the local player lookup.
    CWORLDP_LOGI("  CWorld::PlayerInFocus NOT patched (keeping original value %d)", m_originalPlayerInFocus);

    m_patched = true;
    CWORLDP_LOGI("=== CWorldPlayers patch applied successfully ===");

    return true;

#else
    // ARM32 addresses
    uintptr_t playersPointerAddr = gameBase + 0x6783C0;
    uintptr_t playerInFocusAddr = gameBase + 0x679B5C;

    CWORLDP_LOGI("ARM32 CWorldPlayers patch (not yet implemented)");
    CWORLDP_LOGW("ARM32 support coming soon");
    return false;
#endif
}

inline bool CWorldPlayers::CopyLocalPlayerData()
{
    if (!m_patched || m_originalPlayersPtr == 0)
        return false;

    CPlayerInfoGta* originalPlayers = reinterpret_cast<CPlayerInfoGta*>(m_originalPlayersPtr);

    // Copy slot 0 (local player)
    memcpy(&m_Players[0], &originalPlayers[0], sizeof(CPlayerInfoGta));

    CWORLDP_LOGI("Copied local player data");
    if (m_Players[0].m_pPed != nullptr)
    {
        CWORLDP_LOGI("  Local player ped: 0x%lx", (unsigned long)m_Players[0].m_pPed);
    }

    return true;
}

inline int CWorldPlayers::FindFreeSlot() const
{
    // Start from slot 2 (0 and 1 are reserved for local player)
    for (int i = 2; i < MAX_PLAYERS; ++i)
    {
        if (!m_usedSlots[i])
            return i;
    }
    return -1;  // No free slot
}

inline void CWorldPlayers::SetSlotUsed(int slot, bool used)
{
    if (slot >= 0 && slot < MAX_PLAYERS)
    {
        m_usedSlots[slot] = used;
    }
}

inline bool CWorldPlayers::IsSlotUsed(int slot) const
{
    if (slot >= 0 && slot < MAX_PLAYERS)
        return m_usedSlots[slot];
    return true;  // Invalid slots considered used
}

//=============================================================================
// Disable Game Restart - Simple RET approach (SA-MP style)
//=============================================================================

inline bool CWorldPlayers::DisableGameRestart()
{
#if defined(__aarch64__)
    if (m_gameBase == 0)
    {
        CWORLDP_LOGE("DisableGameRestart: game base not set");
        return false;
    }

    // CGame::InitialiseWhenRestarting is at offset 0x4d6108
    // This function is called during restart and overwrites our CWorld::Players patch
    // By writing a RET instruction at its start, we make it return immediately
    constexpr uint32_t INITIALISE_WHEN_RESTARTING_OFFSET = 0x4d6108;

    uintptr_t funcAddr = m_gameBase + INITIALISE_WHEN_RESTARTING_OFFSET;

    CWORLDP_LOGI("=== DisableGameRestart ===");
    CWORLDP_LOGI("  CGame::InitialiseWhenRestarting at 0x%lx", (unsigned long)funcAddr);

    // Make the memory writable
    if (!MakeWritable(funcAddr, 4))
    {
        CWORLDP_LOGE("  Failed to make function writable!");
        return false;
    }

    // Write ARM64 RET instruction: 0xD65F03C0
    // This is the same as SA-MP's CHook::RET approach
    uint32_t retInstruction = 0xD65F03C0;
    memcpy((void*)funcAddr, &retInstruction, 4);

    // Flush instruction cache
    __builtin___clear_cache((char*)funcAddr, (char*)funcAddr + 4);

    CWORLDP_LOGI("  Wrote RET instruction - function disabled!");
    CWORLDP_LOGI("  Game restarts will no longer overwrite our patch");

    return true;
#else
    CWORLDP_LOGW("DisableGameRestart: ARM32 not implemented");
    return false;
#endif
}

//=============================================================================
// Hook Implementation for Game Restarts (Alternative - crashes due to PC-relative)
//=============================================================================

// Global pointer for hook callback
inline CWorldPlayers* g_worldPlayersHookInstance = nullptr;

// Original CPools::Load function pointer
inline void (*g_originalCPoolsLoad)() = nullptr;

// Hook function for CPools::Load - re-applies patch before original runs
inline void Hook_CPools_Load()
{
    CWORLDP_LOGI(">>> Hook_CPools_Load called! Re-applying CWorld::Players patch...");

    // Re-apply our CWorld::Players patch BEFORE the original function runs
    if (g_worldPlayersHookInstance)
    {
        g_worldPlayersHookInstance->ForceReapplyPatch();
    }

    // Call the original function
    if (g_originalCPoolsLoad)
    {
        CWORLDP_LOGI(">>> Calling original CPools::Load...");
        g_originalCPoolsLoad();
        CWORLDP_LOGI(">>> Original CPools::Load returned");
    }
}

inline bool CWorldPlayers::InstallRestartHooks()
{
#if defined(__aarch64__)
    if (m_gameBase == 0)
    {
        CWORLDP_LOGE("InstallRestartHooks: game base not set");
        return false;
    }

    // Set global instance pointer for hook callback
    g_worldPlayersHookInstance = this;

    // CPools::Load is at offset 0x57bd5c
    // We need to hook it to re-apply our patch during game restarts
    constexpr uint32_t CPOOLS_LOAD_OFFSET = 0x57bd5c;

    uintptr_t hookAddr = m_gameBase + CPOOLS_LOAD_OFFSET;

    CWORLDP_LOGI("Installing CPools::Load hook at 0x%lx", (unsigned long)hookAddr);

    // Allocate trampoline memory for calling original function
    void* trampolineMem = mmap(nullptr, 4096,
                               PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (trampolineMem == MAP_FAILED)
    {
        CWORLDP_LOGE("Failed to allocate trampoline memory");
        return false;
    }

    uint8_t* tramp = (uint8_t*)trampolineMem;

    // Copy first 16 bytes of original function (will be overwritten by hook)
    memcpy(tramp, (void*)hookAddr, 16);

    // Add jump back to original function + 16
    // LDR X16, #8; BR X16; <address>
    uint32_t ldr = 0x58000050;  // LDR X16, [PC, #8]
    uint32_t br = 0xD61F0200;   // BR X16
    uintptr_t returnAddr = hookAddr + 16;

    memcpy(tramp + 16, &ldr, 4);
    memcpy(tramp + 20, &br, 4);
    memcpy(tramp + 24, &returnAddr, 8);

    // Clear cache for trampoline
    __builtin___clear_cache((char*)trampolineMem, (char*)trampolineMem + 32);

    // Store original function pointer (trampoline)
    g_originalCPoolsLoad = (void(*)())trampolineMem;

    // Now install the hook at original location
    // Write: LDR X16, #8; BR X16; <hook address>
    if (!MakeWritable(hookAddr, 16))
    {
        CWORLDP_LOGE("Failed to make hook address writable");
        return false;
    }

    uintptr_t hookFuncAddr = (uintptr_t)&Hook_CPools_Load;
    memcpy((void*)hookAddr, &ldr, 4);
    memcpy((void*)(hookAddr + 4), &br, 4);
    memcpy((void*)(hookAddr + 8), &hookFuncAddr, 8);

    __builtin___clear_cache((char*)hookAddr, (char*)hookAddr + 16);

    CWORLDP_LOGI("CPools::Load hook installed successfully!");
    CWORLDP_LOGI("  Original (trampoline): 0x%lx", (unsigned long)trampolineMem);
    CWORLDP_LOGI("  Hook function: 0x%lx", (unsigned long)hookFuncAddr);

    return true;
#else
    CWORLDP_LOGW("InstallRestartHooks: ARM32 not implemented");
    return false;
#endif
}

} // namespace MTA::Android::Multiplayer

#endif // CWORLD_PLAYERS_H
