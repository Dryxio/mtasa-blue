/*
 * MTA:SA Android - ARM Address Mappings
 *
 * Cross-referenced from SA-MP 2.10 Android (dumps_libGTASA_32and64)
 * ARM32 addresses from: dump 2.1 (32).txt
 * ARM64 addresses from: DUMP 2.1 (64).txt
 *
 * Generated: January 9, 2026
 * Source: https://github.com/kuzia15/SA-MP-2.10
 *
 * Usage:
 *   uintptr_t baseAddr = GetLibGTASABase();
 *   void* func = (void*)(baseAddr + ARM32_CPed_ProcessControl);
 */

#ifndef ARM_ADDRESS_MAP_H
#define ARM_ADDRESS_MAP_H

#include <cstdint>

namespace MTA::Android::ARM
{
    //=========================================================================
    // Architecture Detection
    //=========================================================================

    #if defined(__aarch64__)
        #define MTA_ARM64 1
        #define MTA_ARM32 0
    #elif defined(__arm__)
        #define MTA_ARM64 0
        #define MTA_ARM32 1
    #else
        #error "Unsupported architecture"
    #endif

    //=========================================================================
    // ARM32 Function Addresses (armeabi-v7a)
    // From: dump 2.1 (32).txt
    // Note: Add +1 for Thumb mode functions
    //=========================================================================

    namespace ARM32
    {
        //---------------------------------------------------------------------
        // CPlayerPed
        //---------------------------------------------------------------------
        constexpr uint32_t CPlayerPed_ProcessControl          = 0x4C47E9;
        constexpr uint32_t CPlayerPed_SetupPlayerPed          = 0x4C39A5;
        constexpr uint32_t CPlayerPed_SetInitialState         = 0x4C37B5;
        constexpr uint32_t CPlayerPed_DeactivatePlayerPed     = 0x4C3AD5;
        constexpr uint32_t CPlayerPed_ReactivatePlayerPed     = 0x4C3AED;
        constexpr uint32_t CPlayerPed_RemovePlayerPed         = 0x4C3A61;
        constexpr uint32_t CPlayerPed_SetWantedLevel          = 0x4C9719;
        constexpr uint32_t CPlayerPed_CheatWantedLevel        = 0x4C9739;
        constexpr uint32_t CPlayerPed_ClearWeaponTarget       = 0x4C58E5;
        constexpr uint32_t CPlayerPed_ProcessAnimGroups       = 0x4C5E61;
        constexpr uint32_t CPlayerPed_HandlePlayerBreath      = 0x4C56F5;
        constexpr uint32_t CPlayerPed_HandleSprintEnergy      = 0x4C5809;
        constexpr uint32_t CPlayerPed_ProcessPlayerWeapon     = 0x4C682F;
        constexpr uint32_t CPlayerPed_ProcessWeaponSwitch     = 0x4C5919;
        constexpr uint32_t CPlayerPed_FindWeaponLockOnTarget  = 0x4C6D65;
        constexpr uint32_t CPlayerPed_Compute3rdPersonMouseTarget = 0x4C901D;
        constexpr uint32_t CPlayerPed_GetPadFromPlayer        = 0x4C4749;
        constexpr uint32_t CPlayerPed_Busted                  = 0x4C970D;
        constexpr uint32_t CPlayerPed_Load                    = 0x4851E9;
        constexpr uint32_t CPlayerPed_Save                    = 0x485163;

