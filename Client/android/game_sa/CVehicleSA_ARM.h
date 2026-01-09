/*
 * MTA:SA Android - CVehicleSA ARM Implementation
 *
 * ARM-compatible implementation of CVehicle functionality.
 * Inherits from CPhysical and provides vehicle-specific operations.
 */

#ifndef CVEHICLESA_ARM_H
#define CVEHICLESA_ARM_H

#include "CPedSA_ARM.h"
#include "GameSA_Platform.h"

namespace MTA::Android::GameSA
{

// Forward declarations
class CDoor;
class CColPoint;
class CDamageManager;

//=============================================================================
// Vehicle Types
//=============================================================================

enum eVehicleType
{
    VEHICLE_TYPE_AUTOMOBILE = 0,
    VEHICLE_TYPE_BOAT,
    VEHICLE_TYPE_TRAIN,
    VEHICLE_TYPE_HELI,
    VEHICLE_TYPE_PLANE,
    VEHICLE_TYPE_BIKE,
    VEHICLE_TYPE_MONSTERTRUCK,
    VEHICLE_TYPE_QUADBIKE,
    VEHICLE_TYPE_BMX,
    VEHICLE_TYPE_TRAILER
};

enum eDoorId
{
    DOOR_BONNET = 0,
    DOOR_BOOT,
    DOOR_FRONT_LEFT,
    DOOR_FRONT_RIGHT,
    DOOR_REAR_LEFT,
    DOOR_REAR_RIGHT,
    MAX_DOORS
};

enum ePanelId
{
    PANEL_FRONT_LEFT = 0,
    PANEL_FRONT_RIGHT,
    PANEL_REAR_LEFT,
    PANEL_REAR_RIGHT,
    PANEL_WINDSCREEN,
    PANEL_FRONT_BUMPER,
    PANEL_REAR_BUMPER,
    MAX_PANELS
};

enum eLightId
{
    LIGHT_FRONT_LEFT = 0,
    LIGHT_FRONT_RIGHT,
    LIGHT_REAR_LEFT,
    LIGHT_REAR_RIGHT,
    MAX_LIGHTS
};

enum eWheelPosition
{
    WHEEL_FRONT_LEFT = 0,
    WHEEL_REAR_LEFT,
    WHEEL_FRONT_RIGHT,
    WHEEL_REAR_RIGHT,
    MAX_WHEELS
};

//=============================================================================
// CDoorSAInterface - Door structure
//=============================================================================

struct CDoorSAInterface
{
    float m_fOpenAngle;
    float m_fClosedAngle;
    int16_t m_nDirn;
    uint8_t m_nAxis;
    uint8_t m_nDoorState;
    float m_fAngle;
    float m_fPrevAngle;
    float m_fAngVel;
};

//=============================================================================
// CDamageManagerSAInterface - Vehicle damage tracking
//=============================================================================

struct CDamageManagerSAInterface
{
    float    m_fWheelDamageEffect;
    uint8_t  m_nEngineStatus;
    uint8_t  m_anWheelStatus[MAX_WHEELS];
    uint8_t  m_anDoorStatus[MAX_DOORS];
    uint32_t m_nLightStatus;       // Bitfield
    uint32_t m_nPanelStatus;       // Bitfield
};

//=============================================================================
// CVehicleSAInterface - Vehicle internal structure
//=============================================================================

struct CVehicleSAInterface : public CPhysicalSAInterface
{
    // Note: Actual offsets vary by game version
    // These are approximate based on GTA:SA structure

    // Handling
    void*           m_pHandlingData;      // CHandlingData*

    // Fly handling (planes/helis)
    void*           m_pFlyingHandlingData;

    // Autopilot
    void*           m_pAutoPilot;         // CAutoPilot*

    // Vehicle flags
    uint32_t        m_nVehicleFlags;
    uint32_t        m_nVehicleFlags2;

    // Timestamps
    uint32_t        m_nCreationTime;
    uint8_t         m_nPrimaryColor;
    uint8_t         m_nSecondaryColor;
    uint8_t         m_nTertiaryColor;
    uint8_t         m_nQuaternaryColor;
    int8_t          m_anExtras[2];
    int16_t         m_nAlarmState;

    // Driver/Passengers
    CPedSAInterface* m_pDriver;
    CPedSAInterface* m_apPassengers[8];
    uint8_t         m_nNumPassengers;
    uint8_t         m_nNumGettingIn;
    uint8_t         m_nGettingInFlags;
    uint8_t         m_nGettingOutFlags;
    uint8_t         m_nMaxPassengers;

    // Doors
    CDoorSAInterface m_aDoors[MAX_DOORS];

    // Damage
    CDamageManagerSAInterface m_DamageManager;

    // Health
    float           m_fHealth;
    float           m_fPetrolTankHealth;

    // Wheels
    float           m_afWheelSuspensionHeight[MAX_WHEELS];
    float           m_afWheelRotation[MAX_WHEELS];
    float           m_afWheelSpeed[MAX_WHEELS];

    // Tow/Trailer
    void*           m_pTowedVehicle;
    void*           m_pTowingVehicle;

