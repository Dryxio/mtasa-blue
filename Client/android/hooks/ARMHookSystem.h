/*
 * MTA:SA Android - ARM Hook System
 *
 * Equivalent to Client/game_sa/HookSystem.h for ARM architecture
 * Provides inline code patching and function interception for ARM32/ARM64
 *
 * Key differences from x86:
 * - Fixed 4-byte instructions (ARM mode) or 2/4-byte (Thumb mode)
 * - Different offset calculation: (target - source - 8) >> 2 for ARM
 * - Register preservation: R0-R3 (params), R4-R11 (preserved), R12 (scratch), LR, SP, PC
 * - No direct memory-to-memory ops - use registers
 * - Conditional execution via instruction suffixes, not separate JCC instructions
 */

#ifndef ARM_HOOK_SYSTEM_H
#define ARM_HOOK_SYSTEM_H

#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <functional>

namespace MTA::Android::Hooks
{
    //=========================================================================
    // ARM Instruction Encoding Constants
    //=========================================================================

    // ARM Mode (32-bit instructions)
    namespace ARM32
    {
        // B (Branch) - Unconditional: 0xEA000000 | offset
        // Format: cond(4) | 1010(4) | signed_imm24(24)
        constexpr uint32_t OPCODE_B           = 0xEA000000;  // Branch (always)
        constexpr uint32_t OPCODE_B_COND_MASK = 0x0A000000;  // Conditional branch base

        // BL (Branch with Link) - Function call: 0xEB000000 | offset
        // Format: cond(4) | 1011(4) | signed_imm24(24)
        constexpr uint32_t OPCODE_BL          = 0xEB000000;  // Branch & Link (always)

        // BX LR (Return) - 0xE12FFF1E
        // Format: cond(4) | 0001 0010 1111 1111 1111 0001(24) | Rm(4)
        constexpr uint32_t OPCODE_BX_LR       = 0xE12FFF1E;  // Return (branch to LR)

        // NOP - MOV R0, R0: 0xE1A00000
        constexpr uint32_t OPCODE_NOP         = 0xE1A00000;  // No operation

        // LDR PC, [PC, #offset] - PC-relative load for long jumps
        // Format: cond(4) | 01(2) | I(1) | P(1) | U(1) | B(1) | W(1) | L(1) | Rn(4) | Rd(4) | offset(12)
        constexpr uint32_t OPCODE_LDR_PC_PC   = 0xE59FF000;  // LDR PC, [PC, #imm]

        // PUSH/POP for register preservation
        // STMFD SP!, {regs} - Push multiple: 0xE92D0000 | register_mask
        // LDMFD SP!, {regs} - Pop multiple:  0xE8BD0000 | register_mask
        constexpr uint32_t OPCODE_PUSH        = 0xE92D0000;  // STMFD SP!, {...}
        constexpr uint32_t OPCODE_POP         = 0xE8BD0000;  // LDMFD SP!, {...}

        // Register masks for PUSH/POP
        constexpr uint32_t REG_R0  = (1 << 0);
        constexpr uint32_t REG_R1  = (1 << 1);
        constexpr uint32_t REG_R2  = (1 << 2);
        constexpr uint32_t REG_R3  = (1 << 3);
        constexpr uint32_t REG_R4  = (1 << 4);
        constexpr uint32_t REG_R5  = (1 << 5);
        constexpr uint32_t REG_R6  = (1 << 6);
        constexpr uint32_t REG_R7  = (1 << 7);
        constexpr uint32_t REG_R8  = (1 << 8);
        constexpr uint32_t REG_R9  = (1 << 9);
        constexpr uint32_t REG_R10 = (1 << 10);
        constexpr uint32_t REG_R11 = (1 << 11);  // FP (Frame Pointer)
        constexpr uint32_t REG_R12 = (1 << 12);  // IP (Intra-Procedure scratch)
        constexpr uint32_t REG_SP  = (1 << 13);  // Stack Pointer
        constexpr uint32_t REG_LR  = (1 << 14);  // Link Register
        constexpr uint32_t REG_PC  = (1 << 15);  // Program Counter

        // Common register sets
        constexpr uint32_t REGS_CALLER_SAVED = REG_R0 | REG_R1 | REG_R2 | REG_R3 | REG_R12;
        constexpr uint32_t REGS_CALLEE_SAVED = REG_R4 | REG_R5 | REG_R6 | REG_R7 | REG_R8 | REG_R9 | REG_R10 | REG_R11;
        constexpr uint32_t REGS_ALL_GPR      = REGS_CALLER_SAVED | REGS_CALLEE_SAVED | REG_LR;

