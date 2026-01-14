# MTA:SA Android Port - Project Summary

> Document created: January 9, 2026
> Last updated: January 13, 2026 (Session 22 - PC puresync/bitstream port regression)
> Status: **Phase 7f IN PROGRESS - Regression: remote players not visible after puresync/bitstream changes.**

**Related Documentation:**
- [Progress Log](MTA-ANDROID-PROGRESS-LOG.md) - Historical session logs and daily progress
- [Completed Phases](MTA-ANDROID-COMPLETED-PHASES.md) - Detailed reference for completed phases

---

## 1. Executive Summary

This document summarizes the progress on porting MTA:SA (Multi Theft Auto: San Andreas) to Android.

| Target | Engine | Feasibility | Status |
|--------|--------|-------------|--------|
| GTA SA Definitive Edition | Unreal Engine 4 | Not feasible (95%+ rewrite) | Rejected |
| **GTA SA Android** | **RenderWare** | **Feasible (40-60% rewrite)** | **In Progress** |

**Current Status**: Phases 1-7e complete, Phase 7f - **Regression: remote players no longer visible after PC puresync/bitstream changes; sync positions read as (0,0,0).**

```
Build Status:    APK builds successfully (ARM64 + ARM32)
Test Results:    44 total, 42 passed, 0 failed, 2 skipped
APK Injection:   GTA:SA v2.10 APK with MTA library injected
Game Launch:     GTA:SA runs with OBB files (full game assets)
MTA Library:     libmta_android.so loads via smali patch
Auto-Connect:    MTA connects to server automatically on game launch
Server Module:   net_android.so deployed as net.so on VPS
Full Protocol:   Handshake -> MOD_NAME -> JOINDATA received by server
Server:          37.59.101.35:22004 with net_android.so (as net.so)
Two-Way Sync:    Both clients sending AND receiving PURESYNC packets
Auto-Create:     Players auto-created on first sync reception
Player Count:    "Remote players: 1" shown on both devices!
REMOTE PLAYER:   Regression after PC puresync port; remote positions decode as (0,0,0); players not visible.

=== SESSION 22 (January 13, 2026) ===
PURESYNC REGRESSION AFTER BITSTREAM PORT

  CURRENT ISSUE:
    - Remote players not visible; sync positions decode as (0,0,0)
    - Player manager auto-creates/removes remote players due to invalid positions
    - PURESYNC packets arrive but parsing is still misaligned

  CHANGES MADE (LATEST):
    - Ported PC puresync parsing (keysync, flags, compressed position/rotation/velocity)
    - Added ElementID 17-bit parsing for player/vehicle packets
    - Updated NetBitStream norm vector/quaternion to float form
    - Updated NetBitStream compressed int format to match server net-android

  NEXT STEPS:
    1. Verify remaining bitstream mismatches vs net-android (compressed types + bit order).
    2. Add bit-offset logging during puresync parsing to locate misalignment.
    3. Validate server payload size and fields (latency, keysync, cam orientation) against client reads.

  CURRENT ISSUE:
    - Remote player position updates are correct
    - Animation/heading are incorrect (mirrors local movement or wrong facing)
    - Walk/run state mismatches (remote walking shows as running, wrong rotation)

  TEMPORARY WORKAROUND:
    - Derived input fallback synthesizes controller input from position delta
    - Helps trigger animations but produces incorrect heading and state

  ROOT CAUSE (LIKELY):
    - Android PURESYNC parsing does not read full keysync
    - Camera rotation and key flags are missing or mis-read
    - Derived input is guessing from movement only, not real input state

  NEXT STEPS (TARGET FIX):
    1. Port PC ReadPlayerPuresync + ReadFullKeysync into Android packet handler
    2. Store controller state and camera rotation in RemoteSyncData
    3. Drive CPadHooks from keysync (remove derived input hack)

=== SESSION 20 (January 11, 2026) ===
CPed::Teleport FIX - Remote player position now stable!

  ISSUE SOLVED:
    - Remote players were "warping" every 5 seconds (2572 units)
    - Direct matrix manipulation wasn't sticking - game reset position
    - Position oscillated between correct position and spawn point (Grove Street)

  ROOT CAUSE:
    - WritePedPosition() only updates the matrix, not game's internal state
    - Game's world sector system kept resetting ped to spawn position
    - Some sync packets had invalid (0,0,0) position causing oscillation

  FIX APPLIED:
    1. Use CPed::Teleport (offset 0x59DD90) instead of direct matrix writes
       - Properly updates collision position, world sectors, attached entities
       - Position now persists correctly between frames
    2. Filter out invalid (0,0,0) positions in UpdateSyncData
       - Prevents oscillation from bad sync packets
       - Logs "Ignoring invalid (0,0,0) position in sync data"

  CURRENT STATE:
    - FULLY WORKING: Both devices see each other
    - Position sync is STABLE - no more repeated warping
    - Players move smoothly with interpolation
    - CPed::Teleport used for large position changes (>50 units)
    - WritePedPosition used for small interpolation updates

  FILES MODIFIED:
    - CRemotePlayer.h: Added TeleportPed() method calling CPed::Teleport
    - CRemotePlayer.h: UpdateSyncData() now uses TeleportPed() for warps
    - CRemotePlayer.h: SetPosition() now uses TeleportPed()
    - CRemotePlayer.h: Added (0,0,0) position filter

  ARM64 ADDRESSES USED:
    - CPed::Teleport: 0x59DD90 (void CPed::Teleport(CVector, bool))

=== SESSION 19 (January 11, 2026) ===
DEAD POSE ISSUE FIXED! REMOTE PLAYERS NOW RENDER CORRECTLY!

  *** MAJOR BREAKTHROUGH ***
    - BOTH devices now see each other as STANDING PLAYERS!
    - No more dead body pose!
    - No more green triangle glitch!
    - Players move and sync correctly!
    - THIS IS A HUGE MILESTONE - BASIC MULTIPLAYER IS WORKING!

  ROOT CAUSE (Dead Pose):
    - SA-MP sets specific ped flags after ped creation that we weren't setting
    - m_pIntelligence was NULL (no task system) - but SA-MP has this too
    - The key was setting the CORRECT ped flags at the CORRECT offsets

  FIX APPLIED:
    - Used SA-MP's CPedGTA.h structure to find exact flag offsets
    - m_nPedFlags is at offset 0x5E0 in CPedGTA (ARM64)
    - Set bNeverEverTargetThisPed: offset 0x5E6, bit 4 (0x10)
    - Set bDoesntDropWeaponsWhenDead: offset 0x5E9, bit 1 (0x02)
    - These flags prevent the ped from appearing dead/ragdolled

  CURRENT STATE:
    Device 1 (first joiner):
      - Sees Device 2: STANDING CORRECTLY!
      - Movement syncs in real-time
      - Model renders perfectly

    Device 2 (second joiner):
      - Sees Device 1: STANDING CORRECTLY!
      - Movement syncs in real-time
      - Model renders perfectly

  FILES MODIFIED:
    - CRemotePlayer.h: Updated SetInvulnerable() with SA-MP's exact flag offsets
    - CRemotePlayer.h: SetInvulnerable() now sets bNeverEverTargetThisPed + bDoesntDropWeaponsWhenDead
    - Called SetInvulnerable(true) immediately after ped creation

  WHAT WE LEARNED FROM SA-MP:
    1. SA-MP's CPedGTA.h has complete ARM64 structure with all offsets
    2. SA-MP also has NULL m_pIntelligence for remote peds (commented out)
    3. SA-MP sets specific ped flags to prevent dead appearance
    4. SA-MP hooks CPad functions for remote player input (future work)
    5. The spawn sequence (SetupPlayerPed → DeactivatePlayerPed → ClearSpace → ReactivatePlayerPed → CWorld::Add) is correct

=== SESSION 18 CONTINUED (January 11, 2026) ===
GREEN TRIANGLE ISSUE FIXED! Spawn delay solution worked!

  BREAKTHROUGH:
    - Device 2 now sees Device 1's BODY (not green triangle!)
    - Movement triggers visibility - once Device 1 moves, they appear correctly
    - This was fixed by adding spawn delay after local player initialization

=== SESSION 18 LATE NIGHT (January 10-11, 2026) ===
DEEP INVESTIGATION: Green triangle vs dead body asymmetry

  PREVIOUS STATE (BEFORE FIX):
    - Device 1 sees Device 2: Dead body on ground (MODEL VISIBLE, wrong pose)
    - Device 2 sees Device 1: GREEN TRIANGLE only (no model rendered)
    - Movement sync works on BOTH devices
    - Position updates work (green triangle moves correctly)

  WHAT WE VERIFIED:
    1. RpClump (3D model) IS CREATED: 0xb4000072f34af7b0 ✓
    2. m_bIsVisible flag IS SET: Was already true (0x80 in flags) ✓
    3. CWorld::Add called ✓
    4. ReactivatePlayerPed called ✓
    5. CPed::SetModelIndex(0) called ✓
    6. Health=100, State=IDLE set ✓

  THINGS TRIED BEFORE THE FIX:
    1. Removed CStreaming calls (fixed crash, not visibility)
    2. Added CPed::SetModelIndex call
    3. Added CEntity::CreateRwObject fallback
    4. Set m_bIsVisible, m_bStreamingDontDelete flags
    5. Cleared m_bDontStream, m_bRemoveFromWorld flags

  THE FIX THAT WORKED:
    - Use CGameBypass::IsLocalPlayerSpawned() instead of FindPlayerPed(0)
    - Add 3-second delay after local player spawns before creating remote peds
    - This ensures game/RenderWare is fully initialized

=== SESSION 18 EARLIER (January 10, 2026) ===
DEVICE 2 CRASH FIXED - BOTH DEVICES SEE EACH OTHER!

  MAJOR PROGRESS:
    - Device 2 crash at 0x18f is FIXED (DisableGameRestart)
    - Both devices create peds successfully
    - Position/movement sync works on BOTH devices
    - Each device can see the other player moving in real-time

  CURRENT VISUAL STATE:
    - Device 1: Sees Device 2 as dead body + blood (collision works)
    - Device 2: Sees Device 1 as green triangle glitch + blood (model issue)
    - Both see each other's MOVEMENT in real-time

  FIXES APPLIED THIS SESSION:
    1. ARM64 pending marker bug - high bits conflict with real pointers
       - Added m_pendingSlot member to track pending state separately
       - Check slot range (2-1024) to distinguish from ARM64 pointers
    2. DisableGameRestart - prevents CWorld::Players patch from being overwritten
       - Writes RET instruction at CGame::InitialiseWhenRestarting
    3. Ped state maintenance - continuously set health/state for 5 seconds
       - SetPedHealth(100), SetPedArmor(0), SetPedState(IDLE)
       - Hasn't fully solved dead appearance issue yet
    4. Removed CStreaming calls - they crash (use script system, not thread-safe)

  REMAINING ISSUES:
    1. CRASH when player 2 joins - both devices crash
       - Happens after ped creation completes
       - Previously saw players standing for 1 second before crash
    2. Dead ped state - ped appears dead/ragdoll despite health=100, state=IDLE
       - Game may be overwriting state faster than we can set it
       - May need task system integration (CTaskSimpleStandStill)
    3. Model not loading on Device 2 - only green triangle glitch visible
       - Ped has collision (blood appears when hit)
       - Model 0 (CJ) may need explicit streaming/loading
       - Device 1 sees full model, Device 2 doesn't - timing issue?

  KEY INSIGHT:
    One test showed player appearing correctly after ~2 minutes wait.
    This suggests game initialization eventually stabilizes the ped.
    Need to find what game does to make ped fully visible.

  FILES MODIFIED:
    - CRemotePlayer.h: Added m_pendingSlot, SetPedState, state maintenance
    - CPedFactory.h: Added Step 9 (health/armor/state setting in CreatePedInSlot)
    - CWorldPlayers.h: Added DisableGameRestart function

=== SESSION 17 (January 10, 2026) ===
ROOT CAUSE FOUND AND FIXED - DEVICE 1 SEES REMOTE PLAYER!

  BREAKTHROUGH: Remote player from Device 2 is VISIBLE on Device 1!

  ROOT CAUSE (CWorld::Add crash):
    - Game's CWorld::Players array only supports 2 players (local + 1)
    - SetupPlayerPed(slot>=2) crashes because it accesses out-of-bounds memory
    - Even direct allocation approach crashes because CWorld::Add calls
      virtual functions that expect properly initialized RenderWare objects

  SOLUTION (from SA-MP Android - patches.cpp):
    1. Create custom CWorld::Players array with 1004 entries
    2. Patch game's pointer at g_libGTASA + 0x84E7A8 (ARM64)
    3. Now SetupPlayerPed works for any slot!

  NEW FILES:
    - Client/android/multiplayer/CWorldPlayers.h
      - CPlayerInfoGta structure (0x1D8 bytes ARM64)
      - CPlayerPedData structure (0xD8 bytes ARM64)
      - CWorldPlayers class - applies the patch

  UPDATED FILES:
    - Client/android/multiplayer/CPedFactory.h
      - Integrates with CWorldPlayers
      - Uses slot-based approach when patch is applied
      - Thread safety: defers ped creation to game thread
    - Client/android/multiplayer/CRemotePlayer.h
      - Pending marker pattern (0x80000000 | slot) for async ped creation
      - IsPedPending(), TryResolvePendingPed() methods

  ADDITIONAL FIXES:
    - Thread safety: Game functions called from game thread only
    - Pending marker: Avoid using slot number as fake pointer
    - PlayerInFocus: Removed incorrect patch (it's an int, not pointer)

  CURRENT STATE:
    - Device 1: Works! Can see remote player from Device 2!
    - Device 2: Crashes at fault addr 0x18f (~150ms after ped creation)
    - Suspected: Race condition or timing issue

  NEXT: Debug Device 2 crash (0x18f)

=== SESSION 16 (January 10, 2026) ===
Debugging CWorld::Add crash - happens when 2nd device joins

  SYMPTOMS:
    - Crash during CWorld::Add call
    - SIGSEGV at fault addr 0xab4e6ee8 (bad vtable pointer?)
    - Crash happens even with ClearSpaceForMissionEntity disabled
    - Ped pointer looks valid: 0xb400007375d93d98
    - Ped bytes: 0xf8 0x30 0x67 0x81

=== SESSION 15 BREAKTHROUGH (January 10, 2026) ===
REMOTE PLAYER IS NOW VISIBLE!!!

  MAJOR MILESTONE: Can see other player in game world!
  REMAINING ISSUE: Player spawns in dead/ragdoll state

  MTA PC Direct Allocation Approach - WORKING!

  Implementation that works:
    1. CPed::operator new(0x998) - allocate memory
    2. CPlayerPed::CPlayerPed(1, false) - construct ped
    3. Create CPlayerPedData manually, copy from local player
    4. Set m_nModelIndex directly (offset 0x24)
    5. ClearSpaceForMissionEntity
    6. CWorld::Add
    7. Set matrix position

  ARM64 Addresses (GTA:SA v2.10):
    CPed::operator new:         0x59576C
    CPlayerPed::CPlayerPed:     0x5C0BAC
    CPlayerPedData::ctor:       0x4F10CC
    CWorld::Add:                0x507518

  ARM64 Structure Sizes:
    CPlayerPedGta:              0x998 (2456 bytes)
    CPlayerPedData:             0xD8 (216 bytes)
    pPlayerData offset:         0x5A0
    m_nModelIndex offset:       0x24

  WHAT WORKS:
    - Remote player ped VISIBLE in game!
    - Ped has collision
    - Position sync working
    - Model (CJ) renders correctly

  REMAINING ISSUE:
    - Ped spawns in dead/ragdoll state
    - Need to set health, ped state, or give idle task

=== SESSION 14 (COMPLETE) ===
  SetupPlayerPed crashes in CPlayerPedData::AllocateData()
  Root cause: Game's player info array not initialized for slots 2+
  Solution: Use MTA PC's direct allocation approach (Session 15)
```

