# MTA:SA Android Port - Progress Log

> Document created: January 10, 2026
> This file contains historical session logs and daily progress for the MTA:SA Android port.
> For active development status, see [MTA-ANDROID-PROJECT-SUMMARY.md](MTA-ANDROID-PROJECT-SUMMARY.md)
> For completed phase details, see [MTA-ANDROID-COMPLETED-PHASES.md](MTA-ANDROID-COMPLETED-PHASES.md)

---

## Progress Overview

| Date | Phase | Accomplishment |
|------|-------|----------------|
| 2026-01-09 | 1 | ARM hook framework implemented |
| 2026-01-09 | 1 | Signature scanner created |
| 2026-01-09 | 1 | CMake build system configured |
| 2026-01-09 | 1 | **Phase 1 Complete** |
| 2026-01-09 | 2 | SA-MP Android reference discovered |
| 2026-01-09 | 2 | 200+ ARM addresses mapped |
| 2026-01-09 | 2 | ARMHookInstaller.h created |
| 2026-01-09 | 2 | CMultiplayerSA_ARM ported |
| 2026-01-09 | 2 | **Phase 2 Complete** |
| 2026-01-09 | 3 | GLESGraphics.h/cpp - OpenGL ES 3.0 backend |
| 2026-01-09 | 3 | GLESShaders.h - 17 GLSL ES shaders |
| 2026-01-09 | 3 | RenderWareBridge - RW to GLES conversion |
| 2026-01-09 | 3 | **Phase 3 Complete** |
| 2026-01-09 | 4 | AndroidInput - touch/gamepad system |
| 2026-01-09 | 4 | AndroidFileSystem - storage abstraction |
| 2026-01-09 | 4 | AndroidNetwork - TCP/UDP/HTTP |
| 2026-01-09 | 4 | JNI bridge layer |
| 2026-01-09 | 4 | **Phase 4 Complete** |
| 2026-01-09 | 5 | Gradle build system |
| 2026-01-09 | 5 | Java activities (MTAActivity, TestActivity) |
| 2026-01-09 | 5 | CAndroidCore - main controller |
| 2026-01-09 | 5 | CProfiler - performance profiling |
| 2026-01-09 | 5 | Test harness (30 tests) |
| 2026-01-09 | 5 | APK builds successfully |
| 2026-01-09 | 5 | Tests validated on Genymotion (28/30 pass) |
| 2026-01-09 | 5 | **Phase 5 Complete** |
| 2026-01-09 | 6 | GTASAIntegration.h - Game detection module |
| 2026-01-09 | 6 | GTASALoaderActivity.java - Launcher UI |
| 2026-01-09 | 6 | inject-mta.sh - APK injection script |
| 2026-01-09 | 6 | JNI methods for integration API |
| 2026-01-09 | 6 | PHASE6_INTEGRATION.md documentation |
| 2026-01-09 | 6 | Successfully injected MTA into GTA:SA v1.08 APK |
| 2026-01-09 | 6 | Deployed 2.4GB OBB to emulator |
| 2026-01-09 | 6 | Discovered: Genymotion ARM64-only limitation |
| 2026-01-10 | 6 | Tested with GTA:SA v2.10 (ARM64) on Genymotion |
| 2026-01-10 | 6 | MTA library loads successfully into game |
| 2026-01-10 | 6 | Added Toast notification "MTA:SA Android Loaded!" |
| 2026-01-10 | 6 | Game runs without crashing with MTA loaded |
| 2026-01-10 | 6 | **Phase 6 VERIFIED WORKING** - Ready for Phase 7 |
| 2026-01-10 | 7 | CNetAndroid.h/cpp - Network manager with UDP sockets |
| 2026-01-10 | 7 | NetBitStream - Bitstream serialization |
| 2026-01-10 | 7 | CPacketHandler.h/cpp - 100+ packet types, 50+ RPCs |
| 2026-01-10 | 7 | SyncStructures.h - Player/vehicle sync data |
| 2026-01-10 | 7 | CMakeLists.txt updated with network module |
| 2026-01-10 | 7 | **Phase 7 foundation complete** - Network protocol ready |
| 2026-01-10 | 7 | Added 10 network unit tests to SubsystemTests.cpp |
| 2026-01-10 | 7 | All network tests PASS - 40 total, 38 pass, 2 skip |
| 2026-01-10 | 7 | CServerConnection.h/cpp - Server connection state machine |
| 2026-01-10 | 7 | MD5 password hashing implementation |
| 2026-01-10 | 7 | DNS resolution & connectivity testing |
| 2026-01-10 | 7 | JNI interface for connection testing (7 methods) |
| 2026-01-10 | 7 | Added 4 ServerConnection tests to SubsystemTests.cpp |
| 2026-01-10 | 7 | All tests PASS - 44 total, 42 pass, 2 skip |
| 2026-01-10 | 7 | Fixed JNI linkage for connection testing methods |
| 2026-01-10 | 7 | Added "Test Server Connection" button to TestActivity |
| 2026-01-10 | 7 | Started MTA server on VPS (37.59.101.35:22004) |
| 2026-01-10 | 7 | **SERVER CONNECTION VERIFIED** - Android client reached VPS server |
| 2026-01-10 | 7 | Connection state machine verified: DISCONNECTED → CONNECTING → WAIT_MOD_NAME |
| 2026-01-10 | 7b | Started Ghidra reverse engineering of net.dll |
| 2026-01-10 | 7b | Created ExportRakNetFunctions.java script for Ghidra |
| 2026-01-10 | 7b | Exported 134 decompiled functions from net.dll |
| 2026-01-10 | 7b | **KEY DISCOVERY**: MTA uses RakNet 3.x (not RakNet 4) |
| 2026-01-10 | 7b | Found packet IDs in Shared/sdk/net/packetenums.h |
| 2026-01-10 | 7b | Implemented dual-protocol RakNetHandshake (MTA 3.x + RakNet 4) |
| 2026-01-10 | 7b | Updated test server to support both protocols |
| 2026-01-10 | 7b | **MTA RAKNET 3.x VERIFIED** - Full handshake working on test server |
| 2026-01-10 | 7b | Connection flow: 0x09→0x0A→0x04→0x0E→MOD_NAME→JOIN_DATA→CONNECTED |
| 2026-01-10 | 7c | Tested connection to real MTA server (22004) - no response |
| 2026-01-10 | 7c | Identified root cause: net.dll anti-cheat blocks non-Windows clients |
| 2026-01-10 | 7c | Decision: Build custom net_android.so server module (no AC) |
| 2026-01-10 | 7c | Architecture: Dual-port (22003 PC+AC, 22010 Android no AC) |
| 2026-01-10 | 7c | Started implementation of CNetServerAndroid |
| 2026-01-10 | 7c | Created Server/net-android/ directory structure |
| 2026-01-10 | 7c | Implemented CNetServerAndroid.h/cpp (~800 lines) |
| 2026-01-10 | 7c | Implemented CNetBitStreamAndroid.h/cpp (~500 lines) |
| 2026-01-10 | 7c | Created CMakeLists.txt, build.sh, exports |
| 2026-01-10 | 7c | **BUILD SUCCESS** - net_android.so compiles on macOS |
| 2026-01-10 | 7c | Deployed net_android.so to VPS (/tmp/net-android/) |
| 2026-01-10 | 7c | Built standalone_server test program |
| 2026-01-10 | 7c | Fixed packet ID collision (PING vs PLAYER_JOINDATA both 0x01) |
| 2026-01-10 | 7c | Added client state-based packet ID disambiguation |
| 2026-01-10 | 7c | **FULL FLOW VERIFIED** - Python test: Handshake→MOD_NAME→JOINDATA→JOIN_COMPLETE |
| 2026-01-10 | 7c | Server running on 37.59.101.35:22010, ready for Android client test |
| 2026-01-10 | 7c | **ANDROID CLIENT TEST SUCCESSFUL** - Full connection flow verified |
| 2026-01-10 | 7c | Android client: RAKNET_HANDSHAKE → WAIT_MOD_NAME → SEND_JOIN → CONNECTED |
| 2026-01-10 | 7c | Server received PLAYER_JOINDATA (98 bytes), sent JOIN_COMPLETE + JOINED_GAME |
| 2026-01-10 | 7c | Added CheckCompatibility + GetLibMtaVersion exports for MTA server |
| 2026-01-10 | 7c | Fixed CheckCompatibility pointer handling (MTASA_VERSION_TYPE is not a real pointer) |
| 2026-01-10 | 7c | Module loads in MTA server, passes version check (0xAB) |
| 2026-01-10 | 7c | Issue: vtable incompatibility with deathmatch.so (virtual function order mismatch) |
| 2026-01-10 | 7c | Standalone server (port 22010) verified working |
| 2026-01-10 | 7c | **VTABLE FIX**: Removed CBinaryFileInterface methods from CNetServer |
| 2026-01-10 | 7c | **VTABLE FIX**: Added CNetHTTPDownloadManagerStub (prevents null pointer crash) |
| 2026-01-10 | 7c | Added exports to exports.map: CheckCompatibility, GetLibMtaVersion |
| 2026-01-10 | 7c | **MTA SERVER INTEGRATION COMPLETE** - net_android.so loads and runs |
| 2026-01-10 | 7c | Server running: 37.59.101.35:22004 with net_android.so as network module |
| 2026-01-10 | 7c | Added auto-connect to MTAAndroidMain.cpp (connects 3s after game load) |
| 2026-01-10 | 7c | Rebuilt GTA:SA APK with MTA library that auto-connects |
| 2026-01-10 | 7c | Pushed OBB files to Genymotion (main.8 + patch.8 = 2.4GB) |
| 2026-01-10 | 7c | **MILESTONE: GTA:SA running with OBB files + MTA connected!** |
| 2026-01-10 | 7c | Player ID 1 assigned to Android client while game shows singleplayer menu |
| 2026-01-10 | 7d | Starting Phase 7d: Auto-spawn and position sync |
| 2026-01-10 | 7d | Created CPlayerSync.h with player position reading from game memory |
| 2026-01-10 | 7d | CPed structure offsets for ARM32/ARM64 (matrix, health, armor, vehicle) |
| 2026-01-10 | 7d | Implemented StartPlayerSync() in MTAAndroidMain.cpp |
| 2026-01-10 | 7d | Sync thread runs after MTA connection, waits for player to spawn in-game |
| 2026-01-10 | 7d | **Verified**: Sync initializes correctly (game base: 0x7afaac1000) |
| 2026-01-10 | 7d | Investigated server crash: DoPulse not being called by MTA main loop |
| 2026-01-10 | 7d | **FIX**: Moved timeout handling to network thread (CheckClientTimeouts) |
| 2026-01-10 | 7d | Server no longer crashes on client disconnect/timeout |
| 2026-01-10 | 7d | Created CGameBypass for menu bypass and auto-spawn |
| 2026-01-10 | 7d | Game state monitoring verified: 0→8→9 (GS_PLAYING_GAME) |
| 2026-01-10 | 7d | **AUTO-SPAWN VERIFIED**: Triggers at game state 8 without crash |
| 2026-01-10 | 7d | Fixed crash by making RestartPlayerAt() a stub (proper ARM64 offsets needed) |
| 2026-01-10 | 7d | Player sync thread starts after spawn, logs "Player sync started successfully!" |
| 2026-01-10 | 7d | **ISSUE**: net_android.so crashes with segfault on VPS (needs debugging) |
| 2026-01-10 | 7d | Android client connection times out due to server crash |
| 2026-01-10 | 7e | Created CRemotePlayer.h - remote player with position interpolation |
| 2026-01-10 | 7e | Created CPlayerManager.h - manages all remote players (add/remove/update) |
| 2026-01-10 | 7e | Created CPedFactory.h - ped creation infrastructure (stub for game hooks) |
| 2026-01-10 | 7e | Updated CPacketHandler.cpp - PURESYNC parses and updates CPlayerManager |
| 2026-01-10 | 7e | Added SendPlayerSync() to CServerConnection - outgoing PURESYNC |
| 2026-01-10 | 7e | Wired sync callback in MTAAndroidMain.cpp - position → server |
| 2026-01-10 | 7e | Updated TestActivity.java to use local test server (10.0.3.2:22003) |
| 2026-01-10 | 7e | Started Python test server on host machine (port 22003) |
| 2026-01-10 | 7e | **MILESTONE**: Two Genymotion devices connected simultaneously! |
| 2026-01-10 | 7e | Device 1 (127.0.0.1:6555): Full handshake RakNet→JOINDATA→JOIN_COMPLETE |
| 2026-01-10 | 7e | Device 2 (127.0.0.1:6562): Full handshake RakNet→JOINDATA→JOIN_COMPLETE |
| 2026-01-10 | 7e | Both reached WAIT_JOINED_GAME state before disconnecting |
| 2026-01-10 | 7e | **Phase 7e COMPLETE** - Multi-client sync infrastructure verified! |
| 2026-01-10 | 7f | Started Phase 7f - Remote player rendering |
| 2026-01-10 | 7f | Rewrote CPedFactory with SA-MP's player slot approach |
| 2026-01-10 | 7f | Added ARM64 function addresses from nm symbol dump |
| 2026-01-10 | 7f | Implemented CreatePedInSlot with 8-step spawn sequence |
| 2026-01-10 | 7f | Wired PLAYER_SPAWN → CPlayerManager → CRemotePlayer → CPedFactory |
| 2026-01-10 | 7f | Built and deployed APK to 2 Genymotion devices + OBB files |
| 2026-01-10 | 7f | **Issue**: Connection timeouts from Genymotion to VPS (network config) |
| 2026-01-10 | 7f | **NETWORK COMPLETE**: Bidirectional sync fully working, ready for rendering |
| 2026-01-10 | 7f | **BLOCKER**: SetupPlayerPed crashes in CPlayerPedData::AllocateData() |
| 2026-01-10 | 7f | **Session 15**: MTA PC direct allocation approach implemented |
| 2026-01-10 | 7f | **BREAKTHROUGH**: Remote player ped VISIBLE in game world! |
| 2026-01-10 | 7f | Issue: Ped spawns in dead/ragdoll state |
| 2026-01-10 | 7f | **Session 16**: CWorld::Add crash investigation (vtable issues) |
| 2026-01-10 | 7f | **Session 17**: ROOT CAUSE FOUND - CWorld::Players array limitation |
| 2026-01-10 | 7f | Created CWorldPlayers.h - patches game's Players array (SA-MP approach) |
| 2026-01-10 | 7f | Thread safety fixes - defer ped creation to game thread |
| 2026-01-10 | 7f | Pending marker pattern (0x80000000 | slot) for async ped creation |
| 2026-01-10 | 7f | **Device 1**: Works, can see remote player from Device 2! |
| 2026-01-10 | 7f | **Device 2**: Still crashes at 0x18f - investigating race condition |
| 2026-01-11 | 7f | Session 18: GREEN TRIANGLE FIXED - spawn delay solution |
| 2026-01-11 | 7f | Session 18: Both devices see bodies but Device 1 sees dead pose |
| 2026-01-11 | 7f | **Session 19: DEAD POSE FIXED!** Used SA-MP's ped flag offsets |
| 2026-01-11 | 7f | Set bNeverEverTargetThisPed (0x5E6) + bDoesntDropWeaponsWhenDead (0x5E9) |
| 2026-01-11 | 7f | **PHASE 7f COMPLETE** - BOTH devices see each other standing and moving! |
| 2026-01-11 | 7f | **Session 20: CPed::Teleport FIX** - Position sync now stable |
| 2026-01-11 | 7f | Fixed repeated warping (2572 units every 5s) - game was resetting ped position |
| 2026-01-11 | 7f | Use CPed::Teleport (0x59DD90) instead of direct matrix writes |
| 2026-01-11 | 7f | Added filter for invalid (0,0,0) sync positions |
| 2026-01-11 | 7f | Remote players now teleport correctly and stay at their position |
| 2026-01-12 | 7f | Added derived input fallback for remote animations (temporary) |
| 2026-01-12 | 7f | Animation/heading mismatch identified; need PC keysync/heading rules |