        // Condition codes (top 4 bits)
        constexpr uint32_t COND_EQ = 0x00000000;  // Equal (Z=1)
        constexpr uint32_t COND_NE = 0x10000000;  // Not Equal (Z=0)
        constexpr uint32_t COND_CS = 0x20000000;  // Carry Set / Unsigned Higher or Same
        constexpr uint32_t COND_CC = 0x30000000;  // Carry Clear / Unsigned Lower
        constexpr uint32_t COND_MI = 0x40000000;  // Minus / Negative
        constexpr uint32_t COND_PL = 0x50000000;  // Plus / Positive or Zero
        constexpr uint32_t COND_VS = 0x60000000;  // Overflow Set
        constexpr uint32_t COND_VC = 0x70000000;  // Overflow Clear
        constexpr uint32_t COND_HI = 0x80000000;  // Unsigned Higher
        constexpr uint32_t COND_LS = 0x90000000;  // Unsigned Lower or Same
        constexpr uint32_t COND_GE = 0xA0000000;  // Signed Greater or Equal
        constexpr uint32_t COND_LT = 0xB0000000;  // Signed Less Than
        constexpr uint32_t COND_GT = 0xC0000000;  // Signed Greater Than
        constexpr uint32_t COND_LE = 0xD0000000;  // Signed Less or Equal
        constexpr uint32_t COND_AL = 0xE0000000;  // Always (unconditional)

        // Branch range: +/- 32MB (24-bit signed offset * 4)
        constexpr int32_t BRANCH_MAX_RANGE = 0x02000000;  // 32MB
        constexpr int32_t BRANCH_MIN_RANGE = -0x02000000;
    }

    // Thumb Mode (16/32-bit instructions) - for modern Android
    namespace Thumb2
    {
        // Thumb-2 B.W (32-bit branch): 0xF000B800 base
        constexpr uint32_t OPCODE_B_W         = 0xF000B800;

        // Thumb-2 BL (32-bit): 0xF000D000 base
        constexpr uint32_t OPCODE_BL          = 0xF000D000;

        // Thumb NOP: 0xBF00
        constexpr uint16_t OPCODE_NOP         = 0xBF00;

        // Thumb BX LR: 0x4770
        constexpr uint16_t OPCODE_BX_LR       = 0x4770;

        // Thumb PUSH: 0xB500 | reg_mask (R0-R7, LR)
        constexpr uint16_t OPCODE_PUSH        = 0xB400;

        // Thumb POP: 0xBC00 | reg_mask (R0-R7, PC)
        constexpr uint16_t OPCODE_POP         = 0xBC00;
    }

    // ARM64 (AArch64) instructions
    namespace ARM64
    {
        // B (Branch): 0x14000000 | imm26
        // Range: +/- 128MB
        constexpr uint32_t OPCODE_B           = 0x14000000;

        // BL (Branch with Link): 0x94000000 | imm26
        constexpr uint32_t OPCODE_BL          = 0x94000000;

        // BR Xn (Branch to Register): 0xD61F0000 | (Rn << 5)
        constexpr uint32_t OPCODE_BR          = 0xD61F0000;

        // BLR Xn (Branch with Link to Register): 0xD63F0000 | (Rn << 5)
        constexpr uint32_t OPCODE_BLR         = 0xD63F0000;

        // RET (Return): 0xD65F03C0 (RET X30)
        constexpr uint32_t OPCODE_RET         = 0xD65F03C0;

        // NOP: 0xD503201F
        constexpr uint32_t OPCODE_NOP         = 0xD503201F;

        // LDR Xd, [PC, #imm] for trampolines
        constexpr uint32_t OPCODE_LDR_X_PC    = 0x58000000;

        // Branch range: +/- 128MB (26-bit signed offset * 4)
        constexpr int64_t BRANCH_MAX_RANGE = 0x08000000;   // 128MB
        constexpr int64_t BRANCH_MIN_RANGE = -0x08000000;
    }

    //=========================================================================
    // Hook Information Structures
    //=========================================================================