        //---------------------------------------------------------------------
        // CPed
        //---------------------------------------------------------------------
        constexpr uint32_t CPed_ProcessControl                = 0x4A2541;
        constexpr uint32_t CPed_UpdatePosition                = 0x4A1999;
        constexpr uint32_t CPed_SetModelIndex                 = 0x49FAD5;
        constexpr uint32_t CPed_Initialise                    = 0x49FA19;
        constexpr uint32_t CPed_GiveWeapon                    = 0x49F589;
        constexpr uint32_t CPed_ClearWeapons                  = 0x49F837;
        constexpr uint32_t CPed_ClearWeapon                   = 0x4A52D9;
        constexpr uint32_t CPed_SetAimFlag_Entity             = 0x4A12F7;
        constexpr uint32_t CPed_SetAimFlag_Float              = 0x4A125B;
        constexpr uint32_t CPed_ClearAimFlag                  = 0x4A1031;
        constexpr uint32_t CPed_SetLookFlag_Entity            = 0x4A11C9;
        constexpr uint32_t CPed_SetLookFlag_Float             = 0x4A114D;
        constexpr uint32_t CPed_ClearLookFlag                 = 0x4A1081;
        constexpr uint32_t CPed_SetPedState                   = 0x4A1EA5;
        constexpr uint32_t CPed_SetPedStats                   = 0x49FC4D;
        constexpr uint32_t CPed_SetMoveState                  = 0x4A0C31;
        constexpr uint32_t CPed_SetMoveAnim                   = 0x4A0C39;
        constexpr uint32_t CPed_IsPedInControl                = 0x4A18D1;
        constexpr uint32_t CPed_IsPedShootable                = 0x4A4BBF;
        constexpr uint32_t CPed_CanBeDeleted                  = 0x4A4C89;
        constexpr uint32_t CPed_CanBeArrested                 = 0x4A4C0D;
        constexpr uint32_t CPed_CanSetPedState                = 0x4A4BED;
        constexpr uint32_t CPed_IsPointerValid                = 0x4A7371;
        constexpr uint32_t CPed_GetWeaponSlot                 = 0x4A5179;
        constexpr uint32_t CPed_GetWeaponSkill                = 0x4A5653;
        constexpr uint32_t CPed_GetWeaponSkill_NoParam        = 0x4A12E3;
        constexpr uint32_t CPed_SetWeaponSkill                = 0x4A56E7;
        constexpr uint32_t CPed_GetBonePosition               = 0x4A4B0D;
        constexpr uint32_t CPed_AddWeaponModel                = 0x4A4CE9;
        constexpr uint32_t CPed_RemoveBodyPart                = 0x4AD00D;
        constexpr uint32_t CPed_KillPedWithCar                = 0x4AD139;
        constexpr uint32_t CPed_DoFootLanded                  = 0x4A4381;
        constexpr uint32_t CPed_DoGunFlash                    = 0x4A54DD;
        constexpr uint32_t CPed_PlayFootSteps                 = 0x4A2DE5;
        constexpr uint32_t CPed_ProcessBuoyancy               = 0x4A1F01;
        constexpr uint32_t CPed_SetupLighting                 = 0x410645;
        constexpr uint32_t CPed_RemoveLighting                = 0x4106F5;
        constexpr uint32_t CPed_DeleteRwObject                = 0x49FC87;
        constexpr uint32_t CPed_GetHoldingTask                = 0x4A7E71;
        constexpr uint32_t CPed_AttachPedToBike               = 0x4A7CB5;
        constexpr uint32_t CPed_PutOnGoggles                  = 0x4A4F55;
        constexpr uint32_t CPed_TakeOffGoggles                = 0x4A503D;
        constexpr uint32_t CPed_CanSeeEntity                  = 0x49FC8D;
        constexpr uint32_t CPed_EnablePedSpeech               = 0x4AC97D;
        constexpr uint32_t CPed_DisablePedSpeech              = 0x4AC975;
        constexpr uint32_t CPed_GetPedTalking                 = 0x4AC96D;
        constexpr uint32_t CPed_SetRadioStation               = 0x4A7861;