---

## Session 8: BitStream Debugging (January 10, 2026)

**Goal:** Debug JOINDATA packet handling

**Analysis Method:** Parallel Opus 4.5 agents analyzed deathmatch.so + bitstream

**Findings:**
- Root Cause #1: Server BitStream missing ReadString/ReadStringCharacters
- BitStream Fix: Added ReadString, ReadStringCharacters, CanReadNumberOfBytes
- Hex Dump: Client logs full JOINDATA packet hex dump
- Debug Logging: Server logs all bitstream read operations

**JOINDATA Packet Verified (from client logs):**
```
0000: de 41 60 01 6b 00 0f 00 31 2e 36 2e 30 2d 39 2e  .A`.k...1.6.0-9.
0010: 32 31 30 30 30 2e 30 00 82 dc c8 e4 de d2 c8 a0  21000.0.........
Netcode: 0x41DE, MTA: 0x0160, Bitstream: 0x006B
Version String: "1.6.0-9.21000.0" (15 chars)
```

---

## Session 9: ABI Bypass Fix (January 10, 2026)

**ISSUE #1 - Serial Format (FIXED):**
- Serial format "ANDROID0000..." contained invalid hex chars (N,D,R,I)
- Fixed: Changed to "A1D01D00..." (valid hex only: A-F, 0-9)
- Server validation at CGame.cpp:1840 requires: ^[A-F0-9]{32}$

**ISSUE #2 - Server Crash (FIXED):**
- Server crashed when calling deathmatch.so handler for JOINDATA
- GDB Stack Trace showed bad pointer in pthread_mutex_lock
- ROOT CAUSE: ABI/Vtable Mismatch
  - Mutex address 0xf006b016041de contains "006b 0160 41de" = our packet versions!
  - deathmatch.so expects CRefCountable with CCriticalSection* m_pCS member
  - Our CRefCountableSimple doesn't have that member

**FIX:** Bypass deathmatch.so entirely for JOINDATA
- Modified ProcessIncomingPacket() to NOT queue to deathmatch.so
- Instead, call SendJoinComplete() directly ourselves
- Server no longer crashes!

**RESULT:**
- Server sends JOIN_COMPLETE without crashing
- Full handshake: RakNet → MOD_NAME → JOINDATA → JOIN_COMPLETE

---

## Session 9b: Two Android Test (January 10, 2026)

**GOAL:** Two Android devices see each other (for demo video)

**IMPLEMENTED:**

1. **SYNC BYPASS** - Relay PURESYNC packets between clients directly
   - PLAYER_PURESYNC (0x20), PLAYER_VEHICLE_PURESYNC, PLAYER_KEYSYNC, LIGHTSYNC
   - When client A sends sync, broadcast to all OTHER clients
   - No deathmatch.so involvement → no crash

2. **PLAYER NOTIFICATIONS** - When client joins:
   - NotifyPlayerJoined() → tells other clients about new player
   - SendExistingPlayersTo() → tells new client about existing players

---

## Session 10: Spawn Packet Investigation (January 10, 2026)

**GOAL:** Get two Android clients to see each other (sync packets flowing)

**FIXES APPLIED:**

1. **DEADLOCK FIX:**
   - Original bypass code: lock mutex → call GetClient() → GetClient locks SAME mutex = DEADLOCK
   - Fix: Look up client directly in m_clients map instead of calling GetClient()

2. **PLAYER_SPAWN PACKET:**
   - Clients were timing out because they never received spawn packet
   - Added SendPlayerSpawn() function
   - Fixed format for Android client:
     * uint32_t playerId (4 bytes)
     * float x, y, z, rotation (4 bytes each)
     * uint16_t skinId (2 bytes)
   - Spawn position: Grove Street (2488.6, -1666.9, 13.5)

**CURRENT STATE:**
- Both Android devices connect successfully
- JOIN_COMPLETE sent (no crash)
- JOINED_GAME sent (player ID: 1)
- PLAYER_SPAWN sent (correct format for Android)

**ISSUE:** Clients timeout after ~3 seconds, no sync packets received

---

## Session 10b: Root Cause Found & Fix Applied (January 10, 2026)

**PARALLEL AGENT INVESTIGATION (3 Opus 4.5 agents):**

**Agent 1 Finding (CRITICAL):**
- CPacketHandler class is fully implemented but NEVER INSTANTIATED
- All incoming server packets (PLAYER_SPAWN, PLAYER_PURESYNC) were silently discarded
- m_packetHandler in CNetAndroid was always NULL

**Agent 2 Finding:**
- PC client uses ResetReturnPosition() after spawn to initialize sync timer
- Sets m_ulLastPuresyncTime = 0 to trigger immediate sync

**Agent 3 Finding:**
- Connection state machine reaches CONNECTED correctly
- But packets lost because CPacketHandler not registered

**ROOT CAUSE:** CPacketHandler was NEVER INSTANTIATED or REGISTERED

**FIX APPLIED:** Added packet handler initialization in ConnectToServer()

---

## Session 11: Sync Pipeline Investigation (January 10, 2026)

**BUG 1: CPacketHandler not executing - FIXED & VERIFIED**
- Debug logs added - packet handler IS initializing correctly

**BUG 2: Game bypass spawn not triggering - FIXED**
- ROOT CAUSE: Timing calculation was wrong
- Comment said "300 frames (~5 seconds at 60fps)"
- BUT Process() is called every 100ms from thread, NOT at 60fps
- 300 calls × 100ms = 30 SECONDS, not 5 seconds!
- FIX: Changed from 300 to 30 frames (3 second delay)

**NEW FIX: Connection thread exit - APPLIED**
- ISSUE: Connection thread exited immediately after CONNECTED state
- FIX: Thread now only exits on ERROR_STATE or DISCONNECTED

**CURRENT STATE:**
- Packet handler initializes correctly
- Game bypass spawns player after 3 seconds (state 9)
- Connection thread keeps running after CONNECTED
- Player ped found: FindPlayerPed(0) returns valid address

**ROOT CAUSE IDENTIFIED:** CPlayerSync uses different method to find player than CGameBypass
- CGameBypass: FindPlayerPed(0) - WORKS
- CPlayerSync: Uses g_WorldPlayersPtr + g_PlayerInFocus offsets - FAILS

---

## Session 12: Socket Fix - PURESYNC Working! (January 10, 2026)

**CRITICAL BUG FOUND AND FIXED:**

**ROOT CAUSE: Two Separate Sockets!**
- CServerConnection creates socket for RakNet handshake (connected to server)
- CNetAndroid creates separate socket in its NetworkThread (NOT connected!)
- SendPlayerSync() called m_network->SendPacket() → wrong socket → packets lost!

**FIX APPLIED:** Send directly via CServerConnection's connected socket

**RESULT - PACKETS NOW SENDING!**
```
17:32:18.772 D MTA-Connection: Sent PURESYNC: pos=(2486.0,-1665.2,13.3) rot=-3.14 (35 bytes)
```

---

## Session 12b: Bidirectional Sync (January 10, 2026)

**ADDITIONAL FIXES:**

**FIX #2 - Packet RECEIVING:**
- ProcessConnected() was calling m_network->DoPulse() which used CNetAndroid's disconnected socket
- FIX: Added direct recvfrom() on m_socket

**FIX #3 - Server Player ID Relay:**
- Server was relaying raw sync packets without sender's player ID
- Client expects: [packetID] [playerId (4 bytes)] [syncData...]
- FIX: Server now prepends sender's player ID to relayed sync packets

**VERIFIED WORKING:**
```
Server logs:
  <- [41.250.80.49:61649] 35 bytes (Client 1 sends)
  -> [41.250.80.49:62019] 39 bytes (Server relays to Client 2)
  <- [41.250.80.49:62019] 35 bytes (Client 2 sends)
  -> [41.250.80.49:61649] 39 bytes (Server relays to Client 1)
