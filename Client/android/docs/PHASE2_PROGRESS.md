# Phase 2: Hook Migration - Progress Report

> Started: January 9, 2026
> Status: **In Progress**

---

## Summary

Phase 2 focuses on mapping x86 Windows addresses to ARM Android addresses using GTA-Reversed as the reference.

### Key Findings

| Resource | Details |
|----------|---------|
| **GTA-Reversed** | 25,259 address references across 1,412 files |
| **MTA Hooks** | 400+ hooks extracted from multiplayer_sa |
| **Address Database** | Created with 200+ categorized entries |

### Recent Updates

- Re-enabled CPad hooks with proper remote ped slot mapping; remote animation no longer mirrors local movement.
- Verified dead pose regression is resolved with hooks enabled.
- Added derived input fallback to drive remote animations; current orientation/heading still incorrect.
- Next step: port MTA PC remote anim/heading rules and full keysync parsing (CNetAPI::ReadPlayerPuresync/ReadFullKeysync) into Android to replace the derived-input hack.

---

## Completed Tasks

### 1. GTA-Reversed Analysis ✅

Location: `/Users/salimtrouve/Documents/GitHub/gta-reversed-dryxio`

**Structure discovered:**
```
gta-reversed/source/game_sa/
├── Entity/
│   ├── Entity.h/cpp         # CEntity (0x38 bytes)
│   ├── Physical.h/cpp       # CPhysical
│   ├── Vehicle/             # All vehicle types
│   │   ├── Vehicle.h/cpp    # CVehicle (0x5A0 bytes)
│   │   ├── Automobile.h/cpp # 245KB of code
│   │   ├── Bike.h/cpp
│   │   ├── Boat.h/cpp
│   │   ├── Plane.h/cpp
│   │   ├── Heli.h/cpp
│   │   └── Train.h/cpp
│   └── Ped/
│       ├── Ped.h/cpp
│       └── PlayerPed.h/cpp
├── Camera.h/cpp             # 700+ functions with addresses
├── Streaming.h/cpp
├── Animation/
├── Tasks/
├── Weapon/
└── [560+ subdirectories]
```

**Address format:** `// 0xADDRESS` inline comments

### 2. MTA Hook Extraction ✅

Extracted 400+ hooks from `/Users/salimtrouve/Documents/GitHub/mtasa-blue/Client/multiplayer_sa/`:

| Category | Hook Count | Source Files |
|----------|------------|--------------|
| Core Game | 15+ | CMultiplayerSA.cpp |
| Entity System | 20+ | CMultiplayerSA_*.cpp |
| Vehicle System | 50+ | CMultiplayerSA_Vehicle*.cpp |
| Vehicle VTables | 11 | multiplayer_keysync.h |
| Pedestrian | 18+ | CMultiplayerSA_Peds.cpp |
| Weapons | 22+ | multiplayer_shotsync.* |
| Rendering | 25+ | CMultiplayerSA_Rendering.cpp |
| Camera | 13+ | CMultiplayerSA.cpp |
| World/Collision | 13+ | CMultiplayerSA.cpp |
| Explosions/Fire | 9+ | CMultiplayerSA_Explosions.cpp |
| Animation | 16+ | CMultiplayerSA_CustomAnimations.cpp |
| Tasks | 17+ | CMultiplayerSA_Tasks.cpp |
| Audio | 9+ | CMultiplayerSA_1.3.cpp |
| FX System | 6+ | CMultiplayerSA.cpp |
| RenderWare | 9+ | CMultiplayerSA_RwResources.cpp |
| Crash Fixes | 65+ | CMultiplayerSA_CrashFixHacks.cpp |
| Frame Rate | 30+ | CMultiplayerSA_FrameRateFixes.cpp |

### 3. Address Database Created ✅

File: `Client/android/signatures/AddressDatabase.h`

**Categories implemented:**
- CORE_HOOKS (15 entries)
- ENTITY_HOOKS (20 entries)
- VEHICLE_HOOKS (45 entries)
- VEHICLE_VTABLES (11 entries)
- PED_HOOKS (18 entries)
- WEAPON_HOOKS (22 entries)
- RENDERING_HOOKS (25 entries)
- CAMERA_HOOKS (13 entries)
- WORLD_HOOKS (13 entries)
- EXPLOSION_HOOKS (9 entries)
- ANIMATION_HOOKS (16 entries)
- TASK_HOOKS (17 entries)
- IK_HOOKS (3 entries)
- AUDIO_HOOKS (9 entries)
- FX_HOOKS (6 entries)
- RENDERWARE_HOOKS (9 entries)
- GLASS_HOOKS (6 entries)
- WATER_HOOKS (7 entries)
- D3D_HOOKS (3 entries)
- FILE_HOOKS (6 entries)

**Total: ~270 categorized hooks**

---

### 4. Critical Hook Signatures (ARM32/ARM64) ✅

Signature patterns have been added for the top critical hooks using SA-MP 2.10
`libGTASA.so` references for both ARM32 and ARM64.

