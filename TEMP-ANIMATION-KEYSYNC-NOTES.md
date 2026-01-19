# Phase 7h: Remote Animation + Keysync Pipeline

> Temporary implementation notes - delete after implementation complete
> Created: January 19, 2026
> Last Updated: January 19, 2026

---

## Implementation Status

| Priority | Task | Status |
|----------|------|--------|
| 1 | Capture real local controller state | **DONE** |
| 1 | Add GetLocalControllerState() to CPadHooks | **DONE** |
| 1 | Replace BuildControllerFromVelocity() in SendPlayerSync | **DONE** |
| 1 | Fix JumpJustDown edge-trigger | **DONE** |
| 2 | Wire SetRemotePlayerKeys in CPlayerManager::Process() | **DONE** |
| 2 | Fix walk reduction sign bug | **DONE** |
| 2 | Fix Y axis sign inversion | **DONE** |
| - | Intermittent backward animation | **FIXED** (Y sign inversion) |
| - | Wrong rotation on initial spawn | **INVESTIGATING** |
| 3 | Hook fire/crouch/aim for weapons | NOT STARTED |

---

## Risk Assessment (from code review)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| Data race on m_localPlayerKeys | Low-Medium | Suspect | Sync thread reads while game thread writes. Multi-field snapshot could be inconsistent (old X + new Y). May cause intermittent backward animation. |
| Remote key pollution | N/A | **Verified non-issue** | Guard at CPadHooks.h:305 checks `playerInFocus > 0` for remote path; local capture only in `else` branch at line 319-323. |
| Incomplete input capture | Medium | Deferred | Fire/crouch/aim hooks not installed. Sufficient for walk/run/jump (Priority 1-2). Add for weapons (Priority 3). |
| JumpJustDown edge-trigger | Medium | **FIXED** | Removed bKeys write from JumpJustDown hook; now only GetJump writes to KEY_JUMP. |
| Walk reduction sign bug | High | **FIXED** | Was: `result = 32` (lost sign). Now: `result = (result < 0) ? -32 : 32` (preserves sign). |
| PAD_KEYS vs SControllerState ranges | Low | **Verified** | SyncStructures.h:688-689 does `* 127.0f / 128.0f` scaling |
| Spawn rotation desync | Medium | Investigating | Remote players spawn with wrong rotation initially. Separate from animation issue. |

**Remote key pollution guard (CPadHooks.h:300-325):**
```cpp
inline uint16_t CPadHooks::Hook_GetPedWalkLeftRight(uintptr_t thiz)
{
    uint8_t playerInFocus = *hooks.m_pPlayerInFocus;
    if (playerInFocus > 0 && playerInFocus < MAX_REMOTE_PLAYERS)
    {
        // Remote player path - returns remote keys, NO write to m_localPlayerKeys
        return static_cast<uint16_t>(hooks.m_remotePlayerKeys[playerInFocus].wKeyLR);
    }
    else
    {
        // Local player path - ONLY place where m_localPlayerKeys is written
        uint16_t result = s_CPad_GetPedWalkLeftRight(thiz);
        hooks.m_localPlayerKeys.wKeyLR = static_cast<int16_t>(result);  // <-- capture here
        return result;
    }
}
```

---

## Problem Statement

Remote players currently **slide/idle** instead of animating correctly:
- Position sync works (players teleport to correct positions)
- Remote peds are rendered standing upright (dead pose fixed in Session 19)
- BUT: walk/run/jump/aim/fire animations are not playing

**Root Cause**: We're sending **derived controller state** (synthesized from velocity) instead of **real controller input** from the local player's touch controls.

---

## Current Implementation

### Sending Side (Local Player → Server)

**File**: `Client/android/network/CServerConnection.cpp`

```cpp
// Line 34-68: BuildControllerFromVelocity() - DERIVED INPUT (THE PROBLEM)
// This function guesses stick position from velocity, which is backwards:
// - Animation should come FROM input, not be derived from movement
// - Sprint/walk is guessed based on speed thresholds
// - Direction is guessed from velocity heading vs rotation

// Line 1302: Called in SendPlayerSync()
SControllerState controller = BuildControllerFromVelocity(vx, vy, rotation);
WriteFullKeysync(controller, *bitStream);
```

### Receiving Side (Server → Remote Player)

**File**: `Client/android/network/CPacketHandler.cpp`
- PURESYNC handler reads keysync via `ReadFullKeysync()`
- Parsed controller state stored in `RemoteSyncData`

**File**: `Client/android/multiplayer/CPadHooks.h`
- `SetRemotePlayerKeys()` receives stick X/Y and key flags
- Hooks intercept `CPad::GetPedWalkLeftRight()`, `GetPedWalkUpDown()`, `GetSprint()`, `GetJump()`, etc.
- When `CWorld::PlayerInFocus > 0`, hooks return remote player's keys instead of local

**File**: `Client/android/multiplayer/CPlayerManager.h`
- Should call `SetRemotePlayerPed()` and `SetRemotePlayerKeys()` in `Process()`
- Currently may not be wiring keys correctly

---

## Implementation Plan

