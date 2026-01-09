# Phase 6: GTA:SA Integration

This document describes how to integrate MTA:SA Android with the actual GTA:SA Android game.

## Overview

Phase 6 bridges MTA's infrastructure with the real GTA:SA Android application. This involves:

1. **Library Injection** - Getting `libmta_android.so` loaded into GTA:SA's process
2. **Game Detection** - Finding and validating `libGTASA.so` in memory
3. **Hook Installation** - Installing ARM hooks to intercept game functions
4. **Proof of Concept** - Demonstrating that hooks work (god mode, etc.)

## Prerequisites

- Completed Phases 1-5 (build should succeed)
- GTA:SA Android APK (purchased from Play Store)
- Android device or emulator with ARM support
- Development tools:
  - `apktool` for APK modification
  - `zipalign` and `apksigner` from Android SDK
  - ADB for deployment

## Integration Approaches

### Option A: Modified APK (Recommended, No Root)

This approach modifies the GTA:SA APK to include our native library.

**Pros:**
- Works on any device
- No root required
- Permanent installation

**Cons:**
- Need to re-modify after GTA:SA updates
- Won't pass Play Store integrity checks

#### Steps:

1. **Build MTA Library**
   ```bash
   cd Client/android
   ./gradlew assembleDebug
   ```

2. **Run Injection Script**
   ```bash
   ./tools/inject-mta.sh /path/to/GTA_SA.apk -o gtasa-mta.apk
   ```

3. **Install Modified APK**
   ```bash
   # Uninstall original first (if needed)
   adb uninstall com.rockstargames.gtasa

   # Install modified version
   adb install gtasa-mta.apk
   ```

4. **Launch and Verify**
   ```bash
   adb shell am start -n com.rockstargames.gtasa/.GTA
   adb logcat -s MTA-GTASA MTA-Hooks
   ```

### Option B: Xposed/LSPosed Module (Root Required)

For development, Xposed allows runtime injection without APK modification.

1. Install LSPosed (requires Magisk root)
2. Create MTA Xposed module
3. Enable module for GTA:SA
4. Launch game normally

### Option C: Frida (Root Required, Development Only)

For quick testing during development:

```bash
# Start Frida server on device
adb shell su -c "/data/local/tmp/frida-server &"

# Inject into GTA:SA
frida -U -f com.rockstargames.gtasa -l mta_inject.js
```

## Manual APK Modification

If the automatic script doesn't work, follow these manual steps:

### 1. Decompile APK

```bash
apktool d GTA_SA.apk -o gtasa_decompiled
```

### 2. Add Native Library

Copy `libmta_android.so` to the appropriate architecture folder:

```bash
# For ARM64
cp libmta_android.so gtasa_decompiled/lib/arm64-v8a/

# For ARM32
cp libmta_android.so gtasa_decompiled/lib/armeabi-v7a/
```

### 3. Patch Smali to Load Library

Find the main activity (usually `smali/com/rockstargames/gtasa/GTA.smali`) and add library loading.

**Option A: Static Initializer**

Add to the `.method static constructor <clinit>()V` section:

```smali
.method static constructor <clinit>()V
    .locals 1

    # Load MTA library
    const-string v0, "mta_android"
    invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

    # ... rest of static initializer
    return-void
.end method
```

**Option B: onCreate Method**

Add at the start of `.method protected onCreate`:

```smali
.method protected onCreate(Landroid/os/Bundle;)V
    .locals 2

    # Load MTA library
    const-string v0, "mta_android"
    invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

    # ... rest of onCreate
.end method
```

### 4. Recompile and Sign

```bash
# Recompile
apktool b gtasa_decompiled -o gtasa_unsigned.apk

# Align
zipalign -f -p 4 gtasa_unsigned.apk gtasa_aligned.apk

# Create debug keystore (if needed)
keytool -genkey -v -keystore debug.keystore -alias debug \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -storepass android -keypass android \
    -dname "CN=Debug"

# Sign
apksigner sign --ks debug.keystore --ks-pass pass:android gtasa_aligned.apk

# Rename
mv gtasa_aligned.apk gtasa-mta.apk
```

