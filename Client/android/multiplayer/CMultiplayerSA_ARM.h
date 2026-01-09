/*
 * MTA:SA Android - ARM Multiplayer Hooks
 *
 * This file provides ARM equivalents for the hooks in multiplayer_sa/CMultiplayerSA.cpp
 * Uses addresses from ARMAddressMap.h and hook functions from ARMHookInstaller.h
 *
 * Porting Strategy:
 *   1. x86 naked functions with inline ASM → ARM functions using register conventions
 *   2. HOOKPOS_xxx defines → ARM::ARM32::xxx constants
 *   3. HookInstall() calls → ARMHookInstall() calls
 *   4. Calling convention: x86 cdecl/thiscall → ARM AAPCS
 */

#ifndef CMULTIPLAYERSA_ARM_H
#define CMULTIPLAYERSA_ARM_H

#include "../hooks/ARMHookInstaller.h"
#include "../signatures/ARMAddressMap.h"

namespace MTA::Android::Multiplayer
{
    // Use explicit namespace aliases to avoid ambiguity between
    // Hooks::ARM32/ARM64 and ARM::ARM32/ARM64
    namespace HookUtils = MTA::Android::Hooks;
    namespace Addresses = MTA::Android::ARM;

    //=========================================================================
    // ARM Hook Address Mapping
    // Maps x86 HOOKPOS_xxx to ARM addresses
    //=========================================================================

    namespace HookAddresses
    {
        #if MTA_ARM32
            // Core hooks
            constexpr uint32_t CGame_Process            = Addresses::ARM32::PLT_CCamera_Process;  // Main game loop
            constexpr uint32_t CStreaming_RequestModel  = Addresses::ARM32::CStreaming_RequestModel;

            // Player/Ped hooks
            constexpr uint32_t CPlayerPed_ProcessControl = Addresses::ARM32::CPlayerPed_ProcessControl;
            constexpr uint32_t CPlayerPed_SetupPlayerPed = Addresses::ARM32::CPlayerPed_SetupPlayerPed;
            constexpr uint32_t CPed_ProcessControl      = Addresses::ARM32::CPed_ProcessControl;
            constexpr uint32_t CPed_SetModelIndex       = Addresses::ARM32::CPed_SetModelIndex;
            constexpr uint32_t CPed_GiveWeapon          = Addresses::ARM32::CPed_GiveWeapon;
            constexpr uint32_t CPed_ClearWeapons        = Addresses::ARM32::CPed_ClearWeapons;
            constexpr uint32_t CPed_GetWeaponSkill      = Addresses::ARM32::CPed_GetWeaponSkill;

            // Vehicle hooks
            constexpr uint32_t CVehicle_SetupRender     = Addresses::ARM32::CVehicle_SetupRender;
            constexpr uint32_t CVehicle_ResetAfterRender = Addresses::ARM32::CVehicle_ResetAfterRender;
            constexpr uint32_t CVehicle_ProcessWheel    = Addresses::ARM32::CVehicle_ProcessWheel;
            constexpr uint32_t CVehicle_AddPassenger    = Addresses::ARM32::CVehicle_AddPassenger;
            constexpr uint32_t CVehicle_RemovePassenger = Addresses::ARM32::CVehicle_RemovePassenger;
            constexpr uint32_t CVehicle_InflictDamage   = Addresses::ARM32::CVehicle_InflictDamage;

            constexpr uint32_t CAutomobile_ProcessControl = Addresses::ARM32::CAutomobile_ProcessControl;
            constexpr uint32_t CAutomobile_VehicleDamage = Addresses::ARM32::CAutomobile_VehicleDamage;
            constexpr uint32_t CAutomobile_Fix          = Addresses::ARM32::CAutomobile_Fix;

            // Camera hooks
            constexpr uint32_t CCamera_CamControl       = Addresses::ARM32::CCamera_CamControl;
            constexpr uint32_t CCamera_ProcessFade      = Addresses::ARM32::CCamera_ProcessFade;
            constexpr uint32_t CCamera_TakeControl      = Addresses::ARM32::CCamera_TakeControl;