**Hooks covered:**
- CEntity::Render
- CGame::Process
- CPed::ProcessControl
- CAutomobile::ProcessControl
- CWeapon::Fire
- CCamera::Process
- CStreaming::RequestModel
- CWorld::ProcessLineOfSight
- CPlayerPed::SetupPlayerPed
- CPlayerPed::SetInitialState
- CPlayerPed::DeactivatePlayerPed
- CPlayerPed::ReactivatePlayerPed
- CPed::ClearWeapons
- CPed::SetModelIndex
- CPed::GetWeaponSkill
- CRenderer::RenderOneNonRoad
- CEntity::CreateRwObject
- CEntity::DeleteRwObject

**Implementation:** `Client/android/signatures/SignatureScanner.h`

---

## In Progress

### 4. Signature Pattern Generation (Remaining)

For ARM address resolution, we need byte patterns. These can be derived from:

1. **String references** - Find functions by strings they use
2. **Constant values** - Unique numeric constants
3. **Call patterns** - Sequence of function calls
4. **Structure access** - Offsets into known structures

**Priority hooks for signature generation:**

| Hook | x86 Address | Priority | Reason |
|------|-------------|----------|--------|
| CEntity::Render | 0x534310 | Critical | Core rendering |
| CVehicle::ProcessControl | 0x6B1880 | Critical | Vehicle sync |
| CPed::ProcessControl | 0x60EA90 | Critical | Ped sync |
| CWeapon::Fire | 0x742300 | Critical | Weapon sync |
| CCamera::Process | 0x52B730 | Critical | Camera sync |
| CGame::Process | 0x53BEE0 | Critical | Main loop |
| CStreaming::RequestModel | 0x4087E0 | High | Model loading |
| CWorld::ProcessLineOfSight | 0x56BA00 | High | Collision |

---

## Next Steps

### Immediate (Today)
1. Expand signatures beyond the first critical set (remaining top 20 hooks)
2. Cross-reference with GTA-Reversed source for validation
3. Test pattern matching approach

### Short-term (This Week)
1. Obtain Android GTA:SA binary (libGTASA.so)
2. Run signature scanner against ARM binary
3. Validate first batch of address mappings
4. Begin hook migration for core functions

### Medium-term (Next Week)
1. Complete all hook address mappings
2. Port first hooks to ARM assembly
3. Test basic injection on Android device

---

## GTA-Reversed Cross-Reference

### How to use GTA-Reversed for mapping:

```bash
# Find function by x86 address
grep -r "// 0x534310" /Users/salimtrouve/Documents/GitHub/gta-reversed-dryxio/source/

# Result: game_sa/Entity/Entity.cpp - CEntity::Render

# Get function signature
grep -A20 "CEntity::Render" /Users/salimtrouve/Documents/GitHub/gta-reversed-dryxio/source/game_sa/Entity/Entity.h
```

### Key GTA-Reversed files for MTA hooks:

| MTA Hook Category | GTA-Reversed Path |
|-------------------|-------------------|
| Entity | `source/game_sa/Entity/Entity.cpp` |
| Vehicle | `source/game_sa/Entity/Vehicle/` |
| Ped | `source/game_sa/Entity/Ped/` |
| Camera | `source/game_sa/Camera.cpp` |
| Weapon | `source/game_sa/Weapon/` |
| Animation | `source/game_sa/Animation/` |
| Tasks | `source/game_sa/Tasks/` |
| Streaming | `source/game_sa/Streaming.cpp` |
| World | `source/game_sa/World.cpp` |
| Collision | `source/game_sa/Collision/` |

---

## Files Created in Phase 2

```
Client/android/signatures/
├── SignatureScanner.h      # (Phase 1) Pattern scanner
├── AddressDatabase.h       # (Phase 2) 270+ hook addresses
├── ARMAddressMap.h         # (Phase 2) ARM32/ARM64 resolved addresses
└── ScannerTest.cpp         # (Phase 1) Unit tests

Client/android/docs/
├── ARM_HOOK_PATTERNS.md    # (Phase 1) Migration guide
├── SAMP_ANDROID_REFERENCE.md # (Phase 2) ARM address reference
└── PHASE2_PROGRESS.md      # (Phase 2) This file

Client/android/reference/
└── samp-android-reference/  # (Phase 2) SA-MP 2.10 clone
    ├── app/src/main/cpp/samp/
    │   ├── game/            # Game hooks and patches
    │   └── vendor/armhook/  # ARM hook library
    └── dumps_libGTASA_32and64/
        ├── dump 2.1 (32).txt  # 22,022 ARM32 addresses
        └── DUMP 2.1 (64).txt  # 59,562 ARM64 addresses
```

---

## Statistics

| Metric | Count |
|--------|-------|
| Total hooks identified | 400+ |
| Hooks in database | 270 |
| Categories | 20 |
| With GTA-Reversed ref | ~80 |
| SA-MP ARM32 addresses | 22,022 |
| SA-MP ARM64 addresses | 59,562 |
| **ARM addresses mapped** | **200+** |
| ARMAddressMap.h entries | 200+ (ARM32) + 25+ (ARM64) |

---

*Phase 2 progress as of January 9, 2026*