```

---

## Session 13: Player Auto-Creation Fix (January 10, 2026)

**Problem:** UpdatePlayerSync() was returning if player didn't exist

**Fix:** Now auto-creates player on first sync reception:
```cpp
if (it == m_players.end())
{
    // Auto-create player on first sync reception
    PMGR_LOGI("Auto-creating player %u from sync data", playerId);
    std::string autoNickname = "Player" + std::to_string(playerId);
    auto player = std::make_unique<CRemotePlayer>(playerId, autoNickname);
    m_players[playerId] = std::move(player);
}
```

**VERIFIED WORKING:**
- Device 1 (port 60751): "SYNC: Remote players: 1" - sees Device 2
- Device 2 (port 59258): "SYNC: Remote players: 1" - sees Device 1

Bidirectional sync confirmed:
- Device 1 → Server → Device 2: pos=(2239.7,-1261.9,23.9)
- Device 2 → Server → Device 1: pos=(2247.7,-1262.1,24.0)

**NETWORK LAYER: 100% COMPLETE!**

---

## Session 14: Ped Creation Debugging (January 10, 2026)

**GOAL:** Visual rendering of remote players

**Fixes Applied (Still Crashes):**
1. Position validation - Don't spawn if position is (0,0,0)
2. Game state check - Only spawn when m_gameState == 9 (GS_PLAYING_GAME)
3. OnGameStateChange hook - Updates m_gameState from bypass processing thread

**Crash Stack Trace:**
```
#00 CPed::operator new(unsigned long)+16
#01 CPlayerPed::SetupPlayerPed(int)+32
#02 CPlayerPedData::AllocateData()+72
#03 CPlayerPed::CPlayerPed(int, bool)+76
#04 CPedFactory::CreatePedInSlot (our code)
```

**Root Cause Hypothesis:**
- GTA:SA ped pool not properly initialized for multi-player
- SA-MP creates CUSTOM ped pool with 240 slots
- May need similar approach

**Key Discovery:**
- MTA PC uses DIFFERENT approach in `Client/game_sa/CPlayerPedSA.cpp`
- MTA PC does NOT use SetupPlayerPed
- Instead uses: CPed::operator new → CPlayerPed constructor → CWorld::Add

**ARM64 Addresses Found:**
- CPed::operator new = 0x59576c
- CPlayerPed::CPlayerPed(int, bool) = 0x5c0bac
- CPlayerPedData::CPlayerPedData() = 0x4f10cc

**Files Modified:**
- CPlayerManager.h - Added m_gameState, position validation, game state check
- MTAAndroidMain.cpp - Added OnGameStateChange call from bypass thread

---

## Session 15: MTA PC Direct Allocation - REMOTE PLAYER VISIBLE! (January 10, 2026)

**MAJOR BREAKTHROUGH: Remote player is now VISIBLE in game world!**

**Key Discovery:** MTA PC does NOT use `SetupPlayerPed(slot)` for remote players. Instead, it uses direct memory allocation (see `Client/game_sa/CPlayerPedSA.cpp` lines 32-100).

**Implementation that works:**
1. `CPed::operator new(0x998)` - allocate memory
2. `CPlayerPed::CPlayerPed(1, false)` - construct ped
3. Create CPlayerPedData manually, copy from local player
4. Set m_nModelIndex directly (offset 0x24)
5. ClearSpaceForMissionEntity
6. CWorld::Add
7. Set matrix position

**ARM64 Addresses (GTA:SA v2.10):**
| Function | Offset | Mangled Name |
|----------|--------|--------------|
| CPed::operator new | 0x59576C | `_ZN4CPednwEm` |
| CPlayerPed::CPlayerPed | 0x5C0BAC | `_ZN10CPlayerPedC1Eib` |
| CPlayerPedData::ctor | 0x4F10CC | `_ZN14CPlayerPedDataC1Ev` |
| CWorld::Add | 0x507518 | `_ZN6CWorld3AddEP7CEntityb` |

**ARM64 Structure Sizes:**
| Structure | Size |
|-----------|------|
| CPlayerPedGta | 0x998 (2456 bytes) |
| CPlayerPedData | 0xD8 (216 bytes) |
| pPlayerData offset | 0x5A0 |
| m_nModelIndex offset | 0x24 |

**What Works:**
- Remote player ped VISIBLE in game!
- Ped has collision
- Position sync working
- Model (CJ) renders correctly

**Remaining Issue:**
- Ped spawns in dead/ragdoll state
- Need to set health, ped state, or give idle task

---

## Session 16: CWorld::Add Crash Investigation (January 10, 2026)

**Problem:** CWorld::Add crashes when 2nd device joins

**Symptoms:**
- Crash during CWorld::Add call
- SIGSEGV at fault addr 0xab4e6ee8 (bad vtable pointer?)
- Crash happens even with ClearSpaceForMissionEntity disabled
- Ped pointer looks valid: 0xb400007375d93d98
- Ped bytes: 0xf8 0x30 0x67 0x81

**Investigation:**
- Tried skipping ClearSpaceForMissionEntity - still crashes
- Ped memory allocation succeeds
- CPlayerPed constructor succeeds
- Crash happens specifically in CWorld::Add

**Hypothesis:** Game's CWorld::Players array only supports 2 players (local + 1)

---

## Session 17: CWorld::Players Patch - ROOT CAUSE FIXED! (January 10, 2026)

**ROOT CAUSE FOUND:**
- Game's CWorld::Players array only supports 2 players (local + 1)
- SetupPlayerPed(slot>=2) crashes because it accesses out-of-bounds memory
- Even direct allocation crashes because CWorld::Add calls virtual functions that expect properly initialized RenderWare objects

**SOLUTION (from SA-MP Android - patches.cpp):**
1. Create custom CWorld::Players array with 1004 entries
2. Patch game's pointer at g_libGTASA + 0x84E7A8 (ARM64)
3. Now SetupPlayerPed works for any slot!

**NEW FILES CREATED:**

**Client/android/multiplayer/CWorldPlayers.h:**
- CPlayerInfoGta structure (0x1D8 bytes ARM64)
- CPlayerPedData structure (0xD8 bytes ARM64)
- CWorldPlayers class - applies the patch

**UPDATED FILES:**

**Client/android/multiplayer/CPedFactory.h:**
- Integrates with CWorldPlayers
- Uses slot-based approach when patch is applied
- Falls back to direct allocation if patch fails

**ADDITIONAL FIXES:**

1. **Thread Safety:** Game functions must be called from game thread
   - Added PedOperationType::CreateInSlot
   - Queued ped creation for game thread execution
   - Added CreatePedInSlotInternal function

2. **Pending Marker Pattern:** Fixed fake pointer crash
   - Returning slot number (2) as fake pointer caused crash
   - Fixed: Return 0x80000000 | slotNum as pending marker
   - Updated CRemotePlayer with IsPedPending(), TryResolvePendingPed()

3. **PlayerInFocus Fix:** Removed incorrect patch
   - PlayerInFocus is an int, not a pointer
   - Was causing FindPlayerPed(0) to return NULL

**CURRENT STATE:**
- Device 1: Works, can see remote player from Device 2!
- Device 2: Crashes ~150ms after ped creation with fault addr 0x18f
- Remote player is VISIBLE but spawns dead/ragdoll

**CRASH ANALYSIS:**
```
20:51:54.893 Fatal signal 11 (SIGSEGV), fault addr 0x18f in tid 5005 (MainThread)
20:51:54.921 CreatePed: Creating ped model 0
20:51:54.942 CreatePedInSlot SUCCESS: ped=0xb4000070bcb26d98 slot=2
```

**SUSPECTED ISSUE:** Race condition or timing issue on Device 2
- Added local player existence check before creating remote players
- Still investigating

---

## Files Modified Summary

### Phase 7d Debugging Sessions

**Client/android/MTAAndroidMain.cpp:**
- Added debug logs for packet handler init tracing
- Fixed connection thread to not exit on CONNECTED
- Added OnGameStateChange call from bypass thread

**Client/android/game_sa/CGameBypass.cpp:**
- Fixed spawn delay from 300 to 30 frames (30s → 3s)
- Added spawn progress logging

**Client/android/network/CServerConnection.cpp:**
- Added #include <vector>
- Modified SendPlayerSync() to send directly via m_socket
- Modified ProcessConnected() to receive directly via m_socket

**Client/android/multiplayer/CPlayerManager.h:**
- Added m_gameState member and OnGameStateChange()
- Added position validation in UpdatePlayerSync()
- Added game state check for ped spawning

**VPS /home/ubuntu/mtasa/dev/CNetServerAndroid.cpp:**
- Modified sync bypass to prepend sender's player ID to relayed packets
- Fixed deadlock in client lookup
- Added SendPlayerSpawn() function

---

## VPS Server Commands Reference

```bash
# Connect to VPS
ssh dev

