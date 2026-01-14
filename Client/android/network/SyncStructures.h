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
#include <cstring>
#include "CNetAndroid.h"

namespace MTA::Android::Network
{

//=============================================================================
// Constants
//=============================================================================

constexpr float SYNC_POSITION_LIMIT = 100000.0f;  // Max position value
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;
constexpr uint32_t MAX_SERVER_ELEMENTS = 131072;
constexpr uint32_t ELEMENT_ID_BITS = 17;

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
    enum
    {
        BITCOUNT = 15
    };

    struct
    {
        bool bIsInWater : 1;
        bool bIsOnGround : 1;
        bool bHasJetPack : 1;
        bool bIsDucked : 1;
        bool bWearsGoogles : 1;
        bool bHasContact : 1;
        bool bIsChoking : 1;
        bool bAkimboTargetUp : 1;
        bool bIsOnFire : 1;
        bool bHasAWeapon : 1;
        bool bSyncingVelocity : 1;
        bool bStealthAiming : 1;
        bool isReloadingWeapon : 1;
        bool animInterrupted : 1;
        bool hangingDuringClimb : 1;
    } data;

    SPlayerPuresyncFlags()
    {
        std::memset(&data, 0, sizeof(data));
        data.bIsOnGround = true;
    }

    void Write(NetBitStream& bs) const
    {
        bs.WriteBits(reinterpret_cast<const uint8_t*>(&data), BITCOUNT);
    }

