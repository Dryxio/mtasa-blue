# SA-MP Android Reference for MTA:SA Android

> Extracted from: https://github.com/kuzia15/SA-MP-2.10
> Local clone: `Client/android/reference/samp-android-reference/`
> Date: January 9, 2026

This document contains ARM addresses and techniques discovered from the SA-MP Android project that can accelerate our MTA:SA Android port.

---

## Why This is Valuable

The SA-MP 2.10 Android project has:
1. **Already mapped ARM addresses** for GTA SA Android (32-bit AND 64-bit)
2. **Working ARM hook library** (`armhook/patch.h`)
3. **Tested on actual Android devices**
4. **Both architectures**: `armeabi-v7a` (32-bit) and `arm64-v8a` (64-bit)

---

## ARM Address Mappings

### Global Game Addresses

| Purpose | ARM32 | ARM64 | Notes |
|---------|-------|-------|-------|
| CCamera instance | 0x951FA8 | 0xBBA8D0 | Main camera |
| g_fx pointer | 0x820520 | 0xA062A8 | Effects system |
| HUD visibility | 0x819D88 | 0x9FF3A8 | HUD flag |
| ms_fTimeStep | 0x96B500 | - | Time step |
| ms_fAspectRatio | 0xA26A90 | 0xCC7F00 | Aspect ratio |
| Water shader flag | 0x6B7094 | 0x8944A8 | Water rendering |
| World players ptr | 0x6783C0 | 0x84E7A8 | Player world |
| Player in focus | 0x679B5C | 0x8516D8 | Current player |

### PLT Hook Addresses (32-bit)

| Function | ARM32 PLT |
|----------|-----------|
| CCamera::Process | 0x6717BC |
| CPed::UpdatePosition | 0x671458 |
| FindPlayerSpeed | 0x671BBC |
| CPed::GetWeaponSkill | 0x6749D0 |
| CTaskComplexLeaveCar | 0x671984 |
| CVehicleModelInfo::SetupCommonData | 0x674280 |
| CAnimManager::UncompressAnimation | 0x6750D4 |
| CRadar::ClearBlip | 0x66FF0C |
| RwFrameAddChild | 0x675490 |
| RwTextureDestroy | 0x67332C |
| CCustomRoadsignMgr::RenderRoadsignAtomic | 0x66F5AC |
| rqVertexBufferSelect | 0x677498 |
| rqVertexBufferDelete | 0x679B14 |

### Vehicle Functions

| Function | ARM32 | ARM64 |
|----------|-------|-------|
| CAutomobile::Fix | 0x55D5C0 | 0x67DF0C |
| CAutomobile::SetupDamageAfterLoad | 0x55D886 | 0x67E368 |

### Player/Ped Functions

| Function | ARM32 | ARM64 |
|----------|-------|-------|
| CPlayerPed::SetupPlayerPed | 0x4C39A5 | 0x5C0FD4 |
| CPlayerPed::DeactivatePlayerPed | 0x4C3AD5 | 0x5C1140 |
| CPlayerPed::ReactivatePlayerPed | 0x4C3AED | 0x5C1158 |
| CPlayerPed::SetInitialState | 0x4C37B5 | 0x5C0D50 |
| CPed::GetTransformedBonePosition | 0x4A24A9 | 0x598670 |
| CPed::ClearWeapons | 0x49F837 | 0x595604 |
| CPopulation::RemovePed | 0x4CE6A1 | 0x5CDC64 |
| CPed task crouch apply | 0x4C07B1 | 0x5BCE70 |
| CPed task crouch reset | 0x4C08A9 | 0x5BCFF8 |

### Animation Functions

| Function | ARM32 | ARM64 |
|----------|-------|-------|
| RpAnimBlendClumpGetAssociation | 0x390A25 | 0x46AAF4 |
| RpAnimBlendClumpUpdateAnimations | 0x38BF01 | - |

### Weapon Functions

| Function | ARM32 | ARM64 |
|----------|-------|-------|
| CWeaponInfo::GetWeaponInfo | 0x5E42E9 | 0x709BA8 |
| SetWeaponCurrent | 0x4A51AD | - |

