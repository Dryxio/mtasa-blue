# MTA:SA Android

Native client port of Multi Theft Auto: San Andreas for Android.

## Current Status

See [MTA-ANDROID-PROJECT-SUMMARY.md](../../MTA-ANDROID-PROJECT-SUMMARY.md) for current development status.

**Quick Status:** Phase 7f COMPLETE - Remote players fully working with stable position sync (CPed::Teleport fix)!

## Quick Start

### Prerequisites

- macOS, Linux, or Windows with WSL
- Android SDK 34+ (via Android Studio or command line)
- Android NDK r26+
- Java JDK 17+ (JDK 25 supported with Gradle 9.2.1)
- apktool (for APK injection)
- GTA:SA v2.10 APK with ARM64 support
- GTA:SA OBB files (main.8 + patch.8, ~2.4GB total)

### Build MTA Library (CMake Method - Recommended)

```bash
cd Client/android

# Set NDK path (find yours with: find /opt -name "android.toolchain.cmake")
NDK_ROOT="/opt/homebrew/share/android-commandlinetools/ndk/26.1.10909125"

# Configure for ARM64 (for GTA SA v2.10 with ARM64 libs)
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$NDK_ROOT/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j8

# Output: build/libmta_android.so
```

### Build (Gradle Method - Test APK)

```bash
cd Client/android

# Create local.properties with SDK path
echo "sdk.dir=/opt/homebrew/share/android-commandlinetools" > local.properties

# Build debug APK
./gradlew assembleDebug

# Output APKs in: app/build/outputs/apk/debug/
```

### Inject MTA into GTA:SA APK

```bash
# 1. Decompile GTA:SA APK
apktool d "GTA SA 2.10.apk" -o /tmp/gtasa-decompiled -f

# 2. Copy MTA library to arm64-v8a folder
cp build/libmta_android.so /tmp/gtasa-decompiled/lib/arm64-v8a/

# 3. Patch smali to load MTA library
# Edit: /tmp/gtasa-decompiled/smali/com/rockstargames/gtasa/GTASA.smali
# Find the FIRST occurrence of:
#   const-string v0, "GTASA"
#   invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
#
# AFTER those two lines, add:
#   const-string v0, "mta_android"
#   invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

# 4. Rebuild APK
apktool b /tmp/gtasa-decompiled -o /tmp/gtasa-unsigned.apk

# 5. Align APK (find zipalign: find /opt -name "zipalign")
zipalign -f 4 /tmp/gtasa-unsigned.apk /tmp/gtasa-aligned.apk

# 6. Create debug keystore (only needed once)
keytool -genkeypair -alias android -keypass android \
  -keystore ~/.android/debug.keystore -storepass android \
  -dname "CN=Debug" -keyalg RSA -validity 9999

# 7. Sign APK
apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android \
  --out /tmp/gtasa-mta.apk /tmp/gtasa-aligned.apk

# Output: /tmp/gtasa-mta.apk
```

### Install on Device/Emulator

```bash
# For Genymotion: Use Genymotion's bundled ADB
ADB="/Applications/Genymotion.app/Contents/MacOS/tools/adb"

# For standard Android SDK:
# ADB="adb"

# List connected devices
$ADB devices

# Uninstall old version first (required - signatures won't match)
$ADB uninstall com.rockstargames.gtasa

# Install modified APK
$ADB install /tmp/gtasa-mta.apk
```

### Quick Iterative Development (Recommended Workflow)

Once you have the initial setup done (APK decompiled, smali patched, OBB files pushed), use this fast workflow for iterating on code changes. This preserves OBB files and doesn't require full uninstall/reinstall.