    bool Read(NetBitStream& bs)
    {
        return bs.ReadBits(reinterpret_cast<uint8_t*>(&data), BITCOUNT);
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
// PC-compatible sync helpers (MTA Windows protocol)
//=============================================================================

struct SControllerState
{
    int16_t LeftStickX = 0;
    int16_t LeftStickY = 0;
    int16_t LeftShoulder1 = 0;
    int16_t RightShoulder1 = 0;
    int16_t ButtonSquare = 0;
    int16_t ButtonCross = 0;
    int16_t ButtonCircle = 0;
    int16_t ButtonTriangle = 0;
    int16_t ShockButtonL = 0;
    int16_t m_bPedWalk = 0;
};

inline bool ReadBitsToUInt(NetBitStream& bs, uint32_t& value, size_t bits)
{
    value = 0;
    return bs.ReadBits(reinterpret_cast<uint8_t*>(&value), bits);
}

inline void WriteBitsFromUInt(NetBitStream& bs, uint32_t value, size_t bits)
{
    bs.WriteBits(reinterpret_cast<const uint8_t*>(&value), bits);
}

template <unsigned int integerBits, unsigned int fractionalBits>
struct SFloatSync
{
    bool Read(NetBitStream& bs)
    {
        SFixedPointNumber num{};
        if (bs.ReadBits(reinterpret_cast<uint8_t*>(&num), integerBits + fractionalBits))
        {
            data.fValue = static_cast<float>(static_cast<double>(num.iValue) / (1 << fractionalBits));
            return true;
        }
        return false;
    }

    void Write(NetBitStream& bs) const
    {
        const double limitsMax = (1 << (integerBits - 1)) - 1;
        const double limitsMin = 0 - (1 << (integerBits - 1));
        const double scale = 1 << fractionalBits;

        double dValue = data.fValue;
        if (dValue < limitsMin) dValue = limitsMin;
        if (dValue > limitsMax) dValue = limitsMax;

        SFixedPointNumber num{};
        num.iValue = static_cast<int>(std::lround(dValue * scale));
        bs.WriteBits(reinterpret_cast<const uint8_t*>(&num), integerBits + fractionalBits);
    }

    struct
    {
        float fValue = 0.0f;
    } data;

private:
    struct SFixedPointNumber
    {
        int iValue : integerBits + fractionalBits;
    };
};

inline bool ReadFixedPoint(NetBitStream& bs, int totalBits, int fracBits, float& outValue)
{
    uint32_t raw = 0;
    if (!ReadBitsToUInt(bs, raw, static_cast<size_t>(totalBits)))
        return false;

    const uint32_t signBit = 1u << (totalBits - 1);
    int32_t signedValue = 0;
    if (raw & signBit)
    {
        uint32_t mask = ~((1u << totalBits) - 1u);
        signedValue = static_cast<int32_t>(raw | mask);
    }
    else
    {
        signedValue = static_cast<int32_t>(raw);
    }

    outValue = static_cast<float>(signedValue) / static_cast<float>(1 << fracBits);
    return true;
}

inline void WriteFixedPoint(NetBitStream& bs, int totalBits, int fracBits, float value)
{
    const float scale = static_cast<float>(1 << fracBits);
    const float maxVal = static_cast<float>((1 << (totalBits - 1)) - 1);
    const float minVal = static_cast<float>(-(1 << (totalBits - 1)));
    float clamped = ClampFloat(value, minVal, maxVal);
    int32_t fixed = static_cast<int32_t>(std::lround(clamped * scale));
    WriteBitsFromUInt(bs, static_cast<uint32_t>(fixed), static_cast<size_t>(totalBits));
}

inline float ReadFloatAsBits(NetBitStream& bs, int bits, float minValue, float maxValue)
{
    uint32_t raw = 0;
    if (!ReadBitsToUInt(bs, raw, static_cast<size_t>(bits)))
        return minValue;

    const float alpha = static_cast<float>(raw) / static_cast<float>((1u << bits) - 1u);
    return minValue + (maxValue - minValue) * alpha;
}

inline void WriteFloatAsBits(NetBitStream& bs, int bits, float minValue, float maxValue, float value, bool wrap)
{
    float v = value;
    if (wrap)
    {
        v = WrapAngle(v);
    }
    else
    {
        v = ClampFloat(v, minValue, maxValue);
    }

    float alpha = (v - minValue) / (maxValue - minValue);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    uint32_t raw = static_cast<uint32_t>(std::lround(alpha * static_cast<float>((1u << bits) - 1u)));
    WriteBitsFromUInt(bs, raw, static_cast<size_t>(bits));
}

inline bool ReadElementId(NetBitStream& bs, uint32_t& outId)
{
    uint32_t raw = 0;
    if (!ReadBitsToUInt(bs, raw, ELEMENT_ID_BITS))
        return false;

    const uint32_t maxValue = (1u << ELEMENT_ID_BITS) - 1u;
    if (raw == maxValue)
        outId = 0xFFFFFFFFu;
    else
        outId = raw;
    return true;
}

inline void WriteElementId(NetBitStream& bs, uint32_t id)
{
    uint32_t value = id;
    const uint32_t maxValue = (1u << ELEMENT_ID_BITS) - 1u;
    if (value == 0xFFFFFFFFu)
        value = maxValue;
    WriteBitsFromUInt(bs, value, ELEMENT_ID_BITS);
}

struct SPcPositionSync
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool useFloats = false;

    SPcPositionSync(bool bUseFloats = false) : useFloats(bUseFloats) {}

    bool Read(NetBitStream& bs)
    {
        if (useFloats)
        {
            if (!bs.Read(x)) return false;
            if (!bs.Read(y)) return false;
            if (!bs.Read(z)) return false;
        }
        else
        {
            SFloatSync<14, 10> sx;
            SFloatSync<14, 10> sy;
            if (!sx.Read(bs)) return false;
            if (!sy.Read(bs)) return false;
            if (!bs.Read(z)) return false;
            x = sx.data.fValue;
            y = sy.data.fValue;
        }

        return (x > -SYNC_POSITION_LIMIT && x < SYNC_POSITION_LIMIT &&
                y > -SYNC_POSITION_LIMIT && y < SYNC_POSITION_LIMIT &&
                z > -SYNC_POSITION_LIMIT && z < SYNC_POSITION_LIMIT);
    }

    void Write(NetBitStream& bs) const
    {
        float cx = ClampFloat(x, -SYNC_POSITION_LIMIT + 1.0f, SYNC_POSITION_LIMIT - 1.0f);
        float cy = ClampFloat(y, -SYNC_POSITION_LIMIT + 1.0f, SYNC_POSITION_LIMIT - 1.0f);
        float cz = ClampFloat(z, -SYNC_POSITION_LIMIT + 1.0f, SYNC_POSITION_LIMIT - 1.0f);

        if (useFloats)
        {
            bs.Write(cx);
            bs.Write(cy);
            bs.Write(cz);
        }
        else
        {
            SFloatSync<14, 10> sx;
            SFloatSync<14, 10> sy;
            sx.data.fValue = cx;
            sy.data.fValue = cy;
            sx.Write(bs);
            sy.Write(bs);
            bs.Write(cz);
        }
    }
};

inline bool ReadCameraOrientation(NetBitStream& bs)
{
    (void)ReadFloatAsBits(bs, 8, -PI, PI);
    (void)ReadFloatAsBits(bs, 8, -PI, PI);

    bool useAbsolute = bs.ReadBit();
    (void)useAbsolute;

    uint32_t idx = 0;
    if (!ReadBitsToUInt(bs, idx, 2)) return false;

    struct
    {
        uint32_t bits;
        float range;
    } table[4] = {
        {3, 4.0f},
        {5, 16.0f},
        {9, 256.0f},
        {14, 8192.0f},
    };

    const uint32_t bits = table[idx].bits;
    const float range = table[idx].range;

    (void)ReadFloatAsBits(bs, static_cast<int>(bits), -range, range);
    (void)ReadFloatAsBits(bs, static_cast<int>(bits), -range, range);
    (void)ReadFloatAsBits(bs, static_cast<int>(bits), -range, range);
    return true;
}

struct SPcPedRotationSync
{
    float rotation = 0.0f;