    // Equivalent to x86 SHookInfo
    struct HookInfo
    {
        uintptr_t   address;            // Installation address
        uintptr_t   hook;               // Hook function address
        uint32_t    size;               // Bytes to replace (typically 4 or 8 for ARM)
        uint8_t     originalBytes[16];  // Saved original bytes for restoration
        bool        isThumb;            // True if hooking Thumb code
        bool        installed;          // Track installation state

        HookInfo() : address(0), hook(0), size(0), isThumb(false), installed(false)
        {
            memset(originalBytes, 0, sizeof(originalBytes));
        }

        HookInfo(uintptr_t addr, uintptr_t hookFunc, uint32_t sz, bool thumb = false)
            : address(addr), hook(hookFunc), size(sz), isThumb(thumb), installed(false)
        {
            memset(originalBytes, 0, sizeof(originalBytes));
        }
    };

    // Trampoline for calling original function
    struct Trampoline
    {
        uintptr_t   originalAddress;    // Original function
        uint8_t     code[32];           // Trampoline code (original bytes + jump back)
        size_t      codeSize;           // Actual code size

        Trampoline() : originalAddress(0), codeSize(0)
        {
            memset(code, 0, sizeof(code));
        }
    };

    //=========================================================================
    // Memory Protection Utilities
    //=========================================================================