        //---------------------------------------------------------------------
        // CVehicle
        //---------------------------------------------------------------------
        constexpr uint32_t CVehicle_SetupRender               = 0x582335;
        constexpr uint32_t CVehicle_ResetAfterRender          = 0x5823F1;
        constexpr uint32_t CVehicle_SetModelIndex             = 0x582A3D;
        constexpr uint32_t CVehicle_DeleteRwObject            = 0x581C3D;
        constexpr uint32_t CVehicle_SetupLighting             = 0x410711;
        constexpr uint32_t CVehicle_RemoveLighting            = 0x4107C1;
        constexpr uint32_t CVehicle_AddPassenger              = 0x584339;
        constexpr uint32_t CVehicle_AddPassenger_Seat         = 0x584439;
        constexpr uint32_t CVehicle_RemovePassenger           = 0x584549;
        constexpr uint32_t CVehicle_SetUpDriver               = 0x5848C5;
        constexpr uint32_t CVehicle_RemoveDriver              = 0x5847CD;
        constexpr uint32_t CVehicle_SetupPassenger            = 0x5848FF;
        constexpr uint32_t CVehicle_InflictDamage             = 0x583CCD;
        constexpr uint32_t CVehicle_ProcessWheel              = 0x582D01;
        constexpr uint32_t CVehicle_ProcessBikeWheel          = 0x583359;
        constexpr uint32_t CVehicle_ActivateBomb              = 0x585589;
        constexpr uint32_t CVehicle_ExtinguishCarFire         = 0x585519;
        constexpr uint32_t CVehicle_KillPedsInVehicle         = 0x584ADF;
        constexpr uint32_t CVehicle_ProcessCarAlarm           = 0x585209;
        constexpr uint32_t CVehicle_ProcessOpenDoor           = 0x59189D;
        constexpr uint32_t CVehicle_ProcessWeapons            = 0x58DF19;
        constexpr uint32_t CVehicle_DoVehicleLights           = 0x5910E1;
        constexpr uint32_t CVehicle_DoHeadLightBeam           = 0x590741;
        constexpr uint32_t CVehicle_DoHeadLightEffect         = 0x59032D;
        constexpr uint32_t CVehicle_DoTailLightEffect         = 0x590E15;
        constexpr uint32_t CVehicle_DoBoatSplashes            = 0x589CC1;
        constexpr uint32_t CVehicle_DoSunGlare                = 0x58A279;
        constexpr uint32_t CVehicle_UpdateClumpAlpha          = 0x58A241;
        constexpr uint32_t CVehicle_FlyingControl             = 0x585701;
        constexpr uint32_t CVehicle_FirePlaneGuns             = 0x58E501;
        constexpr uint32_t CVehicle_DoBladeCollision          = 0x587B21;
        constexpr uint32_t CVehicle_GetRemapIndex             = 0x581EF5;
        constexpr uint32_t CVehicle_GetTowBarPos              = 0x58D765;
        constexpr uint32_t CVehicle_GetTowHitchPos            = 0x58D711;
        constexpr uint32_t CVehicle_AddUpgrade                = 0x58CB2D;
        constexpr uint32_t CVehicle_GetUpgrade                = 0x58CAAD;
        constexpr uint32_t CVehicle_RemoveUpgrade             = 0x58D0E9;
        constexpr uint32_t CVehicle_AddVehicleUpgrade         = 0x58C66D;
        constexpr uint32_t CVehicle_GetNewSteeringAmt         = 0x591D11;
        constexpr uint32_t CVehicle_FindWheelWidth            = 0x58DE3D;
        constexpr uint32_t CVehicle_GetPlaneNumGuns           = 0x58F53D;
        constexpr uint32_t CVehicle_SetHasslePosId            = 0x58D517;
        constexpr uint32_t CVehicle_RemoveWinch               = 0x58D621;
        constexpr uint32_t CVehicle_UpdateWinch               = 0x58D549;

        //---------------------------------------------------------------------
        // CAutomobile
        //---------------------------------------------------------------------
        constexpr uint32_t CAutomobile_ProcessControl         = 0x553E45;
        constexpr uint32_t CAutomobile_SetModelIndex          = 0x54EBA9;
        constexpr uint32_t CAutomobile_SetupModelNodes        = 0x54EBF7;
        constexpr uint32_t CAutomobile_VehicleDamage          = 0x551501;
        constexpr uint32_t CAutomobile_ProcessBuoyancy        = 0x553431;
        constexpr uint32_t CAutomobile_ProcessSuspension      = 0x55F431;
        constexpr uint32_t CAutomobile_ResetSuspension        = 0x559509;
        constexpr uint32_t CAutomobile_UpdateWheelMatrix      = 0x559535;
        constexpr uint32_t CAutomobile_HydraulicControl       = 0x54F189;
        constexpr uint32_t CAutomobile_NitrousControl         = 0x556951;
        constexpr uint32_t CAutomobile_BoostJumpControl       = 0x556895;
        constexpr uint32_t CAutomobile_TankControl            = 0x555E59;
        constexpr uint32_t CAutomobile_TowTruckControl        = 0x55665D;
        constexpr uint32_t CAutomobile_FireTruckControl       = 0x5CBC19;
        constexpr uint32_t CAutomobile_ProcessHarvester       = 0x5574F9;
        constexpr uint32_t CAutomobile_ScanForCrimes          = 0x5589A5;
        constexpr uint32_t CAutomobile_PlayCarHorn            = 0x55D945;
        constexpr uint32_t CAutomobile_PlayHornIfNecessary    = 0x558F51;
        constexpr uint32_t CAutomobile_ReduceHornCounter      = 0x553421;
        constexpr uint32_t CAutomobile_SetTaxiLight           = 0x555CAD;
        constexpr uint32_t CAutomobile_SetAllTaxiLights       = 0x55D935;
        constexpr uint32_t CAutomobile_SetTowLink             = 0x55E779;
        constexpr uint32_t CAutomobile_BreakTowLink           = 0x55ECDD;
        constexpr uint32_t CAutomobile_GetTowBarPos           = 0x55E5D5;
        constexpr uint32_t CAutomobile_GetTowHitchPos         = 0x55E56D;
        constexpr uint32_t CAutomobile_SetDoorDamage          = 0x552E3D;
        constexpr uint32_t CAutomobile_SetPanelDamage         = 0x552CDD;
        constexpr uint32_t CAutomobile_SetBumperDamage        = 0x5525CD;
        constexpr uint32_t CAutomobile_SetTotalDamage         = 0x5532A5;
        constexpr uint32_t CAutomobile_SetRandomDamage        = 0x553049;
        constexpr uint32_t CAutomobile_KnockPedOutCar         = 0x55ED67;
        constexpr uint32_t CAutomobile_CloseAllDoors          = 0x55EED9;
        constexpr uint32_t CAutomobile_BlowUpCarsInPath       = 0x55E2D9;
        constexpr uint32_t CAutomobile_DoNitroEffect          = 0x55E395;
        constexpr uint32_t CAutomobile_StopNitroEffect        = 0x54EB21;
        constexpr uint32_t CAutomobile_DoHeliDustEffect       = 0x55FFAD;
        constexpr uint32_t CAutomobile_SetUpWheelColModel     = 0x55D28D;
        constexpr uint32_t CAutomobile_SetHeliOrientation     = 0x5524B5;
        constexpr uint32_t CAutomobile_PlaceOnRoadProperly    = 0x55E935;
        constexpr uint32_t CAutomobile_PopBootUsingPhysics    = 0x55EE0F;
        constexpr uint32_t CAutomobile_SetBusDoorTimer        = 0x55E2AD;
        constexpr uint32_t CAutomobile_GetCarRoll             = 0x55FE51;
        constexpr uint32_t CAutomobile_GetCarPitch            = 0x55FEB1;
        constexpr uint32_t CAutomobile_FindWheelWidth         = 0x55FEED;
        constexpr uint32_t CAutomobile_GetNumContactWheels    = 0x560407;
        constexpr uint32_t CAutomobile_Fix                    = 0x55D5C0;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CAutomobile_SetupDamageAfterLoad   = 0x55D886;  // From SAMP_ANDROID_REFERENCE.md

