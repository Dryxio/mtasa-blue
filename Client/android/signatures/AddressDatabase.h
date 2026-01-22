/*
 * MTA:SA Android - Address Mapping Database
 *
 * Comprehensive mapping of x86 Windows addresses to ARM Android addresses.
 * Generated from MTA:SA multiplayer_sa hooks and GTA-Reversed documentation.
 *
 * Usage:
 *   1. Use SignatureScanner to resolve ARM addresses
 *   2. Manually verify critical addresses with disassembler
 *   3. Update ARM addresses as they are discovered
 *
 * Reference:
 *   - GTA-Reversed: /Users/salimtrouve/Documents/GitHub/gta-reversed-dryxio
 *   - MTA hooks: /Users/salimtrouve/Documents/GitHub/mtasa-blue/Client/multiplayer_sa
 */

#ifndef ADDRESS_DATABASE_H
#define ADDRESS_DATABASE_H

#include "SignatureScanner.h"

namespace MTA::Android::Signatures
{
    //=========================================================================
    // Address Entry Structure
    //=========================================================================

    struct AddressEntry
    {
        const char* name;           // Function/hook name
        const char* category;       // Category for organization
        uint32_t    x86Address;     // Original x86 address
        uint32_t    hookSize;       // Bytes to patch (x86)
        uint32_t    armAddress;     // ARM address (0 = not resolved)
        const char* gtaReversedRef; // GTA-Reversed file reference
        const char* signature;      // Pattern for scanning (optional)
        const char* notes;          // Additional notes
    };

    //=========================================================================
    // CATEGORY: CORE GAME FUNCTIONS
    //=========================================================================

    static const AddressEntry CORE_HOOKS[] = {
        // Function finders
        {"FindPlayerCoors", "Core", 0x56E010, 5, 0, "game_sa/World.cpp", nullptr, "Returns player coordinates"},
        {"FindPlayerCentreOfWorld", "Core", 0x56E250, 5, 0, "game_sa/World.cpp", nullptr, "Returns world center"},
        {"FindPlayerHeading", "Core", 0x56E450, 5, 0, "game_sa/World.cpp", nullptr, "Returns player heading"},

        // Main game loop
        {"CGame::Process", "Core", 0x53BEE0, 5, 0, "game_sa/CGame.cpp", nullptr, "Main game process"},
        {"CGame_Process_Hook", "Core", 0x53C095, 6, 0, nullptr, nullptr, "MTA hook point"},
        {"Idle", "Core", 0x53E981, 6, 0, "game_sa/CGame.cpp", nullptr, "Idle processing"},

        // Streaming
        {"CStreaming_Update_Caller", "Core", 0x53BF09, 5, 0, "game_sa/Streaming.cpp", nullptr, "Streaming update"},
        {"CStreaming::RequestModel", "Core", 0x4087E0, 5, 0, "game_sa/Streaming.cpp", nullptr, "Request model load"},
        {"CStreaming::LoadAllRequestedModels", "Core", 0x40EA10, 5, 0, "game_sa/Streaming.cpp", nullptr, "Load all models"},
        {"CStreaming__ConvertBufferToObject", "Core", 0x40CB88, 9, 0, "game_sa/Streaming.cpp", nullptr, "Buffer conversion"},

        // Scripts
        {"CRunningScript_Process", "Core", 0x469F00, 5, 0, "game_sa/Scripts/CRunningScript.cpp", nullptr, "Script processing"},

        // Collision
        {"CollisionStreamRead", "Core", 0x41B1D0, 6, 0, "game_sa/Collision/CCollision.cpp", nullptr, "Collision loading"},

        // Timer
        {"CTimer_Update", "Core", 0x561B10, 6, 0, "game_sa/CTimer.cpp", nullptr, "Timer update"},
        {"CTimer_Suspend", "Core", 0x5619E9, 6, 0, "game_sa/CTimer.cpp", nullptr, "Timer suspend"},
        {"CTimer_Resume", "Core", 0x561A11, 6, 0, "game_sa/CTimer.cpp", nullptr, "Timer resume"},
    };

    //=========================================================================
    // CATEGORY: ENTITY SYSTEM
    //=========================================================================

    static const AddressEntry ENTITY_HOOKS[] = {
        // CEntity
        {"CEntity::Render", "Entity", 0x534310, 6, 0, "game_sa/Entity/Entity.cpp", nullptr, "Entity rendering"},
        {"CEntity_RenderOneNonRoad", "Entity", 0x553260, 5, 0, "game_sa/Renderer.cpp", nullptr, "Non-road entity render"},
        {"CEntity_IsOnScreen_FixObjectsScale", "Entity", 0x534575, 6, 0, "game_sa/Entity/Entity.cpp", nullptr, "Screen visibility check"},
        {"CEntityDestructor", "Entity", 0x535E97, 6, 0, "game_sa/Entity/Entity.cpp", nullptr, "Entity destructor"},
        {"CEntityAddMid1", "Entity", 0x5348FB, 5, 0, nullptr, nullptr, "Entity add hook 1"},
        {"CEntityAddMid2", "Entity", 0x534A10, 5, 0, nullptr, nullptr, "Entity add hook 2"},
        {"CEntityAddMid3", "Entity", 0x534AA2, 5, 0, nullptr, nullptr, "Entity add hook 3"},
        {"CEntityRemove", "Entity", 0x534AE0, 5, 0, nullptr, nullptr, "Entity remove"},
        {"CEntity_GetBoundRect", "Entity", 0x534131, 5, 0, "game_sa/Entity/Entity.cpp", nullptr, "Get bounding rect"},

        // CPhysical
        {"CPhysical::ProcessCollision", "Entity", 0x54DFB0, 5, 0, "game_sa/Entity/Physical.cpp", nullptr, "Physics collision"},
        {"CPhysical_ApplyGravity", "Entity", 0x543081, 6, 0, "game_sa/Entity/Physical.cpp", nullptr, "Apply gravity"},
        {"CPhysical_ProcessCollisionSectorList", "Entity", 0x54BB93, 7, 0, "game_sa/Entity/Physical.cpp", nullptr, "Collision sector list"},
        {"CPhysicalDestructor", "Entity", 0x542450, 7, 0, "game_sa/Entity/Physical.cpp", nullptr, "Physical destructor"},
        {"CPhysical__ApplyAirResistance", "Entity", 0x544D29, 5, 0, "game_sa/Entity/Physical.cpp", nullptr, "Air resistance"},

        // CObject
        {"CObject_Render", "Entity", 0x59F1ED, 5, 0, "game_sa/Object/Object.cpp", nullptr, "Object render"},
        {"CObject_ProcessBreak", "Entity", 0x5A0F0F, 5, 0, "game_sa/Object/Object.cpp", nullptr, "Object break"},
        {"CObject_ProcessDamage", "Entity", 0x5A0E0D, 6, 0, "game_sa/Object/Object.cpp", nullptr, "Object damage"},
        {"CObject_ProcessCollision", "Entity", 0x548DC7, 6, 0, "game_sa/Object/Object.cpp", nullptr, "Object collision"},
        {"CObjectDestructor", "Entity", 0x59F660, 7, 0, "game_sa/Object/Object.cpp", nullptr, "Object destructor"},
        {"CObject_DTR", "Entity", 0x59F680, 5, 0, nullptr, nullptr, "Object destructor call"},
        {"CObject_Init", "Entity", 0x59F8BE, 30, 0, "game_sa/Object/Object.cpp", nullptr, "Object init"},

        // CBuilding
        {"CBuilding_DTR", "Entity", 0x404180, 5, 0, "game_sa/Building/Building.cpp", nullptr, "Building destructor"},
        {"CDummy_DTR", "Entity", 0x532566, 5, 0, nullptr, nullptr, "Dummy destructor"},
    };

