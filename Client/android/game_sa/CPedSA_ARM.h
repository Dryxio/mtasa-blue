/*
 * MTA:SA Android - CPedSA ARM Implementation
 *
 * ARM-compatible implementation of CPed (pedestrian) functionality.
 * Inherits from CEntitySA and provides ped-specific operations.
 */

#ifndef CPEDSA_ARM_H
#define CPEDSA_ARM_H

#include "CEntitySA_ARM.h"
#include "GameSA_Platform.h"

namespace MTA::Android::GameSA
{

// Forward declarations
class CWeapon;
class CTaskManager;
class CPedIntelligence;
class CVehicle;
class CAnimBlendAssociation;

//=============================================================================
// Weapon slot structure
//=============================================================================

struct CWeaponSlot
{
    uint32_t m_eWeaponType;      // Weapon type ID
    uint32_t m_nState;           // Weapon state
    uint32_t m_nAmmoInClip;      // Ammo in current clip
    uint32_t m_nAmmoTotal;       // Total ammo
    uint32_t m_nTimer;           // Timer for weapon actions
    uint8_t  m_bSlot;            // Slot index
    uint8_t  m_pad[3];
};

//=============================================================================
// CPedSAInterface - Ped internal structure
//=============================================================================

struct CPhysicalSAInterface : public CEntitySAInterface
{
    // Physical properties
    float    m_fMass;              // 0x?? - Mass
    float    m_fTurnMass;          // Turn mass
    float    m_fAirResistance;     // Air resistance
    float    m_fElasticity;        // Elasticity
    float    m_fBuoyancy;          // Buoyancy

    CVector  m_vecMoveSpeed;       // Movement velocity
    CVector  m_vecTurnSpeed;       // Angular velocity
    CVector  m_vecFrictionMoveSpeed;
    CVector  m_vecFrictionTurnSpeed;
    CVector  m_vecForce;
    CVector  m_vecTorque;

    // More fields...
};

struct CPedSAInterface : public CPhysicalSAInterface
{
    // Note: Actual offsets vary by game version
    // These are approximate based on GTA:SA structure

    // Intelligence/AI
    void*           m_pPedIntelligence;  // CPedIntelligence*

    // Animation
    void*           m_pAnimBlendAssociation;

    // Physical state
    uint8_t         m_nPedState;
    uint8_t         m_nMoveState;
    uint8_t         m_nPedType;
    uint8_t         m_nCreatedBy;

    // Current vehicle
    void*           m_pVehicle;          // CVehicleSAInterface*
    uint8_t         m_nPedInVehicle;

    // Weapons
    CWeaponSlot     m_aWeapons[13];
    uint8_t         m_nCurrentWeaponSlot;
    uint8_t         m_nPreviousWeaponSlot;

    // Health/Armor
    float           m_fHealth;
    float           m_fMaxHealth;
    float           m_fArmor;

    // Target entity
    void*           m_pTargetedEntity;

    // Flags
    uint32_t        m_nPedFlags;

    // More fields follow...

    //=========================================================================
    // ARM-Compatible Member Functions
    //=========================================================================

    /**
     * Process ped control (main update function)
     */
    void ProcessControl()
    {
        using FuncType = void(*)(CPedSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_ProcessControl));
        func(this);
    }

    /**
     * Set the ped's model
     */
    void SetModelIndex(uint32_t modelIndex)
    {
        using FuncType = void(*)(CPedSAInterface*, uint32_t);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_SetModelIndex));
        func(this, modelIndex);
    }

    /**
     * Give the ped a weapon
     */
    uint32_t GiveWeapon(uint32_t weaponType, uint32_t ammo, bool likeUnused)
    {
        using FuncType = uint32_t(*)(CPedSAInterface*, uint32_t, uint32_t, bool);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_GiveWeapon));
        return func(this, weaponType, ammo, likeUnused);
    }

    /**
     * Get the weapon slot for a weapon type
     */
    uint8_t GetWeaponSlot(uint32_t weaponType)
    {
        using FuncType = uint8_t(*)(CPedSAInterface*, uint32_t);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_GetWeaponSlot));
        return func(this, weaponType);
    }

    /**
     * Set current weapon slot
     */
    void SetCurrentWeaponSlot(uint8_t slot)
    {
        using FuncType = void(*)(CPedSAInterface*, uint8_t);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_SetCurrentWeaponSlot));
        func(this, slot);
    }

    /**
     * Remove weapon model from hand
     */
    void RemoveWeaponModel(int32_t modelId)
    {
        using FuncType = void(*)(CPedSAInterface*, int32_t);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_RemoveWeaponModel));
        func(this, modelId);
    }

    /**
     * Clear a specific weapon
     */
    void ClearWeapon(uint32_t weaponType)
    {
        using FuncType = void(*)(CPedSAInterface*, uint32_t);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_ClearWeapon));
        func(this, weaponType);
    }

    /**
     * Clear all weapons
     */
    void ClearWeapons()
    {
        using FuncType = void(*)(CPedSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_ClearWeapons));
        func(this);
    }

    /**
     * Get a bone's position in world space
     */
    void GetBonePosition(CVector& outPos, uint32_t boneId, bool includeAnim)
    {
        using FuncType = void(*)(CPedSAInterface*, CVector&, uint32_t, bool);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_GetBonePosition));
        func(this, outPos, boneId, includeAnim);
    }

    /**
     * Get transformed bone position (with animation applied)
     */
    void GetTransformedBonePosition(CVector& outPos, uint32_t boneId)
    {
        using FuncType = void(*)(CPedSAInterface*, CVector&, uint32_t);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_GetTransformedBonePosition));
        func(this, outPos, boneId);
    }

    /**
     * Check if this ped can see another entity
     */
    bool CanSeeEntity(void* entity, float distance)
    {
        using FuncType = bool(*)(CPedSAInterface*, void*, float);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPed_CanSeeEntity));
        return func(this, entity, distance);
    }
};

