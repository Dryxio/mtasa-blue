# MTA:SA Server - Android Network Module

A custom network module that allows Android clients to connect to MTA servers without Anti-Cheat validation.

## Overview

MTA's official `net.dll` contains Anti-Cheat code that blocks non-Windows clients. This module (`net_android.so`) implements the same `CNetServer` interface but without AC, enabling Android clients to connect.

## Architecture

```
Server Architecture:
Port 22003 -> net.dll        (PC clients with Anti-Cheat)
Port 22010 -> net_android.so (Android clients, no AC)
           \      |
            \     v
             -> deathmatch mod (same game logic)
```

## Building

### Requirements

- CMake 3.10+
- C++17 compiler (GCC 7+, Clang 5+)
- Linux/macOS/Windows

### Build Steps

```bash
cd Server/net-android
./build.sh              # Build Release
./build.sh debug        # Build Debug
./build.sh clean        # Clean build
```

Or manually:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Output

- Linux: `net_android.so`
- macOS: `net_android.so` (for testing)
- Windows: `net_android.dll`

## Deployment

### Copy to Server

```bash
scp build/net_android.so your-server:/path/to/mtasa/mods/deathmatch/
```

### Server Configuration

In `mtaserver.conf`:

```xml
<!-- Standard port for PC clients (uses net.dll) -->
<serverport>22003</serverport>

<!-- Android port (uses net_android.so) -->
<android_port>22010</android_port>
```

### Testing

1. Start the MTA server normally
2. Connect Android client to port 22010
3. Check server console for connection messages

## Protocol

This module implements MTA's RakNet 3.x protocol:

1. `OPEN_CONNECTION_REQUEST` (0x09) + cookie
2. `OPEN_CONNECTION_REPLY` (0x0A) + cookie
3. `CONNECTION_REQUEST` (0x04) + GUID + timestamp
4. `CONNECTION_REQUEST_ACCEPTED` (0x0E) + addresses
5. `MOD_NAME` (0x1C) + "deathmatch" + version
6. `PLAYER_JOINDATA` (0x01) + player info
7. `JOIN_COMPLETE` (0x02) + server version

## Files

| File | Description |
|------|-------------|
| `CNetServerAndroid.h` | Main header (CNetServer interface) |
| `CNetServerAndroid.cpp` | Server implementation (~800 lines) |
| `CNetBitStreamAndroid.h` | BitStream interface |
| `CNetBitStreamAndroid.cpp` | BitStream implementation |
| `CMakeLists.txt` | Build configuration |
| `exports.map` | Linux symbol exports |
| `exports_macos.txt` | macOS symbol exports |
| `build.sh` | Build script |

## API

The module exports two functions (matching net.dll):

```cpp
extern "C" {
    CNetServer* InitNetServerInterface();
    void ReleaseNetServerInterface();
}
```

These are called by MTA's server core to initialize the network module.

## Features

- Full MTA RakNet 3.x protocol support
- No Anti-Cheat validation
- Thread-safe client management
- Automatic timeout handling (30s)
- Network statistics
- Logging for debugging

## Limitations

- No Anti-Cheat (by design)
- No HTTP download manager
- Clients get fake serial numbers (ANDROID + GUID)
- Single-port only (no dual-port yet)

## Next Steps

1. Integration with MTA server core
2. Dual-port configuration
3. Player synchronization testing
4. Cross-platform play (Android + PC)

## License

Same as MTA:SA - See LICENSE in the top level directory.
