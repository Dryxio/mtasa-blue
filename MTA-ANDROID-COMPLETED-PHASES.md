# MTA:SA Android Port - Completed Phases Reference

> Document created: January 10, 2026
> This file contains detailed documentation for completed phases.
> For active development status, see [MTA-ANDROID-PROJECT-SUMMARY.md](MTA-ANDROID-PROJECT-SUMMARY.md)
> For historical session logs, see [MTA-ANDROID-PROGRESS-LOG.md](MTA-ANDROID-PROGRESS-LOG.md)

---

## Phase 6: GTA:SA Integration (COMPLETED)

### Completed Components

| Component | File | Description |
|-----------|------|-------------|
| Game Detection | `game_sa/GTASAIntegration.h` | Finds libGTASA.so in /proc/self/maps |
| Version Detection | `GTASAIntegration.h` | Identifies v1.08, 2.10, 2.11 (32/64-bit) |
| Library Validation | `GTASAIntegration.h` | Validates ELF header, GTA:SA strings |
| Proof-of-Concept Hooks | `GTASAIntegration.h` | Hook framework (CGame::Process) |
| Status Reporting | `GTASAIntegration.h` | JSON API for debugging |
| Launcher UI | `GTASALoaderActivity.java` | GTA:SA detection, feature toggles |
| APK Injection | `tools/inject-mta.sh` | Decompile, inject, sign APKs |
| JNI API | `jni/MTANative.cpp` | enableGodMode(), getIntegrationStatus() |
| Toast Notification | `GTASA.smali` (patched) | Visual "MTA:SA Android Loaded!" |

### Verified Test Results (January 2026)

```
Platform:        Genymotion on macOS (Apple Silicon)
Android Version: 11 (API 30)
GTA:SA Version:  2.10 (ARM64)
Input APK:       GTA SA 2.10.apk
Output APK:      gtasa-210-mta.apk (63MB)
OBB Files:       v1.08 OBB (works with v2.10 APK)
```

### What Was Verified Working

| Test | Result | Evidence |
|------|--------|----------|
| APK injection | Pass | `libmta_android.so` present in APK |
| Library loading | Pass | Logcat: "MTA:SA Android initialized successfully!" |
| Game detection | Pass | Logcat: "Found game library: libGTASA.so" |
| Version detection | Pass | Logcat: "Detected version: 2.11 (64-bit)" |
| Library validation | Pass | Logcat: "Library validation passed (found 4/4 GTA strings)" |
| Toast notification | Pass | Visual: "MTA:SA Android Loaded!" on screen |
| Game stability | Pass | Game runs without crashing |

### Integration Approaches

| Approach | Status | Root Required |
|----------|--------|---------------|
| APK Modification | **Verified Working** | No |
| Xposed Module | Planned | Yes |
| Frida Injection | Planned | Yes |

### Known Limitations
- CGame::Process hook disabled (needs version-specific offsets)
- God mode not functional yet (requires reverse engineering)
- Each GTA:SA version needs its own offset mapping

---

## Phase 7: Multiplayer Network (COMPLETED)

### Implemented Components

| Component | File | Status |
|-----------|------|--------|
| Network Manager | `network/CNetAndroid.h/cpp` | Tested |
| Bitstream | `network/CNetAndroid.h` | 5 tests pass |
| Packet Handler | `network/CPacketHandler.h/cpp` | Tested |
| Sync Structures | `network/SyncStructures.h` | 3 tests pass |
| Server Connection | `network/CServerConnection.h/cpp` | **VPS Verified** |
| RakNet Handshake | `network/raknet/RakNetHandshake.h/cpp` | **Dual-protocol** |

**Test Results:** 14 network-specific tests passing (NetBitStream: 5, SyncStructures: 3, CNetAndroid: 2, ServerConnection: 4)

