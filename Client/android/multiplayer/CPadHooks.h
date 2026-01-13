/**
 * CPadHooks.h - CPad function hooks for remote player input
 *
 * Based on SA-MP Android's pad.cpp implementation.
 * These hooks intercept game input queries and return remote player keys
 * when the game is processing a remote player (CWorld::PlayerInFocus > 0).
 *
 * How it works:
 * 1. CPed::ProcessControl is hooked to set CWorld::PlayerInFocus
 * 2. When processing remote player, PlayerInFocus = player slot
 * 3. CPad functions check PlayerInFocus and return remote keys
 * 4. After processing, PlayerInFocus is restored to 0
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <android/log.h>
#include "../hooks/ARMHookInstaller.h"

namespace MTA::Android::Multiplayer
{

//=============================================================================
// Logging
//=============================================================================
#define PAD_LOG_TAG "MTA-CPad"
#define PAD_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, PAD_LOG_TAG, __VA_ARGS__)
#define PAD_LOGI(...) __android_log_print(ANDROID_LOG_INFO, PAD_LOG_TAG, __VA_ARGS__)
#define PAD_LOGW(...) __android_log_print(ANDROID_LOG_WARN, PAD_LOG_TAG, __VA_ARGS__)
#define PAD_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, PAD_LOG_TAG, __VA_ARGS__)

//=============================================================================
// Key Enums (from SA-MP pad.h)
//=============================================================================
enum ePadKeys
{
    KEY_ACTION = 0,           // TAB / ALT GR/LCTRL/NUM0
    KEY_CROUCH,               // C / H / CAPSLOCK
    KEY_FIRE,                 // LCTRL/LMB / LALT
    KEY_SPRINT,               // SPACE / W
    KEY_SECONDARY_ATTACK,     // ENTER
    KEY_JUMP,                 // LSHIFT / S
    KEY_LOOK_RIGHT,           // E
    KEY_HANDBRAKE,            // RMB / SPACE
    KEY_LOOK_LEFT,            // Q
    KEY_SUBMISSION,           // NUM1/MMB / 2/NUMPAD+
    KEY_WALK,                 // LALT
    KEY_ANALOG_UP,            // NUM8
    KEY_ANALOG_DOWN,          // NUM2
    KEY_ANALOG_LEFT,          // NUM4
    KEY_ANALOG_RIGHT,         // NUM6
    KEY_SIZE,
    KEY_YES,                  // Y
    KEY_NO,                   // N
    KEY_CTRL_BACK,            // H
};

//=============================================================================
// PAD_KEYS structure (from SA-MP)
//=============================================================================
struct PAD_KEYS
{
    int16_t wKeyLR;           // Left/Right analog (-128 to 127)
    int16_t wKeyUD;           // Up/Down analog (-128 to 127)
    bool bKeys[KEY_SIZE + 4]; // Boolean key states
    bool bIgnoreJump;         // Jump debounce flag

    void Reset()
    {
        wKeyLR = 0;
        wKeyUD = 0;
        memset(bKeys, 0, sizeof(bKeys));
        bIgnoreJump = false;
    }
};

// Verify size matches SA-MP (24 bytes)
static_assert(sizeof(PAD_KEYS) == 24, "PAD_KEYS size mismatch");

//=============================================================================
// Constants
//=============================================================================
constexpr int MAX_REMOTE_PLAYERS = 32;  // Support up to 32 remote players

//=============================================================================
// ARM64 Addresses (GTA:SA v2.10)
//=============================================================================
namespace ARM64
{
    // Global variable
    constexpr uintptr_t CWORLD_PLAYERINFOCUS = 0xBDCAE8;  // uint8_t

    // CPad functions
    constexpr uintptr_t CPAD_GETPEDWALKLEFTRIGHT = 0x4DCD40;
    constexpr uintptr_t CPAD_GETPEDWALKUPDOWN = 0x4DCDD4;
    constexpr uintptr_t CPAD_GETSPRINT = 0x4DF100;
    constexpr uintptr_t CPAD_JUMPJUSTDOWN = 0x4DEF84;
    constexpr uintptr_t CPAD_GETJUMP = 0x4DEF14;
    constexpr uintptr_t CPAD_GETAUTOCLIMB = 0x4DED68;
    constexpr uintptr_t CPAD_GETABORTCLIMB = 0x4DEE2C;
    constexpr uintptr_t CPAD_DIVEJUSTDOWN = 0x4DF00C;
    constexpr uintptr_t CPAD_SWIMJUMPJUSTDOWN = 0x4DF07C;
    constexpr uintptr_t CPAD_MELEEATTACKJUSTDOWN = 0x4DDF84;
    constexpr uintptr_t CPAD_DUCKJUSTDOWN = 0x4DECB4;
    constexpr uintptr_t CPAD_GETBLOCK = 0x4DE308;
    constexpr uintptr_t CPAD_GETSTEERINGLEFTRIGHT = 0x4DC4F0;
    constexpr uintptr_t CPAD_GETSTEERINGUPDOWN = 0x4DC70C;
    constexpr uintptr_t CPAD_GETACCELERATE = 0x4DE358;
    constexpr uintptr_t CPAD_GETBRAKE = 0x4DD740;
    constexpr uintptr_t CPAD_GETHANDBRAKE = 0x4DD534;
    constexpr uintptr_t CPAD_GETHORN = 0x4DD29C;
    constexpr uintptr_t CPAD_GETENTERTARGETING = 0x4DE59C;
    constexpr uintptr_t CPAD_GETWEAPON = 0x4DD9D0;
    constexpr uintptr_t CPAD_GETNITROFIRED = 0x4DD42C;
    constexpr uintptr_t CPAD_GETTURRETLEFT = 0x4DD080;
    constexpr uintptr_t CPAD_GETTURRETRIGHT = 0x4DD0A0;

    // ProcessControl
    constexpr uintptr_t CPED_PROCESSCONTROL = 0x598730;

    // PLT entries for ProcessControl hooks
    constexpr uintptr_t PLT_CPED_PROCESSCONTROL = 0x833150;
}

//=============================================================================
// CPadHooks Class
//=============================================================================
class CPadHooks
{
public:
    static CPadHooks& GetInstance()
    {
        static CPadHooks instance;
        return instance;
    }

    /**
     * Initialize the hook system
     * @param gameBase Base address of libGTASA.so
     */
    bool Initialize(uintptr_t gameBase)
    {
        if (m_initialized)
            return true;

        m_gameBase = gameBase;

        // Get pointer to CWorld::PlayerInFocus
        m_pPlayerInFocus = reinterpret_cast<uint8_t*>(gameBase + ARM64::CWORLD_PLAYERINFOCUS);

        // Reset all keys
        m_localPlayerKeys.Reset();
        for (int i = 0; i < MAX_REMOTE_PLAYERS; i++)
        {
            m_remotePlayerKeys[i].Reset();
        }

        // Install hooks
        if (!InstallHooks())
        {
            PAD_LOGE("Failed to install CPad hooks");
            return false;
        }

        m_initialized = true;
        PAD_LOGI("CPadHooks initialized successfully");
        PAD_LOGI("  CWorld::PlayerInFocus at 0x%lx", (unsigned long)(gameBase + ARM64::CWORLD_PLAYERINFOCUS));

        return true;
    }

    /**
     * Update remote player keys from sync data
     * @param playerSlot Player slot (2-32, 0=local, 1=unused)
     * @param leftStickX X axis (0-255, center=128)
     * @param leftStickY Y axis (0-255, center=128)
     * @param keyFlags Key bitfield from sync packet
     */
    void SetRemotePlayerKeys(uint8_t playerSlot, uint8_t leftStickX, uint8_t leftStickY, uint16_t keyFlags)
    {
        if (playerSlot >= MAX_REMOTE_PLAYERS)
            return;

        PAD_KEYS& keys = m_remotePlayerKeys[playerSlot];

        // Convert MTA's 0-255 range to signed -128 to 127
        // MTA: 0=full left, 128=center, 255=full right
        // SA-MP: -128=full left, 0=center, 127=full right
        keys.wKeyLR = static_cast<int16_t>((leftStickX - 128));
        keys.wKeyUD = static_cast<int16_t>((leftStickY - 128));

        // Convert key flags to boolean array
        // MTA key flag bits (from SPlayerPuresyncFlags or keysync)
        // These mappings may need adjustment based on actual MTA protocol
        keys.bKeys[KEY_FIRE] = (keyFlags & 0x0004) != 0;
        keys.bKeys[KEY_JUMP] = (keyFlags & 0x0020) != 0;
        keys.bKeys[KEY_SPRINT] = (keyFlags & 0x0008) != 0;
        keys.bKeys[KEY_CROUCH] = (keyFlags & 0x0002) != 0;
        keys.bKeys[KEY_ACTION] = (keyFlags & 0x0001) != 0;
        keys.bKeys[KEY_WALK] = (keyFlags & 0x0400) != 0;
        keys.bKeys[KEY_HANDBRAKE] = (keyFlags & 0x0080) != 0;
        keys.bKeys[KEY_SECONDARY_ATTACK] = (keyFlags & 0x0010) != 0;

        // Reset jump ignore if jump is released
        if (!keys.bKeys[KEY_JUMP])
            keys.bIgnoreJump = false;
    }

    /**
     * Get the current player being processed
     */
    uint8_t GetCurrentPlayer() const { return m_currentPlayer; }

    /**
     * Set the current player being processed (called from player manager)
     * This writes directly to CWorld::PlayerInFocus so CPad functions return correct keys
     */
    void SetCurrentPlayer(uint8_t player)
    {
        m_currentPlayer = player;
        if (m_pPlayerInFocus != nullptr)
        {
            *m_pPlayerInFocus = player;
        }
    }

    /**
     * Check if hooks are initialized
     */
    bool IsInitialized() const { return m_initialized; }