    // Current gear
    uint8_t         m_nCurrentGear;

    // Siren
    uint8_t         m_nSirenOrAlarmActive;

    // More fields follow...

    //=========================================================================
    // ARM-Compatible Member Functions
    //=========================================================================

    /**
     * Process vehicle control (main update function)
     */
    void ProcessControl()
    {
        using FuncType = void(*)(CVehicleSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_ProcessControl));
        func(this);
    }

    /**
     * Setup for rendering
     */
    void SetupRender()
    {
        using FuncType = void(*)(CVehicleSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_SetupRender));
        func(this);
    }

    /**
     * Pre-render processing
     */
    void PreRender()
    {
        using FuncType = void(*)(CVehicleSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_PreRender));
        func(this);
    }

    /**
     * Add a passenger to the vehicle
     */
    bool AddPassenger(CPedSAInterface* ped, uint8_t seat)
    {
        using FuncType = bool(*)(CVehicleSAInterface*, CPedSAInterface*, uint8_t);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_AddPassenger));
        return func(this, ped, seat);
    }

    /**
     * Remove a passenger from the vehicle
     */
    void RemovePassenger(CPedSAInterface* ped)
    {
        using FuncType = void(*)(CVehicleSAInterface*, CPedSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_RemovePassenger));
        func(this, ped);
    }

    /**
     * Set the driver of the vehicle
     */
    void SetDriver(CPedSAInterface* ped)
    {
        using FuncType = void(*)(CVehicleSAInterface*, CPedSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_SetDriver));
        func(this, ped);
    }

    /**
     * Remove the driver from the vehicle
     */
    void RemoveDriver()
    {
        using FuncType = void(*)(CVehicleSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_RemoveDriver));
        func(this);
    }

    /**
     * Open a door
     */
    void OpenDoor(CPedSAInterface* ped, uint32_t component, uint32_t door, float ratio, bool playSound)
    {
        using FuncType = void(*)(CVehicleSAInterface*, CPedSAInterface*, uint32_t, uint32_t, float, bool);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_OpenDoor));
        func(this, ped, component, door, ratio, playSound);
    }

    /**
     * Check if vehicle can be damaged
     */
    bool CanBeDamaged()
    {
        using FuncType = bool(*)(CVehicleSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_CanBeDamaged));
        return func(this);
    }

    /**
     * Set vehicle colors
     */
    void SetColor(uint8_t primary, uint8_t secondary, uint8_t tertiary, uint8_t quaternary)
    {
        m_nPrimaryColor = primary;
        m_nSecondaryColor = secondary;
        m_nTertiaryColor = tertiary;
        m_nQuaternaryColor = quaternary;
    }

    /**
     * Fix the vehicle (repair damage)
     */
    void Fix()
    {
        using FuncType = void(*)(CVehicleSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_Fix));
        func(this);
    }

    /**
     * Blow up the vehicle
     */
    void BlowUp(void* attacker, bool explode)
    {
        using FuncType = void(*)(CVehicleSAInterface*, void*, bool);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_BlowUp));
        func(this, attacker, explode);
    }

    /**
     * Set the landingear position (planes)
     */
    void SetLandingGearPosition(float position)
    {
        using FuncType = void(*)(CVehicleSAInterface*, float);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CVehicle_SetLandingGearPosition));
        func(this, position);
    }

    /**
     * Get the towed vehicle
     */
    CVehicleSAInterface* GetTowedVehicle()
    {
        return reinterpret_cast<CVehicleSAInterface*>(m_pTowedVehicle);
    }

    /**
     * Get the vehicle towing this one
     */
    CVehicleSAInterface* GetTowingVehicle()
    {
        return reinterpret_cast<CVehicleSAInterface*>(m_pTowingVehicle);
    }
};

//=============================================================================
// CAutomobileSAInterface - Car/Bike specific
//=============================================================================

struct CAutomobileSAInterface : public CVehicleSAInterface
{
    // Wheel state
    float m_afWheelRotationX[MAX_WHEELS];
    float m_afWheelPosition[MAX_WHEELS];
    float m_afSuspensionLength[MAX_WHEELS];

    // Burnout
    float m_fBurnoutSmokeMultiplier;

    // More automobile-specific fields...

    /**
     * Process automobile control
     */
    void ProcessControl()
    {
        using FuncType = void(*)(CAutomobileSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CAutomobile_ProcessControl));
        func(this);
    }

    /**
     * Teleport to coordinates
     */
    void Teleport(float x, float y, float z, bool resetRotation)
    {
        using FuncType = void(*)(CAutomobileSAInterface*, float, float, float, bool);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CAutomobile_Teleport));
        func(this, x, y, z, resetRotation);
    }

    /**
     * Process suspension
     */
    void ProcessSuspension()
    {
        using FuncType = void(*)(CAutomobileSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CAutomobile_ProcessSuspension));
        func(this);
    }

    /**
     * Damage a wheel
     */
    void DamageWheel(eWheelPosition wheel, bool burst)
    {
        using FuncType = void(*)(CAutomobileSAInterface*, int, bool);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CAutomobile_DamageWheel));
        func(this, static_cast<int>(wheel), burst);
    }

    /**
     * Set wheel status
     */
    void SetWheelStatus(eWheelPosition wheel, uint8_t status)
    {
        if (wheel < MAX_WHEELS)
        {
            m_DamageManager.m_anWheelStatus[wheel] = status;
        }
    }
};