## Verification

After installation, verify integration is working:

### Check Logs

```bash
adb logcat -s MTA-GTASA MTA-Hooks MTA-Core
```

Expected output:
```
I/MTA-GTASA: =============================================
I/MTA-GTASA: MTA:SA Android - GTA:SA Integration
I/MTA-GTASA: Phase 6 - Proof of Concept
I/MTA-GTASA: =============================================
I/MTA-GTASA: Found game library:
I/MTA-GTASA:   Base: 0x7a12340000
I/MTA-GTASA:   Size: 18432000 bytes (17.58 MB)
I/MTA-GTASA:   Arch: ARM64
I/MTA-GTASA: Detected version: 2.11 (64-bit)
I/MTA-GTASA: Library validation passed
I/MTA-GTASA: GTA:SA integration initialized successfully!
```

### Test Proof-of-Concept Features

The integration includes test features:

1. **God Mode** - Player invincibility
2. **Status Reporting** - Debug info via JNI

Use the MTA Loader app or ADB to toggle features:

```bash
# View integration status
adb shell am broadcast -a com.mtasa.android.STATUS

# Enable god mode (requires in-app button or native call)
```

## Code Architecture

### Integration Flow

```
1. JNI_OnLoad()
   └── MTA::Android::Initialize()

2. Game starts, loads libGTASA.so

3. MTA detects libGTASA.so
   └── GTASA::Initialize()
       ├── FindGameLibrary() - Scan /proc/self/maps
       ├── DetectVersion() - Identify GTA:SA version
       ├── ValidateGameLibrary() - Verify it's GTA:SA
       └── InstallPoChooks() - Install proof-of-concept hooks

4. Game loop runs
   └── Hook_CGame_Process()
       └── ApplyGodMode() - Periodically set health to max
```

### Key Files

| File | Description |
|------|-------------|
| `game_sa/GTASAIntegration.h` | Main integration module |
| `hooks/ARMHookInstaller.h` | ARM hook installation API |
| `signatures/ARMAddressMap.h` | ARM function addresses |
| `java/.../GTASALoaderActivity.java` | Launcher UI |
| `jni/MTANative.cpp` | JNI integration methods |
| `tools/inject-mta.sh` | APK injection script |

## Troubleshooting

### Library Not Found

```
E/MTA-GTASA: Game library not found in memory
```

**Solution:** MTA library is loading before GTA:SA. Ensure library load order is correct.

### Hook Installation Failed

```
E/MTA-Hooks: mprotect failed for 0x...
```

**Solution:** Memory protection issue. May need to:
1. Check SELinux status: `adb shell getenforce`
2. Try with root: `adb shell su -c setenforce 0`
3. Use alternative hook methods

### Version Detection Failed

```
W/MTA-GTASA: Could not determine exact version
```

**Solution:** Using heuristic detection. Hooks may need manual offset adjustment for non-2.10/2.11 versions.

### App Crashes on Launch

**Solutions:**
1. Check for native crash in logcat: `adb logcat -s DEBUG`
2. Verify library architecture matches APK (ARM32 vs ARM64)
3. Test without hooks: Comment out `InstallPoChooks()` call
4. Check hook offsets are correct for game version

## Next Steps

After successful Phase 6 integration:

1. **Phase 7: Multiplayer Logic**
   - Implement player synchronization
   - Port MTA network protocol
   - Add server browser
   - Implement resource/Lua system

2. **Testing**
   - Test with multiple game versions
   - Verify hook stability during gameplay
   - Profile performance impact

## Resources

- [Android NDK Documentation](https://developer.android.com/ndk/guides)
- [APKTool Documentation](https://ibotpeaches.github.io/Apktool/)
- [ARM Architecture Reference](https://developer.arm.com/documentation)
- [Frida Hooking Framework](https://frida.re)

## Legal Notice

This project is intended for educational and research purposes. Users must own a legitimate copy of GTA:SA Android. Modifying APKs may violate terms of service.