            // World hooks
            constexpr uint32_t CWorld_TriggerExplosion  = Addresses::ARM32::CWorld_TriggerExplosion;
            constexpr uint32_t CWorld_SetCarsOnFire     = Addresses::ARM32::CWorld_SetCarsOnFire;
            constexpr uint32_t CWorld_SetPedsOnFire     = Addresses::ARM32::CWorld_SetPedsOnFire;

            // Weapon hooks
            constexpr uint32_t CWeapon_FireInstantHit   = Addresses::ARM32::CWeapon_FireInstantHit;
            constexpr uint32_t CWeapon_FireProjectile   = Addresses::ARM32::CWeapon_FireProjectile;
            constexpr uint32_t CWeapon_DoBulletImpact   = Addresses::ARM32::CWeapon_DoBulletImpact;

            // Entity hooks
            constexpr uint32_t CEntity_SetModelIndex    = Addresses::ARM32::CEntity_SetModelIndex;
            constexpr uint32_t CEntity_CreateRwObject   = Addresses::ARM32::CEntity_CreateRwObject;
            constexpr uint32_t CEntity_DeleteRwObject   = Addresses::ARM32::CEntity_DeleteRwObject;

            // Explosion hooks
            constexpr uint32_t CExplosion_AddExplosion  = Addresses::ARM32::CExplosion_AddExplosion;
            constexpr uint32_t CExplosion_Update        = Addresses::ARM32::CExplosion_Update;

            // Streaming hooks
            constexpr uint32_t CStreaming_RemoveModel   = Addresses::ARM32::CStreaming_RemoveModel;
            constexpr uint32_t CStreaming_FlushChannels = Addresses::ARM32::CStreaming_FlushChannels;

            // PLT hooks (for function interception)
            constexpr uint32_t PLT_CPed_UpdatePosition  = Addresses::ARM32::PLT_CPed_UpdatePosition;
            constexpr uint32_t PLT_CPed_GetWeaponSkill  = Addresses::ARM32::PLT_CPed_GetWeaponSkill;

            // Global addresses
            constexpr uint32_t g_CameraInstance         = Addresses::ARM32::g_CCamera;
            constexpr uint32_t g_PlayerInFocus          = Addresses::ARM32::g_PlayerInFocus;

        #else // ARM64
            // Core hooks
            constexpr uint32_t CPlayerPed_SetupPlayerPed = Addresses::ARM64::CPlayerPed_SetupPlayerPed;
            constexpr uint32_t CPlayerPed_DeactivatePlayerPed = Addresses::ARM64::CPlayerPed_DeactivatePlayerPed;
            constexpr uint32_t CPlayerPed_ReactivatePlayerPed = Addresses::ARM64::CPlayerPed_ReactivatePlayerPed;

            // Vehicle hooks
            constexpr uint32_t CAutomobile_Fix          = Addresses::ARM64::CAutomobile_Fix;

            // Global addresses
            constexpr uint32_t g_CameraInstance         = Addresses::ARM64::g_CCamera;
            constexpr uint32_t g_PlayerInFocus          = Addresses::ARM64::g_PlayerInFocus;
        #endif
    }

    //=========================================================================
    // Original Function Pointers (trampolines)
    //=========================================================================

    namespace OriginalFunctions
    {
        // Player/Ped
        inline uintptr_t CPlayerPed_ProcessControl = 0;
        inline uintptr_t CPed_ProcessControl = 0;
        inline uintptr_t CPed_SetModelIndex = 0;
        inline uintptr_t CPed_GiveWeapon = 0;

        // Vehicle
        inline uintptr_t CVehicle_SetupRender = 0;
        inline uintptr_t CVehicle_ProcessWheel = 0;
        inline uintptr_t CAutomobile_ProcessControl = 0;
        inline uintptr_t CAutomobile_VehicleDamage = 0;

        // Camera
        inline uintptr_t CCamera_CamControl = 0;
        inline uintptr_t CCamera_ProcessFade = 0;

        // Weapon
        inline uintptr_t CWeapon_FireInstantHit = 0;
        inline uintptr_t CWeapon_FireProjectile = 0;