---

## 2. Phase Completion Status

| Phase | Description | Status | Details |
|-------|-------------|--------|---------|
| **Phase 1** | Foundation | Complete | ARM hook framework, signature scanner |
| **Phase 2** | Hook Migration | Complete | 200+ addresses mapped, multiplayer hooks ported |
| **Phase 3** | Graphics | Complete | OpenGL ES 3.0 backend, 17 shaders |
| **Phase 4** | Platform | Complete | Input, filesystem, network, JNI bridge |
| **Phase 5** | Integration | Complete | Build system, test harness, APK generation |
| **Phase 6** | GTA:SA Integration | **COMPLETE** | APK injection working, Toast displayed, game runs |
| **Phase 7** | Multiplayer Network | Complete | Network foundation, RakNet 4 handshake, test server |
| **Phase 7b** | MTA Protocol RE | **COMPLETE** | Ghidra RE of net.dll, MTA RakNet 3.x implemented |
| **Phase 7c** | Custom Server Module | **COMPLETE** | net_android.so integrated with MTA server |
| **Phase 7d** | Position Sync | **COMPLETE** | CPlayerSync ready, server timeout fixed, local sync working |
| **Phase 7e** | Multi-Client Sync | **COMPLETE** | CRemotePlayer, CPlayerManager, TWO clients connected! |
| **Phase 7f** | Remote Player Render | **IN PROGRESS** | Players visible; position sync stable; animation/heading incorrect |