```bash
# === ONE-TIME SETUP (already done if you followed steps above) ===
# - APK decompiled to /tmp/gtasa-decompiled
# - Smali patched to load libmta_android.so
# - OBB files pushed to devices

# === QUICK BUILD & DEPLOY CYCLE ===

# 1. Rebuild library (incremental - fast)
cmake --build /Users/salimtrouve/Documents/GitHub/mtasa-blue/Client/android/build -j8

# 2. Copy new library to decompiled APK
cp /Users/salimtrouve/Documents/GitHub/mtasa-blue/Client/android/build/libmta_android.so /tmp/gtasa-decompiled/lib/arm64-v8a/

# 3. Rebuild APK (uses cached resources - fast)
apktool b /tmp/gtasa-decompiled -o /tmp/gtasa-unsigned.apk

# 4. Align APK
/opt/homebrew/share/android-commandlinetools/build-tools/34.0.0/zipalign -f 4 /tmp/gtasa-unsigned.apk /tmp/gtasa-aligned.apk

# 5. Sign APK
/opt/homebrew/share/android-commandlinetools/build-tools/34.0.0/apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android --out /tmp/gtasa-mta.apk /tmp/gtasa-aligned.apk 2>/dev/null

# 6. Install with -r (reinstall, preserves data including OBB reference)
/Applications/Genymotion.app/Contents/MacOS/tools/adb -s 127.0.0.1:6555 install -r /tmp/gtasa-mta.apk
/Applications/Genymotion.app/Contents/MacOS/tools/adb -s 127.0.0.1:6562 install -r /tmp/gtasa-mta.apk

# 7. Force stop and relaunch
/Applications/Genymotion.app/Contents/MacOS/tools/adb -s 127.0.0.1:6555 shell am force-stop com.rockstargames.gtasa
/Applications/Genymotion.app/Contents/MacOS/tools/adb -s 127.0.0.1:6562 shell am force-stop com.rockstargames.gtasa
/Applications/Genymotion.app/Contents/MacOS/tools/adb -s 127.0.0.1:6555 shell am start -n com.rockstargames.gtasa/.GTASA
/Applications/Genymotion.app/Contents/MacOS/tools/adb -s 127.0.0.1:6562 shell am start -n com.rockstargames.gtasa/.GTASA
```

**Key Points:**
- Use `install -r` (reinstall) instead of uninstall+install - preserves OBB files
- OBB files only need to be pushed once per device (unless you uninstall)
- The decompiled APK in /tmp/gtasa-decompiled is reused - apktool caches resources
- Build time is typically 5-10 seconds for the full cycle

### Push OBB Files (Required for Game Assets)

GTA:SA requires OBB files (~2.4GB total) for game assets. These are NOT included in the APK.

**Where to get OBB files:**
- If you purchased GTA:SA from Play Store, they're downloaded to:
  `/sdcard/Android/obb/com.rockstargames.gtasa/`
- Backup from an existing installation before modifying
- Or extract from a legitimate backup

**OBB file names:**
- `main.8.com.rockstargames.gtasa.obb` (~2.2GB)
- `patch.8.com.rockstargames.gtasa.obb` (~200MB)

**Note:** v1.08 OBB files work with v2.10 APK.

```bash
# Create OBB directory on device
$ADB shell mkdir -p /sdcard/Android/obb/com.rockstargames.gtasa/

# Push OBB files (replace SOURCE_PATH with your OBB location)
SOURCE_PATH="/path/to/your/obb/files"
$ADB push "$SOURCE_PATH/main.8.com.rockstargames.gtasa.obb" /sdcard/Android/obb/com.rockstargames.gtasa/
$ADB push "$SOURCE_PATH/patch.8.com.rockstargames.gtasa.obb" /sdcard/Android/obb/com.rockstargames.gtasa/

# Verify files are present
$ADB shell ls -la /sdcard/Android/obb/com.rockstargames.gtasa/
```

### Launch and Verify

```bash
# Launch GTA:SA
$ADB shell am start -n com.rockstargames.gtasa/.GTASA

# Watch MTA logs (should see "MTA:SA Android initialized successfully!")
$ADB logcat -s MTA:SA MTA-Core MTA-Connection MTA-RakNet

# Expected output:
# I MTA:SA  : ============================================
# I MTA:SA  : MTA:SA Android - Native Client
# I MTA:SA  : Version: 1.6.0
# I MTA:SA  : ============================================
# I MTA:SA  : MTA:SA Android initialized successfully!
```

---

## Genymotion Setup

Genymotion emulators require specific configuration:

### ADB Path

Genymotion uses its own ADB, not the Android SDK one:

```bash
# macOS
ADB="/Applications/Genymotion.app/Contents/MacOS/tools/adb"

# Add to your shell profile for convenience:
echo 'alias gadb="/Applications/Genymotion.app/Contents/MacOS/tools/adb"' >> ~/.zshrc
```

### Device Addresses

| Device | ADB Address |
|--------|-------------|
| Genymotion Device 1 | `127.0.0.1:6555` |
| Genymotion Device 2 | `127.0.0.1:6562` |

### Multi-Device Deployment