### Script Functions

| Function | ARM32 | ARM64 |
|----------|-------|-------|
| CTheScripts::ClearSpaceForMissionEntity | 0x34DA35 | 0x419BE0 |
| CCheat::JetpackCheat | 0x2FE259 | 0x3C2A40 |

### World/Collision

| Function | ARM32 | ARM64 |
|----------|-------|-------|
| CWorld::ProcessLineOfSight (caller) | 0x5DD0B0 | 0x70253C |

---

## Patch Addresses (Memory Modifications)

### FPS Patches

| Purpose | ARM32 | ARM64 |
|---------|-------|-------|
| FPS limit 1 | 0x5E49E0 | 0x70A38C |
| FPS limit 2 | 0x5E492E | 0x70A43C |
| FPS limit 3 | - | 0x70A458 |

### Map/Radar Patches

| Purpose | ARM32 | ARM64 |
|---------|-------|-------|
| Map unlock zones | 0x98D252 | 0xC1BF92 |
| Zones revealed counter | 0x98D2B8 | 0xC1BFF8 |
| Radar blip draw 1 | 0x43FE5A | 0x52522C |
| Radar blip draw 2 | 0x4409AE | 0x525E14 |
| Radar coord blip color 1 | 0x43FB5E | 0x524F58 |
| Radar coord blip color 2 | 0x43FB86 | 0x524E88 |
| Radar entity blip color 1 | 0x4404C0 | 0x5258D8 |
| Radar entity blip color 2 | 0x440538 | 0x525960 |
| Map legend text | 0x2ABA08 | 0x36A6E8 |
| Map legend icons | 0x2ABA14 | 0x36A6F8 |
| Area name display | 0x2AB4A6 | 0x36A190 |

### Audio Patches

| Purpose | ARM32 | ARM64 |
|---------|-------|-------|
| Vehicle audio disable 1 | 0x553E96 | 0x674610 |
| Vehicle audio disable 2 | 0x561AC2 | 0x682C1C |
| Vehicle audio disable 3 | 0x56BED4 | 0x68DD0C |

### Vehicle Patches

| Purpose | ARM32 | ARM64 |
|---------|-------|-------|
| Gun equip on exit 1 | 0x584884 | 0x6A852C |
| Gun equip on exit 2 | 0x584850 | 0x6A84E0 |
| Vehicle engine light 1 | 0x591272 | - |
| Vehicle engine light 2 | 0x59128E | - |

### Other Patches

| Purpose | ARM32 | ARM64 |
|---------|-------|-------|
| Alpha raster fix | 0x1AE8DE | 0x23FDE0 |
| Black icon render 1 | 0x442120 | 0x52737C |
| Black icon render 2 | 0x44217C | 0x5273F4 |
| Save filename | 0x6B012C | 0x88CB08 |
| Sun reflection crash | 0x3F61B6 | 0x4D8700 |
| Camera weapon mode | 0x4C5902 | 0x5C3258 |
| Streaming shutdown | 0x3F395E | - |
| Ped spawn optimization | 0x3F4138 | 0x4D644C |
| Player ped task fix | 0x4C36E2 | 0x5C0BC4 |
| Message system hook | 0x40BF26 | - |

---

## ARM Hook Library (armhook/patch.h)

### Key Functions

```cpp
// Inline function hooking using shadowhook
template<typename RET, typename... Args>
void InlineHook(const char* sym, RET(*hook)(Args...), RET(**orig)(Args...));

// PLT hooking (Procedure Linkage Table)
void InstallPLT(void* addr, void* func, void** orig = nullptr);

// Redirect function to another
void Redirect(const char* sym, void* to);

// Memory write with cache coherency
template<typename T>
void Write(uintptr_t addr, T value);

// Fill with NOPs
void NOP(uintptr_t addr, size_t size);

// Make function return immediately
void RET(uintptr_t addr);

// Prepare memory for modification
void UnFuck(uintptr_t addr, size_t size);
```

### Architecture Macros