# Build and deploy net_android.so
cd /home/ubuntu/mtasa/dev && cmake . && make && cp net_android.so x64/net.so

# Restart MTA server
pkill -f mta-server64 && nohup ./mta-server64 > /tmp/mta-server.log 2>&1 &

# View logs
tail -f /tmp/mta-server.log

# Check listening ports
sudo ss -tulpn | grep -E "22003|22004|22010"
```

---

---

## Session 19 (January 11, 2026) - DEAD POSE FIXED! REMOTE PLAYERS WORKING!

### MAJOR MILESTONE ACHIEVED

**Result:** BOTH devices now see each other as STANDING PLAYERS! The dead pose issue is FIXED!

### Root Cause Analysis

The problem was that we weren't setting the correct ped flags after creation. SA-MP sets specific flags in the `m_nPedFlags` array that prevent the ped from appearing dead/ragdolled.

**Key Discovery from SA-MP:**
- SA-MP's `CPedGTA.h` has the complete ARM64 structure with all offsets
- `m_nPedFlags` is at offset **0x5E0** in CPedGTA (ARM64)
- SA-MP also has NULL `m_pIntelligence` for remote peds (it's commented out in their code!)
- The key flags that prevent dead appearance:
  - `bNeverEverTargetThisPed`: offset 0x5E6, bit 4 (0x10)
  - `bDoesntDropWeaponsWhenDead`: offset 0x5E9, bit 1 (0x02)

### Investigation Process

1. **Searched SA-MP source** for how they handle ped spawning
2. **Found CPedGTA.h** with complete ARM64 structure including m_nPedFlags layout
3. **Found playerped.cpp** line 89-92 where SA-MP sets these flags after ped creation
4. **Calculated exact offsets** based on SA-MP's structure:
   - CPhysical: 0x198 bytes
   - Audio entities: 0x400 bytes (m_PedAudioEntity + m_PedSpeechAudioEntity + m_PedWeaponAudioEntity)
   - m_pIntelligence at 0x598
   - m_pPlayerData at 0x5A0
   - m_nPedFlags at 0x5E0

### Fix Applied

Updated `SetInvulnerable()` in CRemotePlayer.h to use SA-MP's exact flag offsets:

```cpp
constexpr uint32_t PED_FLAGS_OFFSET = 0x5E0;

