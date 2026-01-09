/*
 * MTA:SA Android - Game SA Platform Abstraction
 *
 * This header provides platform-independent macros and utilities for
 * the game_sa interface layer. It abstracts away differences between:
 *
 *   - x86 Windows (original MTA)
 *   - ARM32 Android
 *   - ARM64 Android
 *
 * Key abstractions:
 *   1. Function address resolution (x86 hardcoded vs ARM lookup)
 *   2. Calling conventions (__thiscall vs ARM ABI)
 *   3. Inline assembly (x86 ASM vs ARM ASM vs C++)
 *   4. Memory layout differences
 *
 * Usage:
 *   Instead of:  ((void(__thiscall*)(CEntity*))0x446F90)(this);
 *   Use:         GAME_CALL(void, CEntity_UpdateRW, (CEntity*), (this));
 */

#ifndef GAMESA_PLATFORM_H
#define GAMESA_PLATFORM_H

#include <cstdint>

//=============================================================================
// Platform Detection
//=============================================================================

#if defined(MTA_ARM32) || defined(__arm__) || defined(_M_ARM)
    #define PLATFORM_ARM32 1
    #define PLATFORM_ARM 1
#elif defined(MTA_ARM64) || defined(__aarch64__) || defined(_M_ARM64)
    #define PLATFORM_ARM64 1
    #define PLATFORM_ARM 1
#elif defined(_M_IX86) || defined(__i386__)
    #define PLATFORM_X86 1
#elif defined(_M_X64) || defined(__x86_64__)
    #define PLATFORM_X64 1
#else
    #error "Unknown platform"
#endif

//=============================================================================
// ARM Address Includes
//=============================================================================

#ifdef PLATFORM_ARM
    #include "signatures/ARMAddressMap.h"

    // Base address of libGTASA.so - set at runtime
    extern uintptr_t g_GTASABase;

    // Get absolute address from offset
    inline uintptr_t GetGameAddress(uint32_t offset)
    {
        return g_GTASABase + offset;
    }
#endif

//=============================================================================
// Calling Convention Abstraction
//=============================================================================

#ifdef PLATFORM_X86
    // x86 Windows uses __thiscall for member functions
    #define THISCALL __thiscall
    #define CDECL __cdecl
    #define STDCALL __stdcall
#else
    // ARM uses standard C ABI - 'this' is first parameter
    #define THISCALL
    #define CDECL
    #define STDCALL
#endif

//=============================================================================
// Function Call Macros
//=============================================================================