private:
    CPadHooks() = default;

    bool InstallHooks();

    // Instance data
    uintptr_t m_gameBase = 0;
    bool m_initialized = false;
    uint8_t* m_pPlayerInFocus = nullptr;
    uint8_t m_currentPlayer = 0;

    // Key storage
    PAD_KEYS m_localPlayerKeys;
    PAD_KEYS m_remotePlayerKeys[MAX_REMOTE_PLAYERS];

    //=========================================================================
    // Original function pointers
    //=========================================================================
    static uint16_t (*s_CPad_GetPedWalkLeftRight)(uintptr_t thiz);
    static uint16_t (*s_CPad_GetPedWalkUpDown)(uintptr_t thiz);
    static uint32_t (*s_CPad_GetSprint)(uintptr_t thiz, uint32_t unk);
    static uint32_t (*s_CPad_JumpJustDown)(uintptr_t thiz);
    static uint32_t (*s_CPad_GetJump)(uintptr_t thiz);
    static void (*s_CPed_ProcessControl)(uintptr_t thiz);

    //=========================================================================
    // Hook implementations
    //=========================================================================
    static uint16_t Hook_GetPedWalkLeftRight(uintptr_t thiz);
    static uint16_t Hook_GetPedWalkUpDown(uintptr_t thiz);
    static uint32_t Hook_GetSprint(uintptr_t thiz, uint32_t unk);
    static uint32_t Hook_JumpJustDown(uintptr_t thiz);
    static uint32_t Hook_GetJump(uintptr_t thiz);
    static void Hook_ProcessControl(uintptr_t thiz);

    // Friend declarations for hook access
    friend uint16_t Hook_GetPedWalkLeftRight(uintptr_t);
    friend uint16_t Hook_GetPedWalkUpDown(uintptr_t);
};