---

## 3. Test Results

Validated on Genymotion emulator (ARM64):

| Subsystem | Tests | Status | Notes |
|-----------|-------|--------|-------|
| Platform | 4 | Pass | ARM64 detected, page size, CPU info |
| Input | 5 | Pass | Touch, multi-touch, virtual controls |
| FileSystem | 4 | 2 Skip | Needs full JNI asset manager setup |
| Network | 3 | Pass | Sockets, DNS resolution (92ms) |
| Hooks | 4 | Pass | RWX memory works, pattern matching |
| Scanner | 3 | Pass | Library enumeration, libc, patterns |
| Graphics | 2 | Pass | GLES available, EGL available |
| Profiler | 4 | Pass | Scoped timing, categories |
| Memory | 2 | Pass | Allocation, alignment |
| **NetBitStream** | 5 | Pass | BasicTypes, Bits, Compressed, Vectors, String |
| **SyncStructures** | 3 | Pass | Position, Health, PlayerFlags |
| **CNetAndroid** | 2 | Pass | Initialize, BitStreamAlloc |
| **ServerConnection** | 4 | **Verified** | Initialize, DNS, MD5, StateTransitions, VPS server test |

---

## 4. Build & Deploy

**See [Client/android/README.md](Client/android/README.md) for complete build, inject, and deploy instructions.**