//=============================================================================
// CBoatSAInterface - Boat specific
//=============================================================================

struct CBoatSAInterface : public CVehicleSAInterface
{
    float m_fMovingHiRotation;
    float m_fPropRotation;

    /**
     * Process boat control
     */
    void ProcessControl()
    {
        using FuncType = void(*)(CBoatSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CBoat_ProcessControl));
        func(this);
    }
};

//=============================================================================
// CHeliSAInterface - Helicopter specific
//=============================================================================

struct CHeliSAInterface : public CAutomobileSAInterface
{
    float m_fRotorRotation;
    float m_fRotorSpeed;

    /**
     * Process helicopter control
     */
    void ProcessControl()
    {
        using FuncType = void(*)(CHeliSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CHeli_ProcessControl));
        func(this);
    }
};

//=============================================================================
// CPlaneSAInterface - Plane specific
//=============================================================================

struct CPlaneSAInterface : public CAutomobileSAInterface
{
    float m_fPropSpeed;
    float m_fLandingGearPosition;

    /**
     * Process plane control
     */
    void ProcessControl()
    {
        using FuncType = void(*)(CPlaneSAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CPlane_ProcessControl));
        func(this);
    }
};

//=============================================================================
// CVehicleSA - MTA wrapper class
//=============================================================================

class CVehicleSA : public CEntitySA
{
public:
    CVehicleSA();
    virtual ~CVehicleSA();

    // Cast helper
    CVehicleSAInterface* GetVehicleInterface()
    {
        return reinterpret_cast<CVehicleSAInterface*>(m_pInterface);
    }

    //=========================================================================
    // Vehicle Properties
    //=========================================================================

    float GetHealth();
    void SetHealth(float health);

    void GetColors(uint8_t& primary, uint8_t& secondary, uint8_t& tertiary, uint8_t& quaternary);
    void SetColors(uint8_t primary, uint8_t secondary, uint8_t tertiary, uint8_t quaternary);

    uint8_t GetCurrentGear();
    float GetEngineState();

    //=========================================================================
    // Occupants
    //=========================================================================

    void* GetDriver();
    void SetDriver(void* ped);

    void* GetPassenger(uint8_t seat);
    void AddPassenger(void* ped, uint8_t seat);
    void RemovePassenger(void* ped);
    void RemoveAllOccupants();

    uint8_t GetMaxPassengers();
    uint8_t GetNumOccupants();

    //=========================================================================
    // Doors/Windows
    //=========================================================================

    void OpenDoor(uint32_t door, float ratio, bool instant);
    void CloseDoor(uint32_t door, bool instant);
    float GetDoorOpenRatio(uint32_t door);
    bool IsDoorOpen(uint32_t door);
    bool IsDoorDamaged(uint32_t door);

    //=========================================================================
    // Damage
    //=========================================================================

    void Fix();
    void BlowUp(bool explode);

    uint8_t GetWheelStatus(uint8_t wheel);
    void SetWheelStatus(uint8_t wheel, uint8_t status);

    uint8_t GetDoorStatus(uint8_t door);
    void SetDoorStatus(uint8_t door, uint8_t status);

    uint8_t GetPanelStatus(uint8_t panel);
    void SetPanelStatus(uint8_t panel, uint8_t status);

    bool IsLightDamaged(uint8_t light);
    void SetLightDamaged(uint8_t light, bool damaged);

    //=========================================================================
    // Towing
    //=========================================================================

    void* GetTowedVehicle();
    void* GetTowingVehicle();
    bool SetTowLink(void* vehicle);
    void BreakTowLink();

    //=========================================================================
    // Special Features
    //=========================================================================

    void SetLandingGearDown(bool down);
    float GetLandingGearPosition();

    bool IsSirenActive();
    void SetSirenActive(bool active);

    //=========================================================================
    // Vehicle Type
    //=========================================================================

    eVehicleType GetVehicleType();
    bool IsAutomobile();
    bool IsBike();
    bool IsBoat();
    bool IsPlane();
    bool IsHeli();

protected:
    eVehicleType m_eVehicleType;
};

//=============================================================================
// CAutomobileSA - Automobile wrapper
//=============================================================================

class CAutomobileSA : public CVehicleSA
{
public:
    CAutomobileSA();
    virtual ~CAutomobileSA();

    CAutomobileSAInterface* GetAutomobileInterface()
    {
        return reinterpret_cast<CAutomobileSAInterface*>(m_pInterface);
    }

    void ProcessSuspension();
    void BurstTyre(uint8_t wheel);
    void FixTyre(uint8_t wheel);
};

} // namespace MTA::Android::GameSA

#endif // CVEHICLESA_ARM_H