// bNeverEverTargetThisPed: byte 6 of m_nPedFlags, bit 4 (0x10)
uint8_t* flagsByte6 = reinterpret_cast<uint8_t*>(m_pedPtr + PED_FLAGS_OFFSET + 6);
// bDoesntDropWeaponsWhenDead: byte 9 of m_nPedFlags, bit 1 (0x02)
uint8_t* flagsByte9 = reinterpret_cast<uint8_t*>(m_pedPtr + PED_FLAGS_OFFSET + 9);

*flagsByte6 |= 0x10;  // bNeverEverTargetThisPed = true
*flagsByte9 |= 0x02;  // bDoesntDropWeaponsWhenDead = true
```

### Files Modified

- **CRemotePlayer.h**: Updated `SetInvulnerable()` with SA-MP's exact flag offsets

### What We Learned

1. **Trust SA-MP's structure definitions** - They have the correct ARM64 offsets
2. **m_pIntelligence being NULL is expected** - SA-MP has it commented out too
3. **Ped flags are critical** - The game's default behavior makes peds appear dead without proper flags
4. **The spawn sequence was correct** - SetupPlayerPed → DeactivatePlayerPed → ClearSpace → ReactivatePlayerPed → CWorld::Add works

### Current State

| Device | Sees Other As | Movement Sync | Model Render |
|--------|---------------|---------------|--------------|
| Device 1 | **STANDING** | ✓ Real-time | ✓ Perfect |
| Device 2 | **STANDING** | ✓ Real-time | ✓ Perfect |

### Next Steps (Phase 7g)

Now that basic remote player rendering works:
1. CPad hooks for remote player input (walking animations)
2. Weapon sync
3. Animation sync
4. Vehicle sync

---

## Session 21: Remote Animation Mismatch (January 12, 2026)

**Current Issue:**
- Remote player position sync is correct, but animation and facing are wrong.
- Remote animation mirrors local movement or shows incorrect walk/run state.
- Remote facing can be flipped (appears looking at local player).

**Temporary Workaround:**
- Derived input fallback to synthesize controller input from position delta.
- Helps trigger animations but does not match correct heading/state.

**Likely Root Cause:**
- Android PURESYNC parsing does not read full keysync data.
- Camera rotation and key flags are missing or mis-read.
- Derived input is guessing instead of using real controller state.

**Next Steps:**
1. Port PC `ReadPlayerPuresync` + `ReadFullKeysync` into Android packet handler.
2. Store controller state and camera rotation in RemoteSyncData.
3. Drive CPad hooks from keysync and remove derived input fallback.

---

*Document updated: January 12, 2026 - Session 21: Remote animation mismatch*

## Session 22: Puresync Regression After Bitstream Port (January 13, 2026)

**Current Issue:**
- Remote players no longer visible after PC puresync/bitstream changes.
- Sync positions decode as (0,0,0); players get auto-created then removed as stale.
- PURESYNC packets arrive (31/41 bytes) but parsing is still misaligned.

**Changes Made (This Session):**
1. Ported PC puresync read order (latency, keysync, flags, compressed position/rotation/velocity).
2. Added 17-bit ElementID parsing for player/vehicle packets.
3. Switched NetBitStream norm vector/quaternion to float form (matches net-android server).
4. Updated NetBitStream compressed int format to match net-android server behavior.

**Result:**
- Regression persists; remote positions still decode as (0,0,0).

**Next Steps:**
1. Compare bit offsets during client puresync parsing with server write order.
2. Verify remaining mismatches (compressed floats, camera orientation block).
3. Add targeted logging for bit offsets and values to locate alignment break.

---
