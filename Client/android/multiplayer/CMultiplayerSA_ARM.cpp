/*
 * MTA:SA Android - ARM Multiplayer Hook Implementations
 *
 * This file implements the hook handlers declared in CMultiplayerSA_ARM.h
 * These are ARM equivalents of the x86 hooks in multiplayer_sa/CMultiplayerSA.cpp
 */

#include "CMultiplayerSA_ARM.h"
#include <cstdio>

namespace MTA::Android::Multiplayer
{
    //=========================================================================
    // MTA Callback System
    // These callbacks are set by the MTA core to receive game events
    //=========================================================================

    namespace Callbacks
    {
        // Pre/Post process callbacks
        static void (*pfnPreProcessControl)(void* pPed) = nullptr;
        static void (*pfnPostProcessControl)(void* pPed) = nullptr;

        // Vehicle callbacks
        static void (*pfnPreVehicleProcess)(void* pVehicle) = nullptr;
        static void (*pfnPostVehicleProcess)(void* pVehicle) = nullptr;
        static bool (*pfnVehicleDamageHandler)(void* pVehicle, float damage,
                                               void* damager, int weaponType) = nullptr;

        // Weapon callbacks
        static bool (*pfnWeaponFireHandler)(void* pWeapon, void* pOwner,
                                            void* pTarget, void* pOrigin) = nullptr;

        // Explosion callback
        static bool (*pfnExplosionHandler)(void* pOwner, int type, void* pPosition) = nullptr;

        // Entity callbacks
        static void (*pfnEntityModelChange)(void* pEntity, uint32_t newModel) = nullptr;
    }

    //=========================================================================
    // Callback Registration (called by MTA core)
    //=========================================================================

    void SetPreProcessCallback(void (*callback)(void*))
    {
        Callbacks::pfnPreProcessControl = callback;
    }

    void SetPostProcessCallback(void (*callback)(void*))
    {
        Callbacks::pfnPostProcessControl = callback;
    }

    void SetVehicleDamageCallback(bool (*callback)(void*, float, void*, int))
    {
        Callbacks::pfnVehicleDamageHandler = callback;
    }

    void SetWeaponFireCallback(bool (*callback)(void*, void*, void*, void*))
    {
        Callbacks::pfnWeaponFireHandler = callback;
    }

    void SetExplosionCallback(bool (*callback)(void*, int, void*))
    {
        Callbacks::pfnExplosionHandler = callback;
    }

    void SetEntityModelChangeCallback(void (*callback)(void*, uint32_t))
    {
        Callbacks::pfnEntityModelChange = callback;
    }

} // namespace MTA::Android::Multiplayer

//=============================================================================
// Hook Implementations
// These are the actual hook functions called by the game
//=============================================================================