//=============================================================================
// Static member definitions
//=============================================================================
inline uint16_t (*CPadHooks::s_CPad_GetPedWalkLeftRight)(uintptr_t) = nullptr;
inline uint16_t (*CPadHooks::s_CPad_GetPedWalkUpDown)(uintptr_t) = nullptr;
inline uint32_t (*CPadHooks::s_CPad_GetSprint)(uintptr_t, uint32_t) = nullptr;
inline uint32_t (*CPadHooks::s_CPad_JumpJustDown)(uintptr_t) = nullptr;
inline uint32_t (*CPadHooks::s_CPad_GetJump)(uintptr_t) = nullptr;
inline void (*CPadHooks::s_CPed_ProcessControl)(uintptr_t) = nullptr;

//=============================================================================
// Hook implementations
//=============================================================================

inline uint16_t CPadHooks::Hook_GetPedWalkLeftRight(uintptr_t thiz)
{
    auto& hooks = GetInstance();
    uint8_t playerInFocus = *hooks.m_pPlayerInFocus;

    if (playerInFocus > 0 && playerInFocus < MAX_REMOTE_PLAYERS)
    {
        // Remote player - return stored keys
        PAD_KEYS& keys = hooks.m_remotePlayerKeys[playerInFocus];
        int16_t result = keys.wKeyLR;

        // Walking adjustment (from SA-MP)
        if ((result == -128 || result == 128) && keys.bKeys[KEY_WALK])
        {
            result = 32;  // Reduced for walking
        }

        return static_cast<uint16_t>(result);
    }
    else
    {
        // Local player - call original
        uint16_t result = s_CPad_GetPedWalkLeftRight(thiz);
        hooks.m_localPlayerKeys.wKeyLR = static_cast<int16_t>(result);
        return result;
    }
}