        // Explosion
        inline uintptr_t CExplosion_AddExplosion = 0;

        // Entity
        inline uintptr_t CEntity_SetModelIndex = 0;
    }

    //=========================================================================
    // Hook Function Declarations
    // These match the signatures from CMultiplayerSA.cpp
    //=========================================================================

    // Forward declarations of MTA hook handlers
    // These will be implemented in CMultiplayerSA_ARM.cpp

    // Player hooks
    extern "C" void HOOK_CPlayerPed_ProcessControl(void* pThis);
    extern "C" void HOOK_CPed_ProcessControl(void* pThis);
    extern "C" void HOOK_CPed_SetModelIndex(void* pThis, uint32_t modelIndex);

    // Vehicle hooks
    extern "C" void HOOK_CVehicle_SetupRender(void* pThis);
    extern "C" void HOOK_CVehicle_ResetAfterRender(void* pThis);
    extern "C" void HOOK_CAutomobile_ProcessControl(void* pThis);
    extern "C" bool HOOK_CAutomobile_VehicleDamage(void* pThis, float damage, uint16_t bodyPart,
                                                    void* damager, void* weaponType);

    // Camera hooks
    extern "C" void HOOK_CCamera_Process(void* pThis);

    // Weapon hooks
    extern "C" bool HOOK_CWeapon_FireInstantHit(void* pThis, void* owner, void* target,
                                                 void* origin, void* direction);

    // Explosion hooks
    extern "C" void* HOOK_CExplosion_AddExplosion(void* owner, void* creator, int type,
                                                   void* position, uint32_t time,
                                                   bool makeSound, float camShake);

    //=========================================================================
    // Hook Installation
    //=========================================================================

    /**
     * Install all core multiplayer hooks
     * Called during MTA Android initialization
     */
    inline bool InstallCoreHooks()
    {
        LOGI("Installing MTA core hooks...");

        bool success = true;

        #if MTA_ARM32
            // Player/Ped hooks
            success &= HookUtils::ARMHookInstallWithOriginal(
                HookAddresses::CPlayerPed_ProcessControl,
                (uintptr_t)HOOK_CPlayerPed_ProcessControl,
                &OriginalFunctions::CPlayerPed_ProcessControl,
                8  // Prologue size
            );

            success &= HookUtils::ARMHookInstallWithOriginal(
                HookAddresses::CPed_ProcessControl,
                (uintptr_t)HOOK_CPed_ProcessControl,
                &OriginalFunctions::CPed_ProcessControl,
                8
            );

            // Vehicle hooks
            success &= HookUtils::ARMHookInstallWithOriginal(
                HookAddresses::CVehicle_SetupRender,
                (uintptr_t)HOOK_CVehicle_SetupRender,
                &OriginalFunctions::CVehicle_SetupRender,
                8
            );

            success &= HookUtils::ARMHookInstallWithOriginal(
                HookAddresses::CAutomobile_ProcessControl,
                (uintptr_t)HOOK_CAutomobile_ProcessControl,
                &OriginalFunctions::CAutomobile_ProcessControl,
                8
            );

            // Camera hook via PLT
            success &= HookUtils::ARMHookPLT(
                Addresses::ARM32::PLT_CCamera_Process,
                (uintptr_t)HOOK_CCamera_Process,
                &OriginalFunctions::CCamera_CamControl
            );

            // Weapon hooks
            success &= HookUtils::ARMHookInstallWithOriginal(
                HookAddresses::CWeapon_FireInstantHit,
                (uintptr_t)HOOK_CWeapon_FireInstantHit,
                &OriginalFunctions::CWeapon_FireInstantHit,
                8
            );

            // Explosion hook
            success &= HookUtils::ARMHookInstallWithOriginal(
                HookAddresses::CExplosion_AddExplosion,
                (uintptr_t)HOOK_CExplosion_AddExplosion,
                &OriginalFunctions::CExplosion_AddExplosion,
                8
            );
        #endif

        LOGI("Core hooks installed: %s", success ? "SUCCESS" : "FAILED");
        return success;
    }

