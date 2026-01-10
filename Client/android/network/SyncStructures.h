/*
 * MTA:SA Android - Sync Structures
 *
 * Data structures for network synchronization of game entities.
 * Based on Shared/sdk/net/SyncStructures.h from the MTA Windows client.
 *
 * Phase 7: Multiplayer Logic
 */

#ifndef SYNC_STRUCTURES_ANDROID_H
#define SYNC_STRUCTURES_ANDROID_H

#include <cstdint>
#include <cmath>
#include <algorithm>
#include "CNetAndroid.h"

namespace MTA::Android::Network
{

//=============================================================================
// Constants
//=============================================================================

constexpr float SYNC_POSITION_LIMIT = 100000.0f;  // Max position value
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

//=============================================================================
// Utility Functions
//=============================================================================

inline float WrapAngle(float angle)
{
    while (angle < -PI) angle += TWO_PI;
    while (angle > PI) angle -= TWO_PI;
    return angle;
}

inline float ClampFloat(float value, float min, float max)
{
    return std::max(min, std::min(max, value));
}

//=============================================================================
// Player Puresync Flags
//=============================================================================

struct SPlayerPuresyncFlags
{
    bool isInWater : 1;
    bool isOnGround : 1;
    bool hasJetPack : 1;
    bool isDucked : 1;
    bool wearsGoggles : 1;
    bool hasContact : 1;
    bool isChoking : 1;
    bool akimboTargetUp : 1;
    bool isOnFire : 1;
    bool hasAWeapon : 1;
    bool syncingVelocity : 1;
    bool stealthAiming : 1;
    bool isReloadingWeapon : 1;
    bool animInterrupted : 1;
    bool hangingDuringClimb : 1;

    SPlayerPuresyncFlags()
    {
        isInWater = false;
        isOnGround = true;
        hasJetPack = false;
        isDucked = false;
        wearsGoggles = false;
        hasContact = false;
        isChoking = false;
        akimboTargetUp = false;
        isOnFire = false;
        hasAWeapon = false;
        syncingVelocity = false;
        stealthAiming = false;
        isReloadingWeapon = false;
        animInterrupted = false;
        hangingDuringClimb = false;
    }

    void Write(NetBitStream& bs) const
    {
        bs.WriteBit(isInWater);
        bs.WriteBit(isOnGround);
        bs.WriteBit(hasJetPack);
        bs.WriteBit(isDucked);
        bs.WriteBit(wearsGoggles);
        bs.WriteBit(hasContact);
        bs.WriteBit(isChoking);
        bs.WriteBit(akimboTargetUp);
        bs.WriteBit(isOnFire);
        bs.WriteBit(hasAWeapon);
        bs.WriteBit(syncingVelocity);
        bs.WriteBit(stealthAiming);
        bs.WriteBit(isReloadingWeapon);
        bs.WriteBit(animInterrupted);
        bs.WriteBit(hangingDuringClimb);
    }

    bool Read(NetBitStream& bs)
    {
        isInWater = bs.ReadBit();
        isOnGround = bs.ReadBit();
        hasJetPack = bs.ReadBit();
        isDucked = bs.ReadBit();
        wearsGoggles = bs.ReadBit();
        hasContact = bs.ReadBit();
        isChoking = bs.ReadBit();
        akimboTargetUp = bs.ReadBit();
        isOnFire = bs.ReadBit();
        hasAWeapon = bs.ReadBit();
        syncingVelocity = bs.ReadBit();
        stealthAiming = bs.ReadBit();
        isReloadingWeapon = bs.ReadBit();
        animInterrupted = bs.ReadBit();
        hangingDuringClimb = bs.ReadBit();
        return true;
    }
};

//=============================================================================
// Vehicle Puresync Flags
//=============================================================================

struct SVehiclePuresyncFlags
{
    bool isWearingGoggles : 1;
    bool isDoingGangDriveby : 1;
    bool isSirenOrAlarmActive : 1;
    bool isSmokeTrailEnabled : 1;
    bool isLandingGearDown : 1;
    bool isOnGround : 1;
    bool isInWater : 1;
    bool isDerailed : 1;
    bool isAircraft : 1;
    bool hasAWeapon : 1;
    bool isHeliSearchLightVisible : 1;