    bool Read(NetBitStream& bs)
    {
        uint16_t raw = 0;
        if (!bs.Read(raw)) return false;
        rotation = (static_cast<float>(raw) / 65535.0f * TWO_PI) - PI;
        return true;
    }

    void Write(NetBitStream& bs) const
    {
        float wrapped = WrapAngle(rotation);
        float normalized = (wrapped + PI) / TWO_PI;
        uint16_t raw = static_cast<uint16_t>(normalized * 65535.0f);
        bs.Write(raw);
    }
};

struct SPcCameraRotationSync
{
    float rotation = 0.0f;

    bool Read(NetBitStream& bs)
    {
        rotation = ReadFloatAsBits(bs, 12, -PI, PI);
        return true;
    }

    void Write(NetBitStream& bs) const
    {
        WriteFloatAsBits(bs, 12, -PI, PI, rotation, true);
    }
};

struct SPcVelocitySync
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    bool Read(NetBitStream& bs)
    {
        if (!bs.ReadBit())
        {
            x = y = z = 0.0f;
            return true;
        }

        float modulus = 0.0f;
        if (!bs.Read(modulus)) return false;

        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
        if (!bs.ReadNormVector(nx, ny, nz)) return false;

        x = nx * modulus;
        y = ny * modulus;
        z = nz * modulus;
        return true;
    }

    void Write(NetBitStream& bs) const
    {
        float modulus = std::sqrt(x * x + y * y + z * z);
        if (modulus <= 0.0f)
        {
            bs.WriteBit(false);
            return;
        }

        bs.WriteBit(true);
        bs.Write(modulus);
        bs.WriteNormVector(x / modulus, y / modulus, z / modulus);
    }
};

struct SFullKeysyncSync
{
    struct
    {
        bool          bLeftShoulder1 : 1;
        bool          bRightShoulder1 : 1;
        bool          bButtonSquare : 1;
        bool          bButtonCross : 1;
        bool          bButtonCircle : 1;
        bool          bButtonTriangle : 1;
        bool          bShockButtonL : 1;
        bool          bPedWalk : 1;
        unsigned char ucButtonSquare;
        unsigned char ucButtonCross;
        short         sLeftStickX;
        short         sLeftStickY;
    } data{};

    bool Read(NetBitStream& bs)
    {
        uint8_t keyBits = 0;
        if (!bs.ReadBits(&keyBits, 8)) return false;
        data.bLeftShoulder1 = (keyBits & (1u << 0)) != 0;
        data.bRightShoulder1 = (keyBits & (1u << 1)) != 0;
        data.bButtonSquare = (keyBits & (1u << 2)) != 0;
        data.bButtonCross = (keyBits & (1u << 3)) != 0;
        data.bButtonCircle = (keyBits & (1u << 4)) != 0;
        data.bButtonTriangle = (keyBits & (1u << 5)) != 0;
        data.bShockButtonL = (keyBits & (1u << 6)) != 0;
        data.bPedWalk = (keyBits & (1u << 7)) != 0;

        if (bs.ReadBit())
        {
            if (!bs.Read(data.ucButtonSquare)) return false;
        }
        else
        {
            data.ucButtonSquare = 0;
        }

        if (bs.ReadBit())
        {
            if (!bs.Read(data.ucButtonCross)) return false;
        }
        else
        {
            data.ucButtonCross = 0;
        }

        int8_t leftX = 0;
        int8_t leftY = 0;
        if (!bs.Read(leftX)) return false;
        if (!bs.Read(leftY)) return false;

        data.sLeftStickX = static_cast<short>(static_cast<float>(leftX) * 128.0f / 127.0f);
        data.sLeftStickY = static_cast<short>(static_cast<float>(leftY) * 128.0f / 127.0f);
        return true;
    }