```bash
ADB="/Applications/Genymotion.app/Contents/MacOS/tools/adb"
APK="/tmp/gtasa-mta.apk"

# Deploy to both devices
$ADB -s 127.0.0.1:6555 install -r "$APK"
$ADB -s 127.0.0.1:6562 install -r "$APK"

# Push OBB to both (only needed once per device)
SOURCE_PATH="/path/to/your/obb/files"
for DEVICE in 127.0.0.1:6555 127.0.0.1:6562; do
  $ADB -s $DEVICE shell mkdir -p /sdcard/Android/obb/com.rockstargames.gtasa/
  $ADB -s $DEVICE push "$SOURCE_PATH/main.8.com.rockstargames.gtasa.obb" /sdcard/Android/obb/com.rockstargames.gtasa/
  $ADB -s $DEVICE push "$SOURCE_PATH/patch.8.com.rockstargames.gtasa.obb" /sdcard/Android/obb/com.rockstargames.gtasa/
done
```

---

## Server Connection Test

```bash
# The MTA library auto-connects to the test server on game launch
# Server: 37.59.101.35:22004

# Monitor connection in logs:
$ADB logcat -s MTA-Connection MTA-RakNet

# Expected flow:
# DISCONNECTED → RAKNET_HANDSHAKE → WAIT_MOD_NAME → SENDING_JOIN
# → WAIT_JOIN_COMPLETE → WAIT_JOINED_GAME → CONNECTED
```

---

## Output APKs

| File | Size | Architecture |
|------|------|--------------|
| `mta-android-*-arm64-v8a.apk` | ~10 MB | 64-bit ARM |
| `mta-android-*-armeabi-v7a.apk` | ~9 MB | 32-bit ARM |
| `mta-android-*-universal.apk` | ~16 MB | Both |

---

## Project Structure

```
Client/android/
├── MTAAndroidMain.cpp       # Entry point, auto-connect logic
├── CMakeLists.txt           # Native build configuration
├── build.gradle             # Root Gradle config
│
├── core/                    # Core integration
│   ├── CAndroidCore.h/cpp   # Main controller
│   └── CProfiler.h          # Performance profiler
│
├── java/com/mtasa/android/  # Java source
│   ├── MTAActivity.java     # Main Activity
│   ├── MTANative.java       # JNI declarations
│   └── test/TestActivity.java
│
├── platform/                # Platform abstraction
│   ├── AndroidInput.h/cpp   # Touch/gamepad input
│   ├── AndroidFileSystem.h/cpp
│   └── AndroidNetwork.h/cpp
│
├── graphics/                # OpenGL ES backend
│   ├── GLESGraphics.h/cpp   # GLES 3.0 renderer
│   ├── GLESShaders.h        # 17 GLSL ES shaders
│   └── RenderWareBridge.h/cpp
│
├── hooks/                   # ARM hook system
│   ├── ARMHookSystem.h
│   └── ARMHookInstaller.h
│
├── multiplayer/             # Player sync (Phase 7e-7f)
│   ├── CRemotePlayer.h      # Remote player with interpolation
│   ├── CPlayerManager.h     # Manages all remote players
│   └── CPedFactory.h        # Ped creation (MTA PC approach)
│
├── network/                 # Network protocol (Phase 7)
│   ├── CNetAndroid.h/cpp    # UDP networking
│   ├── CPacketHandler.h/cpp # Packet handling
│   ├── CServerConnection.h/cpp # Connection state machine
│   └── raknet/RakNetHandshake.h/cpp # MTA RakNet 3.x
│
├── game_sa/                 # GTA:SA game interface
│   ├── GTASAIntegration.h   # Game detection
│   ├── CPlayerSync.h        # Position sync
│   └── CGameBypass.h/cpp    # Menu bypass, auto-spawn
│
├── signatures/              # Address mapping
│   └── ARMAddressMap.h      # 200+ ARM addresses
│
├── tools/                   # Build tools
│   ├── inject-mta.sh        # APK injection script
│   └── mta_test_server.py   # Python test server
│
├── reference/               # Reference material
│   └── samp-android-reference/  # SA-MP ARM addresses & symbols
│
└── docs/                    # Documentation
    └── PHASE6_INTEGRATION.md  # Detailed integration guide
```

---

## Reverse Engineering ARM64 Addresses

For finding new ARM64 function addresses in libGTASA.so.

### Method 1: Symbol Export (Fastest)

GTA:SA Android's `libGTASA.so` has **exported symbols**:

```bash
# Extract the library from APK
unzip -j gtasa.apk lib/arm64-v8a/libGTASA.so -d /tmp/

# List all exported symbols with addresses
nm -D /tmp/libGTASA.so | grep -i "function_name"

# Examples:
nm -D /tmp/libGTASA.so | grep -i "FindPlayerPed"
# Output: 00000000004efae0 T _Z13FindPlayerPedi

nm -D /tmp/libGTASA.so | grep -i "Teleport"
# Output: 000000000059dd90 T _ZN4CPed8TeleportE7CVectorh
```