```cpp
// Thumb mode handling (32-bit only)
#define DETHUMB(x)  ((x) & ~1)      // Clear Thumb bit
#define RETHUMB(x)  ((x) | 1)       // Set Thumb bit
#define THUMBMODE(x) ((x) & 1)      // Check if Thumb

// Get link register for offset calculation
#define GET_LR()  // Returns LR value

// Architecture detection
#define __32BIT  // Defined for ARM32
#define __64BIT  // Defined for ARM64

// NOP instructions
// ARM32: 0x00BF (Thumb NOP)
// ARM64: 0xD5032003 (NOP)
```

---

## How to Use This for MTA:SA Android

### 1. Address Mapping Strategy

SA-MP uses **g_libGTASA** base address + offset pattern:
```cpp
uintptr_t g_libGTASA;  // Base address of libGTASA.so

// Access function at offset
auto func = (void*)(g_libGTASA + 0x55D5C0);
```

### 2. Hook Installation Pattern

```cpp
// PLT hook example
CHook::InstallPLT(
    (void*)(g_libGTASA + 0x6717BC),  // CCamera::Process PLT
    (void*)CCamera_Process_Hook,
    (void**)&CCamera_Process_Orig
);

// Inline hook example
CHook::InlineHook(
    "CPlayerPed::SetupPlayerPed",
    SetupPlayerPed_Hook,
    &SetupPlayerPed_Orig
);
```

### 3. Architecture Handling

```cpp
#ifdef VER_x32
    // Use ARM32 addresses
    uintptr_t addr = 0x4C39A4 + 1;  // +1 for Thumb
#else
    // Use ARM64 addresses
    uintptr_t addr = 0x5C0FD4;
#endif
```

---

## MTA Hook → ARM Address Mapping (Partial)

Based on function names, here's a partial mapping of MTA x86 hooks to SA-MP ARM addresses:

| MTA Hook (x86) | x86 Addr | ARM32 (SA-MP) | ARM64 (SA-MP) |
|----------------|----------|---------------|---------------|
| CCamera::Process | 0x52B730 | PLT: 0x6717BC | - |
| CPed::GetWeaponSkill | 0x5E3B60 | PLT: 0x6749D0 | - |
| CAutomobile::Fix | - | 0x55D5C0 | 0x67DF0C |
| RpAnimBlendClumpGetAssociation | 0x4D68B0 | 0x390A24 | 0x46AAF4 |
| CWeaponInfo::GetWeaponInfo | - | 0x5E42E8 | 0x709BA8 |
| CWorld::ProcessLineOfSight | 0x56BA00 | caller: 0x5DD0B0 | 0x70253C |

**Note**: More mappings need to be created by cross-referencing:
1. MTA x86 addresses with GTA-Reversed function names
2. GTA-Reversed function names with SA-MP ARM addresses

---

## Repository Structure Reference

```
SA-MP-2.10/app/src/main/cpp/samp/
├── game/
│   ├── game.cpp/h         # Core game interface
│   ├── hooks.cpp/h        # Hook installation
│   ├── patches.cpp        # Memory patches
│   ├── vehicle.cpp/h      # Vehicle functions
│   ├── playerped.cpp/h    # Player/ped functions
│   ├── MemoryMgr.cpp/h    # Memory management
│   └── ...
├── vendor/
│   ├── armhook/
│   │   ├── patch.h        # ARM hooking interface
│   │   └── patch.cpp      # Implementation
│   └── raknet/            # Networking (same as MTA)
└── dumps_libGTASA_32and64/
    ├── dump 2.1 (32).txt  # ARM32 address dump
    └── DUMP 2.1 (64).txt  # ARM64 address dump
```

---

## Conclusion

**This SA-MP 2.10 repository is extremely valuable because:**

1. **Pre-mapped ARM addresses** - We don't need to reverse engineer from scratch
2. **Both architectures** - ARM32 for older devices, ARM64 for modern
3. **Working hook library** - Tested `armhook` implementation
4. **Similar architecture** - SA-MP and MTA:SA have similar needs

**Action Items:**
1. ✅ Extract ARM addresses (done - this document)
2. Cross-reference with MTA x86 hooks
3. Adapt armhook library or use as reference
4. Test on actual Android device with GTA SA

---

*Reference document for MTA:SA Android port project*
