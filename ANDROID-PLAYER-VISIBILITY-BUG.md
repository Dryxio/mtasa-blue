# Android MTA Player Visibility - Progress Documentation

**Date:** 2026-01-15 (Updated)
**Status:** ROOT CAUSE IDENTIFIED - Fix ready to implement
**Goal:** Get two Android players to see each other in-game

---

## Root Cause Analysis (Session 38)

### The Bug: Two Mismatches

After detailed analysis comparing MTA PC source code (`Shared/sdk/net/Packets.h`) with the Android implementation, we identified **two distinct problems**:

#### Problem 1: Packet ID Mismatch

The Android server sends `PLAYER_SPAWN` with packet ID `0x18`, but the MTA protocol defines:

| Packet | MTA PC (Correct) | Android Server (Wrong) |
|--------|------------------|------------------------|
| `RPC` | 0x18 (24) | - |
| `PLAYER_SPAWN` | 0x1A (26) | 0x18 (24) **WRONG** |

**Result:** When client receives packet `0x18`, the switch statement routes it to `Packet_RPC()` instead of `Packet_PlayerSpawn()`. Remote players are never created.

**Key insight:** The client's `PacketID` enum in `CNetAndroid.h` is **correct** - it matches MTA PC protocol. The bug is in the **server** sending the wrong ID (0x18 instead of 0x1A).

```cpp
// Client enum (CORRECT - matches MTA PC):
RPC,                    // position 24 = 0x18
PLAYER_LIST,            // position 25 = 0x19
PLAYER_SPAWN,           // position 26 = 0x1A

// Server sends 0x18 for PLAYER_SPAWN (WRONG)
// Fix: Either remap on client OR fix server to send 0x1A
```

#### Problem 2: Packet Format Mismatch

Even if routing worked, the client expects **MTA PC bitstream format**, but the server sends **raw little-endian format**:

| Field | MTA PC Format | Android Server Format |
|-------|---------------|----------------------|
| Player ID | 17-bit ElementID | 16-bit uint16_t |
| Flags | uint8_t (present) | **Missing** |
| Position | 3 floats | 3 floats |
| Rotation | float | float |
| Model ID | uint16_t | uint16_t |
| Interior | uint8_t (present) | **Missing** |
| Dimension | uint16_t (present) | **Missing** |
| Team ID | 17-bit ElementID | uint16_t |
| TimeContext | uint8_t (present) | **Missing** |
| Health/Armor | *Not in standard* | float, float **(non-standard)** |
| Nickname | *Not in standard* | uint8_t len + string **(non-standard)** |

---

## Reference Sources

### MTA PC Source (Authoritative)

- **Packet IDs:** `Shared/sdk/net/Packets.h` - defines correct packet ID values
- **Packet Format:** `Client/mods/deathmatch/logic/CPacketHandler.cpp:1085-1131` - shows correct `Packet_PlayerSpawn` parsing
- **Status:** This is the source of truth for MTA protocol

### SA-MP Android Source (Secondary Reference Only)

- **Location:** `Client/android/reference/samp-android-reference/`
- **Useful for:** Understanding Android GTA:SA engine interaction (`pGame->NewPlayer()`, `CRemotePlayer::Spawn()`)
- **NOT useful for:** MTA packet format decisions (SA-MP uses different protocol)
- **Status:** Inspiration only, not authoritative

---

## Fix Options

### Option A: Client-Side Adaptation (Quick Fix)

Adapt the client to handle the server's non-standard format. Minimal changes, works immediately.

**File 1: `Client/android/network/CServerConnection.cpp`**

Map packet ID 0x18 -> PLAYER_SPAWN at dispatch time (3 locations):

```cpp
// Lines 789, 835, 889 - change:
m_network->DispatchPacket(static_cast<PacketID>(packetId), bitStream);

// To:
PacketID mappedId = (packetId == 0x18) ? PacketID::PLAYER_SPAWN
                                       : static_cast<PacketID>(packetId);
m_network->DispatchPacket(mappedId, bitStream);
```

**File 2: `Client/android/network/CPacketHandler.cpp`**

Fix `Packet_PlayerJoin` (line 670+) to parse raw format:
```cpp
// Replace ReadElementId + uint16_t nicknameLength with:
uint16_t rawPlayerId = 0;
if (!bitStream.Read(rawPlayerId)) { fail("playerId"); return; }
playerId = rawPlayerId;

uint8_t nicknameLength = 0;  // Changed from uint16_t
if (!bitStream.Read(nicknameLength)) { fail("nicknameLength"); return; }
```