    //=========================================================================
    // CATEGORY: VEHICLE SYSTEM
    //=========================================================================

    static const AddressEntry VEHICLE_HOOKS[] = {
        // CVehicle base
        {"CVehicle_SetupRender", "Vehicle", 0x6D6512, 5, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Vehicle render setup"},
        {"CVehicle_ResetAfterRender", "Vehicle", 0x6D0E3E, 5, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Reset after render"},
        {"CVehicle_BurstTyre", "Vehicle", 0x6A32B0, 5, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Burst tire"},
        {"CVehicle_InflictDamage", "Vehicle", 0x6D7C90, 5, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Inflict damage"},
        {"CVehicle_ApplyBoatWaterResistance", "Vehicle", 0x6D2771, 6, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Boat water resistance"},
        {"CVehicle_DoVehicleLights", "Vehicle", 0x6E1A60, 5, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Vehicle lights"},
        {"CVehicle_DoHeadLightBeam_1", "Vehicle", 0x6E0E20, 5, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Headlight beam 1"},
        {"CVehicle_DoHeadLightBeam_2", "Vehicle", 0x6E13A4, 5, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Headlight beam 2"},
        {"CVehicle_AddExhaustParticles", "Vehicle", 0x6DE240, 6, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Exhaust particles"},
        {"CVehicle_AddWheelDirtAndWater", "Vehicle", 0x6D2D50, 6, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Wheel dirt/water"},
        {"CVehicle_DoBoatSplashes", "Vehicle", 0x6DD130, 6, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Boat splashes"},
        {"CVehicleDestructor", "Vehicle", 0x6E2B40, 7, 0, "game_sa/Entity/Vehicle/Vehicle.cpp", nullptr, "Vehicle destructor"},

        // CAutomobile
        {"CAutomobile::ProcessControl", "Vehicle", 0x6B1880, 5, 0, "game_sa/Entity/Vehicle/Automobile.cpp", nullptr, "Auto process control"},
        {"CAutomobile_BurstTyre", "Vehicle", 0x6A331C, 5, 0, "game_sa/Entity/Vehicle/Automobile.cpp", nullptr, "Auto burst tire"},
        {"CAutomobile_ProcessControl_VehicleDamage", "Vehicle", 0x6B1F3B, 6, 0, "game_sa/Entity/Vehicle/Automobile.cpp", nullptr, "Auto damage processing"},
        {"CAutomobile__ProcessSwingingDoor", "Vehicle", 0x6A9DAF, 5, 0, "game_sa/Entity/Vehicle/Automobile.cpp", nullptr, "Swinging door"},
        {"CAutomobile_VehicleDamage1", "Vehicle", 0x6A7650, 7, 0, "game_sa/Entity/Vehicle/Automobile.cpp", nullptr, "Damage type 1"},
        {"CAutomobile_VehicleDamage2", "Vehicle", 0x6A8325, 6, 0, "game_sa/Entity/Vehicle/Automobile.cpp", nullptr, "Damage type 2"},
        {"CAutomobile__dmgDrawCarCollidingParticles", "Vehicle", 0x6A6FF0, 5, 0, nullptr, nullptr, "Collision particles"},
        {"CAutomobile_DoNitroEffect_1", "Vehicle", 0x6A3BE2, 6, 0, nullptr, nullptr, "Nitro effect 1"},
        {"CAutomobile_DoNitroEffect_2", "Vehicle", 0x6A3C68, 41, 0, nullptr, nullptr, "Nitro effect 2"},
        {"CAutomobile_ProcessCarOnFireAndExplode", "Vehicle", 0x6A717F, 6, 0, nullptr, nullptr, "Fire and explode"},
        {"CAutomobile__UpdateWheelMatrix", "Vehicle", 0x6AA78A, 5, 0, nullptr, nullptr, "Wheel matrix update"},

        // CBike
        {"CBike::ProcessControl", "Vehicle", 0x6B7F70, 5, 0, "game_sa/Entity/Vehicle/Bike.cpp", nullptr, "Bike process control"},
        {"CBike_BurstTyre", "Vehicle", 0x6BEB94, 10, 0, "game_sa/Entity/Vehicle/Bike.cpp", nullptr, "Bike burst tire"},
        {"CBike_ProcessControl_VehicleDamage", "Vehicle", 0x6B9AA5, 6, 0, "game_sa/Entity/Vehicle/Bike.cpp", nullptr, "Bike damage"},
        {"CBike_VehicleDamage1", "Vehicle", 0x6B8EC0, 5, 0, nullptr, nullptr, "Bike damage 1"},
        {"CBike_VehicleDamage2", "Vehicle", 0x6B91C2, 6, 0, nullptr, nullptr, "Bike damage 2"},
        {"CBike_FixHandsToBars", "Vehicle", 0x6B8053, 6, 0, nullptr, nullptr, "Hands to bars"},

        // CBoat
        {"CBoat::ProcessControl", "Vehicle", 0x6F1770, 5, 0, "game_sa/Entity/Vehicle/Boat.cpp", nullptr, "Boat process control"},
        {"CBoat_ProcessControl_VehicleDamage", "Vehicle", 0x6F1864, 5, 0, "game_sa/Entity/Vehicle/Boat.cpp", nullptr, "Boat damage"},
        {"CBoat_ApplyDamage", "Vehicle", 0x6F1C32, 5, 0, nullptr, nullptr, "Boat apply damage"},

        // CPlane
        {"CPlane::ProcessControl", "Vehicle", 0x6C9320, 5, 0, "game_sa/Entity/Vehicle/Plane.cpp", nullptr, "Plane process control"},
        {"CPlane_VehicleDamage1", "Vehicle", 0x6CC4B0, 8, 0, nullptr, nullptr, "Plane damage 1"},
        {"CPlane_VehicleDamage2", "Vehicle", 0x6CC6C8, 6, 0, nullptr, nullptr, "Plane damage 2"},
        {"CPlane_PreRender", "Vehicle", 0x6CA937, 6, 0, nullptr, nullptr, "Plane pre-render"},
        {"CPlane__ProcessFlyingCarStuff", "Vehicle", 0x6CBE4B, 6, 0, nullptr, nullptr, "Plane flying"},

        // CHeli
        {"CHeli::ProcessControl", "Vehicle", 0x6C4400, 5, 0, "game_sa/Entity/Vehicle/Heli.cpp", nullptr, "Heli process control"},
        {"CHeli_ProcessHeliKill", "Vehicle", 0x6DB201, 6, 0, "game_sa/Entity/Vehicle/Heli.cpp", nullptr, "Heli kill"},
        {"CHeli__ProcessFlyingCarStuff", "Vehicle", 0x6C4F13, 42, 0, nullptr, nullptr, "Heli flying"},

        // CTrain
        {"CTrain::ProcessControl", "Vehicle", 0x6F86A0, 5, 0, "game_sa/Entity/Vehicle/Train.cpp", nullptr, "Train process control"},
        {"CTrain_ProcessControl_Derail", "Vehicle", 0x6F8DBA, 5, 0, nullptr, nullptr, "Train derail"},
        {"CTrain_ProcessControl_VehicleDamage", "Vehicle", 0x6F86BB, 5, 0, nullptr, nullptr, "Train damage"},
        {"CTrain__ProcessControl_CrashFix", "Vehicle", 0x6F8F89, 6, 0, nullptr, nullptr, "Train crash fix"},

        // Other vehicle types
        {"CMonsterTruck::ProcessControl", "Vehicle", 0x6C7090, 5, 0, nullptr, nullptr, "Monster truck control"},
        {"CTrailer::ProcessControl", "Vehicle", 0x6CF3F0, 5, 0, nullptr, nullptr, "Trailer control"},
        {"CQuadBike::ProcessControl", "Vehicle", 0x6CDF30, 5, 0, nullptr, nullptr, "Quad bike control"},
        {"CBmx::ProcessControl", "Vehicle", 0x6C7530, 5, 0, nullptr, nullptr, "BMX control"},

        // Trailer
        {"Trailer_BreakTowLink", "Vehicle", 0x6E0027, 5, 0, nullptr, nullptr, "Break tow link"},

        // Damage manager
        {"CDamageManager__ProgressDoorDamage", "Vehicle", 0x6C2320, 7, 0, "game_sa/Entity/Vehicle/CDamageManager.cpp", nullptr, "Door damage"},

        // Transmission
        {"Transmission_CalculateDriveAcceleration", "Vehicle", 0x6D05E0, 6, 0, nullptr, nullptr, "Drive acceleration"},
        {"CHandlingData_isNotRWD", "Vehicle", 0x6A048C, 7, 0, nullptr, nullptr, "Not RWD check"},
        {"CHandlingData_isNotFWD", "Vehicle", 0x6A04BC, 7, 0, nullptr, nullptr, "Not FWD check"},
    };

    //=========================================================================
    // CATEGORY: VEHICLE VTABLES
    //=========================================================================

    static const AddressEntry VEHICLE_VTABLES[] = {
        {"VTBL_CPlayerPed__ProcessControl", "VTable", 0x86D190, 4, 0, nullptr, nullptr, "Player ped vtable"},
        {"VTBL_CAutomobile__ProcessControl", "VTable", 0x871148, 4, 0, nullptr, nullptr, "Automobile vtable"},
        {"VTBL_CMonsterTruck__ProcessControl", "VTable", 0x871800, 4, 0, nullptr, nullptr, "Monster truck vtable"},
        {"VTBL_CTrailer__ProcessControl", "VTable", 0x871C50, 4, 0, nullptr, nullptr, "Trailer vtable"},
        {"VTBL_CQuadBike__ProcessControl", "VTable", 0x871B10, 4, 0, nullptr, nullptr, "Quad bike vtable"},
        {"VTBL_CPlane__ProcessControl", "VTable", 0x871970, 4, 0, nullptr, nullptr, "Plane vtable"},
        {"VTBL_CBmx__ProcessControl", "VTable", 0x871550, 4, 0, nullptr, nullptr, "BMX vtable"},
        {"VTBL_CTrain__ProcessControl", "VTable", 0x872398, 4, 0, nullptr, nullptr, "Train vtable"},
        {"VTBL_CBoat__ProcessControl", "VTable", 0x8721C8, 4, 0, nullptr, nullptr, "Boat vtable"},
        {"VTBL_CBike__ProcessControl", "VTable", 0x871388, 4, 0, nullptr, nullptr, "Bike vtable"},
        {"VTBL_CHeli__ProcessControl", "VTable", 0x8716A8, 4, 0, nullptr, nullptr, "Heli vtable"},
    };

    //=========================================================================
    // CATEGORY: PEDESTRIAN SYSTEM
    //=========================================================================

    static const AddressEntry PED_HOOKS[] = {
        // CPed
        {"CPed::ProcessControl", "Ped", 0x60EA90, 5, 0, "game_sa/Entity/Ped/Ped.cpp", nullptr, "Ped process control"},
        {"CPed_DoFootLanded", "Ped", 0x5E5380, 6, 0, "game_sa/Entity/Ped/Ped.cpp", nullptr, "Foot landed"},
        {"CPed_IsPlayer", "Ped", 0x5DF8F0, 6, 0, "game_sa/Entity/Ped/Ped.cpp", nullptr, "Is player check"},
        {"CPed_GetWeaponSkill", "Ped", 0x5E3B60, 8, 0, "game_sa/Entity/Ped/Ped.cpp", nullptr, "Get weapon skill"},
        {"CPed_AddGogglesModel", "Ped", 0x5E3ACB, 9, 0, nullptr, nullptr, "Add goggles"},
        {"CPed_SetPedPositionInCar_1", "Ped", 0x5DF98B, 7, 0, nullptr, nullptr, "Position in car 1"},
        {"CPed_SetPedPositionInCar_2", "Ped", 0x5DFA56, 6, 0, nullptr, nullptr, "Position in car 2"},
        {"CPed_SetPedPositionInCar_3", "Ped", 0x5DFA04, 7, 0, nullptr, nullptr, "Position in car 3"},
        {"CPed_SetPedPositionInCar_4", "Ped", 0x5DFA80, 6, 0, nullptr, nullptr, "Position in car 4"},
        {"CPed__PreRenderAfterTest", "Ped", 0x5E7181, 6, 0, nullptr, nullptr, "Pre-render after test"},

        // CPlayerPed
        {"CPlayerPed::ProcessControl", "Ped", 0x60EA90, 5, 0, "game_sa/Entity/Ped/PlayerPed.cpp", nullptr, "Player ped control"},
        {"CPlayerPedDestructor", "Ped", 0x6093B0, 7, 0, "game_sa/Entity/Ped/PlayerPed.cpp", nullptr, "Player ped destructor"},
        {"CPlayerInfo__Process", "Ped", 0x5700F5, 6, 0, "game_sa/CPlayerInfo.cpp", nullptr, "Player info process"},

        // CPedIK
        {"CPedIK__PointGunInDirection", "Ped", 0x5FDC00, 5, 0, "game_sa/Entity/Ped/PedIK.cpp", nullptr, "Point gun direction"},

        // Clothes
        {"CClothes_RebuildPlayer", "Ped", 0x5A82C0, 8, 0, "game_sa/Clothes/CClothes.cpp", nullptr, "Rebuild player clothes"},
        {"CClothesDeleteRwObject", "Ped", 0x5A8243, 5, 0, nullptr, nullptr, "Delete clothes RW object"},
        {"PostCPedDress", "Ped", 0x5A835C, 5, 0, nullptr, nullptr, "Post ped dress"},
        {"CClothesBuilderCreateSkinnedClump", "Ped", 0x5A69D0, 6, 0, nullptr, nullptr, "Create skinned clump"},
    };

    //=========================================================================
    // CATEGORY: WEAPONS SYSTEM
    //=========================================================================

    static const AddressEntry WEAPON_HOOKS[] = {
        // CWeapon
        {"CWeapon::Fire", "Weapon", 0x742300, 5, 0, "game_sa/Weapon/CWeapon.cpp", nullptr, "Weapon fire"},
        {"CWeapon__PostFire", "Weapon", 0x742A02, 5, 0, nullptr, nullptr, "Post fire"},
        {"CWeapon__PostFire2", "Weapon", 0x742423, 5, 0, nullptr, nullptr, "Post fire 2"},
        {"CWeapon_FireAreaEffect", "Weapon", 0x73EBFE, 5, 0, nullptr, nullptr, "Fire area effect"},
        {"CWeapon_FireSniper", "Weapon", 0x7424A6, 6, 0, nullptr, nullptr, "Fire sniper"},
        {"CWeapon_FireInstantHit", "Weapon", 0x740B42, 5, 0, nullptr, nullptr, "Fire instant hit"},
        {"CWeapon_FireInstantHit_Mid", "Weapon", 0x740B89, 5, 0, nullptr, nullptr, "Instant hit mid"},
        {"CWeapon_FireSniper_Mid", "Weapon", 0x73AE31, 5, 0, nullptr, nullptr, "Sniper mid"},
        {"CWeapon_DoBulletImpact", "Weapon", 0x73B550, 5, 0, nullptr, nullptr, "Bullet impact"},
        {"CWeapon__TakePhotograph", "Weapon", 0x73C26E, 5, 0, nullptr, nullptr, "Take photograph"},
        {"CWeapon_GenerateDamageEvent", "Weapon", 0x73A530, 7, 0, nullptr, nullptr, "Generate damage event"},
        {"CWeapon_Update", "Weapon", 0x73DC3D, 5, 0, nullptr, nullptr, "Weapon update"},

        // CShotInfo
        {"CShotInfo_Update", "Weapon", 0x739E60, 6, 0, "game_sa/Weapon/CShotInfo.cpp", nullptr, "Shot info update"},

        // CProjectile
        {"CProjectile::CProjectile", "Weapon", 0x5A4030, 5, 0, "game_sa/Weapon/CProjectile.cpp", nullptr, "Projectile constructor"},
        {"CProjectileDestructor", "Weapon", 0x5A40E0, 6, 0, nullptr, nullptr, "Projectile destructor"},
        {"CProjectileInfo__AddProjectile", "Weapon", 0x737C80, 5, 0, "game_sa/Weapon/CProjectileInfo.cpp", nullptr, "Add projectile"},
        {"CProjectileInfo__Update", "Weapon", 0x738C63, 5, 0, nullptr, nullptr, "Projectile update"},
        {"CProjectileInfo_FindPlayerPed", "Weapon", 0x739321, 5, 0, nullptr, nullptr, "Find player ped"},
        {"CProjectileInfo_FindPlayerVehicle", "Weapon", 0x739570, 5, 0, nullptr, nullptr, "Find player vehicle"},

        // Events
        {"CEventDamage__AffectsPed", "Weapon", 0x4B35A0, 5, 0, "game_sa/Events/CEventDamage.cpp", nullptr, "Damage affects ped"},
        {"CEventVehicleExplosion__AffectsPed", "Weapon", 0x4B0E58, 5, 0, nullptr, nullptr, "Explosion affects ped"},
        {"CEventHitByWaterCannon", "Weapon", 0x729899, 5, 0, nullptr, nullptr, "Hit by water cannon"},
    };

    //=========================================================================
    // CATEGORY: RENDERING SYSTEM
    //=========================================================================

    static const AddressEntry RENDERING_HOOKS[] = {
        // CRenderer
        {"CRenderer_Render", "Rendering", 0x53EA12, 5, 0, "game_sa/Renderer.cpp", nullptr, "Renderer render"},
        {"CRenderer_EverythingBarRoads", "Rendering", 0x553C78, 5, 0, nullptr, nullptr, "Everything bar roads"},
        {"CRenderer_SetupEntityVisibility", "Rendering", 0x554230, 8, 0, nullptr, nullptr, "Entity visibility"},
        {"Render3DStuff", "Rendering", 0x53EABF, 5, 0, nullptr, nullptr, "Render 3D stuff"},
        {"RenderScene_end", "Rendering", 0x53E159, 5, 0, nullptr, nullptr, "Render scene end"},
        {"CallIdle", "Rendering", 0x53ECBD, 5, 0, nullptr, nullptr, "Call idle"},

        // CVisibilityPlugins
        {"CVisibilityPlugins_RenderWeaponPedsForPC_Mid", "Rendering", 0x733080, 6, 0, "game_sa/VisibilityPlugins.cpp", nullptr, "Render weapon peds mid"},
        {"CVisibilityPlugins_RenderWeaponPedsForPC_End", "Rendering", 0x73314D, 5, 0, nullptr, nullptr, "Render weapon peds end"},
        {"CVisibilityPlugins_RenderPedCB", "Rendering", 0x7335B0, 5, 0, nullptr, nullptr, "Render ped callback"},
        {"CVisibilityPlugins_CalculateFadingAtomicAlpha", "Rendering", 0x732500, 5, 0, nullptr, nullptr, "Fading atomic alpha"},
        {"Check_NoOfVisibleLods", "Rendering", 0x5534F9, 6, 0, nullptr, nullptr, "Visible LODs check"},
        {"Check_NoOfVisibleEntities", "Rendering", 0x55352D, 6, 0, nullptr, nullptr, "Visible entities check"},

        // CHud
        {"CHud_Draw_Caller", "Rendering", 0x53E4FA, 5, 0, "game_sa/Hud/CHud.cpp", nullptr, "HUD draw caller"},

        // CClouds
        {"CClouds_RenderSkyPolys", "Rendering", 0x714650, 5, 0, "game_sa/Clouds/CClouds.cpp", nullptr, "Render sky polys"},
        {"CClouds__MovingFog_Update", "Rendering", 0x716BA6, 22, 0, nullptr, nullptr, "Moving fog update"},

        // CPlantMgr
        {"CPlantMgr_Render", "Rendering", 0x5DBC4C, 6, 0, "game_sa/Plant/CPlantMgr.cpp", nullptr, "Plant manager render"},

        // Effects
        {"PreFxRender", "Rendering", 0x49E650, 5, 0, nullptr, nullptr, "Pre-FX render"},
        {"PostColorFilterRender", "Rendering", 0x705099, 5, 0, nullptr, nullptr, "Post color filter"},
        {"PreHUDRender", "Rendering", 0x53EAD8, 5, 0, nullptr, nullptr, "Pre-HUD render"},
        {"RenderEffects_HeliLight", "Rendering", 0x53E1B9, 5, 0, nullptr, nullptr, "Heli light effect"},
        {"EndWorldColors", "Rendering", 0x561795, 5, 0, nullptr, nullptr, "End world colors"},

        // Window
        {"WinLoop", "Rendering", 0x748A93, 5, 0, nullptr, nullptr, "Window loop"},
        {"psGrabScreen", "Rendering", 0x7452FC, 5, 0, nullptr, nullptr, "Grab screen"},

        // Camera
        {"RwCameraSetNearClipPlane", "Rendering", 0x7EE1D0, 5, 0, nullptr, nullptr, "Near clip plane"},
        {"CWorldScan_ScanWorld", "Rendering", 0x55555E, 5, 0, nullptr, nullptr, "Scan world"},
    };

    //=========================================================================
    // CATEGORY: CAMERA SYSTEM
    //=========================================================================

    static const AddressEntry CAMERA_HOOKS[] = {
        {"CCamera::Process", "Camera", 0x52B730, 5, 0, "game_sa/Camera.cpp", nullptr, "Camera process"},
        {"CCamera__Process_FrameRate", "Camera", 0x52C723, 18, 0, nullptr, nullptr, "Camera frame rate fix"},
        {"CCam_ProcessFixed", "Camera", 0x51D470, 5, 0, nullptr, nullptr, "Fixed camera process"},
        {"VehicleCamStart", "Camera", 0x5245ED, 6, 0, nullptr, nullptr, "Vehicle cam start"},
        {"VehicleCamTargetZTweak", "Camera", 0x524A68, 6, 0, nullptr, nullptr, "Vehicle cam Z tweak"},
        {"VehicleCamLookDir1", "Camera", 0x524DF1, 5, 0, nullptr, nullptr, "Vehicle cam look dir 1"},
        {"VehicleCamLookDir2", "Camera", 0x525B0E, 6, 0, nullptr, nullptr, "Vehicle cam look dir 2"},
        {"VehicleCamHistory", "Camera", 0x525C56, 6, 0, nullptr, nullptr, "Vehicle cam history"},
        {"VehicleCamColDetect", "Camera", 0x525D8D, 5, 0, nullptr, nullptr, "Vehicle cam collision"},
        {"VehicleCamEnd", "Camera", 0x525E3C, 6, 0, nullptr, nullptr, "Vehicle cam end"},
        {"VehicleLookBehind", "Camera", 0x5207E3, 6, 0, nullptr, nullptr, "Vehicle look behind"},
        {"VehicleLookAside", "Camera", 0x520F70, 6, 0, nullptr, nullptr, "Vehicle look aside"},
        {"CCollision__CheckCameraCollisionObjects", "Camera", 0x41AB8E, 5, 0, nullptr, nullptr, "Camera collision check"},
    };

    //=========================================================================
    // CATEGORY: WORLD/COLLISION
    //=========================================================================

    static const AddressEntry WORLD_HOOKS[] = {
        {"CWorld::ProcessLineOfSight", "World", 0x56BA00, 12, 0, "game_sa/World.cpp", nullptr, "Line of sight"},
        {"CWorld_ProcessVerticalLineSectorList", "World", 0x563357, 6, 0, nullptr, nullptr, "Vertical line sector"},
        {"CWorld_SetWorldOnFire", "World", 0x56B983, 6, 0, nullptr, nullptr, "Set world on fire"},
        {"CWorld_GetIsLineOfSightClear", "World", 0x56A490, 12, 0, nullptr, nullptr, "Line of sight clear"},
        {"CWorld_TriggerExplosion", "World", 0x56B82E, 8, 0, nullptr, nullptr, "Trigger explosion"},
        {"CWorld_TriggerExplosionSectorList", "World", 0x5677F4, 7, 0, nullptr, nullptr, "Explosion sector list"},
        {"CWorld_RemoveFallenPeds", "World", 0x565D0D, 5, 0, nullptr, nullptr, "Remove fallen peds"},
        {"CWorld_RemoveFallenCars", "World", 0x565F52, 5, 0, nullptr, nullptr, "Remove fallen cars"},
        {"CWorld__FindObjectsKindaCollidingSectorList", "World", 0x56508C, 10, 0, nullptr, nullptr, "Colliding sector list"},
        {"CWorld_LOD_SETUP", "World", 0x406224, 5, 0, nullptr, nullptr, "LOD setup 1"},
        {"CWorld_LOD_SETUP2", "World", 0x406326, 5, 0, nullptr, nullptr, "LOD setup 2"},
        {"AddBuildingInstancesToWorld_CWorldAdd", "World", 0x5B5348, 5, 0, nullptr, nullptr, "Add building instances"},
        {"LoadIPLInstance", "World", 0x4061E8, 5, 0, nullptr, nullptr, "Load IPL instance"},
    };

    //=========================================================================
    // CATEGORY: EXPLOSIONS/FIRE
    //=========================================================================

    static const AddressEntry EXPLOSION_HOOKS[] = {
        {"CExplosion_AddExplosion", "Explosion", 0x736A50, 5, 0, "game_sa/Fx/CExplosion.cpp", nullptr, "Add explosion"},
        {"CExplosion_Update", "Explosion", 0x7377D3, 5, 0, nullptr, nullptr, "Explosion update"},
        {"CFireManager__StartFire", "Explosion", 0x53A050, 5, 0, "game_sa/Fire/CFireManager.cpp", nullptr, "Start fire"},
        {"CFire_ProcessFire", "Explosion", 0x53AC1A, 5, 0, "game_sa/Fire/CFire.cpp", nullptr, "Process fire"},
        {"CFire_ProcessFire_Dummy", "Explosion", 0x53A714, 5, 0, nullptr, nullptr, "Process fire dummy"},
        {"OccupiedVehicleBurnCheck", "Explosion", 0x570C84, 6, 0, nullptr, nullptr, "Occupied burn check"},
        {"UnoccupiedVehicleBurnCheck", "Explosion", 0x6A76DC, 8, 0, nullptr, nullptr, "Unoccupied burn check"},
        {"ApplyCarBlowHop", "Explosion", 0x6B3816, 6, 0, nullptr, nullptr, "Car blow hop"},
        {"CTaskSimplePlayerOnFire_ProcessPed", "Explosion", 0x6336DA, 6, 0, nullptr, nullptr, "Player on fire"},
    };

    //=========================================================================
    // CATEGORY: ANIMATION SYSTEM
    //=========================================================================

    static const AddressEntry ANIMATION_HOOKS[] = {
        {"CAnimManager::BlendAnimation", "Animation", 0x4D6150, 5, 0, "game_sa/Animation/CAnimManager.cpp", nullptr, "Blend animation"},
        {"CAnimManager_AddAnimation", "Animation", 0x4D3AA0, 5, 0, nullptr, nullptr, "Add animation"},
        {"CAnimManager_AddAnimationAndSync", "Animation", 0x4D3B30, 5, 0, nullptr, nullptr, "Add animation sync"},
        {"CAnimManager_BlendAnimation_Hierarchy", "Animation", 0x4D453E, 5, 0, nullptr, nullptr, "Blend hierarchy"},
        {"CAnimManager__BlendAnimation_CrashFix", "Animation", 0x4D4610, 7, 0, nullptr, nullptr, "Blend crash fix"},
        {"CAnimManager_CreateAnimAssocGroups", "Animation", 0x4D3D52, 5, 0, nullptr, nullptr, "Create assoc groups"},
        {"RpAnimBlendClumpGetAssociation", "Animation", 0x4D68B0, 5, 0, nullptr, nullptr, "Get association"},
        {"RpAnimBlendClumpGetFirstAssociation", "Animation", 0x4D6A70, 6, 0, nullptr, nullptr, "Get first assoc"},
        {"RpAnimBlendClumpUpdateAnimations", "Animation", 0x4D34F0, 5, 0, nullptr, nullptr, "Update animations"},
        {"CAnimBlendAssociation_SetCurrentTime", "Animation", 0x4CEA80, 8, 0, nullptr, nullptr, "Set current time"},
        {"CAnimBlendAssoc_destructor", "Animation", 0x4CECF0, 5, 0, nullptr, nullptr, "Assoc destructor"},
        {"CAnimBlendAssocGroupCopyAnimation", "Animation", 0x4CE130, 5, 0, nullptr, nullptr, "Copy animation"},
        {"CheckAnimMatrix", "Animation", 0x7C5A5C, 5, 0, nullptr, nullptr, "Check anim matrix"},
        {"CAnimBlendNode_GetCurrentTranslation", "Animation", 0x4CFCB5, 6, 0, nullptr, nullptr, "Get current translation"},
        {"GetAnimHierarchyFromSkinClump", "Animation", 0x734A5D, 7, 0, nullptr, nullptr, "Get anim hierarchy"},
        {"CTaskSimpleRunNamedAnimDestructor", "Animation", 0x61BEF0, 8, 0, nullptr, nullptr, "Named anim destructor"},
    };

    //=========================================================================
    // CATEGORY: TASK SYSTEM
    //=========================================================================

    static const AddressEntry TASK_HOOKS[] = {
        {"CTaskSimplePlayerOnFoot__MakeAbortable", "Task", 0x68584D, 6, 0, "game_sa/Tasks/", nullptr, "Make abortable"},
        {"CTaskSimplePlayerOnFoot_ProcessPlayerWeapon", "Task", 0x6859A0, 7, 0, nullptr, nullptr, "Process player weapon"},
        {"CTaskSimplePlayerOnFoot_ProcessWeaponFire", "Task", 0x685ABA, 5, 0, nullptr, nullptr, "Process weapon fire"},
        {"CTaskSimpleUseGun__SetMoveAnim", "Task", 0x61E4F2, 6, 0, nullptr, nullptr, "Set move anim"},
        {"CTaskSimpleUsegun_ProcessPed", "Task", 0x62A380, 5, 0, nullptr, nullptr, "Use gun process ped"},
        {"CTaskSimpleGangDriveBy__ProcessPed", "Task", 0x62D5A7, 5, 0, nullptr, nullptr, "Gang drive-by"},
        {"CTaskSimpleGangDriveBy__PlayerTarget", "Task", 0x621A57, 5, 0, nullptr, nullptr, "Drive-by player target"},
        {"CTaskComplexJump__CreateSubTask", "Task", 0x67DABE, 5, 0, nullptr, nullptr, "Jump create subtask"},
        {"CTaskSimpleSwim_ProcessSwimmingResistance", "Task", 0x68A4EF, 6, 0, nullptr, nullptr, "Swimming resistance"},
        {"CTaskSimpleSwim__ProcessEffects", "Task", 0x68AD3B, 6, 0, nullptr, nullptr, "Swim effects"},
        {"CTaskSimpleSwim__ProcessEffectsBubbleFix", "Task", 0x68AC31, 7, 0, nullptr, nullptr, "Swim bubble fix"},
        {"CTaskSimpleJetpack_ProcessInput", "Task", 0x67E7F1, 5, 0, nullptr, nullptr, "Jetpack input"},
        {"CTaskComplexCarSlowBeDraggedOut_CreateFirstSubTask", "Task", 0x6485AC, 6, 0, nullptr, nullptr, "Dragged out subtask"},
        {"CTaskComplexCarSlowBeDraggedOutAndStandUp__CreateFirstSubTask", "Task", 0x648AAF, 6, 0, nullptr, nullptr, "Dragged stand up"},
        {"CTaskManager::GetActiveTask", "Task", 0x681720, 5, 0, "game_sa/Tasks/CTaskManager.cpp", nullptr, "Get active task"},
        {"CEventHandler_ComputeKnockOffBikeResponse", "Task", 0x4BA06F, 6, 0, nullptr, nullptr, "Knock off bike"},
        {"ComputeDamageResponse_StartChoking", "Task", 0x4C05B9, 5, 0, nullptr, nullptr, "Start choking"},
    };

    //=========================================================================
    // CATEGORY: IK (INVERSE KINEMATICS)
    //=========================================================================

    static const AddressEntry IK_HOOKS[] = {
        {"IKChainManager_PointArm", "IK", 0x618B66, 5, 0, nullptr, nullptr, "Point arm"},
        {"IKChainManager_LookAt", "IK", 0x618970, 5, 0, nullptr, nullptr, "Look at"},
        {"IKChainManager_SkipAim", "IK", 0x62AEE7, 5, 0, nullptr, nullptr, "Skip aim"},
    };

    //=========================================================================
    // CATEGORY: AUDIO SYSTEM
    //=========================================================================

    static const AddressEntry AUDIO_HOOKS[] = {
        {"CAERadioTrackManager__ChooseMusicTrackIndex", "Audio", 0x4EA296, 10, 0, nullptr, nullptr, "Choose music track"},
        {"CAEVehicleAudioEntity__ProcessDummyHeli", "Audio", 0x4FE9B9, 6, 0, nullptr, nullptr, "Dummy heli audio"},
        {"CAEVehicleAudioEntity__ProcessDummyProp", "Audio", 0x4FD96D, 6, 0, nullptr, nullptr, "Dummy prop audio"},
        {"CAEAmbienceTrackManager__UpdateAmbienceTrackAndVolume_StartRadio", "Audio", 0x4D7198, 5, 0, nullptr, nullptr, "Start radio"},
        {"CAEAmbienceTrackManager__UpdateAmbienceTrackAndVolume_StopRadio", "Audio", 0x4D71E7, 5, 0, nullptr, nullptr, "Stop radio"},
        {"CVehicleAudio_ProcessSirenSound1", "Audio", 0x501FC2, 5, 0, nullptr, nullptr, "Siren sound 1"},
        {"CVehicleAudio_ProcessSirenSound2", "Audio", 0x502067, 5, 0, nullptr, nullptr, "Siren sound 2"},
        {"CVehicleAudio_ProcessSirenSound3", "Audio", 0x5021AE, 5, 0, nullptr, nullptr, "Siren sound 3"},
        {"CVehicleAudio_ProcessSirenSound", "Audio", 0x4F62BB, 5, 0, nullptr, nullptr, "Process siren"},
    };

    //=========================================================================
    // CATEGORY: FX SYSTEM
    //=========================================================================

    static const AddressEntry FX_HOOKS[] = {
        {"FxManager_CreateFxSystem", "FX", 0x4A9BE0, 8, 0, "game_sa/Fx/FxManager.cpp", nullptr, "Create FX system"},
        {"FxManager_DestroyFxSystem", "FX", 0x4A9810, 7, 0, nullptr, nullptr, "Destroy FX system"},
        {"FxManager_c__DestroyFxSystem", "FX", 0x4A989A, 5, 0, nullptr, nullptr, "Destroy FX (alt)"},
        {"Fx_AddBulletImpact", "FX", 0x49F3E8, 5, 0, nullptr, nullptr, "Add bullet impact"},
        {"FxSystemBP_c__Load", "FX", 0x5C0A15, 19, 0, nullptr, nullptr, "FX system BP load"},
        {"FxPrim_c__Enable", "FX", 0x4A9F50, 10, 0, nullptr, nullptr, "FX prim enable"},
    };

    //=========================================================================
    // CATEGORY: RENDERWARE RESOURCES
    //=========================================================================

    static const AddressEntry RENDERWARE_HOOKS[] = {
        {"RwTextureCreate", "RenderWare", 0x7F37C0, 5, 0, nullptr, nullptr, "Create texture"},
        {"RwTextureDestroy", "RenderWare", 0x7F3820, 5, 0, nullptr, nullptr, "Destroy texture"},
        {"RwRasterCreate", "RenderWare", 0x7FB230, 5, 0, nullptr, nullptr, "Create raster"},
        {"RwRasterDestroy", "RenderWare", 0x7FB020, 5, 0, nullptr, nullptr, "Destroy raster"},
        {"RwGeometryCreate", "RenderWare", 0x74CA90, 7, 0, nullptr, nullptr, "Create geometry"},
        {"RwGeometryDestroy", "RenderWare", 0x74CCC0, 5, 0, nullptr, nullptr, "Destroy geometry"},
        {"RpClumpForAllAtomics", "RenderWare", 0x749B70, 6, 0, nullptr, nullptr, "Clump for all atomics"},
        {"RwMatrixMultiply", "RenderWare", 0x7F18B0, 6, 0, nullptr, nullptr, "Matrix multiply"},
        {"CCustomRoadsignMgr__RenderRoadsignAtomic", "RenderWare", 0x6FF35B, 5, 0, nullptr, nullptr, "Render roadsign"},
    };

    //=========================================================================
    // CATEGORY: GLASS SYSTEM
    //=========================================================================

    static const AddressEntry GLASS_HOOKS[] = {
        {"CGlass_WindowRespondsToCollision", "Glass", 0x71BC40, 8, 0, "game_sa/Glass/CGlass.cpp", nullptr, "Window collision"},
        {"CGlass__BreakGlassPhysically", "Glass", 0x71D14B, 5, 0, nullptr, nullptr, "Break glass physically"},
        {"CGlass_WindowRespondsToExplosion", "Glass", 0x71C255, 5, 0, nullptr, nullptr, "Window explosion"},
        {"CFallingGlassPane__Update_A", "Glass", 0x71AABF, 6, 0, nullptr, nullptr, "Falling glass A"},
        {"CFallingGlassPane__Update_B", "Glass", 0x71AAEA, 6, 0, nullptr, nullptr, "Falling glass B"},
        {"CFallingGlassPane__Update_C", "Glass", 0x71AB29, 6, 0, nullptr, nullptr, "Falling glass C"},
    };

    //=========================================================================
    // CATEGORY: WATER SYSTEM
    //=========================================================================

    static const AddressEntry WATER_HOOKS[] = {
        {"CWaterLevel_TestLineAgainstWater", "Water", 0x6E61B0, 10, 0, nullptr, nullptr, "Test line against water"},
        {"CWaterCannon__Update_OncePerFrame", "Water", 0x72A29B, 5, 0, nullptr, nullptr, "Water cannon update"},
        {"CWaterCannon__Update_OncePerFrame_PushPedFix", "Water", 0x72A37B, 6, 0, nullptr, nullptr, "Water cannon push fix"},
        {"CWaterCannon__Render_FxFix", "Water", 0x729437, 5, 0, nullptr, nullptr, "Water cannon FX fix"},
        {"CWaterCannon__Render", "Water", 0x72932A, 5, 0, nullptr, nullptr, "Water cannon render"},
        {"cBuoyancy__AddSplashParticles", "Water", 0x6C34E0, 6, 0, nullptr, nullptr, "Buoyancy splash"},
        {"CWeather__AddRain", "Water", 0x72AAA8, 6, 0, nullptr, nullptr, "Add rain"},
    };

    //=========================================================================
    // CATEGORY: DIRECT3D (To be replaced with OpenGL ES)
    //=========================================================================

    static const AddressEntry D3D_HOOKS[] = {
        {"PreCreateDevice", "D3D", 0x7F675B, 6, 0, nullptr, nullptr, "Pre create D3D device"},
        {"PostCreateDevice", "D3D", 0x7F6784, 6, 0, nullptr, nullptr, "Post create D3D device"},
        {"CTrafficLights_DisplayActualLight", "D3D", 0x49E1D9, 6, 0, nullptr, nullptr, "Traffic lights display"},
    };

    //=========================================================================
    // CATEGORY: FILE SYSTEM
    //=========================================================================

    static const AddressEntry FILE_HOOKS[] = {
        {"Rtl_fopen", "File", 0x8232D8, 6, 0, nullptr, nullptr, "File open"},
        {"Rtl_fclose", "File", 0x82318B, 6, 0, nullptr, nullptr, "File close"},
        {"CStreamingRemoveModel", "File", 0x4089A0, 6, 0, nullptr, nullptr, "Remove model from streaming"},
        {"CallCStreamingInfoAddToList", "File", 0x408962, 5, 0, nullptr, nullptr, "Add to streaming list"},
        {"CStreamingLoadRequestedModels", "File", 0x5670A0, 5, 0, nullptr, nullptr, "Load requested models"},
        {"LoadingPlayerImgDir", "File", 0x5A69E3, 5, 0, nullptr, nullptr, "Load player img dir"},
    };

    //=========================================================================
    // DATABASE REGISTRATION FUNCTION
    //=========================================================================

    /**
     * Register all addresses in the database with the AddressMapper
     */
    inline void RegisterAllAddresses()
    {
        auto& mapper = AddressMapper::Instance();

        // Helper macro for registration
        #define REGISTER_ARRAY(arr) \
            for (const auto& entry : arr) { \
                mapper.Register(entry.name, entry.x86Address, entry.signature); \
            }

        REGISTER_ARRAY(CORE_HOOKS);
        REGISTER_ARRAY(ENTITY_HOOKS);
        REGISTER_ARRAY(VEHICLE_HOOKS);
        REGISTER_ARRAY(VEHICLE_VTABLES);
        REGISTER_ARRAY(PED_HOOKS);
        REGISTER_ARRAY(WEAPON_HOOKS);
        REGISTER_ARRAY(RENDERING_HOOKS);
        REGISTER_ARRAY(CAMERA_HOOKS);
        REGISTER_ARRAY(WORLD_HOOKS);
        REGISTER_ARRAY(EXPLOSION_HOOKS);
        REGISTER_ARRAY(ANIMATION_HOOKS);
        REGISTER_ARRAY(TASK_HOOKS);
        REGISTER_ARRAY(IK_HOOKS);
        REGISTER_ARRAY(AUDIO_HOOKS);
        REGISTER_ARRAY(FX_HOOKS);
        REGISTER_ARRAY(RENDERWARE_HOOKS);
        REGISTER_ARRAY(GLASS_HOOKS);
        REGISTER_ARRAY(WATER_HOOKS);
        REGISTER_ARRAY(D3D_HOOKS);
        REGISTER_ARRAY(FILE_HOOKS);

        #undef REGISTER_ARRAY
    }

    /**
     * Get statistics about the address database
     */
    inline void GetDatabaseStats(size_t& total, size_t& withSignatures, size_t& withGTARef)
    {
        total = 0;
        withSignatures = 0;
        withGTARef = 0;

        #define COUNT_ARRAY(arr) \
            for (const auto& entry : arr) { \
                total++; \
                if (entry.signature) withSignatures++; \
                if (entry.gtaReversedRef) withGTARef++; \
            }

        COUNT_ARRAY(CORE_HOOKS);
        COUNT_ARRAY(ENTITY_HOOKS);
        COUNT_ARRAY(VEHICLE_HOOKS);
        COUNT_ARRAY(VEHICLE_VTABLES);
        COUNT_ARRAY(PED_HOOKS);
        COUNT_ARRAY(WEAPON_HOOKS);
        COUNT_ARRAY(RENDERING_HOOKS);
        COUNT_ARRAY(CAMERA_HOOKS);
        COUNT_ARRAY(WORLD_HOOKS);
        COUNT_ARRAY(EXPLOSION_HOOKS);
        COUNT_ARRAY(ANIMATION_HOOKS);
        COUNT_ARRAY(TASK_HOOKS);
        COUNT_ARRAY(IK_HOOKS);
        COUNT_ARRAY(AUDIO_HOOKS);
        COUNT_ARRAY(FX_HOOKS);
        COUNT_ARRAY(RENDERWARE_HOOKS);
        COUNT_ARRAY(GLASS_HOOKS);
        COUNT_ARRAY(WATER_HOOKS);
        COUNT_ARRAY(D3D_HOOKS);
        COUNT_ARRAY(FILE_HOOKS);

        #undef COUNT_ARRAY
    }

} // namespace MTA::Android::Signatures

#endif // ADDRESS_DATABASE_H