### Priority 1: Capture Real Local Controller State

**Goal**: Replace derived input with actual touch input from hooks.

1. **Hook local CPad functions to capture input**
   - `CPadHooks::Hook_GetPedWalkLeftRight()` already captures local keys (line 323)
   - `CPadHooks::Hook_GetPedWalkUpDown()` already captures local keys (line 350)
   - `CPadHooks::Hook_GetSprint()` already captures (line 368)
   - `CPadHooks::Hook_JumpJustDown()` already captures (line 392)
   - **BUT**: These are only stored in `m_localPlayerKeys`, not exposed for SendPlayerSync

2. **Export captured local controller state**
   - Add `GetLocalControllerState()` method to `CPadHooks`
   - Returns current `m_localPlayerKeys` state
   - Convert PAD_KEYS to SControllerState format
   - **For jump: use bKeys[KEY_JUMP] from GetJump (held), NOT JumpJustDown (edge)**
     - Both hooks currently write to same field; GetJump is called more often so it prevails
     - Consider separate tracking if edge-vs-held distinction needed later

3. **Wire into SendPlayerSync**
   - Replace `BuildControllerFromVelocity()` call with `GetLocalControllerState()`
   - Keep velocity-based fallback only if hooks not initialized

4. **Threading consideration**
   - Sync thread calls `SendPlayerSync()` (via CPlayerSync callback)
   - Game thread updates `m_localPlayerKeys` in hooks
   - For MVP: accept slight inconsistency (individual field writes are atomic on ARM64)
   - Optional: add simple spinlock in `GetLocalControllerState()` if issues arise

**Key Files to Modify**:
- `Client/android/multiplayer/CPadHooks.h` - Add getter for local keys
- `Client/android/network/CServerConnection.cpp` - Use real keys instead of derived

### Priority 2: Apply Controller State to Remote Peds

**Goal**: Ensure remote ped animates based on received keysync.

1. **Verify CPed::ProcessControl runs for remote peds**
   - Hook is installed at `ARM64::CPED_PROCESSCONTROL` (0x598730)
   - `Hook_ProcessControl()` sets `PlayerInFocus` for remote peds (line 456-491)
   - **Issue**: Guard at line 468 may be skipping remote peds with missing `m_pIntelligence`

2. **Ensure SetRemotePlayerPed + SetRemotePlayerKeys are called**
   - In `CPlayerManager::Process()`, for each remote player:
     ```cpp
     CPadHooks::GetInstance().SetRemotePlayerPed(slot, pedPtr);
     CPadHooks::GetInstance().SetRemotePlayerKeys(slot, stickX, stickY, keyFlags);
     ```
   - Check if this is being called consistently

3. **Debug logging for hook activity**
   - Add one-time log when remote ped animation triggers
   - Log key values being returned for remote peds

**Key Files to Modify**:
- `Client/android/multiplayer/CPlayerManager.h` - Verify Process() wiring
- `Client/android/multiplayer/CPadHooks.h` - Debug logging

### Priority 3: Port PC Animation/Task Rules

**Goal**: Match PC client's animation state handling for remote players.

**Reference**: `Client/mods/deathmatch/logic/CNetAPI.cpp` lines 800-900

**Current hooks installed** (sufficient for walk/run/jump):
| Hook | Captures | Status |
|------|----------|--------|
| GetPedWalkLeftRight | wKeyLR (stick X) | Done |
| GetPedWalkUpDown | wKeyUD (stick Y) | Done |
| GetSprint | KEY_SPRINT | Done |
| GetJump | KEY_JUMP (held) | Done |
| JumpJustDown | KEY_JUMP (edge) | Done |

**Hooks needed for weapons** (Priority 3):
| Hook | Address | Captures |
|------|---------|----------|
| GetWeapon | 0x4DD9D0 | KEY_FIRE |
| DuckJustDown | 0x4DECB4 | KEY_CROUCH |
| GetEnterTargeting | 0x4DE59C | Aim mode |
| GetBlock | 0x4DE308 | Block/secondary |

1. **Additional flags to handle**:
   - `bIsDucked` → crouch animation
   - `bStealthAiming` → stealth mode
   - `bHasAWeapon` + weapon slot → weapon holding animation
   - `isReloadingWeapon` → reload animation

2. **Aim sync for weapons**:
   - `SWeaponAimSync` contains arm direction
   - Need to apply aim direction to remote ped

3. **Task system integration** (if needed):
   - `CTaskSimpleStandStill` for idle
   - `CTaskSimplePlayerOnFoot` for movement
   - May need to give remote peds tasks directly

---

## Key Data Structures

### PAD_KEYS (SA-MP format, used in CPadHooks)
```cpp
struct PAD_KEYS {
    int16_t wKeyLR;           // Left/Right (-128 to 127)
    int16_t wKeyUD;           // Up/Down (-128 to 127)
    bool bKeys[KEY_SIZE + 4]; // Boolean key states
    bool bIgnoreJump;         // Jump debounce
};
```