Fix `Packet_PlayerSpawn` (line 781+) to parse raw format:
```cpp
// Replace PC bitstream parsing with:
uint16_t rawPlayerId = 0;
uint16_t modelId = 0;
uint16_t teamId = 0;
float health = 0.0f;
float armor = 0.0f;
uint8_t nicknameLength = 0;

if (!bitStream.Read(rawPlayerId)) { fail("playerId"); return; }
playerId = rawPlayerId;
if (!bitStream.Read(modelId)) { fail("modelId"); return; }
if (!bitStream.Read(teamId)) { fail("teamId"); return; }
if (!bitStream.Read(x)) { fail("x"); return; }
if (!bitStream.Read(y)) { fail("y"); return; }
if (!bitStream.Read(z)) { fail("z"); return; }
if (!bitStream.Read(rotation)) { fail("rotation"); return; }
if (!bitStream.Read(health)) { fail("health"); return; }
if (!bitStream.Read(armor)) { fail("armor"); return; }
if (!bitStream.Read(nicknameLength)) { fail("nicknameLength"); return; }
// Read nickname if needed...

skinId = modelId;
```

### Option B: Server-Side Correction (Proper Fix)

Fix the server to use correct MTA protocol. More work, but proper long-term solution.

**File: `Server/net-android/CNetServerAndroid.cpp`**

1. Change packet ID from 0x18 to 0x1A (line 975):
   ```cpp
   packet[offset++] = 0x1A;  // Correct PLAYER_SPAWN ID
   ```

2. Use MTA PC bitstream format with 17-bit ElementID, flags, interior, dimension, etc.

---

## Current State (Session 37-38)

### What Works
- RakNet handshake completes successfully
- Both Android clients connect to server
- Server sends PLAYER_JOIN (0x03) and PLAYER_SPAWN (0x18) packets
- Clients RECEIVE these packets (confirmed in logs)
- Both clients send PURESYNC packets
- Server broadcasts sync packets between clients (SYNC BYPASS working)

### What's Broken
- **Packet 0x18 routes to wrong handler** (RPC instead of PLAYER_SPAWN)
- **Packet format doesn't match** (raw vs PC bitstream)
- No remote player objects are created

### Evidence from Logs

**Device 1 received:**
```
Received packet 0x18 from 37.59.101.35:22004 (44 bytes)  <- Routed to Packet_RPC()!
Received packet 0x03 from 37.59.101.35:22004 (17 bytes)  <- PLAYER_JOIN
```

**Device 2 received:**
```
Received packet 0x03 from 37.59.101.35:22004 (17 bytes)  <- PLAYER_JOIN
Received packet 0x18 from 37.59.101.35:22004 (44 bytes)  <- Routed to Packet_RPC()!
```

---

## Packet Formats Reference

### Server's Current Format (Non-Standard)

**PLAYER_JOIN (0x03):** *(ID is correct)*
```
[0]     uint8   packet ID (0x03)
[1-2]   uint16  player ID (little-endian)  # Should be 17-bit ElementID
[3]     uint8   nickname length
[4+]    char[]  nickname
```

**PLAYER_SPAWN (0x18 - should be 0x1A):**
```
[0]     uint8   packet ID (0x18)           # WRONG - should be 0x1A
[1-2]   uint16  player ID (little-endian)  # Should be 17-bit ElementID
[3-4]   uint16  model ID (little-endian)
[5-6]   uint16  team ID (little-endian)    # Should be 17-bit ElementID
[7-10]  float   position X
[11-14] float   position Y
[15-18] float   position Z
[19-22] float   rotation
[23-26] float   health                     # NON-STANDARD (not in MTA PC)
[27-30] float   armor                      # NON-STANDARD (not in MTA PC)
[31]    uint8   nickname length            # NON-STANDARD (not in MTA PC)
[32+]   char[]  nickname                   # NON-STANDARD (not in MTA PC)
# MISSING: flags, interior, dimension, timeContext (required by MTA PC)
```

### MTA PC Format (Correct/Standard)

**PLAYER_SPAWN (0x1A):**
```
ElementID   player ID (17-bit)
uint8       flags
float       position X
float       position Y
float       position Z
float       rotation
uint16      model ID
uint8       interior
uint16      dimension
ElementID   team ID (17-bit)
uint8       time context
```

---

## Patching deathmatch.so - Complete Guide

The MTA server uses a precompiled `deathmatch.so` that has multiple version checks. Since we can't recompile it, we patch the binary directly to bypass these checks.

### File Locations
- **Original binary:** `/home/ubuntu/mtasa/dev/x64/deathmatch.so` (on VPS)
- **Local copy:** `/Users/salimtrouve/Documents/GitHub/mtasa-blue/ghidra-analysis/deathmatch.so`
- **Backup:** `/Users/salimtrouve/Documents/GitHub/mtasa-blue/ghidra-analysis/deathmatch.so.backup`