#ifdef PLATFORM_X86

    /**
     * Call a game function by address (x86)
     * @param RetType Return type
     * @param FuncName Function name (for documentation)
     * @param ParamTypes Parameter types in parentheses
     * @param Args Arguments in parentheses
     * @param Address x86 address
     */
    #define GAME_CALL_ADDR(RetType, FuncName, ParamTypes, Args, Address) \
        ((RetType(THISCALL*)ParamTypes)(Address))Args

    /**
     * Call a game thiscall function (x86)
     */
    #define GAME_THISCALL(RetType, Address, This, ...) \
        ((RetType(THISCALL*)(void*, ##__VA_ARGS__))(Address))(This, ##__VA_ARGS__)

    /**
     * Call a game cdecl function (x86)
     */
    #define GAME_CDECL(RetType, Address, ...) \
        ((RetType(CDECL*)(__VA_ARGS__))(Address))(__VA_ARGS__)

#else // PLATFORM_ARM

    /**
     * Call a game function by address (ARM)
     * On ARM, use offset from g_GTASABase
     */
    #define GAME_CALL_ADDR(RetType, FuncName, ParamTypes, Args, Address) \
        ((RetType(*)ParamTypes)(GetGameAddress(Address)))Args

    /**
     * Call a game thiscall function (ARM)
     * On ARM, 'this' is just the first parameter
     */
    #define GAME_THISCALL(RetType, Address, This, ...) \
        ((RetType(*)(void*, ##__VA_ARGS__))(GetGameAddress(Address)))(This, ##__VA_ARGS__)

    /**
     * Call a game cdecl function (ARM)
     */
    #define GAME_CDECL(RetType, Address, ...) \
        ((RetType(*)(__VA_ARGS__))(GetGameAddress(Address)))(__VA_ARGS__)

#endif

//=============================================================================
// Address Definition Macros
//=============================================================================

#ifdef PLATFORM_ARM32
    // Use ARM32 addresses from ARMAddressMap.h
    #define FUNC_ADDR(x86_addr, arm32_addr, arm64_addr) (arm32_addr)
#elif defined(PLATFORM_ARM64)
    // Use ARM64 addresses from ARMAddressMap.h
    #define FUNC_ADDR(x86_addr, arm32_addr, arm64_addr) (arm64_addr)
#else
    // Use x86 addresses (original)
    #define FUNC_ADDR(x86_addr, arm32_addr, arm64_addr) (x86_addr)
#endif

//=============================================================================
// Common Game Addresses (x86 / ARM32 / ARM64)
//=============================================================================

namespace GameAddr
{
    // CEntity
    constexpr uint32_t CEntity_UpdateRW = FUNC_ADDR(0x446F90, 0x2DB2D4, 0);
    constexpr uint32_t CEntity_UpdateRpHAnim = FUNC_ADDR(0x532B20, 0x3958C0, 0);
    constexpr uint32_t CEntity_GetBoundCentre = FUNC_ADDR(0x534250, 0x39728C, 0);
    constexpr uint32_t CEntity_TransformFromObjectSpace = FUNC_ADDR(0x533560, 0x396738, 0);
    constexpr uint32_t CEntity_GetDistanceFromCentreOfMassToBaseOfModel = FUNC_ADDR(0x536BE0, 0x399290, 0);
    constexpr uint32_t CEntity_SetRwObjectAlpha = FUNC_ADDR(0x5332C0, 0x396534, 0);
    constexpr uint32_t CEntity_IsVisible = FUNC_ADDR(0x536BC0, 0x399270, 0);

    // CPed
    constexpr uint32_t CPed_ProcessControl = FUNC_ADDR(0x60EA90, 0x4A2541, 0);
    constexpr uint32_t CPed_SetModelIndex = FUNC_ADDR(0x5E4880, 0x47D2E9, 0);
    constexpr uint32_t CPed_GiveWeapon = FUNC_ADDR(0x5E6080, 0x47EA01, 0);
    constexpr uint32_t CPed_GetWeaponSlot = FUNC_ADDR(0x5DF200, 0x476555, 0);
    constexpr uint32_t CPed_SetCurrentWeaponSlot = FUNC_ADDR(0x5E61F0, 0x47EB6D, 0);
    constexpr uint32_t CPed_RemoveWeaponModel = FUNC_ADDR(0x5E3990, 0x47C371, 0);
    constexpr uint32_t CPed_ClearWeapon = FUNC_ADDR(0x5E62B0, 0x47EC1D, 0);
    constexpr uint32_t CPed_ClearWeapons = FUNC_ADDR(0x5E6320, 0x47EC89, 0);
    constexpr uint32_t CPed_GetBonePosition = FUNC_ADDR(0x5E4280, 0x47CC85, 0);
    constexpr uint32_t CPed_GetTransformedBonePosition = FUNC_ADDR(0x5E01C0, 0x477A89, 0);
    constexpr uint32_t CPed_CanSeeEntity = FUNC_ADDR(0x5E0730, 0x47802D, 0);

    // CPlayerPed
    constexpr uint32_t CPlayerPed_ProcessControl = FUNC_ADDR(0x60EA90, 0x4C47E9, 0);
    constexpr uint32_t CPlayerPed_Constructor = FUNC_ADDR(0x60D5B0, 0x4C32F5, 0);
    constexpr uint32_t CPlayerPed_SetInitialState = FUNC_ADDR(0x60CD20, 0x4C28D1, 0);

    // CVehicle
    constexpr uint32_t CVehicle_ProcessControl = FUNC_ADDR(0x6D6A10, 0x5828C5, 0);
    constexpr uint32_t CVehicle_SetupRender = FUNC_ADDR(0x6D6480, 0x582335, 0);
    constexpr uint32_t CVehicle_PreRender = FUNC_ADDR(0x6D5F40, 0x581E19, 0);
    constexpr uint32_t CVehicle_AddPassenger = FUNC_ADDR(0x6D6E00, 0x582CB9, 0);
    constexpr uint32_t CVehicle_RemovePassenger = FUNC_ADDR(0x6D6280, 0x582131, 0);
    constexpr uint32_t CVehicle_SetDriver = FUNC_ADDR(0x6D6CB0, 0x582B69, 0);
    constexpr uint32_t CVehicle_RemoveDriver = FUNC_ADDR(0x6D6400, 0x5822B5, 0);
    constexpr uint32_t CVehicle_OpenDoor = FUNC_ADDR(0x6D5CF0, 0x581BD9, 0);
    constexpr uint32_t CVehicle_CanBeDamaged = FUNC_ADDR(0x6D1280, 0x57D14D, 0);
    constexpr uint32_t CVehicle_Fix = FUNC_ADDR(0x6D3590, 0x57F449, 0);
    constexpr uint32_t CVehicle_BlowUp = FUNC_ADDR(0x6D45A0, 0x570459, 0);
    constexpr uint32_t CVehicle_SetLandingGearPosition = FUNC_ADDR(0x6CAD10, 0x576BC5, 0);
    constexpr uint32_t CVehicle_AddVehicleUpgrade = FUNC_ADDR(0x6E3290, 0x58E935, 0);
    constexpr uint32_t CVehicle_RemoveVehicleUpgrade = FUNC_ADDR(0x6DF930, 0x58B0F9, 0);
    constexpr uint32_t CVehicle_GetBaseVehicleType = FUNC_ADDR(0x411D50, 0x2B5EF4, 0);
    constexpr uint32_t CVehicle_IsUpsideDown = FUNC_ADDR(0x6D1D90, 0x57DC59, 0);
    constexpr uint32_t CVehicle_SetEngineOn = FUNC_ADDR(0x41BDD0, 0x2BFDFC, 0);
    constexpr uint32_t CVehicle_IsPassenger = FUNC_ADDR(0x6D1BD0, 0x57DA9D, 0);
    constexpr uint32_t CVehicle_BurstTyre = FUNC_ADDR(0x6A32C0, 0x54F5B5, 0);
    constexpr uint32_t CVehicle_GetTowBarPos = FUNC_ADDR(0x6DFFC0, 0x58B779, 0);
    constexpr uint32_t CVehicle_SetTowLink = FUNC_ADDR(0x6B44B0, 0x560425, 0);
    constexpr uint32_t CVehicle_BreakTowLink = FUNC_ADDR(0x6A4400, 0x5506B1, 0);

    // CAutomobile
    constexpr uint32_t CAutomobile_ProcessControl = FUNC_ADDR(0x6B1880, 0x553E45, 0);
    constexpr uint32_t CAutomobile_Teleport = FUNC_ADDR(0x6A9F00, 0x556201, 0);
    constexpr uint32_t CAutomobile_ProcessSuspension = FUNC_ADDR(0x6ABF60, 0x558265, 0);
    constexpr uint32_t CAutomobile_DamageWheel = FUNC_ADDR(0x6A3150, 0x54F441, 0);
    constexpr uint32_t CAutomobile_Fix = FUNC_ADDR(0x6A3440, 0x54F725, 0);
    constexpr uint32_t CAutomobile_PlaceOnRoadProperly = FUNC_ADDR(0x6AF420, 0x55B5A9, 0);
    constexpr uint32_t CAutomobile_SpawnFlyingComponent = FUNC_ADDR(0x6A8580, 0x554915, 0);
    constexpr uint32_t CAutomobile_VehicleDamage = FUNC_ADDR(0x6A7EA0, 0x554231, 0);

    // CBoat
    constexpr uint32_t CBoat_ProcessControl = FUNC_ADDR(0x6F1770, 0x59CE11, 0);

    // CHeli
    constexpr uint32_t CHeli_ProcessControl = FUNC_ADDR(0x6C4400, 0x5703C1, 0);

    // CPlane
    constexpr uint32_t CPlane_ProcessControl = FUNC_ADDR(0x6C9260, 0x575191, 0);

    // CCamera
    constexpr uint32_t CCamera_Process = FUNC_ADDR(0x52B730, 0x3D693D, 0);
    constexpr uint32_t CCamera_CamControl = FUNC_ADDR(0x527FA0, 0x3D30F1, 0);
    constexpr uint32_t CCamera_TakeControl = FUNC_ADDR(0x50C7C0, 0x3B891D, 0);
    constexpr uint32_t CCamera_Restore = FUNC_ADDR(0x50B930, 0x3B7AC5, 0);
    constexpr uint32_t CCamera_SetCameraDirectlyBehindForFollowPed = FUNC_ADDR(0x50EB70, 0x3BAC71, 0);

    // CWorld
    constexpr uint32_t CWorld_ProcessLineOfSight = FUNC_ADDR(0x56BA00, 0x410BC5, 0);
    constexpr uint32_t CWorld_IsLineOfSightClear = FUNC_ADDR(0x56B700, 0x4108C1, 0);
    constexpr uint32_t CWorld_Add = FUNC_ADDR(0x563220, 0x4082D5, 0);
    constexpr uint32_t CWorld_Remove = FUNC_ADDR(0x563280, 0x408335, 0);
    constexpr uint32_t CWorld_FindGroundZForCoord = FUNC_ADDR(0x5696C0, 0x40E6D1, 0);
    constexpr uint32_t CWorld_FindGroundZFor3DCoord = FUNC_ADDR(0x569660, 0x40E671, 0);
    constexpr uint32_t CWorld_FindLowestZForCoord = FUNC_ADDR(0x5697D0, 0x40E7E1, 0);
    constexpr uint32_t CWorld_GetWaterLevel = FUNC_ADDR(0x6E8580, 0x593C35, 0);
    constexpr uint32_t CWorld_FindObjectsInRange = FUNC_ADDR(0x564B70, 0x409BB9, 0);
    constexpr uint32_t CWorld_FindObjectsIntersectingCube = FUNC_ADDR(0x565300, 0x40A345, 0);
    constexpr uint32_t CWorld_TestSphereAgainstWorld = FUNC_ADDR(0x56D5E0, 0x41264D, 0);
    constexpr uint32_t CWorld_ClearExcitingStuffFromArea = FUNC_ADDR(0x56C450, 0x411505, 0);
    constexpr uint32_t CWorld_SetCurrentArea = FUNC_ADDR(0x56F870, 0x41493D, 0);
    constexpr uint32_t CWorld_ProcessCamera = FUNC_ADDR(0x56E6C0, 0x413781, 0);

    // CColModel
    constexpr uint32_t CColModel_TestLine = FUNC_ADDR(0x4103A0, 0x2B4571, 0);
    constexpr uint32_t CColModel_TestSphere = FUNC_ADDR(0x410460, 0x2B4631, 0);

    // CWeapon
    constexpr uint32_t CWeapon_Fire = FUNC_ADDR(0x742300, 0x5CE8C9, 0);
    constexpr uint32_t CWeapon_FireInstantHit = FUNC_ADDR(0x73FB10, 0x5CC085, 0);
    constexpr uint32_t CWeapon_FireSniper = FUNC_ADDR(0x73AAC0, 0x5C6E69, 0);
    constexpr uint32_t CWeapon_TakePhotograph = FUNC_ADDR(0x73C1F0, 0x5C85C9, 0);
    constexpr uint32_t CWeapon_DoBulletImpact = FUNC_ADDR(0x73B550, 0x5C7921, 0);

    // CExplosion
    constexpr uint32_t CExplosion_AddExplosion = FUNC_ADDR(0x736A50, 0x5C2D65, 0);

    // CStreaming
    constexpr uint32_t CStreaming_RequestModel = FUNC_ADDR(0x4087E0, 0x2AC965, 0);
    constexpr uint32_t CStreaming_LoadAllRequestedModels = FUNC_ADDR(0x40EA10, 0x2B2B51, 0);
    constexpr uint32_t CStreaming_HasModelLoaded = FUNC_ADDR(0x4044C0, 0x2A867D, 0);
    constexpr uint32_t CStreaming_RemoveModel = FUNC_ADDR(0x4089A0, 0x2ACB31, 0);

    // CModelInfo
    constexpr uint32_t CModelInfo_GetModelInfo = FUNC_ADDR(0x4C5940, 0x3681A1, 0);

    // CPool operations
    constexpr uint32_t CPools_GetPed = FUNC_ADDR(0x54FF90, 0x3F5E5D, 0);
    constexpr uint32_t CPools_GetVehicle = FUNC_ADDR(0x54FFA0, 0x3F5E6D, 0);
    constexpr uint32_t CPools_GetObject = FUNC_ADDR(0x54FFB0, 0x3F5E7D, 0);

    // CTimer
    constexpr uint32_t CTimer_Update = FUNC_ADDR(0x561B10, 0x406B49, 0);

    // Audio
    constexpr uint32_t CAESound_Stop = FUNC_ADDR(0x4EF680, 0x39500D, 0);

    // RenderWare
    constexpr uint32_t RpClumpRender = FUNC_ADDR(0x7F6060, 0x1A5E69, 0);
    constexpr uint32_t RpAtomicRender = FUNC_ADDR(0x7F5F10, 0x1A5D25, 0);
    constexpr uint32_t RwFrameUpdateObjects = FUNC_ADDR(0x7F0910, 0x19FCE9, 0);
    constexpr uint32_t RwMatrixScale = FUNC_ADDR(0x7F2060, 0x1A1455, 0);
}

//=============================================================================
// Inline Assembly Abstraction
//=============================================================================

#ifdef PLATFORM_X86

    // x86 inline assembly is supported
    #define ASM_BLOCK_BEGIN __asm {
    #define ASM_BLOCK_END }
    #define ASM_SUPPORTED 1

#elif defined(PLATFORM_ARM)

    // ARM: Inline assembly not directly compatible
    // Use C++ alternatives or ARM inline assembly
    #define ASM_BLOCK_BEGIN /* ARM assembly not supported - use C++ */
    #define ASM_BLOCK_END
    #define ASM_SUPPORTED 0

    // Helper for common x86 ASM patterns

    /**
     * Replaces x86 pattern:
     *   mov ecx, this
     *   call func
     *
     * With ARM-compatible C++ call
     */
    template<typename Ret, typename... Args>
    inline Ret CallThisCall(uintptr_t funcAddr, void* thisPtr, Args... args)
    {
        using FuncType = Ret(*)(void*, Args...);
        return reinterpret_cast<FuncType>(funcAddr)(thisPtr, args...);
    }

    /**
     * Replaces x86 pattern:
     *   push args
     *   call func
     *   add esp, N
     *
     * With ARM-compatible C++ call
     */
    template<typename Ret, typename... Args>
    inline Ret CallCdecl(uintptr_t funcAddr, Args... args)
    {
        using FuncType = Ret(*)(Args...);
        return reinterpret_cast<FuncType>(funcAddr)(args...);
    }

#endif

//=============================================================================
// Memory Access Utilities
//=============================================================================

/**
 * Read value from game memory
 */
template<typename T>
inline T ReadGameMemory(uintptr_t address)
{
#ifdef PLATFORM_ARM
    address = GetGameAddress(address);
#endif
    return *reinterpret_cast<T*>(address);
}

/**
 * Write value to game memory
 */
template<typename T>
inline void WriteGameMemory(uintptr_t address, T value)
{
#ifdef PLATFORM_ARM
    address = GetGameAddress(address);
#endif
    *reinterpret_cast<T*>(address) = value;
}

/**
 * Get pointer to game memory
 */
template<typename T>
inline T* GetGamePointer(uintptr_t address)
{
#ifdef PLATFORM_ARM
    address = GetGameAddress(address);
#endif
    return reinterpret_cast<T*>(address);
}

//=============================================================================
// Structure Offset Validation
//=============================================================================

// On Android, structure layouts should match Windows
// Use static_assert to verify critical offsets

#ifdef PLATFORM_ARM
    // These will be validated at compile time
    // Add structure offset checks here if needed
#endif

//=============================================================================
// Platform-specific Initialization
//=============================================================================

#ifdef PLATFORM_ARM
    /**
     * Initialize platform-specific components
     * Must be called after libGTASA.so is loaded
     */
    bool InitializeGamePlatform(uintptr_t gtasaBase);

    /**
     * Shutdown platform-specific components
     */
    void ShutdownGamePlatform();
#endif

#endif // GAMESA_PLATFORM_H
