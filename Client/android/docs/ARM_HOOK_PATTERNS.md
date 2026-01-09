# ARM Hook Patterns for MTA:SA Android

This document describes the ARM hook patterns used in MTA:SA Android, providing equivalents to the x86 patterns used in the Windows version.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [ARM32 vs ARM64](#arm32-vs-arm64)
3. [Hook Types](#hook-types)
4. [Instruction Encoding](#instruction-encoding)
5. [Register Conventions](#register-conventions)
6. [Migration Guide: x86 to ARM](#migration-guide-x86-to-arm)
7. [Code Examples](#code-examples)

---

## Architecture Overview

### x86 (Windows MTA:SA)
- Variable-length instructions (1-15 bytes)
- CISC architecture
- Few registers (EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP)
- Direct memory operations

### ARM (Android MTA:SA)
- Fixed-length instructions (4 bytes ARM, 2/4 bytes Thumb)
- RISC architecture
- Many registers (R0-R15 for ARM32, X0-X30 for ARM64)
- Load/store architecture (no direct memory ops)

---

## ARM32 vs ARM64

### ARM32 (ARMv7-A) - Primary Target for GTA:SA Android

```
Registers:
  R0-R3   : Arguments and return value (caller-saved)
  R4-R11  : Local variables (callee-saved)
  R12 (IP): Intra-procedure scratch
  R13 (SP): Stack pointer
  R14 (LR): Link register (return address)
  R15 (PC): Program counter
```

### ARM64 (AArch64) - Future Support

```
Registers:
  X0-X7   : Arguments and return value
  X8      : Indirect result location
  X9-X15  : Temporary (caller-saved)
  X16-X17 : Intra-procedure call (platform use)
  X18     : Platform register
  X19-X28 : Callee-saved
  X29 (FP): Frame pointer
  X30 (LR): Link register
  SP      : Stack pointer
  PC      : Program counter (not directly accessible)
```

---

## Hook Types

### 1. Branch Hook (B instruction)

**Equivalent to x86 JMP (0xE9)**

```
x86:   E9 xx xx xx xx       (5 bytes: JMP rel32)
ARM32: EA xx xx xx          (4 bytes: B imm24)
ARM64: 14 xx xx xx          (4 bytes: B imm26)
```

**Installation:**
```cpp
// x86
MemPut<BYTE>(addr, 0xE9);
MemPut<DWORD>(addr + 1, target - addr - 5);

// ARM32
uint32_t branch = 0xEA000000 | ((target - addr - 8) >> 2) & 0x00FFFFFF;
MemPut<uint32_t>(addr, branch);
```

### 2. Call Hook (BL instruction)

**Equivalent to x86 CALL (0xE8)**

```
x86:   E8 xx xx xx xx       (5 bytes: CALL rel32)
ARM32: EB xx xx xx          (4 bytes: BL imm24)
ARM64: 94 xx xx xx          (4 bytes: BL imm26)
```

### 3. VTable Hook

**Same concept on both platforms - pointer replacement**

```cpp
// Both x86 and ARM
MemPut<uintptr_t>(vtable_addr + method_offset, hook_function);
```

### 4. NOP Filling

```
x86:   90                   (1 byte NOP)
ARM32: E1A00000             (4 bytes: MOV R0, R0)
ARM64: D503201F             (4 bytes: NOP)
```

---

## Instruction Encoding

### ARM32 Branch Instructions

```
B (Branch):
  31-28: Condition (0xE = always)
  27-24: 0b1010 (opcode)
  23-0:  Signed 24-bit offset (words, not bytes)

  Encoding: 0xEA000000 | (offset & 0x00FFFFFF)
  Range: ±32MB

BL (Branch with Link):
  Same as B but bits 27-24 = 0b1011
  Encoding: 0xEB000000 | (offset & 0x00FFFFFF)

Offset calculation:
  offset = (target - source - 8) >> 2
  The -8 accounts for PC being 2 instructions ahead
```

### ARM32 Data Processing

```
PUSH {regs}:
  Encoding: 0xE92D0000 | register_mask
  Example: PUSH {R4, R5, LR} = 0xE92D4030

POP {regs}:
  Encoding: 0xE8BD0000 | register_mask
  Example: POP {R4, R5, PC} = 0xE8BD8030
```

### ARM64 Branch Instructions

```
B (Branch):
  31-26: 0b000101 (opcode)
  25-0:  Signed 26-bit offset (words)

  Encoding: 0x14000000 | (offset & 0x03FFFFFF)
  Range: ±128MB

BL (Branch with Link):
  31-26: 0b100101 (opcode)
  Encoding: 0x94000000 | (offset & 0x03FFFFFF)

Offset calculation:
  offset = (target - source) >> 2
  No PC offset adjustment needed (unlike ARM32)
```

---

## Register Conventions

### Function Calls

```
x86 (cdecl):
  Arguments: Stack (right to left)
  Return: EAX
  Preserved: EBX, ESI, EDI, EBP

ARM32 (AAPCS):
  Arguments: R0-R3 (first 4), then stack
  Return: R0 (and R1 for 64-bit)
  Preserved: R4-R11

ARM64 (AAPCS64):
  Arguments: X0-X7 (first 8), then stack
  Return: X0 (and X1 for 128-bit)
  Preserved: X19-X28
```

### Register Preservation in Hooks

**x86 (pushad/popad):**
```asm
pushad          ; Save EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
; ... hook code ...
popad           ; Restore all
```

**ARM32 (STMFD/LDMFD):**
```asm
stmfd sp!, {r0-r12, lr}   ; Save R0-R12 and LR
; ... hook code ...
ldmfd sp!, {r0-r12, lr}   ; Restore all
```

**ARM64 (STP/LDP pairs):**
```asm
stp x0, x1, [sp, #-16]!   ; Save X0, X1
stp x2, x3, [sp, #-16]!   ; Save X2, X3
; ... repeat for all needed registers ...
; ... hook code ...
ldp x2, x3, [sp], #16     ; Restore X2, X3
ldp x0, x1, [sp], #16     ; Restore X0, X1
```

---

## Migration Guide: x86 to ARM

### Step-by-Step Hook Migration

1. **Identify the hook location**
   - x86: `HOOKPOS_CEntity_Render = 0x534310`
   - ARM: Use SignatureScanner to find equivalent

2. **Determine hook size**
   - x86: Varies (5-6+ bytes for JMP + displaced code)
   - ARM32: Usually 4 bytes (one instruction)
   - ARM64: Usually 4 bytes (one instruction)

3. **Calculate return address**
   - x86: `RETURN_CEntity_Render = HOOKPOS + HOOKSIZE`
   - ARM: Same concept, but align to instruction boundary

4. **Convert naked function**

**x86 Original:**
```cpp
static void __declspec(naked) HOOK_CEntity_Render()
{
    __asm
    {
        pushad
        call OnEntityRender
        popad

        // Original code
        mov eax, [ecx+14h]
        jmp RETURN_CEntity_Render
    }
}
```

**ARM32 Equivalent:**
```cpp
__attribute__((naked))
void HOOK_CEntity_Render()
{
    __asm__ __volatile__(
        "stmfd sp!, {r0-r12, lr}\n"
        "bl OnEntityRender\n"
        "ldmfd sp!, {r0-r12, lr}\n"

        // Original code (equivalent)
        "ldr r0, [r0, #0x14]\n"
        "b RETURN_CEntity_Render\n"
    );
}
```

### Common Patterns

| x86 Pattern | ARM32 Equivalent |
|-------------|------------------|
| `push ebp; mov ebp, esp` | `stmfd sp!, {fp, lr}; mov fp, sp` |
| `mov eax, [ecx]` | `ldr r0, [r0]` |
| `call func` | `bl func` |
| `ret` | `bx lr` or `ldmfd sp!, {..., pc}` |
| `jmp addr` | `b addr` |
| `test eax, eax; jz label` | `cmp r0, #0; beq label` |

---

## Code Examples

### Example 1: Simple Function Hook

**x86 Original:**
```cpp
#define HOOKPOS_CVehicle_BurstTyre 0x6A32B0
#define HOOKSIZE_CVehicle_BurstTyre 6

static void __declspec(naked) HOOK_CVehicle_BurstTyre()
{
    __asm
    {
        pushad
        push edi        // tire index
        push esi        // vehicle
        call OnVehicleBurstTyre
        add esp, 8
        test al, al
        jz skip
        popad
        push 1
        push edi
        mov ecx, ebx
        jmp RETURN_CVehicle_BurstTyre
    skip:
        popad
        ret
    }
}
```

**ARM32 Port:**
```cpp
ARM_HOOKPOS(CVehicle_BurstTyre, 0xXXXXXX)  // Found via signature
ARM_HOOKSIZE(CVehicle_BurstTyre, 4)
ARM_RETURN(CVehicle_BurstTyre, 0xXXXXXX)

__attribute__((naked))
void HOOK_CVehicle_BurstTyre()
{
    __asm__ __volatile__(
        // Save all registers
        "stmfd sp!, {r0-r12, lr}\n"

        // Call handler: OnVehicleBurstTyre(vehicle, tireIndex)
        // Assuming R0 = vehicle, R1 = tire index on entry
        "bl OnVehicleBurstTyre\n"

        // Check return value
        "cmp r0, #0\n"
        "beq skip\n"

        // Restore and continue
        "ldmfd sp!, {r0-r12, lr}\n"
        // Original code here...
        "b RETURN_CVehicle_BurstTyre\n"

    "skip:\n"
        // Restore and return early
        "ldmfd sp!, {r0-r12, lr}\n"
        "bx lr\n"
    );
}
```

### Example 2: Trampoline for Calling Original

```cpp
class TrampolineGenerator
{
public:
    // Generate trampoline that:
    // 1. Executes original displaced instructions
    // 2. Jumps back to original code after hook

    static void* CreateTrampoline(uintptr_t originalAddr, size_t instrSize)
    {
        // Allocate executable memory
        void* trampoline = mmap(nullptr, 32,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (!trampoline) return nullptr;

        uint32_t* code = static_cast<uint32_t*>(trampoline);

        // Copy original instructions
        memcpy(code, (void*)originalAddr, instrSize);

        // Add jump back
        uintptr_t returnAddr = originalAddr + instrSize;
        size_t instrCount = instrSize / 4;
        code[instrCount] = CreateARMBranch(
            reinterpret_cast<uintptr_t>(&code[instrCount]),
            returnAddr
        );

        // Clear cache
        __builtin___clear_cache(
            (char*)trampoline,
            (char*)trampoline + 32
        );

        return trampoline;
    }
};
```

---

## Thumb Mode Considerations

GTA:SA Android may use Thumb mode for some code. Key differences:

- Thumb addresses have bit 0 set (e.g., 0x1001 is Thumb, 0x1000 is ARM)
- Thumb instructions are 2 or 4 bytes
- Must use Thumb-compatible branch instructions
- BX instruction switches between ARM/Thumb modes

```cpp
// Check if address is Thumb
bool isThumb = (address & 1) != 0;

// Clear Thumb bit for actual address
uintptr_t realAddr = address & ~1;

// When calling Thumb code from ARM, use BX
// When hooking Thumb code, use Thumb instructions
```

---

## Memory Cache Considerations

ARM requires explicit cache maintenance after code modification:

```cpp
// After writing hook code, MUST clear instruction cache
__builtin___clear_cache(
    (char*)hookAddress,
    (char*)hookAddress + hookSize
);
```

This is critical - without cache clearing, the CPU may execute stale cached instructions.

---

## Summary: Key Differences

| Aspect | x86 | ARM32 | ARM64 |
|--------|-----|-------|-------|
| Instruction size | Variable | 4 bytes | 4 bytes |
| Branch range | 2GB | ±32MB | ±128MB |
| Branch opcode | 0xE9 | 0xEA | 0x14 |
| Call opcode | 0xE8 | 0xEB | 0x94 |
| NOP | 0x90 | 0xE1A00000 | 0xD503201F |
| Save all regs | pushad | stmfd | stp pairs |
| Restore all regs | popad | ldmfd | ldp pairs |
| Return | ret | bx lr | ret |
| Cache clear | Not needed | Required | Required |

---

*This document is part of the MTA:SA Android port project.*