inline uint16_t CPadHooks::Hook_GetPedWalkUpDown(uintptr_t thiz)
{
    auto& hooks = GetInstance();
    uint8_t playerInFocus = *hooks.m_pPlayerInFocus;

    if (playerInFocus > 0 && playerInFocus < MAX_REMOTE_PLAYERS)
    {
        // Remote player - return stored keys
        PAD_KEYS& keys = hooks.m_remotePlayerKeys[playerInFocus];
        int16_t result = keys.wKeyUD;

        // Walking adjustment
        if ((result == -128 || result == 128) && keys.bKeys[KEY_WALK])
        {
            result = 32;
        }

        return static_cast<uint16_t>(result);
    }
    else
    {
        // Local player
        uint16_t result = s_CPad_GetPedWalkUpDown(thiz);
        hooks.m_localPlayerKeys.wKeyUD = static_cast<int16_t>(result);
        return result;
    }
}

inline uint32_t CPadHooks::Hook_GetSprint(uintptr_t thiz, uint32_t unk)
{
    auto& hooks = GetInstance();
    uint8_t playerInFocus = *hooks.m_pPlayerInFocus;

    if (playerInFocus > 0 && playerInFocus < MAX_REMOTE_PLAYERS)
    {
        return hooks.m_remotePlayerKeys[playerInFocus].bKeys[KEY_SPRINT] ? 1 : 0;
    }
    else
    {
        uint32_t result = s_CPad_GetSprint(thiz, unk);
        hooks.m_localPlayerKeys.bKeys[KEY_SPRINT] = result != 0;
        return result;
    }
}

inline uint32_t CPadHooks::Hook_JumpJustDown(uintptr_t thiz)
{
    auto& hooks = GetInstance();
    uint8_t playerInFocus = *hooks.m_pPlayerInFocus;

    if (playerInFocus > 0 && playerInFocus < MAX_REMOTE_PLAYERS)
    {
        PAD_KEYS& keys = hooks.m_remotePlayerKeys[playerInFocus];

        if (!keys.bIgnoreJump && keys.bKeys[KEY_JUMP] && !keys.bKeys[KEY_HANDBRAKE])
        {
            keys.bIgnoreJump = true;
            return 1;
        }
        return 0;
    }
    else
    {
        uint32_t result = s_CPad_JumpJustDown(thiz);
        hooks.m_localPlayerKeys.bKeys[KEY_JUMP] = result != 0;
        return result;
    }
}

inline uint32_t CPadHooks::Hook_GetJump(uintptr_t thiz)
{
    auto& hooks = GetInstance();
    uint8_t playerInFocus = *hooks.m_pPlayerInFocus;

    if (playerInFocus > 0 && playerInFocus < MAX_REMOTE_PLAYERS)
    {
        PAD_KEYS& keys = hooks.m_remotePlayerKeys[playerInFocus];
        if (keys.bIgnoreJump) return 0;
        return keys.bKeys[KEY_JUMP] ? 1 : 0;
    }
    else
    {
        uint32_t result = s_CPad_GetJump(thiz);
        hooks.m_localPlayerKeys.bKeys[KEY_JUMP] = result != 0;
        return result;
    }
}

inline void CPadHooks::Hook_ProcessControl(uintptr_t thiz)
{
    auto& hooks = GetInstance();

    // Find which player this ped belongs to
    // For now, we check if it's a remote player by comparing ped pointers
    // This will be improved to use proper player lookup

    uint8_t playerSlot = 0;  // 0 = local player

    // TODO: Implement FindPlayerNumFromPedPtr equivalent
    // For now, we use a simple approach based on CWorld::PlayerInFocus

    uint8_t savedPlayerInFocus = *hooks.m_pPlayerInFocus;

    if (playerSlot > 0)
    {
        // Remote player
        *hooks.m_pPlayerInFocus = playerSlot;
        s_CPed_ProcessControl(thiz);
        *hooks.m_pPlayerInFocus = savedPlayerInFocus;
    }
    else
    {
        // Local player
        s_CPed_ProcessControl(thiz);
    }
}