        //---------------------------------------------------------------------
        // CCamera
        //---------------------------------------------------------------------
        constexpr uint32_t CCamera_CamControl                 = 0x3D693D;
        constexpr uint32_t CCamera_FindCamFOV                 = 0x3DB765;
        constexpr uint32_t CCamera_ProcessFade                = 0x3DE3DD;
        constexpr uint32_t CCamera_ProcessShake               = 0x3DE6AD;
        constexpr uint32_t CCamera_ProcessShake_Float         = 0x3E2151;
        constexpr uint32_t CCamera_ProcessFOVLerp             = 0x3E23C1;
        constexpr uint32_t CCamera_ProcessFOVLerp_Float       = 0x3E2495;
        constexpr uint32_t CCamera_ProcessMusicFade           = 0x3DE4FD;
        constexpr uint32_t CCamera_SetRwCamera                = 0x3E161D;
        constexpr uint32_t CCamera_TakeControl                = 0x3E1715;
        constexpr uint32_t CCamera_GetScreenRect              = 0x3DB425;
        constexpr uint32_t CCamera_SetColVarsPed              = 0x3D4715;
        constexpr uint32_t CCamera_SetFadeColour              = 0x3E150D;
        constexpr uint32_t CCamera_SetMotionBlur              = 0x3D4AF9;
        constexpr uint32_t CCamera_AddShakeSimple             = 0x3E1C39;
        constexpr uint32_t CCamera_FinishCutscene             = 0x3DB6C1;
        constexpr uint32_t CCamera_SetToSphereMap             = 0x3DF2FD;
        constexpr uint32_t CCamera_ImproveNearClip            = 0x3D2AD5;
        constexpr uint32_t CCamera_IsSphereVisible            = 0x2FC0ED;
        constexpr uint32_t CCamera_LoadPathSplines            = 0x462C4D;
        constexpr uint32_t CCamera_SetWideScreenOn            = 0x3E163F;
        constexpr uint32_t CCamera_SetWideScreenOff           = 0x3D9EC9;
        constexpr uint32_t CCamera_StartTransition            = 0x3DAA1D;
        constexpr uint32_t CCamera_AvoidTheGeometry           = 0x3D5495;
        constexpr uint32_t CCamera_GetLookDirection           = 0x3DBA5B;
        constexpr uint32_t CCamera_RenderMotionBlur           = 0x3E139F;
        constexpr uint32_t CCamera_VectorMoveLinear           = 0x3E1B45;
        constexpr uint32_t CCamera_IsItTimeForNewcam          = 0x3DBAC1;
        constexpr uint32_t CCamera_IsTargetingActive          = 0x3D9F55;
        constexpr uint32_t CCamera_IsTargetingActive_Ped      = 0x3E157D;