### Network Features
- **CNetAndroid**: UDP socket management, connection state, packet queuing
- **NetBitStream**: Bit-level read/write, compressed types, vectors, quaternions
- **CPacketHandler**: 100+ packet types, 50+ RPC functions, event callbacks
- **SyncStructures**: Player puresync, vehicle puresync, keysync, health/armor
- **CServerConnection**: Connection state machine, MD5 password hashing, DNS resolution
- **RakNetHandshake**: Dual-protocol support (MTA RakNet 3.x + RakNet 4)

### Packet Types Implemented
- Connection: JOIN, JOINDATA, QUIT, TIMEOUT
- Sync: PURESYNC, KEYSYNC, VEHICLE_PURESYNC, LIGHTSYNC
- Chat: CHAT_ECHO, CONSOLE_ECHO, DEBUG_ECHO
- Entities: ENTITY_ADD, ENTITY_REMOVE
- Vehicles: VEHICLE_SPAWN, VEHICLE_INOUT, VEHICLE_DAMAGE_SYNC
- RPC: SET_ELEMENT_POSITION, SET_TIME, SET_WEATHER, etc.

---

## Phase 7b: MTA Protocol Reverse Engineering (COMPLETED)

### The Challenge: Closed-Source net.dll

MTA's network layer (`net.dll` for servers, `netc.dll` for clients) is **closed-source** and contains the RakNet implementation. Without understanding this protocol, Android clients cannot connect to real MTA servers.

### Reverse Engineering with Ghidra