    SVehiclePuresyncFlags()
    {
        isWearingGoggles = false;
        isDoingGangDriveby = false;
        isSirenOrAlarmActive = false;
        isSmokeTrailEnabled = false;
        isLandingGearDown = true;
        isOnGround = true;
        isInWater = false;
        isDerailed = false;
        isAircraft = false;
        hasAWeapon = false;
        isHeliSearchLightVisible = false;
    }

    void Write(NetBitStream& bs) const
    {
        bs.WriteBit(isWearingGoggles);
        bs.WriteBit(isDoingGangDriveby);
        bs.WriteBit(isSirenOrAlarmActive);
        bs.WriteBit(isSmokeTrailEnabled);
        bs.WriteBit(isLandingGearDown);
        bs.WriteBit(isOnGround);
        bs.WriteBit(isInWater);
        bs.WriteBit(isDerailed);
        bs.WriteBit(isAircraft);
        bs.WriteBit(hasAWeapon);
        bs.WriteBit(isHeliSearchLightVisible);
    }

    bool Read(NetBitStream& bs)
    {
        isWearingGoggles = bs.ReadBit();
        isDoingGangDriveby = bs.ReadBit();
        isSirenOrAlarmActive = bs.ReadBit();
        isSmokeTrailEnabled = bs.ReadBit();
        isLandingGearDown = bs.ReadBit();
        isOnGround = bs.ReadBit();
        isInWater = bs.ReadBit();
        isDerailed = bs.ReadBit();
        isAircraft = bs.ReadBit();
        hasAWeapon = bs.ReadBit();
        isHeliSearchLightVisible = bs.ReadBit();
        return true;
    }
};

//=============================================================================
// Position Sync (compressed)
//=============================================================================

struct SPositionSync
{
    float x, y, z;

    SPositionSync() : x(0), y(0), z(0) {}
    SPositionSync(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    void Write(NetBitStream& bs) const
    {
        // Validate position
        float cx = ClampFloat(x, -SYNC_POSITION_LIMIT, SYNC_POSITION_LIMIT);
        float cy = ClampFloat(y, -SYNC_POSITION_LIMIT, SYNC_POSITION_LIMIT);
        float cz = ClampFloat(z, -SYNC_POSITION_LIMIT, SYNC_POSITION_LIMIT);

        bs.Write(cx);
        bs.Write(cy);
        bs.Write(cz);
    }

    bool Read(NetBitStream& bs)
    {
        if (!bs.Read(x)) return false;
        if (!bs.Read(y)) return false;
        if (!bs.Read(z)) return false;

        // Validate
        if (std::abs(x) > SYNC_POSITION_LIMIT ||
            std::abs(y) > SYNC_POSITION_LIMIT ||
            std::abs(z) > SYNC_POSITION_LIMIT)
        {
            return false;
        }
        return true;
    }
};

//=============================================================================
// Velocity Sync
//=============================================================================

struct SVelocitySync
{
    float x, y, z;

    SVelocitySync() : x(0), y(0), z(0) {}
    SVelocitySync(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    bool HasVelocity() const
    {
        return x != 0.0f || y != 0.0f || z != 0.0f;
    }

    void Write(NetBitStream& bs) const
    {
        bs.WriteBit(HasVelocity());
        if (HasVelocity())
        {
            bs.Write(x);
            bs.Write(y);
            bs.Write(z);
        }
    }

    bool Read(NetBitStream& bs)
    {
        if (bs.ReadBit())
        {
            if (!bs.Read(x)) return false;
            if (!bs.Read(y)) return false;
            if (!bs.Read(z)) return false;
        }
        else
        {
            x = y = z = 0.0f;
        }
        return true;
    }
};

//=============================================================================
// Rotation Sync (compressed to 12 bits)
//=============================================================================

struct SRotationSync
{
    float rotation;

    SRotationSync() : rotation(0) {}
    SRotationSync(float r) : rotation(r) {}

    void Write(NetBitStream& bs) const
    {
        // Compress rotation to 12 bits (0-4095)
        float wrapped = WrapAngle(rotation);
        uint16_t compressed = static_cast<uint16_t>((wrapped + PI) / TWO_PI * 4095.0f);
        compressed &= 0x0FFF;  // 12 bits

        bs.WriteBits(reinterpret_cast<uint8_t*>(&compressed), 12);
    }

    bool Read(NetBitStream& bs)
    {
        uint16_t compressed = 0;
        if (!bs.ReadBits(reinterpret_cast<uint8_t*>(&compressed), 12)) return false;

        rotation = (compressed / 4095.0f * TWO_PI) - PI;
        return true;
    }
};

//=============================================================================
// Health Sync (8 bits)
//=============================================================================

struct SHealthSync
{
    float health;