### Patches Applied

#### Patch 1: Netcode Version Check (DONE)
**Location:** `CGame::Packet_PlayerJoinData` at offset `0x2832f6`

**Original code:**
```asm
2832f0: 66 81 7b 20 da 01    cmpw   $0x1DA, 32(%rbx)   ; compare netcode
2832f6: 0f 85 82 03 00 00    jne    0x28367e           ; jump to "Bad version" if not equal
```

**Patched to:**
```asm
2832f6: 90 90 90 90 90 90    nop (x6)                  ; skip the check
```

**Command:**
```bash
printf '\x90\x90\x90\x90\x90\x90' | dd of=deathmatch.so bs=1 seek=$((0x2832f6)) conv=notrunc
```

#### Patch 2: Minimum Client Version Check (DONE)
**Location:** `CGame::Packet_PlayerJoinData` at offset `0x2833d6`

**Original code:**
```asm
2833cf: e8 cc 40 ff ff       callq  IsBelowMinimumClient
2833d4: 84 c0                testb  %al, %al
2833d6: 0f 85 ee 04 00 00    jne    0x2838ca           ; jump to "below minimum" if true
```

**Patched to:**
```asm
2833d6: 90 90 90 90 90 90    nop (x6)                  ; skip the check
```

**Command:**
```bash
printf '\x90\x90\x90\x90\x90\x90' | dd of=deathmatch.so bs=1 seek=$((0x2833d6)) conv=notrunc
```

#### Patch 3: Version Mismatch Check (DONE)
**Location:** `CGame::Packet_PlayerJoinData` at offsets `0x283c30` and `0x283c7a`

The version mismatch check has TWO paths that both lead to the error message. Both must be patched.

**String:** `"CONNECT: %s failed to connect (Version mismatch) (%s)"` at `0x9a0f10`

**Patch 3a - First conditional (0x283c30):**
```asm
; Original:
283c29: 48 3b 90 b8 0a 00 00    cmpq   2744(%rax), %rdx
283c30: 74 36                    je     0x283c68          ; jump if equal (skip error)

; Patched to unconditional jump:
283c30: eb 36                    jmp    0x283c68          ; always skip error
```

**Command:**
```bash
printf '\xeb' | dd of=deathmatch.so bs=1 seek=$((0x283c30)) conv=notrunc
```

**Patch 3b - Second conditional (0x283c7a):**
```asm
; Original:
283c78: 85 c0                    testl  %eax, %eax
283c7a: 75 b6                    jne    0x283c32          ; jump back to error if not equal

; Patched to NOPs:
283c7a: 90 90                    nop nop                  ; never jump to error
```

**Command:**
```bash
printf '\x90\x90' | dd of=deathmatch.so bs=1 seek=$((0x283c7a)) conv=notrunc
```

### How to Apply All Patches

```bash
# 1. Copy deathmatch.so from VPS to local (or use existing patched copy)
scp dev:/home/ubuntu/mtasa/dev/x64/deathmatch.so /Users/salimtrouve/Documents/GitHub/mtasa-blue/ghidra-analysis/

# 2. Backup original (if not already done)
cp ghidra-analysis/deathmatch.so ghidra-analysis/deathmatch.so.backup

# 3. Apply Patch 1 (netcode check) - 6 NOPs
printf '\x90\x90\x90\x90\x90\x90' | dd of=ghidra-analysis/deathmatch.so bs=1 seek=$((0x2832f6)) conv=notrunc

# 4. Apply Patch 2 (minimum version check) - 6 NOPs
printf '\x90\x90\x90\x90\x90\x90' | dd of=ghidra-analysis/deathmatch.so bs=1 seek=$((0x2833d6)) conv=notrunc

# 5. Apply Patch 3a (version mismatch - first path) - JE to JMP
printf '\xeb' | dd of=ghidra-analysis/deathmatch.so bs=1 seek=$((0x283c30)) conv=notrunc

# 6. Apply Patch 3b (version mismatch - second path) - 2 NOPs
printf '\x90\x90' | dd of=ghidra-analysis/deathmatch.so bs=1 seek=$((0x283c7a)) conv=notrunc

# 7. Verify patches
echo "Checking patch locations..."
xxd ghidra-analysis/deathmatch.so | grep -E "002832f0|002833d0|00283c30|00283c70"
# Expected:
# 002832f0: ... 9090 9090 9090 ... (Patch 1)
# 002833d0: ... 9090 9090 9090 ... (Patch 2)
# 00283c30: eb36 ...              (Patch 3a - jmp instead of je)
# 00283c70: ... 9090 ...          (Patch 3b - NOPs at offset +0xa)

# 8. Upload to VPS
scp ghidra-analysis/deathmatch.so dev:/home/ubuntu/mtasa/dev/x64/deathmatch.so

# 9. Restart server
ssh dev "pkill -9 -f mta-server64; sleep 2; cd /home/ubuntu/mtasa/dev && ./mta-server64 > /tmp/mta.log 2>&1 &"
```