Quick reference:
- Build: CMake or Gradle methods
- APK injection: apktool decompile → copy lib → patch smali → rebuild → sign
- OBB files: Required for game assets (~2.4GB)
- Genymotion devices: `127.0.0.1:6555` and `127.0.0.1:6562`

---

## 5. MTA PC Ped Creation Approach (Session 15)

### The Breakthrough
MTA PC does NOT use `SetupPlayerPed(slot)` for remote players. Instead, it uses direct memory allocation:

### MTA PC Code Reference
See `Client/game_sa/CPlayerPedSA.cpp` lines 32-100:

```cpp
// 1. Allocate memory for ped
DWORD dwPedPointer;
__asm {
    push    SIZEOF_CPLAYERPED           // 0x7A4 on PC, 0x998 on ARM64
    call    FUNC_CPlayerPedOperatorNew  // 0x5E4720 on PC, 0x59576C on ARM64
    mov     dwPedPointer, eax
}

// 2. Call CPlayerPed constructor
__asm {
    mov     ecx, dwPedPointer
    push    0                           // false - behave like AI peds
    push    1                           // NOT a slot number! Just internal flag
    call    FUNC_CPlayerPedConstructor  // 0x60D5B0 on PC, 0x5C0BAC on ARM64
}

// 3. Create player data manually
CPlayerPedDataSAInterface* pData = new CPlayerPedDataSAInterface();
((CPlayerPedSAInterface*)dwPedPointer)->pPlayerData = pData;

// 4. Add to world
CWorld::Add(dwPedPointer);
```