    SHealthSync() : health(100.0f) {}
    SHealthSync(float h) : health(h) {}

    void Write(NetBitStream& bs) const
    {
        // 0-255 maps to 0-200 health
        uint8_t compressed = static_cast<uint8_t>(ClampFloat(health, 0.0f, 200.0f) * 1.275f);
        bs.Write(compressed);
    }

    bool Read(NetBitStream& bs)
    {
        uint8_t compressed;
        if (!bs.Read(compressed)) return false;
        health = compressed / 1.275f;
        return true;
    }
};

//=============================================================================
// Armor Sync (8 bits)
//=============================================================================

struct SArmorSync
{
    float armor;

    SArmorSync() : armor(0.0f) {}
    SArmorSync(float a) : armor(a) {}

    void Write(NetBitStream& bs) const
    {
        // 0-255 maps to 0-100 armor
        uint8_t compressed = static_cast<uint8_t>(ClampFloat(armor, 0.0f, 100.0f) * 2.55f);
        bs.Write(compressed);
    }

    bool Read(NetBitStream& bs)
    {
        uint8_t compressed;
        if (!bs.Read(compressed)) return false;
        armor = compressed / 2.55f;
        return true;
    }
};

//=============================================================================
// Vehicle Health Sync (12 bits)
//=============================================================================

struct SVehicleHealthSync
{
    float health;

    SVehicleHealthSync() : health(1000.0f) {}
    SVehicleHealthSync(float h) : health(h) {}

    void Write(NetBitStream& bs) const
    {
        // 0-4095 maps to 0-2047.5 health
        uint16_t compressed = static_cast<uint16_t>(ClampFloat(health, 0.0f, 2047.5f) * 2.0f);
        compressed &= 0x0FFF;  // 12 bits
        bs.WriteBits(reinterpret_cast<uint8_t*>(&compressed), 12);
    }

    bool Read(NetBitStream& bs)
    {
        uint16_t compressed = 0;
        if (!bs.ReadBits(reinterpret_cast<uint8_t*>(&compressed), 12)) return false;
        health = compressed / 2.0f;
        return true;
    }
};

//=============================================================================
// Weapon Slot Sync
//=============================================================================

struct SWeaponSlotSync
{
    uint8_t slot;

    SWeaponSlotSync() : slot(0) {}
    SWeaponSlotSync(uint8_t s) : slot(s) {}

    void Write(NetBitStream& bs) const
    {
        // 4 bits for weapon slot (0-12)
        uint8_t compressed = slot & 0x0F;
        bs.WriteBits(&compressed, 4);
    }

    bool Read(NetBitStream& bs)
    {
        uint8_t compressed = 0;
        if (!bs.ReadBits(&compressed, 4)) return false;
        slot = compressed;
        return true;
    }
};

//=============================================================================
// Ammo Sync
//=============================================================================

struct SAmmoSync
{
    uint16_t ammoInClip;
    uint16_t totalAmmo;

    SAmmoSync() : ammoInClip(0), totalAmmo(0) {}

    void Write(NetBitStream& bs) const
    {
        bs.WriteCompressed(ammoInClip);
        bs.WriteCompressed(totalAmmo);
    }

    bool Read(NetBitStream& bs)
    {
        if (!bs.ReadCompressed(ammoInClip)) return false;
        if (!bs.ReadCompressed(totalAmmo)) return false;
        return true;
    }
};

//=============================================================================
// Full Player Puresync Data
//=============================================================================

struct SPlayerPuresyncData
{
    uint16_t timeContext;
    SPlayerPuresyncFlags flags;
    SPositionSync position;
    SRotationSync rotation;
    SVelocitySync velocity;
    SHealthSync health;
    SArmorSync armor;
    SWeaponSlotSync weaponSlot;
    SAmmoSync ammo;

    // Contact element (if touching something)
    uint32_t contactElement;

    // Camera rotation
    float cameraRotation;

    // Animation
    uint32_t animationFlags;

    void Write(NetBitStream& bs) const
    {
        bs.Write(timeContext);
        flags.Write(bs);
        position.Write(bs);
        rotation.Write(bs);

        if (flags.syncingVelocity)
        {
            velocity.Write(bs);
        }

        health.Write(bs);
        armor.Write(bs);

        // Camera rotation (compressed)
        SRotationSync camRot(cameraRotation);
        camRot.Write(bs);

        if (flags.hasAWeapon)
        {
            weaponSlot.Write(bs);
            ammo.Write(bs);
        }
    }