    void Write(NetBitStream& bs) const
    {
        uint8_t keyBits = 0;
        keyBits |= data.bLeftShoulder1 ? (1u << 0) : 0;
        keyBits |= data.bRightShoulder1 ? (1u << 1) : 0;
        keyBits |= data.bButtonSquare ? (1u << 2) : 0;
        keyBits |= data.bButtonCross ? (1u << 3) : 0;
        keyBits |= data.bButtonCircle ? (1u << 4) : 0;
        keyBits |= data.bButtonTriangle ? (1u << 5) : 0;
        keyBits |= data.bShockButtonL ? (1u << 6) : 0;
        keyBits |= data.bPedWalk ? (1u << 7) : 0;
        bs.WriteBits(&keyBits, 8);

        if (data.ucButtonSquare >= 1 && data.ucButtonSquare <= 254)
        {
            bs.WriteBit(true);
            bs.Write(data.ucButtonSquare);
        }
        else
        {
            bs.WriteBit(false);
        }

        if (data.ucButtonCross >= 1 && data.ucButtonCross <= 254)
        {
            bs.WriteBit(true);
            bs.Write(data.ucButtonCross);
        }
        else
        {
            bs.WriteBit(false);
        }

        int8_t leftX = static_cast<int8_t>(static_cast<float>(data.sLeftStickX) * 127.0f / 128.0f);
        int8_t leftY = static_cast<int8_t>(static_cast<float>(data.sLeftStickY) * 127.0f / 128.0f);
        bs.Write(leftX);
        bs.Write(leftY);
    }
};

struct SFullKeysyncDebug
{
    uint8_t keyBits = 0;
    bool hasButtonSquare = false;
    bool hasButtonCross = false;
    uint8_t buttonSquare = 0;
    uint8_t buttonCross = 0;
    int8_t leftX = 0;
    int8_t leftY = 0;
};

inline bool ReadFullKeysync(SControllerState& controller, NetBitStream& bs, SFullKeysyncDebug* debug = nullptr)
{
    SFullKeysyncSync keys;
    uint8_t keyBits = 0;
    if (!bs.ReadBits(&keyBits, 8)) return false;
    keys.data.bLeftShoulder1 = (keyBits & (1u << 0)) != 0;
    keys.data.bRightShoulder1 = (keyBits & (1u << 1)) != 0;
    keys.data.bButtonSquare = (keyBits & (1u << 2)) != 0;
    keys.data.bButtonCross = (keyBits & (1u << 3)) != 0;
    keys.data.bButtonCircle = (keyBits & (1u << 4)) != 0;
    keys.data.bButtonTriangle = (keyBits & (1u << 5)) != 0;
    keys.data.bShockButtonL = (keyBits & (1u << 6)) != 0;
    keys.data.bPedWalk = (keyBits & (1u << 7)) != 0;

    short sButtonSquare = keys.data.bButtonSquare ? 255 : 0;
    short sButtonCross = keys.data.bButtonCross ? 255 : 0;

    bool hasButtonSquare = bs.ReadBit();
    if (hasButtonSquare)
    {
        if (!bs.Read(keys.data.ucButtonSquare)) return false;
        sButtonSquare = static_cast<short>(keys.data.ucButtonSquare);
    }
    else
    {
        keys.data.ucButtonSquare = 0;
    }

    bool hasButtonCross = bs.ReadBit();
    if (hasButtonCross)
    {
        if (!bs.Read(keys.data.ucButtonCross)) return false;
        sButtonCross = static_cast<short>(keys.data.ucButtonCross);
    }
    else
    {
        keys.data.ucButtonCross = 0;
    }

    int8_t leftX = 0;
    int8_t leftY = 0;
    if (!bs.Read(leftX)) return false;
    if (!bs.Read(leftY)) return false;
    keys.data.sLeftStickX = static_cast<short>(static_cast<float>(leftX) * 128.0f / 127.0f);
    keys.data.sLeftStickY = static_cast<short>(static_cast<float>(leftY) * 128.0f / 127.0f);

    controller.LeftShoulder1 = keys.data.bLeftShoulder1;
    controller.RightShoulder1 = keys.data.bRightShoulder1;
    controller.ButtonSquare = sButtonSquare;
    controller.ButtonCross = sButtonCross;
    controller.ButtonCircle = keys.data.bButtonCircle;
    controller.ButtonTriangle = keys.data.bButtonTriangle;
    controller.ShockButtonL = keys.data.bShockButtonL;
    controller.m_bPedWalk = keys.data.bPedWalk;
    controller.LeftStickX = keys.data.sLeftStickX;
    controller.LeftStickY = keys.data.sLeftStickY;

    if (debug)
    {
        debug->keyBits = keyBits;
        debug->hasButtonSquare = hasButtonSquare;
        debug->hasButtonCross = hasButtonCross;
        debug->buttonSquare = keys.data.ucButtonSquare;
        debug->buttonCross = keys.data.ucButtonCross;
        debug->leftX = leftX;
        debug->leftY = leftY;
    }
    return true;
}