**Symbol name decoding (C++ mangling):**
- `_Z13FindPlayerPedi` → `FindPlayerPed(int)`
- `_ZN4CPed8TeleportE7CVectorh` → `CPed::Teleport(CVector, unsigned char)`
- Use `c++filt` to demangle: `c++filt _ZN4CPed8TeleportE7CVectorh`

### Method 2: SA-MP Symbol Dump

The SA-MP reference includes pre-extracted symbol dumps:

```bash
# Search the ARM64 symbol dump
grep "CPlayerPed" reference/samp-android-reference/dumps_libGTASA_32and64/DUMP\ 2.1\ \(64\).txt

# Format: index  paddr  vaddr  bind  type  size  lib  name  demangled
```

### Method 3: Ghidra (For Non-Exported Symbols)

If a function isn't exported, use Ghidra for static analysis.

### Key ARM64 Addresses (GTA:SA v2.10)

| Function | Address | Purpose |
|----------|---------|---------|
| `FindPlayerPed` | `0x4EFAE0` | Get player ped pointer |
| `CPed::Teleport` | `0x59DD90` | Teleport ped (updates world sectors) |
| `CPlaceable::SetMatrix` | `0x4EBF5C` | Set entity transform |
| `CPed::operator new` | `0x59576C` | Allocate ped memory |
| `CPlayerPed::CPlayerPed` | `0x5C0BAC` | Construct player ped |
| `CPlayerPed::SetInitialState` | `0x5C0D50` | Flush tasks, set idle state |
| `CPed::SetIdle` | `0x59DA8C` | Set idle animation |
| `CWorld::Add` | `0x507518` | Add entity to world |

### Ped Matrix Structure (Direct Memory Access)

```cpp
// Get ped pointer
void* pPed = FindPlayerPed(0);
uintptr_t pedAddr = (uintptr_t)pPed;

// Matrix pointer is at offset 0x18 from ped base
uintptr_t* pMatrixPtr = (uintptr_t*)(pedAddr + 0x18);
uintptr_t matrixAddr = *pMatrixPtr;

// Position is in the 4th column of the matrix
float* posX = (float*)(matrixAddr + 0x30);
float* posY = (float*)(matrixAddr + 0x34);
float* posZ = (float*)(matrixAddr + 0x38);

// Write new position
*posX = 2488.56f;  // Grove Street X
*posY = -1666.86f; // Grove Street Y
*posZ = 12.88f;    // Grove Street Z
```

---

## Tested Configurations

| Component | Version | Notes |
|-----------|---------|-------|
| macOS | 15.x (Apple Silicon) | Primary dev environment |
| Android SDK | 34 | Target API |
| Android NDK | r26.1.10909125 | ARM64 native |
| Java | JDK 25 | Via Homebrew |
| Gradle | 9.2.1 | Required for JDK 25 |
| AGP | 8.7.3 | Android Gradle Plugin |
| Genymotion | 3.x | ARM64 emulator |

---

## Troubleshooting

See [docs/PHASE6_INTEGRATION.md](docs/PHASE6_INTEGRATION.md) for detailed troubleshooting:
- Library not found errors
- Hook installation failures
- Version detection issues
- App crashes on launch

### Common Issues

**"INSTALL_FAILED_UPDATE_INCOMPATIBLE"**
```bash
# Uninstall the existing version first
$ADB uninstall com.rockstargames.gtasa
```

**Game shows black screen / crashes immediately**
- OBB files are missing or corrupted
- Verify OBB files are in correct location and have correct size

**MTA logs don't appear**
- Library not loading - check smali patch is correct
- Wrong architecture - ensure ARM64 library for ARM64 APK

---

## Documentation

| Document | Description |
|----------|-------------|
| [MTA-ANDROID-PROJECT-SUMMARY.md](../../MTA-ANDROID-PROJECT-SUMMARY.md) | Current status & next steps |
| [MTA-ANDROID-PROGRESS-LOG.md](../../MTA-ANDROID-PROGRESS-LOG.md) | Session history |
| [MTA-ANDROID-COMPLETED-PHASES.md](../../MTA-ANDROID-COMPLETED-PHASES.md) | Completed phase reference |
| [docs/PHASE6_INTEGRATION.md](docs/PHASE6_INTEGRATION.md) | Detailed GTA:SA integration guide |

---

## License

Same license as MTA:SA - see root LICENSE file.