    bool Read(NetBitStream& bs)
    {
        if (!bs.Read(timeContext)) return false;
        if (!flags.Read(bs)) return false;
        if (!position.Read(bs)) return false;
        if (!rotation.Read(bs)) return false;

        if (flags.syncingVelocity)
        {
            if (!velocity.Read(bs)) return false;
        }

        if (!health.Read(bs)) return false;
        if (!armor.Read(bs)) return false;

        SRotationSync camRot;
        if (!camRot.Read(bs)) return false;
        cameraRotation = camRot.rotation;

        if (flags.hasAWeapon)
        {
            if (!weaponSlot.Read(bs)) return false;
            if (!ammo.Read(bs)) return false;
        }

        return true;
    }
};

//=============================================================================
// Full Vehicle Puresync Data
//=============================================================================

struct SVehiclePuresyncData
{
    uint16_t timeContext;
    SVehiclePuresyncFlags flags;
    SPositionSync position;
    float rotationX, rotationY, rotationZ;  // Euler angles
    SVelocitySync velocity;
    float turnSpeedX, turnSpeedY, turnSpeedZ;
    SVehicleHealthSync health;

    // Driver info
    uint8_t playerHealth;
    uint8_t playerArmor;
    uint8_t currentWeapon;

    // Trailer
    uint32_t trailerId;
    bool hasTrailer;

    SVehiclePuresyncData()
    {
        timeContext = 0;
        rotationX = rotationY = rotationZ = 0;
        turnSpeedX = turnSpeedY = turnSpeedZ = 0;
        playerHealth = 100;
        playerArmor = 0;
        currentWeapon = 0;
        trailerId = 0;
        hasTrailer = false;
    }

    void Write(NetBitStream& bs) const
    {
        bs.Write(timeContext);
        flags.Write(bs);
        position.Write(bs);

        // Rotation (Euler angles)
        bs.Write(rotationX);
        bs.Write(rotationY);
        bs.Write(rotationZ);

        // Velocity
        velocity.Write(bs);

        // Turn speed
        bs.Write(turnSpeedX);
        bs.Write(turnSpeedY);
        bs.Write(turnSpeedZ);

        // Health
        health.Write(bs);

        // Driver info
        bs.Write(playerHealth);
        bs.Write(playerArmor);

        if (flags.hasAWeapon)
        {
            bs.Write(currentWeapon);
        }

        // Trailer
        bs.WriteBit(hasTrailer);
        if (hasTrailer)
        {
            bs.Write(trailerId);
        }
    }

    bool Read(NetBitStream& bs)
    {
        if (!bs.Read(timeContext)) return false;
        if (!flags.Read(bs)) return false;
        if (!position.Read(bs)) return false;

        if (!bs.Read(rotationX)) return false;
        if (!bs.Read(rotationY)) return false;
        if (!bs.Read(rotationZ)) return false;

        if (!velocity.Read(bs)) return false;

        if (!bs.Read(turnSpeedX)) return false;
        if (!bs.Read(turnSpeedY)) return false;
        if (!bs.Read(turnSpeedZ)) return false;

        if (!health.Read(bs)) return false;

        if (!bs.Read(playerHealth)) return false;
        if (!bs.Read(playerArmor)) return false;

        if (flags.hasAWeapon)
        {
            if (!bs.Read(currentWeapon)) return false;
        }

        hasTrailer = bs.ReadBit();
        if (hasTrailer)
        {
            if (!bs.Read(trailerId)) return false;
        }

        return true;
    }
};

//=============================================================================
// Keysync Data
//=============================================================================

struct SKeysyncData
{
    uint16_t timeContext;

    // Controller state
    uint16_t leftStickX;    // -128 to 127
    uint16_t leftStickY;
    uint16_t keys;          // Button states

    // Aim data
    float aimDirectionX;
    float aimDirectionY;

    // Camera rotation
    float cameraRotation;
    float playerRotation;

    SKeysyncData()
    {
        timeContext = 0;
        leftStickX = 128;  // Center
        leftStickY = 128;
        keys = 0;
        aimDirectionX = 0;
        aimDirectionY = 0;
        cameraRotation = 0;
        playerRotation = 0;
    }