extern "C" {

using namespace MTA::Android::Multiplayer;

/**
 * CPlayerPed::ProcessControl hook
 * Called every frame for the local player
 * Used for syncing player state to server
 */
void HOOK_CPlayerPed_ProcessControl(void* pThis)
{
    // Pre-process callback (for sync preparation)
    if (Callbacks::pfnPreProcessControl)
    {
        Callbacks::pfnPreProcessControl(pThis);
    }

    // Call original function
    CallOriginal_CPlayerPed_ProcessControl(pThis);

    // Post-process callback (for state capture)
    if (Callbacks::pfnPostProcessControl)
    {
        Callbacks::pfnPostProcessControl(pThis);
    }
}

/**
 * CPed::ProcessControl hook
 * Called for all peds (NPCs and remote players)
 */
void HOOK_CPed_ProcessControl(void* pThis)
{
    // Pre-process
    if (Callbacks::pfnPreProcessControl)
    {
        Callbacks::pfnPreProcessControl(pThis);
    }

    // Call original
    CallOriginal_CPed_ProcessControl(pThis);

    // Post-process
    if (Callbacks::pfnPostProcessControl)
    {
        Callbacks::pfnPostProcessControl(pThis);
    }
}

/**
 * CPed::SetModelIndex hook
 * Called when a ped's model changes (skin change)
 */
void HOOK_CPed_SetModelIndex(void* pThis, uint32_t modelIndex)
{
    // Notify MTA of model change
    if (Callbacks::pfnEntityModelChange)
    {
        Callbacks::pfnEntityModelChange(pThis, modelIndex);
    }

    // Call original
    CallOriginal_CPed_SetModelIndex(pThis, modelIndex);
}

/**
 * CVehicle::SetupRender hook
 * Called before vehicle rendering
 */
void HOOK_CVehicle_SetupRender(void* pThis)
{
    // Pre-render callback for syncing visual state
    if (Callbacks::pfnPreVehicleProcess)
    {
        Callbacks::pfnPreVehicleProcess(pThis);
    }

    // Call original via trampoline
    if (OriginalFunctions::CVehicle_SetupRender)
    {
        ((void(*)(void*))OriginalFunctions::CVehicle_SetupRender)(pThis);
    }
}

/**
 * CVehicle::ResetAfterRender hook
 */
void HOOK_CVehicle_ResetAfterRender(void* pThis)
{
    // Post-render callback
    if (Callbacks::pfnPostVehicleProcess)
    {
        Callbacks::pfnPostVehicleProcess(pThis);
    }
}

/**
 * CAutomobile::ProcessControl hook
 * Called every frame for vehicles
 */
void HOOK_CAutomobile_ProcessControl(void* pThis)
{
    // Pre-process for vehicle sync
    if (Callbacks::pfnPreVehicleProcess)
    {
        Callbacks::pfnPreVehicleProcess(pThis);
    }

    // Call original
    CallOriginal_CAutomobile_ProcessControl(pThis);

    // Post-process
    if (Callbacks::pfnPostVehicleProcess)
    {
        Callbacks::pfnPostVehicleProcess(pThis);
    }
}

/**
 * CAutomobile::VehicleDamage hook
 * Called when a vehicle takes damage
 * Return false to block damage
 */
bool HOOK_CAutomobile_VehicleDamage(void* pThis, float damage, uint16_t bodyPart,
                                     void* damager, void* weaponType)
{
    // Check if MTA wants to block this damage
    if (Callbacks::pfnVehicleDamageHandler)
    {
        if (!Callbacks::pfnVehicleDamageHandler(pThis, damage, damager, (int)(uintptr_t)weaponType))
        {
            return false;  // Block damage
        }
    }

    // Call original
    if (OriginalFunctions::CAutomobile_VehicleDamage)
    {
        typedef bool (*VehicleDamageFunc)(void*, float, uint16_t, void*, void*);
        return ((VehicleDamageFunc)OriginalFunctions::CAutomobile_VehicleDamage)(
            pThis, damage, bodyPart, damager, weaponType);
    }

    return true;
}

/**
 * CCamera::Process hook
 * Used for camera sync
 */
void HOOK_CCamera_Process(void* pThis)
{
    // Call original
    if (OriginalFunctions::CCamera_CamControl)
    {
        ((void(*)(void*))OriginalFunctions::CCamera_CamControl)(pThis);
    }

    // MTA may modify camera state here for spectating, etc.
}

/**
 * CWeapon::FireInstantHit hook
 * Called when an instant-hit weapon (bullet) is fired
 * Used for weapon sync
 */
bool HOOK_CWeapon_FireInstantHit(void* pThis, void* owner, void* target,
                                  void* origin, void* direction)
{
    // Notify MTA of weapon fire
    if (Callbacks::pfnWeaponFireHandler)
    {
        if (!Callbacks::pfnWeaponFireHandler(pThis, owner, target, origin))
        {
            return false;  // Block fire
        }
    }

    // Call original
    if (OriginalFunctions::CWeapon_FireInstantHit)
    {
        typedef bool (*FireFunc)(void*, void*, void*, void*, void*);
        return ((FireFunc)OriginalFunctions::CWeapon_FireInstantHit)(
            pThis, owner, target, origin, direction);
    }

    return true;
}

/**
 * CExplosion::AddExplosion hook
 * Called when an explosion is created
 * Used for explosion sync
 */
void* HOOK_CExplosion_AddExplosion(void* owner, void* creator, int type,
                                    void* position, uint32_t time,
                                    bool makeSound, float camShake)
{
    // Notify MTA of explosion
    if (Callbacks::pfnExplosionHandler)
    {
        if (!Callbacks::pfnExplosionHandler(owner, type, position))
        {
            return nullptr;  // Block explosion
        }
    }

    // Call original
    if (OriginalFunctions::CExplosion_AddExplosion)
    {
        typedef void* (*AddExplosionFunc)(void*, void*, int, void*, uint32_t, bool, float);
        return ((AddExplosionFunc)OriginalFunctions::CExplosion_AddExplosion)(
            owner, creator, type, position, time, makeSound, camShake);
    }

    return nullptr;
}

} // extern "C"