---

## Build & Deploy Commands

### Build Android Client
```bash
cmake --build /Users/salimtrouve/Documents/GitHub/mtasa-blue/Client/android/build -j8
```

### Deploy Android Client to Device
```bash
cp /Users/salimtrouve/Documents/GitHub/mtasa-blue/Client/android/build/libmta_android.so /tmp/gtasa-decompiled/lib/arm64-v8a/
apktool b /tmp/gtasa-decompiled -o /tmp/gtasa-unsigned.apk
/opt/homebrew/share/android-commandlinetools/build-tools/34.0.0/zipalign -f 4 /tmp/gtasa-unsigned.apk /tmp/gtasa-aligned.apk
/opt/homebrew/share/android-commandlinetools/build-tools/34.0.0/apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android --out /tmp/gtasa-mta.apk /tmp/gtasa-aligned.apk
/Applications/Genymotion.app/Contents/MacOS/tools/adb -s 127.0.0.1:6555 install -r /tmp/gtasa-mta.apk
```

### Build Server net-android Module
```bash
scp Server/net-android/*.cpp Server/net-android/*.h dev:/home/ubuntu/mtasa/dev/
ssh dev "cd /home/ubuntu/mtasa/dev && make -j4 && cp net_android.so x64/net.so"
```

### Restart Server
```bash
ssh dev "pkill -9 -f mta-server64; sleep 2; cd /home/ubuntu/mtasa/dev && ./mta-server64 > /tmp/mta.log 2>&1 &"
```

### Check Logs
```bash
# Server logs
ssh dev "tail -50 /tmp/mta.log"

# Client logs (Device 1)
/Applications/Genymotion.app/Contents/MacOS/tools/adb -s 127.0.0.1:6555 logcat -d -s MTA-Connection | tail -30

# Client logs (Device 2)
/Applications/Genymotion.app/Contents/MacOS/tools/adb -s 127.0.0.1:6562 logcat -d -s MTA-Connection | tail -30
```

---

## Test Devices

| Device | ADB Address | Status |
|--------|-------------|--------|
| Device 1 | 127.0.0.1:6555 | CONNECTED |
| Device 2 | 127.0.0.1:6562 | CONNECTED |

---

## Progress Timeline

| Session | Achievement |
|---------|-------------|
| 35 | RakNet handshake working |
| 36 | Binary patched deathmatch.so (4 patches), clients connect |
| 37 | Fixed packet IDs (PLAYER_JOIN=0x03, PLAYER_SPAWN=0x18), packets received by clients |
| 38 | **ROOT CAUSE IDENTIFIED:** Packet ID routing error (0x18->RPC) + format mismatch |
| Next | Implement fix (Option A or B) |

---

## Architecture Overview

```
+------------------+         +------------------+
|  Android Client  |         |  Android Client  |
|    (Device 1)    |         |    (Device 2)    |
+--------+---------+         +--------+---------+
         |                            |
         |  PURESYNC (0x20)           |  PURESYNC (0x20)
         |  ------------>             |  ------------>
         |                            |
         v                            v
+--------------------------------------------------+
|              MTA Server (VPS)                     |
|  +------------------+  +----------------------+  |
|  |  net_android.so  |  |   deathmatch.so      |  |
|  |  (custom)        |  |   (patched binary)   |  |
|  |                  |  |                      |  |
|  |  - RakNet proto  |  |  - Game logic        |  |
|  |  - SYNC BYPASS   |  |  - Version checks    |  |
|  |  - NAT handling  |  |    (bypassed)        |  |
|  +------------------+  +----------------------+  |
+--------------------------------------------------+
         |                            |
         |  PLAYER_JOIN (0x03)        |  PLAYER_JOIN (0x03)
         |  PLAYER_SPAWN (0x18)       |  PLAYER_SPAWN (0x18)
         |  <------------             |  <------------
         v                            v
    [BUG: 0x18 routes to Packet_RPC() instead of Packet_PlayerSpawn()]
    [BUG: Format mismatch - raw vs PC bitstream]
```

---

## VPS Access

```bash
ssh dev  # Alias for the MTA development server
# Server directory: /home/ubuntu/mtasa/dev
# Binary: ./mta-server64
# Logs: /tmp/mta.log
```