//=============================================================================
// Hook installation
//=============================================================================

inline bool CPadHooks::InstallHooks()
{
#if defined(__aarch64__)
    using namespace MTA::Android::Hooks;

    // Prologue size for ARM64 functions - typically 4 instructions (16 bytes)
    // This needs to be at least the size of our hook jump
    constexpr size_t PROLOG_SIZE = 16;

    uintptr_t trampoline = 0;

    // Hook GetPedWalkLeftRight
    if (!ARMHookInstallWithOriginal(
            static_cast<uint32_t>(ARM64::CPAD_GETPEDWALKLEFTRIGHT),
            reinterpret_cast<uintptr_t>(Hook_GetPedWalkLeftRight),
            &trampoline, PROLOG_SIZE))
    {
        PAD_LOGE("Failed to hook GetPedWalkLeftRight");
        return false;
    }
    s_CPad_GetPedWalkLeftRight = reinterpret_cast<uint16_t(*)(uintptr_t)>(trampoline);
    PAD_LOGI("Hooked GetPedWalkLeftRight at 0x%lx", (unsigned long)(m_gameBase + ARM64::CPAD_GETPEDWALKLEFTRIGHT));

    // Hook GetPedWalkUpDown
    if (!ARMHookInstallWithOriginal(
            static_cast<uint32_t>(ARM64::CPAD_GETPEDWALKUPDOWN),
            reinterpret_cast<uintptr_t>(Hook_GetPedWalkUpDown),
            &trampoline, PROLOG_SIZE))
    {
        PAD_LOGE("Failed to hook GetPedWalkUpDown");
        return false;
    }
    s_CPad_GetPedWalkUpDown = reinterpret_cast<uint16_t(*)(uintptr_t)>(trampoline);
    PAD_LOGI("Hooked GetPedWalkUpDown at 0x%lx", (unsigned long)(m_gameBase + ARM64::CPAD_GETPEDWALKUPDOWN));

    // Hook GetSprint
    if (!ARMHookInstallWithOriginal(
            static_cast<uint32_t>(ARM64::CPAD_GETSPRINT),
            reinterpret_cast<uintptr_t>(Hook_GetSprint),
            &trampoline, PROLOG_SIZE))
    {
        PAD_LOGE("Failed to hook GetSprint");
        return false;
    }
    s_CPad_GetSprint = reinterpret_cast<uint32_t(*)(uintptr_t, uint32_t)>(trampoline);
    PAD_LOGI("Hooked GetSprint at 0x%lx", (unsigned long)(m_gameBase + ARM64::CPAD_GETSPRINT));

    // Hook JumpJustDown
    if (!ARMHookInstallWithOriginal(
            static_cast<uint32_t>(ARM64::CPAD_JUMPJUSTDOWN),
            reinterpret_cast<uintptr_t>(Hook_JumpJustDown),
            &trampoline, PROLOG_SIZE))
    {
        PAD_LOGE("Failed to hook JumpJustDown");
        return false;
    }
    s_CPad_JumpJustDown = reinterpret_cast<uint32_t(*)(uintptr_t)>(trampoline);
    PAD_LOGI("Hooked JumpJustDown at 0x%lx", (unsigned long)(m_gameBase + ARM64::CPAD_JUMPJUSTDOWN));

    // Hook GetJump
    if (!ARMHookInstallWithOriginal(
            static_cast<uint32_t>(ARM64::CPAD_GETJUMP),
            reinterpret_cast<uintptr_t>(Hook_GetJump),
            &trampoline, PROLOG_SIZE))
    {
        PAD_LOGE("Failed to hook GetJump");
        return false;
    }
    s_CPad_GetJump = reinterpret_cast<uint32_t(*)(uintptr_t)>(trampoline);
    PAD_LOGI("Hooked GetJump at 0x%lx", (unsigned long)(m_gameBase + ARM64::CPAD_GETJUMP));

    PAD_LOGI("All CPad hooks installed successfully");
    return true;
#else
    PAD_LOGE("CPadHooks only supports ARM64");
    return false;
#endif
}

} // namespace MTA::Android::Multiplayer