### ARM64 Implementation (Current)
```cpp
// ARM64 function pointers
typedef void* (*CPedOperatorNew_t)(size_t size);
typedef void* (*CPlayerPedCtor_t)(void* thisPtr, int arg1, bool arg2);

CPedOperatorNew_t CPedOperatorNew = (CPedOperatorNew_t)(gameBase + 0x59576C);
CPlayerPedCtor_t CPlayerPedCtor = (CPlayerPedCtor_t)(gameBase + 0x5C0BAC);

// Create ped
void* pedMem = CPedOperatorNew(0x998);  // ARM64 CPlayerPed size
CPlayerPedCtor(pedMem, 1, false);        // Construct

// Set player data at offset 0x5A0
void* pPlayerData = calloc(1, 0xD8);     // ARM64 CPlayerPedData size
*(void**)((char*)pedMem + 0x5A0) = pPlayerData;

// CWorld::Add - ped appears in world with collision
CWorld::Add(pedMem);

// PROBLEM: SetModelIndex crashes from network thread!
// SetModelIndex(pedMem, 0);  // Crashes in CRunningScript::CollectNextParameterWithoutIncreasingPC
```

### Known Issue: Threading
SetModelIndex must be called from the main game thread. Currently called from network thread:
- **Symptom**: Ped has collision (blood visible) but no model rendered
- **Cause**: SetModelIndex crashes when called from wrong thread
- **Solution**: Need to queue ped operations for game thread (see SA-MP's approach)

### Key ARM64 Addresses

| Function | ARM64 Offset | Mangled Name | Purpose |
|----------|-------------|--------------|---------|
| `CPed::operator new(size_t)` | **0x59576C** | `_ZN4CPednwEm` | Allocate memory for ped |
| `CPlayerPed::CPlayerPed(int, bool)` | **0x5C0BAC** | `_ZN10CPlayerPedC1Eib` | Construct the ped |
| `CPlayerPedData::CPlayerPedData()` | **0x4F10CC** | `_ZN14CPlayerPedDataC1Ev` | Create player data |
| `CWorld::Add(CEntity*)` | 0x507518 | `_ZN6CWorld3AddEP7CEntityb` | Add to game world |

### Structure Sizes (ARM64 from SA-MP)

| Structure | Size | Notes |
|-----------|------|-------|
| `CPlayerPedGta` | **0x998** (2456 bytes) | VALIDATE_SIZE in SA-MP |
| `CPlayerPedData` | **0xD8** (216 bytes) | VALIDATE_SIZE in SA-MP |
| `pPlayerData` offset | **0x5A0** | Offset in CPlayerPed |

---

## 6. Reverse Engineering Resources

### Available Resources

| Resource | Location | Description |
|----------|----------|-------------|
| **SA-MP ARM64 Symbol Dump** | `Client/android/reference/samp-android-reference/dumps_libGTASA_32and64/DUMP 2.1 (64).txt` | ~20,000+ symbols with ARM64 addresses |
| **SA-MP Android Reference** | `Client/android/reference/samp-android-reference/` | Full SA-MP Android source - working multiplayer |
| **MTA PC Source (game_sa)** | `Client/game_sa/` | MTA's PC game layer - ped creation approach |
| **GTA-Reversed (PC)** | `/Users/salimtrouve/Documents/GitHub/mta-misc/gta-reversed` | 90%+ reversed GTA:SA PC |

### How to Search ARM64 Addresses

```bash
# Search for a function
grep "CPlayerPed" "Client/android/reference/samp-android-reference/dumps_libGTASA_32and64/DUMP 2.1 (64).txt"

# Format: index  paddr  vaddr  bind  type  size  lib  name  demangled
```

---

## 7. Next Steps

### Phase 7f Status: Remote Players Visible, Animation Pending
Players are visible and position sync is stable, but animation/heading is incorrect.

### Completed Investigation Steps
- [x] Remote player ped is VISIBLE on Device 1
- [x] CWorld::Players patch working
- [x] Device 2 crash at 0x18f FIXED (DisableGameRestart)
- [x] Both devices see each other's movement
- [x] Set ped health to 100.0f
- [x] Set ped state to IDLE (1)
- [x] GREEN TRIANGLE FIXED (spawn delay solution)
- [x] Device 2 sees Device 1's body correctly
- [x] DEAD PED POSE FIXED (SA-MP ped flags)
- [x] Both devices see standing players; position sync stable
- [ ] Remote animation/heading correct (pending PC keysync port)

### Next: Phase 7g - Enhanced Multiplayer Features
Now that basic remote player rendering works, next priorities:

**Priority 1: CPad Hooks for Remote Player Input (In Progress)**

SA-MP hooks CPad functions to provide input for remote players.
This enables walking/running/jumping animations for remote players.

| Component | Status | Notes |
|-----------|--------|-------|
| CPadHooks.h infrastructure | ✅ Complete | All hooks implemented |
| ARM64 addresses mapped | ✅ Complete | 20+ CPad functions |
| Key storage in RemoteSyncData | ✅ Added | leftStickX/Y, keyFlags |
| Slot management (2-31) | ✅ Working | For PlayerInFocus routing |
| Hook installation | ✅ Enabled | Dead pose regression resolved |
| Key extraction from packets | ❌ Missing | Needs PC ReadFullKeysync port |
| ProcessControl hook | ⚠️ In progress | Needs real keysync data for correct anim |

**Files:**
- `Client/android/multiplayer/CPadHooks.h` - Full implementation (disabled)
- `Client/android/multiplayer/CPlayerManager.h` - Integration (commented out)

**ARM64 Addresses (CPadHooks.h):**
```cpp
CWORLD_PLAYERINFOCUS     = 0xBDCAE8  // Which player is being processed
CPAD_GETPEDWALKLEFTRIGHT = 0x4DCD40
CPAD_GETPEDWALKUPDOWN    = 0x4DCDD4
CPAD_GETSPRINT           = 0x4DF100
CPAD_GETJUMP             = 0x4DEF14
```

**To Enable:**
1. Debug why CPadHooks caused dead pose regression
2. Fix hook installation timing/order
3. Implement FindPlayerNumFromPedPtr for ProcessControl
4. Extract proper key data from PURESYNC packets

**Priority 2: Weapon Sync**
- Sync current weapon between players
- Show correct weapon model on remote players
- Weapon fire sync

**Priority 3: Animation Sync**
- Sync animation states (running, jumping, etc.)
- Apply animations to remote players based on sync data

**Priority 4: Vehicle Sync**
- Remote players entering/exiting vehicles
- Vehicle position sync
- Driver/passenger sync

### Later: Phase 8 - Android UI
- Server browser
- Chat interface
- HUD elements
- Settings menu

---

## 8. Resources

### Local Repositories

| Repository | Path | Description |
|------------|------|-------------|
| MTA:SA Blue | `/Users/salimtrouve/Documents/GitHub/mtasa-blue` | Main codebase |
| MTA Android | `Client/android/` | Android port |
| SA-MP Reference | `Client/android/reference/samp-android-reference/` | ARM addresses |
| GTA-Reversed | `/Users/salimtrouve/Documents/GitHub/mta-misc/gta-reversed` | 90% reversed GTA:SA |

### Development Environment

See [Client/android/README.md](Client/android/README.md#tested-configurations) for SDK/NDK/Java versions.

### VPS Server

| Property | Value |
|----------|-------|
| IP Address | `37.59.101.35` |
| SSH Alias | `dev` |
| MTA Port | 22004 |

```bash
# Connect
ssh dev

# View logs
tail -f /tmp/mta-server.log

# Restart server
cd /home/ubuntu/mtasa/dev && pkill -f mta-server64 && nohup ./mta-server64 > /tmp/mta-server.log 2>&1 &
```

---

*Document updated: January 13, 2026 (Session 22 - Puresync regression after bitstream port)*
*For historical progress, see [MTA-ANDROID-PROGRESS-LOG.md](MTA-ANDROID-PROGRESS-LOG.md)*
*For completed phase details, see [MTA-ANDROID-COMPLETED-PHASES.md](MTA-ANDROID-COMPLETED-PHASES.md)*
*For build, inject, and deploy instructions, see [Client/android/README.md](Client/android/README.md)*