### SControllerState (MTA format, used in network)
```cpp
struct SControllerState {
    int16_t LeftStickX;       // -128 to 128
    int16_t LeftStickY;       // -128 to 128
    int16_t LeftShoulder1;    // L1 - Aim
    int16_t RightShoulder1;   // R1 - Fire
    int16_t ButtonSquare;     // Crouch
    int16_t ButtonCross;      // Jump/Sprint (0-255)
    int16_t ButtonCircle;     // Punch/Enter vehicle
    int16_t ButtonTriangle;   // Exit vehicle
    int16_t ShockButtonL;     // Shock
    int16_t m_bPedWalk;       // Walk mode
};
```

### Key Flag Mappings (CPadHooks line 198-205)
```cpp
keys.bKeys[KEY_FIRE]             = (keyFlags & 0x0004) != 0;
keys.bKeys[KEY_JUMP]             = (keyFlags & 0x0020) != 0;
keys.bKeys[KEY_SPRINT]           = (keyFlags & 0x0008) != 0;
keys.bKeys[KEY_CROUCH]           = (keyFlags & 0x0002) != 0;
keys.bKeys[KEY_ACTION]           = (keyFlags & 0x0001) != 0;
keys.bKeys[KEY_WALK]             = (keyFlags & 0x0400) != 0;
keys.bKeys[KEY_HANDBRAKE]        = (keyFlags & 0x0080) != 0;
keys.bKeys[KEY_SECONDARY_ATTACK] = (keyFlags & 0x0010) != 0;
```

---

## ARM64 Addresses Reference

### CPad Functions (GTA:SA v2.10)
| Function | Offset | Purpose |
|----------|--------|---------|
| CPad::GetPedWalkLeftRight | 0x4DCD40 | L-stick X |
| CPad::GetPedWalkUpDown | 0x4DCDD4 | L-stick Y |
| CPad::GetSprint | 0x4DF100 | Sprint button |
| CPad::JumpJustDown | 0x4DEF84 | Jump (just pressed) |
| CPad::GetJump | 0x4DEF14 | Jump (held) |
| CPed::ProcessControl | 0x598730 | Main ped update |

### Global Variables
| Variable | Offset | Purpose |
|----------|--------|---------|
| CWorld::PlayerInFocus | 0xBDCAE8 | Current player slot being processed |

---

## Testing Plan

1. **Verify keysync sending**
   - Add log when local controller state is captured
   - Log keysync bytes in outgoing PURESYNC
   - Compare with PC client keysync format

2. **Verify keysync receiving**
   - Log parsed keysync on remote player sync receive
   - Log when SetRemotePlayerKeys is called
   - Log values returned from hooked CPad functions

3. **Verify animation result**
   - Does remote player walk when moving?
   - Does remote player sprint when running?
   - Does remote player jump?

---

## Files Summary

### To Modify
| File | Changes |
|------|---------|
| `CPadHooks.h` | Add `GetLocalControllerState()`, debug logging |
| `CServerConnection.cpp` | Replace derived input with real controller state |
| `CPlayerManager.h` | Ensure `SetRemotePlayerPed`/`Keys` called in Process() |
| `CPacketHandler.cpp` | Debug logging for keysync parsing |

### Reference Files (PC Client)
| File | Purpose |
|------|---------|
| `Client/mods/deathmatch/logic/CNetAPI.cpp` | PC keysync read/write |
| `Shared/sdk/net/SyncStructures.h` | PC sync structure definitions |

### Reference Files (SA-MP)
| File | Purpose |
|------|---------|
| `samp-android-reference/app/src/main/cpp/samp/game/pad.cpp` | SA-MP pad hooks |
| `samp-android-reference/app/src/main/cpp/samp/game/playerped.cpp` | SA-MP remote ped |

---

## After Animation Works

Next priorities:
1. **Weapon sync** - current weapon slot, ammo, firing events
2. **Vehicle sync** - driver/passenger, entering/exiting, vehicle puresync
3. **Android UI** - server browser, chat, HUD

---

## Open Issues (January 19, 2026)

### 1. Intermittent Backward Animation
- **Symptom**: Remote player sometimes animates backward when walking/running
- **Not position** - ped moves in correct direction, but animation faces wrong way
- **ROOT CAUSE FOUND**: Y axis sign was inverted. Android touch reports positive Y for forward, but PC/MTA expects negative Y for forward.
- **FIX APPLIED**: In `GetLocalControllerState()` (CPadHooks.h:256-257):
  ```cpp
  // Invert Y axis: Android touch reports positive for forward, but PC/MTA expects negative for forward
  state.LeftStickY = static_cast<int16_t>(-std::clamp<int>(m_localPlayerKeys.wKeyUD, -128, 128));
  ```
- **Status**: Fix deployed, awaiting test confirmation

### 2. Wrong Rotation on Initial Spawn
- **Symptom**: Remote player spawns facing wrong direction initially
- **Separate issue** from backward animation
- **Likely cause**: Rotation not set correctly on spawn, or one-frame stale rotation

### Next Investigation Steps
1. Add debug logging for LeftStickY and rotation values
2. Check if data race is causing sign flip (add snapshot mechanism)
3. Investigate spawn rotation sequence

---

*Notes last updated: January 19, 2026*