    /**
     * Install vehicle-specific hooks
     */
    inline bool InstallVehicleHooks()
    {
        LOGI("Installing vehicle hooks...");
        bool success = true;

        #if MTA_ARM32
            // Vehicle damage hook
            success &= HookUtils::ARMHookInstallWithOriginal(
                HookAddresses::CAutomobile_VehicleDamage,
                (uintptr_t)HOOK_CAutomobile_VehicleDamage,
                &OriginalFunctions::CAutomobile_VehicleDamage,
                8
            );

            // Vehicle render hooks (for syncing visual state)
            success &= HookUtils::ARMHookInstall(
                HookAddresses::CVehicle_ResetAfterRender,
                (uintptr_t)HOOK_CVehicle_ResetAfterRender,
                0
            );
        #endif

        LOGI("Vehicle hooks installed: %s", success ? "SUCCESS" : "FAILED");
        return success;
    }

    /**
     * Install ped/player hooks
     */
    inline bool InstallPedHooks()
    {
        LOGI("Installing ped hooks...");
        bool success = true;

        #if MTA_ARM32
            success &= HookUtils::ARMHookInstallWithOriginal(
                HookAddresses::CPed_SetModelIndex,
                (uintptr_t)HOOK_CPed_SetModelIndex,
                &OriginalFunctions::CPed_SetModelIndex,
                8
            );

            // PLT hook for ped updates
            success &= HookUtils::ARMHookPLT(
                HookAddresses::PLT_CPed_UpdatePosition,
                (uintptr_t)HOOK_CPed_ProcessControl,
                nullptr
            );
        #endif

        LOGI("Ped hooks installed: %s", success ? "SUCCESS" : "FAILED");
        return success;
    }

    /**
     * Install all multiplayer hooks
     * Main entry point called from MTAAndroidMain.cpp
     */
    inline bool InstallAllHooks()
    {
        if (!HookUtils::Initialize())
        {
            LOGE("Failed to initialize hook system");
            return false;
        }

        bool success = true;
        success &= InstallCoreHooks();
        success &= InstallVehicleHooks();
        success &= InstallPedHooks();

        if (success)
        {
            LOGI("All multiplayer hooks installed successfully!");
        }
        else
        {
            LOGE("Some hooks failed to install");
        }

        return success;
    }

    //=========================================================================
    // Calling Original Functions
    // Helper macros/functions to call original game functions
    //=========================================================================

    // Type definitions for original function calls
    typedef void (*pfn_ProcessControl)(void*);
    typedef void (*pfn_SetModelIndex)(void*, uint32_t);
    typedef void (*pfn_VehicleDamage)(void*, float, uint16_t, void*, void*);
    typedef void* (*pfn_AddExplosion)(void*, void*, int, void*, uint32_t, bool, float);

    inline void CallOriginal_CPlayerPed_ProcessControl(void* pThis)
    {
        if (OriginalFunctions::CPlayerPed_ProcessControl)
        {
            ((pfn_ProcessControl)OriginalFunctions::CPlayerPed_ProcessControl)(pThis);
        }
    }

    inline void CallOriginal_CPed_ProcessControl(void* pThis)
    {
        if (OriginalFunctions::CPed_ProcessControl)
        {
            ((pfn_ProcessControl)OriginalFunctions::CPed_ProcessControl)(pThis);
        }
    }

    inline void CallOriginal_CAutomobile_ProcessControl(void* pThis)
    {
        if (OriginalFunctions::CAutomobile_ProcessControl)
        {
            ((pfn_ProcessControl)OriginalFunctions::CAutomobile_ProcessControl)(pThis);
        }
    }

    inline void CallOriginal_CPed_SetModelIndex(void* pThis, uint32_t modelIndex)
    {
        if (OriginalFunctions::CPed_SetModelIndex)
        {
            ((pfn_SetModelIndex)OriginalFunctions::CPed_SetModelIndex)(pThis, modelIndex);
        }
    }

} // namespace MTA::Android::Multiplayer

#endif // CMULTIPLAYERSA_ARM_H