        //---------------------------------------------------------------------
        // CEntity
        //---------------------------------------------------------------------
        constexpr uint32_t CEntity_UpdateAnim                 = 0x3EC07D;
        constexpr uint32_t CEntity_GetColModel                = 0x3EE045;
        constexpr uint32_t CEntity_CreateEffects              = 0x3EB2A1;
        constexpr uint32_t CEntity_GetIsOnScreen              = 0x3EC199;
        constexpr uint32_t CEntity_RenderEffects              = 0x3ED1FD;
        constexpr uint32_t CEntity_SetModelIndex              = 0x3EB00D;
        constexpr uint32_t CEntity_SetupLighting              = 0x4104DD;
        constexpr uint32_t CEntity_RemoveLighting             = 0x41051D;
        constexpr uint32_t CEntity_UpdateRpHAnim              = 0x3EC047;
        constexpr uint32_t CEntity_UpdateRwFrame              = 0x3EC039;
        constexpr uint32_t CEntity_CreateRwObject             = 0x3EB90D;
        constexpr uint32_t CEntity_DeleteRwObject             = 0x3EBBFD;
        constexpr uint32_t CEntity_DestroyEffects             = 0x3EB7B1;
        constexpr uint32_t CEntity_PruneReferences            = 0x40EAB1;
        constexpr uint32_t CEntity_AttachToRwObject           = 0x3EBA81;
        constexpr uint32_t CEntity_IsEntityOccluded           = 0x5B0269;
        constexpr uint32_t CEntity_SetRwObjectAlpha           = 0x3EE325;
        constexpr uint32_t CEntity_SetupBigBuilding           = 0x3EE085;
        constexpr uint32_t CEntity_GetRandom2dEffect          = 0x3EEDA5;
        constexpr uint32_t CEntity_RegisterReference          = 0x40E871;
        constexpr uint32_t CEntity_ResolveReferences          = 0x40EB35;
        constexpr uint32_t CEntity_DetachFromRwObject         = 0x3EBB55;
        constexpr uint32_t CEntity_BuildWindSockMatrix        = 0x3ED065;
        constexpr uint32_t CEntity_CleanUpOldReference        = 0x40EAFD;
        constexpr uint32_t CEntity_HasPreRenderEffects        = 0x3EB0B1;

        //---------------------------------------------------------------------
        // CWorld
        //---------------------------------------------------------------------
        constexpr uint32_t CWorld_CastShadow                  = 0x427565;
        constexpr uint32_t CWorld_Initialise                  = 0x422E29;
        constexpr uint32_t CWorld_UseDetonator                = 0x42B8AD;
        constexpr uint32_t CWorld_SetCarsOnFire               = 0x42ACF5;
        constexpr uint32_t CWorld_SetPedsOnFire               = 0x42AA3D;
        constexpr uint32_t CWorld_ClearScanCodes              = 0x428455;
        constexpr uint32_t CWorld_SetPedsChoking              = 0x42ABC1;
        constexpr uint32_t CWorld_SetWorldOnFire              = 0x42AE05;
        constexpr uint32_t CWorld_ClearForRestart             = 0x423339;
        constexpr uint32_t CWorld_PrintCarChanges             = 0x42BB1D;
        constexpr uint32_t CWorld_SprayPaintWorld             = 0x42B0FD;
        constexpr uint32_t CWorld_RemoveFallenCars            = 0x42823D;
        constexpr uint32_t CWorld_RemoveFallenPeds            = 0x428069;
        constexpr uint32_t CWorld_TriggerExplosion            = 0x4268C9;
        constexpr uint32_t CWorld_ClearCarsFromArea           = 0x42C1C1;
        constexpr uint32_t CWorld_ClearPedsFromArea           = 0x42C355;
        constexpr uint32_t CWorld_FindObjectsInRange          = 0x4285D1;