//=============================================================================
// CPlayerPedSAInterface - Player ped structure
//=============================================================================

struct CPlayerPedSAInterface : public CPedSAInterface
{
    // Player-specific data
    void*    m_pPlayerData;      // CPlayerData*
    uint32_t m_nWantedLevel;

    // Additional player state...

    /**
     * Process player ped control
     */
    void ProcessControl()
    {
        using FuncType = void(*)(CPlayerPedSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPlayerPed_ProcessControl));
        func(this);
    }

    /**
     * Set initial player state
     */
    void SetInitialState()
    {
        using FuncType = void(*)(CPlayerPedSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPlayerPed_SetInitialState));
        func(this);
    }
};

//=============================================================================
// CPedSA - MTA wrapper class
//=============================================================================

class CPedSA : public CEntitySA
{
public:
    CPedSA();
    virtual ~CPedSA();

    // Cast helper
    CPedSAInterface* GetPedInterface() { return reinterpret_cast<CPedSAInterface*>(m_pInterface); }

    //=========================================================================
    // Health/Armor
    //=========================================================================

    float GetHealth();
    void SetHealth(float health);
    float GetMaxHealth();
    void SetMaxHealth(float maxHealth);
    float GetArmor();
    void SetArmor(float armor);

    //=========================================================================
    // Weapons
    //=========================================================================

    uint32_t GiveWeapon(uint32_t weaponType, uint32_t ammo);
    void SetCurrentWeapon(uint32_t slot);
    uint32_t GetCurrentWeaponSlot();
    void ClearWeapons();
    CWeaponSlot* GetWeapon(uint32_t slot);

    //=========================================================================
    // Vehicle
    //=========================================================================

    void* GetVehicle();
    bool IsInVehicle();
    void WarpIntoVehicle(void* vehicle, uint32_t seat);
    void RemoveFromVehicle();

    //=========================================================================
    // Animation
    //=========================================================================

    void* GetAnimBlendAssociation(uint32_t animId);
    void SetAnimationProgress(uint32_t animId, float progress);

    //=========================================================================
    // AI/Tasks
    //=========================================================================

    void* GetPedIntelligence();
    void* GetTaskManager();
    void SetTask(void* task, int priority);
    void ClearTasks();

    //=========================================================================
    // State
    //=========================================================================

    uint8_t GetPedState();
    void SetPedState(uint8_t state);
    uint8_t GetMoveState();
    void SetMoveState(uint8_t state);

    //=========================================================================
    // Bones
    //=========================================================================

    void GetBonePosition(uint32_t boneId, CVector& outPos);
    void GetTransformedBonePosition(uint32_t boneId, CVector& outPos);

protected:
    // Weapon storage (mirrors game data)
    CWeaponSlot m_Weapons[13];
};

//=============================================================================
// CPlayerPedSA - Player ped wrapper
//=============================================================================

class CPlayerPedSA : public CPedSA
{
public:
    CPlayerPedSA();
    virtual ~CPlayerPedSA();

    // Cast helper
    CPlayerPedSAInterface* GetPlayerPedInterface()
    {
        return reinterpret_cast<CPlayerPedSAInterface*>(m_pInterface);
    }

    //=========================================================================
    // Player-Specific
    //=========================================================================

    uint32_t GetWantedLevel();
    void SetWantedLevel(uint32_t level);

    void* GetPlayerData();
};

//=============================================================================
// Bone IDs (GTA:SA standard)
//=============================================================================

enum eBoneTag
{
    BONE_ROOT = 0,
    BONE_PELVIS = 1,
    BONE_SPINE = 2,
    BONE_SPINE1 = 3,
    BONE_NECK = 4,
    BONE_HEAD = 5,
    BONE_RIGHTCLAVICLE = 21,
    BONE_RIGHTSHOULDER = 22,
    BONE_RIGHTELBOW = 23,
    BONE_RIGHTWRIST = 24,
    BONE_RIGHTHAND = 34,
    BONE_LEFTCLAVICLE = 31,
    BONE_LEFTSHOULDER = 32,
    BONE_LEFTELBOW = 33,
    BONE_LEFTWRIST = 34,
    BONE_LEFTHAND = 44,
    BONE_RIGHTHIP = 41,
    BONE_RIGHTKNEE = 42,
    BONE_RIGHTANKLE = 43,
    BONE_RIGHTFOOT = 53,
    BONE_LEFTHIP = 51,
    BONE_LEFTKNEE = 52,
    BONE_LEFTANKLE = 53,
    BONE_LEFTFOOT = 63,
};

} // namespace MTA::Android::GameSA

#endif // CPEDSA_ARM_H