    void Write(NetBitStream& bs) const
    {
        bs.Write(timeContext);

        // Controller (compressed)
        bs.Write(static_cast<uint8_t>(leftStickX));
        bs.Write(static_cast<uint8_t>(leftStickY));

        // Keys
        bs.Write(keys);

        // Rotations (compressed to 12 bits each)
        SRotationSync camRot(cameraRotation);
        SRotationSync playerRot(playerRotation);
        camRot.Write(bs);
        playerRot.Write(bs);

        // Aim direction
        bs.Write(aimDirectionX);
        bs.Write(aimDirectionY);
    }

    bool Read(NetBitStream& bs)
    {
        if (!bs.Read(timeContext)) return false;

        uint8_t stickX, stickY;
        if (!bs.Read(stickX)) return false;
        if (!bs.Read(stickY)) return false;
        leftStickX = stickX;
        leftStickY = stickY;

        if (!bs.Read(keys)) return false;

        SRotationSync camRot, playerRot;
        if (!camRot.Read(bs)) return false;
        if (!playerRot.Read(bs)) return false;
        cameraRotation = camRot.rotation;
        playerRotation = playerRot.rotation;

        if (!bs.Read(aimDirectionX)) return false;
        if (!bs.Read(aimDirectionY)) return false;

        return true;
    }
};

//=============================================================================
// Key States Bitmask
//=============================================================================

namespace KeyState
{
    constexpr uint16_t LEFT_STICK_LEFT     = 0x0001;
    constexpr uint16_t LEFT_STICK_RIGHT    = 0x0002;
    constexpr uint16_t LEFT_STICK_UP       = 0x0004;
    constexpr uint16_t LEFT_STICK_DOWN     = 0x0008;
    constexpr uint16_t RIGHT_STICK_LEFT    = 0x0010;
    constexpr uint16_t RIGHT_STICK_RIGHT   = 0x0020;
    constexpr uint16_t RIGHT_STICK_UP      = 0x0040;
    constexpr uint16_t RIGHT_STICK_DOWN    = 0x0080;

    constexpr uint16_t BUTTON_CROSS        = 0x0100;  // Jump / Sprint
    constexpr uint16_t BUTTON_CIRCLE       = 0x0200;  // Punch / Enter vehicle
    constexpr uint16_t BUTTON_SQUARE       = 0x0400;  // Crouch
    constexpr uint16_t BUTTON_TRIANGLE     = 0x0800;  // Exit vehicle

    constexpr uint16_t BUTTON_L1           = 0x1000;  // Aim
    constexpr uint16_t BUTTON_R1           = 0x2000;  // Fire
    constexpr uint16_t BUTTON_L2           = 0x4000;  // Look behind
    constexpr uint16_t BUTTON_R2           = 0x8000;  // Secondary fire
}

//=============================================================================
// Unoccupied Vehicle Sync
//=============================================================================

struct SUnoccupiedVehicleSync
{
    uint16_t timeContext;
    uint32_t vehicleId;
    SPositionSync position;
    float rotationX, rotationY, rotationZ;
    SVelocitySync velocity;
    float turnSpeedX, turnSpeedY, turnSpeedZ;
    SVehicleHealthSync health;

    void Write(NetBitStream& bs) const
    {
        bs.Write(timeContext);
        bs.Write(vehicleId);
        position.Write(bs);
        bs.Write(rotationX);
        bs.Write(rotationY);
        bs.Write(rotationZ);
        velocity.Write(bs);
        bs.Write(turnSpeedX);
        bs.Write(turnSpeedY);
        bs.Write(turnSpeedZ);
        health.Write(bs);
    }

    bool Read(NetBitStream& bs)
    {
        if (!bs.Read(timeContext)) return false;
        if (!bs.Read(vehicleId)) return false;
        if (!position.Read(bs)) return false;
        if (!bs.Read(rotationX)) return false;
        if (!bs.Read(rotationY)) return false;
        if (!bs.Read(rotationZ)) return false;
        if (!velocity.Read(bs)) return false;
        if (!bs.Read(turnSpeedX)) return false;
        if (!bs.Read(turnSpeedY)) return false;
        if (!bs.Read(turnSpeedZ)) return false;
        if (!health.Read(bs)) return false;
        return true;
    }
};

} // namespace MTA::Android::Network

#endif // SYNC_STRUCTURES_ANDROID_H