We used Ghidra (NSA's reverse engineering tool) to analyze MTA's network library:

```bash
# 1. Install Ghidra (if not installed)
brew install --cask ghidra   # macOS
# Or download from https://ghidra-sre.org/

# 2. Create project and analyze net.dll
ghidra_11.3.2_PUBLIC/support/analyzeHeadless \
    ~/ghidra-projects mta-net \
    -import /path/to/mtasa-blue/Bin/server/x64/net.dll \
    -postScript ExportRakNetFunctions.java

# 3. Export decompiled functions
# Output: ~/ghidra-exports/mta-net/ (134 functions)
```

### Key Discovery: MTA Uses RakNet 3.x (Not RakNet 4)

Through source analysis of `Shared/sdk/net/packetenums.h` and Ghidra RE, we discovered:

| Packet | MTA RakNet 3.x | Standard RakNet 4 |
|--------|----------------|-------------------|
| OPEN_CONNECTION_REQUEST | **0x09** | 0x05 |
| OPEN_CONNECTION_REPLY | **0x0A** | 0x06 |
| CONNECTION_REQUEST | **0x04** | 0x09 |
| CONNECTION_REQUEST_ACCEPTED | **0x0E** | 0x10 |

**Key Differences:**
- MTA RakNet 3.x: No magic bytes, uses 4-byte cookie
- RakNet 4: 16-byte magic bytes (OFFLINE_MESSAGE_ID)
- MTA: 1 round-trip for open connection
- RakNet 4: 2 round-trips for open connection

### Dual-Protocol Implementation

We implemented both protocols in `RakNetHandshake.cpp` (~750 lines):

```cpp
enum class RakNetProtocol {
    MTA_RAKNET3,    // For real MTA servers (default)
    RAKNET4         // For test servers
};

// MTA RakNet 3.x handshake:
// Client → Server: 0x09 + cookie (5 bytes)
// Server → Client: 0x0A + cookie (5 bytes)
// Client → Server: 0x04 + GUID + timestamp (18 bytes)
// Server → Client: 0x0E + addresses + timestamps (96 bytes)
```

### Files Created

| File | Description |
|------|-------------|
| `network/raknet/RakNetHandshake.h` | Dual-protocol header |
| `network/raknet/RakNetHandshake.cpp` | Implementation (~750 lines) |
| `tools/mta_test_server.py` | Python server (both protocols) |
| `~/ghidra-exports/mta-net/` | 134 decompiled functions |
| `~/ghidra-tools/ExportRakNetFunctions.java` | Ghidra export script |

### Connection Test Results

**Test Server (37.59.101.35:22010):**
```
Protocol:     MTA RakNet 3.x
Handshake:    0x09 → 0x0A → 0x04 → 0x0E
Cookie:       Verified (random 4-byte value echoed)
MOD_NAME:     Received (deathmatch, v0x06B)
JOIN_DATA:    Sent successfully
JOIN_COMPLETE: Received (v1.6.0)
Result:       CONNECTED
```

---

## Phase 7c: Custom Server Module (COMPLETED)

### Why Custom Server Module?

MTA's `net.dll` (closed-source) contains anti-cheat that blocks non-Windows clients:

```
Current Problem:
Android Client → net.dll [AC CHECK] → BLOCKED
                         ↓
              deathmatch mod never sees connection
```

**Solution:** Build a custom `CNetServer` implementation that accepts Android clients without AC:

```
With net_android.so:
Android Client → net_android.so [NO AC] → ACCEPTED
                         ↓
              deathmatch mod receives connection
                         ↓
              Android + PC players on same server
```

### Architecture

```
Server/
├── net.dll              # Original (PC clients with AC)
├── net_android.so       # NEW: Android clients (no AC)
│
└── mods/deathmatch/     # Unchanged - handles both client types
    └── logic/CGame.cpp  # Receives packets from both modules
```

### Implementation

**Files Created:**
```
Server/net-android/
├── CNetServerAndroid.h      # CNetServer interface implementation (~1200 lines)
├── CNetServerAndroid.cpp
├── CNetBitStreamAndroid.h   # BitStream implementation (~500 lines)
├── CNetBitStreamAndroid.cpp
├── CMakeLists.txt           # Build configuration
└── exports.cpp              # DLL/SO exports (InitNetServerInterface)
```

### Dual-Port Configuration

| Port | Module | Clients | AC |
|------|--------|---------|-----|
| 22003 | net.dll | PC only | Enabled |
| 22010 | net_android.so | Android + PC | Disabled |

### Success Criteria Achieved

- [x] net_android.so builds successfully (Linux VPS)
- [x] Standalone test server loads and runs the module
- [x] Full handshake flow verified (OPEN_CONNECTION → CONNECTION_REQUEST → ACCEPTED)
- [x] MOD_NAME packet sent to client
- [x] PLAYER_JOINDATA received and parsed correctly
- [x] JOIN_COMPLETE + JOINED_GAME sent to client
- [x] Packet ID 0x01 collision fixed (PING vs PLAYER_JOINDATA, state-based)
- [x] Android client completes full connection (Genymotion test PASSED)
- [x] **MTA server integration COMPLETE** (replaces net.so, loads with deathmatch.so)
- [x] **Vtable compatibility fixed** (removed CBinaryFileInterface, added HTTP stub)
- [x] **Server disconnect handling fixed** (timeout checking moved to network thread)

---

## VPS Server Infrastructure

### Server Details

| Property | Value |
|----------|-------|
| IP Address | `37.59.101.35` |
| SSH Alias | `dev` (configured in ~/.ssh/config) |
| SSH User | `ubuntu` |
| SSH Key | `~/Documents/Github/wosa/key` |

### SSH Configuration (~/.ssh/config)

```
Host dev
    HostName 37.59.101.35
    User ubuntu
    IdentityFile ~/Documents/Github/wosa/key
```

### Server Ports

| Port | Service | Description |
|------|---------|-------------|
| 22003 | MTA Default | Standard MTA server port |
| 22004 | MTA Server | Real MTA server running (with resources) |
| 22005 | MTA ASE | Server browser query port |
| 22010 | Test Server | Python test server (MTA RakNet 3.x + RakNet 4) |

### Server Commands

```bash
# Connect to VPS
ssh dev

# Check MTA server status
ps aux | grep mta-server

# View MTA server logs
tail -f /tmp/mta-server.log

# Start/restart MTA server
cd /home/ubuntu/mtasa/dev && nohup ./mta-server64 > /tmp/mta-server.log 2>&1 &

# Start test server (for Android testing)
sudo python3 /opt/mta-test/mta_test_server.py 22010

# Check listening ports
sudo ss -tulpn | grep -E "22003|22004|22010"
```

### Deploy Test Server

```bash
# Copy test server to VPS
scp tools/mta_test_server.py dev:/opt/mta-test/

# SSH and start
ssh dev 'sudo python3 /opt/mta-test/mta_test_server.py 22010'
```

---

## Wireshark Setup for MTA Traffic Capture

### Install Wireshark

```bash
# macOS
brew install --cask wireshark

# Windows
# Download from https://www.wireshark.org/download.html

# Linux
sudo apt install wireshark
sudo usermod -aG wireshark $USER
```

### Capture MTA Traffic

**Step 1: Start Wireshark**
```bash
sudo wireshark &
```

**Step 2: Select Network Interface**
- Choose `en0` (Wi-Fi) or `en1` (Ethernet) on macOS

**Step 3: Set Capture Filter**
```
udp port 22003 or udp port 22004 or udp port 22005
```

**Step 4: Start Capture & Connect**
- Click the blue shark fin button
- Launch MTA:SA on PC
- Connect to server: `37.59.101.35:22004`

### Key Packets to Look For

| Direction | Packet ID | Description |
|-----------|-----------|-------------|
| Client → Server | `0x09` | OPEN_CONNECTION_REQUEST |
| Server → Client | `0x0A` | OPEN_CONNECTION_REPLY |
| Client → Server | `0x04` | CONNECTION_REQUEST |
| Server → Client | `0x0E` | CONNECTION_REQUEST_ACCEPTED |
| Server → Client | `0x1C` | MOD_NAME |
| Client → Server | `0x01` | PLAYER_JOINDATA |

### Analyze with tshark (Command Line)

```bash
# Capture MTA traffic to file
sudo tshark -i en0 -f "udp port 22003 or udp port 22004" -w mta-capture.pcap

# Read and display packets
tshark -r mta-capture.pcap -T fields -e frame.number -e ip.src -e ip.dst -e udp.port -e data

# Show first 50 bytes of each packet payload
tshark -r mta-capture.pcap -T fields -e data | head -20
```

---

## Technical Architecture

### Why Android is Feasible

| Aspect | Windows | Android | Compatibility |
|--------|---------|---------|---------------|
| Engine | RenderWare | RenderWare | Same |
| Data structures | RwMatrix, RpClump... | RwMatrix, RpClump... | Same |
| File formats | TXD, DFF, COL | TXD, DFF, COL | Same |
| Architecture | x86 32-bit | ARM 32/64-bit | Different |
| Graphics API | Direct3D 9 | OpenGL ES | Different |
| Memory addresses | 0x4XXXXX-0x7XXXXX | Different offsets | Must remap |

### Hook System

| x86 | ARM32 (Thumb) | ARM64 | Description |
|-----|---------------|-------|-------------|
| `JMP rel32` | `LDR PC, [PC]` | `LDR X16, #8; BR X16` | Branch |
| `CALL rel32` | `BL` | `BL` | Function call |
| `NOP` | `0xBF00` | `0xD503201F` | No operation |
| `RET` | `BX LR` | `RET` | Return |

---

## Estimated Effort (Remaining)

| Phase | Effort | Description |
|-------|--------|-------------|
| ~~Device Testing~~ | ~~1-2 days~~ | **Completed** - Verified on Genymotion |
| ~~Phase 7~~ | ~~1-2 days~~ | **Completed** - Network foundation, RakNet 4 |
| ~~Phase 7b~~ | ~~2-3 days~~ | **Completed** - Ghidra RE, MTA RakNet 3.x |
| ~~Phase 7c~~ | ~~5-7 days~~ | **Completed** - MTA server integration working |
| ~~Phase 7d~~ | ~~3-5 days~~ | **Completed** - CPlayerSync, game state monitoring |
| ~~Phase 7e~~ | ~~2-3 days~~ | **Completed** - Multi-client sync verified! |
| Phase 7f | 1-2 weeks | Remote player rendering (spawn CPed, visual sync) |
| Polish | 2-3 weeks | UI, server browser, stability |

**Total remaining**: 3-5 weeks for full multiplayer functionality.

---

*Document updated: January 10, 2026*