        //---------------------------------------------------------------------
        // CWeapon
        //---------------------------------------------------------------------
        constexpr uint32_t CWeapon_Initialise                 = 0x5DB901;
        constexpr uint32_t CWeapon_FireSniper                 = 0x5DD741;
        constexpr uint32_t CWeapon_AddGunshell                = 0x5E02B9;
        constexpr uint32_t CWeapon_FireFromCar                = 0x5DEFA1;
        constexpr uint32_t CWeapon_DoDoomAiming               = 0x5DFF25;
        constexpr uint32_t CWeapon_StaticUpdate               = 0x5E2345;
        constexpr uint32_t CWeapon_IsType2Handed              = 0x5E25F1;
        constexpr uint32_t CWeapon_LaserScopeDot              = 0x5E1871;
        constexpr uint32_t CWeapon_UpdateWeapons              = 0x5DB8E9;
        constexpr uint32_t CWeapon_DoBulletImpact             = 0x5E07D9;
        constexpr uint32_t CWeapon_DoWeaponEffect             = 0x5E1755;
        constexpr uint32_t CWeapon_FireAreaEffect             = 0x5DE6A9;
        constexpr uint32_t CWeapon_FireInstantHit             = 0x5DC179;
        constexpr uint32_t CWeapon_FireProjectile             = 0x5DDECD;
        constexpr uint32_t CWeapon_TakePhotograph             = 0x5DEA19;
        constexpr uint32_t CWeapon_ShutdownWeapons            = 0x5DB8BD;
        constexpr uint32_t CWeapon_DoTankDoomAiming           = 0x5E1B4D;
        constexpr uint32_t CWeapon_IsTypeProjectile           = 0x5E260F;
        constexpr uint32_t CWeapon_StopWeaponEffect           = 0x5E2325;
        constexpr uint32_t CWeapon_FireM16_1stPerson          = 0x5DDA79;
        constexpr uint32_t CWeapon_InitialiseWeapons          = 0x5DB889;
        constexpr uint32_t CWeapon_ProcessLineOfSight         = 0x5DF749;
        constexpr uint32_t CWeapon_GenerateDamageEvent        = 0x5E1395;
        constexpr uint32_t CWeapon_DoDriveByAutoAiming        = 0x5DFB81;
        constexpr uint32_t CWeapon_SetUpPelletCol             = 0x5E0431;
        constexpr uint32_t CWeaponInfo_GetWeaponInfo          = 0x5E42E9;  // From SAMP_ANDROID_REFERENCE.md

        //---------------------------------------------------------------------
        // CStreaming
        //---------------------------------------------------------------------
        constexpr uint32_t CStreaming_ClearSlots              = 0x2D8CD5;
        constexpr uint32_t CStreaming_IsVeryBusy              = 0x2D0E0D;
        constexpr uint32_t CStreaming_ReadIniFile             = 0x472615;
        constexpr uint32_t CStreaming_RemoveModel             = 0x2D0129;
        constexpr uint32_t CStreaming_RequestFile             = 0x2D6B31;
        constexpr uint32_t CStreaming_MakeSpaceFor            = 0x2D39E5;
        constexpr uint32_t CStreaming_RemoveEntity            = 0x2D65C9;
        constexpr uint32_t CStreaming_RenderEntity            = 0x2D6591;
        constexpr uint32_t CStreaming_RequestModel            = 0x2D299D;
        constexpr uint32_t CStreaming_FlushChannels           = 0x2D4879;
        constexpr uint32_t CStreaming_InitImageList           = 0x2CF681;
        constexpr uint32_t CStreaming_IsInitialised           = 0x2CF66D;
        constexpr uint32_t CStreaming_RetryLoadFile           = 0x2D2315;
        constexpr uint32_t CStreaming_AddImageToList          = 0x2CF7D1;
        constexpr uint32_t CStreaming_GetDiscInDrive          = 0x2D26B9;
        constexpr uint32_t CStreaming_GetModelCDName          = 0x2CF5D1;
        constexpr uint32_t CStreaming_RemoveCarModel          = 0x2D2E81;
        constexpr uint32_t CStreaming_ClearFlagForAll         = 0x2D50B5;
        constexpr uint32_t CStreaming_DisableCopBikes         = 0x2D6DA9;
        constexpr uint32_t CStreaming_GetNextFileOnCd         = 0x2D3A39;
        constexpr uint32_t CStreaming_LoadCdDirectory         = 0x46C0AD;

        //---------------------------------------------------------------------
        // CExplosion
        //---------------------------------------------------------------------
        constexpr uint32_t CExplosion_Initialise              = 0x5D78C1;
        constexpr uint32_t CExplosion_AddExplosion            = 0x5D7A1D;
        constexpr uint32_t CExplosion_GetExplosionType        = 0x5D79F5;
        constexpr uint32_t CExplosion_ClearAllExplosions      = 0x5D7935;
        constexpr uint32_t CExplosion_GetExplosionPosition    = 0x5D7A09;
        constexpr uint32_t CExplosion_DoesExplosionMakeSound  = 0x5D79DD;
        constexpr uint32_t CExplosion_TestForExplosionInArea  = 0x5D90E5;
        constexpr uint32_t CExplosion_GetExplosionActiveCounter = 0x5D79AD;
        constexpr uint32_t CExplosion_RemoveAllExplosionsInArea = 0x5D917D;
        constexpr uint32_t CExplosion_ResetExplosionActiveCounter = 0x5D79C5;
        constexpr uint32_t CExplosion_Update                  = 0x5D89DD;
        constexpr uint32_t CExplosion_Shutdown                = 0x5D79A9;