inline void WriteFullKeysync(const SControllerState& controller, NetBitStream& bs)
{
    SFullKeysyncSync keys;
    keys.data.bLeftShoulder1 = (controller.LeftShoulder1 != 0);
    keys.data.bRightShoulder1 = (controller.RightShoulder1 != 0);
    keys.data.bButtonSquare = (controller.ButtonSquare != 0);
    keys.data.bButtonCross = (controller.ButtonCross != 0);
    keys.data.bButtonCircle = (controller.ButtonCircle != 0);
    keys.data.bButtonTriangle = (controller.ButtonTriangle != 0);
    keys.data.bShockButtonL = (controller.ShockButtonL != 0);
    keys.data.bPedWalk = (controller.m_bPedWalk != 0);
    keys.data.ucButtonSquare = static_cast<unsigned char>(controller.ButtonSquare);
    keys.data.ucButtonCross = static_cast<unsigned char>(controller.ButtonCross);
    keys.data.sLeftStickX = controller.LeftStickX;
    keys.data.sLeftStickY = controller.LeftStickY;
    keys.Write(bs);
}

struct SPcWeaponSlotSync
{
    uint8_t slot = 0;

    bool Read(NetBitStream& bs)
    {
        uint32_t raw = 0;
        if (!ReadBitsToUInt(bs, raw, 4)) return false;
        slot = static_cast<uint8_t>(raw & 0x0F);
        return true;
    }

    void Write(NetBitStream& bs) const
    {
        WriteBitsFromUInt(bs, static_cast<uint32_t>(slot & 0x0F), 4);
    }
};

struct SWeaponAmmoSync
{
    uint16_t totalAmmo = 0;
    uint16_t ammoInClip = 0;

    bool Read(NetBitStream& bs, bool syncTotalAmmo = true, bool syncAmmoInClip = true)
    {
        if (syncTotalAmmo && !bs.ReadCompressed(totalAmmo)) return false;
        if (syncAmmoInClip && !bs.ReadCompressed(ammoInClip)) return false;
        return true;
    }

    void Write(NetBitStream& bs, bool syncTotalAmmo = true, bool syncAmmoInClip = true) const
    {
        if (syncTotalAmmo) bs.WriteCompressed(totalAmmo);
        if (syncAmmoInClip) bs.WriteCompressed(ammoInClip);
    }
};

struct SWeaponAimSync
{
    float arm = 0.0f;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
    float weaponRange = 0.0f;
    bool full = true;

    explicit SWeaponAimSync(float range = 0.0f, bool isFull = true)
        : weaponRange(range), full(isFull)
    {}

    bool Read(NetBitStream& bs)
    {
        int16_t armRaw = 0;
        if (!bs.Read(armRaw)) return false;
        arm = (static_cast<float>(armRaw) * PI / 180.0f) / 90.0f;

        if (full)
        {
            if (!bs.Read(originX)) return false;
            if (!bs.Read(originY)) return false;
            if (!bs.Read(originZ)) return false;

            float dirX = 0.0f, dirY = 0.0f, dirZ = 0.0f;
            if (!bs.ReadNormVector(dirX, dirZ, dirY)) return false;

            targetX = originX + dirX * weaponRange;
            targetY = originY + dirY * weaponRange;
            targetZ = originZ + dirZ * weaponRange;
        }

        return true;
    }

    void Write(NetBitStream& bs) const
    {
        int16_t armRaw = static_cast<int16_t>(arm * 90.0f * 180.0f / PI);
        bs.Write(armRaw);

        if (full)
        {
            bs.Write(originX);
            bs.Write(originY);
            bs.Write(originZ);

            float dirX = targetX - originX;
            float dirY = targetY - originY;
            float dirZ = targetZ - originZ;
            float len = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
            if (len > 0.0f)
            {
                dirX /= len;
                dirY /= len;
                dirZ /= len;
            }
            bs.WriteNormVector(dirX, dirZ, dirY);
        }
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

        if (flags.data.bSyncingVelocity)
        {
            velocity.Write(bs);
        }

        health.Write(bs);
        armor.Write(bs);

        // Camera rotation (compressed)
        SRotationSync camRot(cameraRotation);
        camRot.Write(bs);

        if (flags.data.bHasAWeapon)
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

        if (flags.data.bSyncingVelocity)
        {
            if (!velocity.Read(bs)) return false;
        }

        if (!health.Read(bs)) return false;
        if (!armor.Read(bs)) return false;

        SRotationSync camRot;
        if (!camRot.Read(bs)) return false;
        cameraRotation = camRot.rotation;

        if (flags.data.bHasAWeapon)
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