    /**
     * Make memory region writable for patching
     * @param address Target address
     * @param size    Size in bytes
     * @return Previous protection flags, or -1 on failure
     */
    inline int MemoryUnprotect(uintptr_t address, size_t size)
    {
        // Align to page boundary
        uintptr_t pageSize = sysconf(_SC_PAGESIZE);
        uintptr_t pageStart = address & ~(pageSize - 1);
        size_t fullSize = (address - pageStart) + size;

        // Make writable and executable
        if (mprotect((void*)pageStart, fullSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
        {
            return -1;
        }
        return PROT_READ | PROT_EXEC;  // Previous protection (assumed)
    }

    /**
     * Restore memory protection
     */
    inline void MemoryReprotect(uintptr_t address, size_t size, int protection)
    {
        uintptr_t pageSize = sysconf(_SC_PAGESIZE);
        uintptr_t pageStart = address & ~(pageSize - 1);
        size_t fullSize = (address - pageStart) + size;
        mprotect((void*)pageStart, fullSize, protection);
    }

    /**
     * Safe memory write with protection handling
     */
    template<typename T>
    inline bool MemPut(uintptr_t address, T value)
    {
        int oldProt = MemoryUnprotect(address, sizeof(T));
        if (oldProt < 0) return false;

        *reinterpret_cast<T*>(address) = value;

        // Clear instruction cache (critical for ARM!)
        __builtin___clear_cache(reinterpret_cast<char*>(address),
                                reinterpret_cast<char*>(address + sizeof(T)));

        MemoryReprotect(address, sizeof(T), oldProt);
        return true;
    }

    /**
     * Safe memory read
     */
    template<typename T>
    inline T MemGet(uintptr_t address)
    {
        return *reinterpret_cast<T*>(address);
    }

    /**
     * Safe memory copy
     */
#ifndef MTA_MEMCOPY_DEFINED
#define MTA_MEMCOPY_DEFINED
    inline bool MemCopy(uintptr_t dest, const void* src, size_t size)
    {
        int oldProt = MemoryUnprotect(dest, size);
        if (oldProt < 0) return false;

        memcpy(reinterpret_cast<void*>(dest), src, size);

        __builtin___clear_cache(reinterpret_cast<char*>(dest),
                                reinterpret_cast<char*>(dest + size));

        MemoryReprotect(dest, size, oldProt);
        return true;
    }
#endif

    /**
     * Fill memory with value
     */
    inline bool MemSet(uintptr_t dest, uint8_t value, size_t size)
    {
        int oldProt = MemoryUnprotect(dest, size);
        if (oldProt < 0) return false;

        memset(reinterpret_cast<void*>(dest), value, size);

        __builtin___clear_cache(reinterpret_cast<char*>(dest),
                                reinterpret_cast<char*>(dest + size));

        MemoryReprotect(dest, size, oldProt);
        return true;
    }

    //=========================================================================
    // ARM32 Hook Creation Functions
    //=========================================================================

    /**
     * Calculate ARM branch offset
     * ARM: offset = (target - source - 8) / 4
     * The -8 accounts for PC being 2 instructions ahead in ARM
     */
    inline int32_t CalculateARMBranchOffset(uintptr_t from, uintptr_t to)
    {
        int64_t offset = static_cast<int64_t>(to) - static_cast<int64_t>(from) - 8;
        return static_cast<int32_t>(offset >> 2);
    }

    /**
     * Create ARM B (branch) instruction
     */
    inline uint32_t CreateARMBranch(uintptr_t from, uintptr_t to)
    {
        int32_t offset = CalculateARMBranchOffset(from, to);
        return ARM32::OPCODE_B | (offset & 0x00FFFFFF);
    }

    /**
     * Create ARM BL (branch with link) instruction
     */
    inline uint32_t CreateARMBranchLink(uintptr_t from, uintptr_t to)
    {
        int32_t offset = CalculateARMBranchOffset(from, to);
        return ARM32::OPCODE_BL | (offset & 0x00FFFFFF);
    }

    /**
     * Check if target is within ARM branch range (+/- 32MB)
     */
    inline bool IsWithinARMBranchRange(uintptr_t from, uintptr_t to)
    {
        int64_t offset = static_cast<int64_t>(to) - static_cast<int64_t>(from) - 8;
        return offset >= ARM32::BRANCH_MIN_RANGE && offset <= ARM32::BRANCH_MAX_RANGE;
    }

    /**
     * Create ARM PUSH instruction
     * @param regMask Bitmask of registers to push (use REG_* constants)
     */
    inline uint32_t CreateARMPush(uint32_t regMask)
    {
        return ARM32::OPCODE_PUSH | (regMask & 0xFFFF);
    }

    /**
     * Create ARM POP instruction
     */
    inline uint32_t CreateARMPop(uint32_t regMask)
    {
        return ARM32::OPCODE_POP | (regMask & 0xFFFF);
    }

    //=========================================================================
    // ARM64 Hook Creation Functions
    //=========================================================================

    /**
     * Calculate ARM64 branch offset
     * ARM64: offset = (target - source) / 4
     */
    inline int32_t CalculateARM64BranchOffset(uintptr_t from, uintptr_t to)
    {
        int64_t offset = static_cast<int64_t>(to) - static_cast<int64_t>(from);
        return static_cast<int32_t>(offset >> 2);
    }

    /**
     * Create ARM64 B instruction
     */
    inline uint32_t CreateARM64Branch(uintptr_t from, uintptr_t to)
    {
        int32_t offset = CalculateARM64BranchOffset(from, to);
        return ARM64::OPCODE_B | (offset & 0x03FFFFFF);
    }

    /**
     * Create ARM64 BL instruction
     */
    inline uint32_t CreateARM64BranchLink(uintptr_t from, uintptr_t to)
    {
        int32_t offset = CalculateARM64BranchOffset(from, to);
        return ARM64::OPCODE_BL | (offset & 0x03FFFFFF);
    }

    /**
     * Check if within ARM64 branch range (+/- 128MB)
     */
    inline bool IsWithinARM64BranchRange(uintptr_t from, uintptr_t to)
    {
        int64_t offset = static_cast<int64_t>(to) - static_cast<int64_t>(from);
        return offset >= ARM64::BRANCH_MIN_RANGE && offset <= ARM64::BRANCH_MAX_RANGE;
    }

    //=========================================================================
    // Thumb Mode Hook Functions
    //=========================================================================

    /**
     * Detect if address is Thumb code
     * Thumb addresses have bit 0 set (convention)
     */
    inline bool IsThumbAddress(uintptr_t address)
    {
        return (address & 1) != 0;
    }

    /**
     * Get actual address from potentially Thumb-marked address
     */
    inline uintptr_t ClearThumbBit(uintptr_t address)
    {
        return address & ~1UL;
    }

    /**
     * Set Thumb bit on address
     */
    inline uintptr_t SetThumbBit(uintptr_t address)
    {
        return address | 1;
    }

    //=========================================================================
    // Hook Installation Functions
    //=========================================================================

    /**
     * Install a simple branch hook (ARM32)
     * Replaces instruction at address with B <hookFunction>
     *
     * @param address       Address to hook
     * @param hookFunction  Function to redirect to
     * @param info          HookInfo to store original bytes (optional)
     * @return true on success
     */
    inline bool HookInstallBranch(uintptr_t address, uintptr_t hookFunction, HookInfo* info = nullptr)
    {
        // Check range
        if (!IsWithinARMBranchRange(address, hookFunction))
        {
            // Need trampoline for long jump - TODO
            return false;
        }

        // Save original bytes
        if (info)
        {
            info->address = address;
            info->hook = hookFunction;
            info->size = 4;
            info->isThumb = false;
            memcpy(info->originalBytes, reinterpret_cast<void*>(address), 4);
        }

        // Create and install branch
        uint32_t branch = CreateARMBranch(address, hookFunction);
        if (!MemPut<uint32_t>(address, branch))
        {
            return false;
        }

        if (info) info->installed = true;
        return true;
    }

    /**
     * Install a call hook (ARM32)
     * Replaces BL instruction with BL <hookFunction>
     */
    inline bool HookInstallCall(uintptr_t address, uintptr_t hookFunction, HookInfo* info = nullptr)
    {
        if (!IsWithinARMBranchRange(address, hookFunction))
        {
            return false;
        }

        if (info)
        {
            info->address = address;
            info->hook = hookFunction;
            info->size = 4;
            info->isThumb = false;
            memcpy(info->originalBytes, reinterpret_cast<void*>(address), 4);
        }

        uint32_t branchLink = CreateARMBranchLink(address, hookFunction);
        if (!MemPut<uint32_t>(address, branchLink))
        {
            return false;
        }

        if (info) info->installed = true;
        return true;
    }

    /**
     * Install a VTable hook
     * Simply replaces pointer at vtable offset
     */
    inline bool HookInstallVTable(uintptr_t vtableAddress, uintptr_t hookFunction,
                                   uintptr_t* originalFunction = nullptr)
    {
        if (originalFunction)
        {
            *originalFunction = MemGet<uintptr_t>(vtableAddress);
        }
        return MemPut<uintptr_t>(vtableAddress, hookFunction);
    }

    /**
     * Uninstall a hook by restoring original bytes
     */
    inline bool HookUninstall(HookInfo& info)
    {
        if (!info.installed || info.size == 0)
        {
            return false;
        }

        if (!MemCopy(info.address, info.originalBytes, info.size))
        {
            return false;
        }

        info.installed = false;
        return true;
    }

    /**
     * Fill address with NOPs
     */
    inline bool HookNop(uintptr_t address, size_t count)
    {
#if defined(__aarch64__)
        for (size_t i = 0; i < count; i++)
        {
            if (!MemPut<uint32_t>(address + i * 4, ARM64::OPCODE_NOP))
                return false;
        }
#else
        for (size_t i = 0; i < count; i++)
        {
            if (!MemPut<uint32_t>(address + i * 4, ARM32::OPCODE_NOP))
                return false;
        }
#endif
        return true;
    }

    //=========================================================================
    // Hook Macros (Equivalent to x86 EZHookInstall)
    //=========================================================================

    // Hook position and size definition macros
    #define ARM_HOOKPOS(name, addr)     constexpr uintptr_t HOOKPOS_##name = addr
    #define ARM_HOOKSIZE(name, size)    constexpr uint32_t HOOKSIZE_##name = size
    #define ARM_HOOKCHECK(name, val)    constexpr uint32_t HOOKCHECK_##name = val
    #define ARM_RETURN(name, addr)      constexpr uintptr_t RETURN_##name = addr

    // Hook installation macros
    #define ARM_HOOK_INSTALL(name) \
        MTA::Android::Hooks::HookInstallBranch(HOOKPOS_##name, reinterpret_cast<uintptr_t>(HOOK_##name))

    #define ARM_HOOK_INSTALL_CALL(name) \
        MTA::Android::Hooks::HookInstallCall(HOOKPOS_##name, reinterpret_cast<uintptr_t>(HOOK_##name))

    // Hook function declaration macro
    #define ARM_HOOK_DECLARE(name) \
        extern "C" void HOOK_##name()

    //=========================================================================
    // Register Preservation (for inline assembly hooks)
    //=========================================================================

    // ARM32 register save/restore (equivalent to x86 pushad/popad)
    // Note: In ARM, we use STMFD/LDMFD (PUSH/POP multiple)

    /*
     * ARM32 Calling Convention (AAPCS):
     * - R0-R3: Arguments and return value (caller-saved)
     * - R4-R11: Local variables (callee-saved)
     * - R12 (IP): Intra-procedure scratch (caller-saved)
     * - R13 (SP): Stack pointer
     * - R14 (LR): Link register (return address)
     * - R15 (PC): Program counter
     *
     * For hook functions, we typically save R0-R12, LR
     */

    // Inline assembly macros for ARM32 (GCC syntax)
    #if defined(__arm__) && !defined(__aarch64__)

    #define ARM_SAVE_REGS() \
        __asm__ __volatile__( \
            "push {r0-r12, lr}\n" \
            : : : "memory" \
        )

    #define ARM_RESTORE_REGS() \
        __asm__ __volatile__( \
            "pop {r0-r12, lr}\n" \
            : : : "memory" \
        )

    #define ARM_RETURN_TO(addr) \
        __asm__ __volatile__( \
            "ldr pc, =" #addr "\n" \
            : : : "memory" \
        )

    #endif // __arm__

    // Inline assembly macros for ARM64 (GCC syntax)
    #if defined(__aarch64__)

    #define ARM64_SAVE_REGS() \
        __asm__ __volatile__( \
            "stp x0, x1, [sp, #-16]!\n" \
            "stp x2, x3, [sp, #-16]!\n" \
            "stp x4, x5, [sp, #-16]!\n" \
            "stp x6, x7, [sp, #-16]!\n" \
            "stp x8, x9, [sp, #-16]!\n" \
            "stp x10, x11, [sp, #-16]!\n" \
            "stp x12, x13, [sp, #-16]!\n" \
            "stp x14, x15, [sp, #-16]!\n" \
            "stp x16, x17, [sp, #-16]!\n" \
            "stp x18, x19, [sp, #-16]!\n" \
            "stp x20, x21, [sp, #-16]!\n" \
            "stp x22, x23, [sp, #-16]!\n" \
            "stp x24, x25, [sp, #-16]!\n" \
            "stp x26, x27, [sp, #-16]!\n" \
            "stp x28, x29, [sp, #-16]!\n" \
            "str x30, [sp, #-16]!\n" \
            : : : "memory" \
        )

    #define ARM64_RESTORE_REGS() \
        __asm__ __volatile__( \
            "ldr x30, [sp], #16\n" \
            "ldp x28, x29, [sp], #16\n" \
            "ldp x26, x27, [sp], #16\n" \
            "ldp x24, x25, [sp], #16\n" \
            "ldp x22, x23, [sp], #16\n" \
            "ldp x20, x21, [sp], #16\n" \
            "ldp x18, x19, [sp], #16\n" \
            "ldp x16, x17, [sp], #16\n" \
            "ldp x14, x15, [sp], #16\n" \
            "ldp x12, x13, [sp], #16\n" \
            "ldp x10, x11, [sp], #16\n" \
            "ldp x8, x9, [sp], #16\n" \
            "ldp x6, x7, [sp], #16\n" \
            "ldp x4, x5, [sp], #16\n" \
            "ldp x2, x3, [sp], #16\n" \
            "ldp x0, x1, [sp], #16\n" \
            : : : "memory" \
        )

    #endif // __aarch64__

    //=========================================================================
    // Hook Manager Class
    //=========================================================================

    class HookManager
    {
    public:
        static HookManager& Instance()
        {
            static HookManager instance;
            return instance;
        }

        // Install a hook and track it
        bool Install(const char* name, uintptr_t address, uintptr_t hook, uint32_t size = 4)
        {
            HookInfo info(address, hook, size);
            if (!HookInstallBranch(address, hook, &info))
            {
                return false;
            }
            m_hooks.push_back({name, info});
            return true;
        }

        // Uninstall all hooks
        void UninstallAll()
        {
            for (auto& entry : m_hooks)
            {
                HookUninstall(entry.info);
            }
            m_hooks.clear();
        }

        // Find hook by name
        HookInfo* Find(const char* name)
        {
            for (auto& entry : m_hooks)
            {
                if (strcmp(entry.name, name) == 0)
                {
                    return &entry.info;
                }
            }
            return nullptr;
        }

    private:
        struct HookEntry
        {
            const char* name;
            HookInfo info;
        };

        std::vector<HookEntry> m_hooks;

        HookManager() = default;
        ~HookManager() { UninstallAll(); }
        HookManager(const HookManager&) = delete;
        HookManager& operator=(const HookManager&) = delete;
    };

} // namespace MTA::Android::Hooks

#endif // ARM_HOOK_SYSTEM_H