        //---------------------------------------------------------------------
        // Global Game Addresses (from SAMP_ANDROID_REFERENCE.md)
        //---------------------------------------------------------------------
        constexpr uint32_t g_CCamera                          = 0x951FA8;
        constexpr uint32_t g_fx                               = 0x820520;
        constexpr uint32_t g_HUDVisibility                    = 0x819D88;
        constexpr uint32_t ms_fTimeStep                       = 0x96B500;
        constexpr uint32_t ms_fAspectRatio                    = 0xA26A90;
        constexpr uint32_t g_WaterShaderFlag                  = 0x6B7094;
        constexpr uint32_t g_WorldPlayersPtr                  = 0x6783C0;
        constexpr uint32_t g_PlayerInFocus                    = 0x679B5C;

        //---------------------------------------------------------------------
        // Game State Addresses (from SA-MP 2.10 dumps)
        //---------------------------------------------------------------------
        constexpr uint32_t gGameState                         = 0xA987C8;  // Current game state (SystemState enum)
        constexpr uint32_t DoGameState                        = 0x5E4765;  // DoGameState() function
        constexpr uint32_t MainMenuScreen_OnStartGame         = 0x29DB85;  // MainMenuScreen::OnStartGame()
        constexpr uint32_t StartGameScreen_OnNewGameCheck     = 0x2A7271;  // StartGameScreen::OnNewGameCheck()
        constexpr uint32_t CLoadingScreen_DisplayPCScreen     = 0x2A7BB1;  // CLoadingScreen::DisplayPCScreen()

        //---------------------------------------------------------------------
        // PLT Hook Addresses (from SAMP_ANDROID_REFERENCE.md)
        //---------------------------------------------------------------------
        constexpr uint32_t PLT_CCamera_Process                = 0x6717BC;
        constexpr uint32_t PLT_CPed_UpdatePosition            = 0x671458;
        constexpr uint32_t PLT_FindPlayerSpeed                = 0x671BBC;
        constexpr uint32_t PLT_CPed_GetWeaponSkill            = 0x6749D0;
        constexpr uint32_t PLT_CTaskComplexLeaveCar           = 0x671984;
        constexpr uint32_t PLT_CVehicleModelInfo_SetupCommonData = 0x674280;
        constexpr uint32_t PLT_CAnimManager_UncompressAnimation = 0x6750D4;
        constexpr uint32_t PLT_CRadar_ClearBlip               = 0x66FF0C;
        constexpr uint32_t PLT_RwFrameAddChild                = 0x675490;
        constexpr uint32_t PLT_RwTextureDestroy               = 0x67332C;
        constexpr uint32_t PLT_CCustomRoadsignMgr_RenderRoadsignAtomic = 0x66F5AC;

    } // namespace ARM32

    //=========================================================================
    // ARM64 Function Addresses (arm64-v8a)
    // From: DUMP 2.1 (64).txt
    // Note: Uses mangled C++ names in the dump
    //=========================================================================

    namespace ARM64
    {
        //---------------------------------------------------------------------
        // CPlayerPed
        //---------------------------------------------------------------------
        constexpr uint32_t CPlayerPed_SetupPlayerPed          = 0x5C0FD4;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CPlayerPed_DeactivatePlayerPed     = 0x5C1140;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CPlayerPed_ReactivatePlayerPed     = 0x5C1158;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CPlayerPed_SetInitialState         = 0x5C0D50;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CPlayerPed_SetMoveAnim             = 0x5C9C18;
        constexpr uint32_t CPlayerPed_IsHidden                = 0x5C40B8;
        constexpr uint32_t CPlayerPed_DisbandPlayerGroup      = 0x5C7D6C;

        //---------------------------------------------------------------------
        // CPed
        //---------------------------------------------------------------------
        constexpr uint32_t CPed_GetTransformedBonePosition    = 0x598670;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CPed_ClearWeapons                  = 0x595604;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CPed_UseFreeAimMagnetize           = 0x59F3E4;
        constexpr uint32_t CPed_FlagToDestroyWhenNextProcessed = 0x59E484;
        constexpr uint32_t CPed_ReplaceWeaponForScriptedCutscene = 0x59BC6C;

        //---------------------------------------------------------------------
        // CVehicle
        //---------------------------------------------------------------------
        constexpr uint32_t CVehicle_IsDoorFullyOpen           = 0x68F1F4;
        constexpr uint32_t CVehicle_IsDoorReady               = 0x68F1EC;
        constexpr uint32_t CVehicle_RemovePassenger           = 0x6A813C;

        //---------------------------------------------------------------------
        // CAutomobile
        //---------------------------------------------------------------------
        constexpr uint32_t CAutomobile_Fix                    = 0x67DF0C;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CAutomobile_SetupDamageAfterLoad   = 0x67E368;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CAutomobile_ProcessFlyingCarStuff  = 0x672964;

        //---------------------------------------------------------------------
        // CWorld
        //---------------------------------------------------------------------
        constexpr uint32_t CWorld_ProcessLineOfSight_Caller   = 0x70253C;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CWorld_FindRoofZFor3DCoord         = 0x50F4A4;

        //---------------------------------------------------------------------
        // Game State Addresses (from SA-MP 2.10 64-bit dumps)
        //---------------------------------------------------------------------
        constexpr uint32_t gGameState                         = 0xD3D640;  // Current game state (SystemState enum)
        constexpr uint32_t DoGameState                        = 0x70A208;  // DoGameState() function
        constexpr uint32_t MainMenuScreen_OnStartGame         = 0x35A31C;  // MainMenuScreen::OnStartGame()
        constexpr uint32_t StartGameScreen_OnNewGameCheck     = 0x365EA0;  // StartGameScreen::OnNewGameCheck()

        //---------------------------------------------------------------------
        // CWeapon
        //---------------------------------------------------------------------
        constexpr uint32_t CWeaponInfo_GetWeaponInfo          = 0x709BA8;  // From SAMP_ANDROID_REFERENCE.md

        //---------------------------------------------------------------------
        // Animation
        //---------------------------------------------------------------------
        constexpr uint32_t RpAnimBlendClumpGetAssociation     = 0x46AAF4;  // From SAMP_ANDROID_REFERENCE.md

        //---------------------------------------------------------------------
        // Scripts
        //---------------------------------------------------------------------
        constexpr uint32_t CTheScripts_ClearSpaceForMissionEntity = 0x419BE0;  // From SAMP_ANDROID_REFERENCE.md
        constexpr uint32_t CCheat_JetpackCheat                = 0x3C2A40;  // From SAMP_ANDROID_REFERENCE.md

        //---------------------------------------------------------------------
        // Global Game Addresses (from SAMP_ANDROID_REFERENCE.md)
        //---------------------------------------------------------------------
        constexpr uint32_t g_CCamera                          = 0xBBA8D0;
        constexpr uint32_t g_fx                               = 0xA062A8;
        constexpr uint32_t g_HUDVisibility                    = 0x9FF3A8;
        constexpr uint32_t ms_fAspectRatio                    = 0xCC7F00;
        constexpr uint32_t g_WaterShaderFlag                  = 0x8944A8;
        constexpr uint32_t g_WorldPlayersPtr                  = 0x84E7A8;
        constexpr uint32_t g_PlayerInFocus                    = 0x8516D8;

        //---------------------------------------------------------------------
        // Population
        //---------------------------------------------------------------------
        constexpr uint32_t CPopulation_RemovePed              = 0x5CDC64;  // From SAMP_ANDROID_REFERENCE.md

    } // namespace ARM64

    //=========================================================================
    // Architecture-independent Address Access
    //=========================================================================

    #if MTA_ARM32
        #define ARM_ADDR(name) ARM32::name
    #else
        #define ARM_ADDR(name) ARM64::name
    #endif

    //=========================================================================
    // Helper Functions
    //=========================================================================

    /**
     * Get the base address of libGTASA.so
     * Must be called after the library is loaded
     */
    inline uintptr_t GetLibGTASABase()
    {
        // This would be implemented by scanning /proc/self/maps
        // or using dl_iterate_phdr
        extern uintptr_t g_libGTASA;
        return g_libGTASA;
    }

    /**
     * Get absolute address from offset
     */
    inline void* GetAddress(uint32_t offset)
    {
        return reinterpret_cast<void*>(GetLibGTASABase() + offset);
    }

    /**
     * Get absolute address with Thumb bit handling (ARM32 only)
     */
    inline void* GetThumbAddress(uint32_t offset)
    {
        #if MTA_ARM32
            // Set Thumb bit for ARM32 function calls
            return reinterpret_cast<void*>(GetLibGTASABase() + offset + 1);
        #else
            return GetAddress(offset);
        #endif
    }

} // namespace MTA::Android::ARM

#endif // ARM_ADDRESS_MAP_H
