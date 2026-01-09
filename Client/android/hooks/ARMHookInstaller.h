/*
 * MTA:SA Android - ARM Hook Installer
 *
 * Bridges MTA's multiplayer_sa hooks to ARM architecture.
 * Provides equivalent functions to multiplayer_hooksystem.cpp for ARM32/ARM64.
 *
 * Usage:
 *   // Replace x86 HookInstall with ARM equivalent
 *   #ifdef MTA_ANDROID
 *       ARMHookInstall(ARM32::CPed_ProcessControl, (uintptr_t)HOOK_CPed_ProcessControl, 4);
 *   #else
 *       HookInstall(0x4A2541, (DWORD)HOOK_CPed_ProcessControl, 5);
 *   #endif
 */

#ifndef ARM_HOOK_INSTALLER_H
#define ARM_HOOK_INSTALLER_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "MTA-Hooks"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace MTA::Android::Hooks
{
    //=========================================================================
    // Global State
    //=========================================================================

    // Base address of libGTASA.so (set during initialization)
    inline uintptr_t g_libGTASA = 0;

    // Page size for memory protection
    inline size_t g_pageSize = 0;

    //=========================================================================
    // Architecture Detection
    //=========================================================================

    #if defined(__aarch64__)
        constexpr bool IS_ARM64 = true;
        constexpr bool IS_ARM32 = false;
        constexpr size_t HOOK_SIZE = 16;  // ARM64 needs more bytes for hook
    #elif defined(__arm__)
        constexpr bool IS_ARM64 = false;
        constexpr bool IS_ARM32 = true;
        constexpr size_t HOOK_SIZE = 8;   // ARM32 Thumb hook size
    #else
        #error "Unsupported architecture"
    #endif

    //=========================================================================
    // Initialization
    //=========================================================================

    /**
     * Initialize the hook system
     * Must be called after libGTASA.so is loaded
     * @return true if initialization successful
     */
    inline bool Initialize()
    {
        g_pageSize = sysconf(_SC_PAGESIZE);

        // Find libGTASA.so base address by parsing /proc/self/maps
        FILE* maps = fopen("/proc/self/maps", "r");
        if (!maps)
        {
            LOGE("Failed to open /proc/self/maps");
            return false;
        }

        char line[512];
        while (fgets(line, sizeof(line), maps))
        {
            if (strstr(line, "libGTASA.so") && strstr(line, "r-xp"))
            {
                // Parse base address from line format: "addr1-addr2 perms offset ..."
                uintptr_t addr;
                if (sscanf(line, "%" SCNxPTR, &addr) == 1)
                {
                    g_libGTASA = addr;
                    LOGI("Found libGTASA.so at 0x%lx", (unsigned long)g_libGTASA);
                    fclose(maps);
                    return true;
                }
            }
        }

        fclose(maps);
        LOGE("Failed to find libGTASA.so");
        return false;
    }

    /**
     * Get absolute address from offset
     */
    inline uintptr_t GetAbsoluteAddress(uint32_t offset)
    {
        return g_libGTASA + offset;
    }

    //=========================================================================
    // Memory Protection
    //=========================================================================

    /**
     * Unprotect memory region for writing
     * @param addr Address to unprotect
     * @param size Size of region
     * @return true if successful
     */
    inline bool UnprotectMemory(uintptr_t addr, size_t size)
    {
        uintptr_t pageStart = addr & ~(g_pageSize - 1);
        size_t pageSize = (addr + size - pageStart + g_pageSize - 1) & ~(g_pageSize - 1);

        if (mprotect((void*)pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
        {
            LOGE("mprotect failed for 0x%lx", (unsigned long)addr);
            return false;
        }
        return true;
    }

    /**
     * Clear instruction cache after writing hooks
     */
    inline void ClearCache(uintptr_t addr, size_t size)
    {
        __builtin___clear_cache((char*)addr, (char*)(addr + size));
    }

    //=========================================================================
    // Memory Write Utilities
    //=========================================================================

    template<typename T>
    inline void MemWrite(uintptr_t addr, T value)
    {
        if (UnprotectMemory(addr, sizeof(T)))
        {
            *reinterpret_cast<T*>(addr) = value;
            ClearCache(addr, sizeof(T));
        }
    }

#ifndef MTA_MEMCOPY_DEFINED
#define MTA_MEMCOPY_DEFINED
    inline bool MemCopy(uintptr_t dest, const void* src, size_t size)
    {
        if (!UnprotectMemory(dest, size))
        {
            return false;
        }
        memcpy((void*)dest, src, size);
        ClearCache(dest, size);
        return true;
    }
#endif

    inline void MemFill(uintptr_t addr, uint8_t value, size_t size)
    {
        if (UnprotectMemory(addr, size))
        {
            memset((void*)addr, value, size);
            ClearCache(addr, size);
        }
    }

    //=========================================================================
    // ARM32 Hook Implementation (Thumb mode)
    //=========================================================================

    namespace ARM32
    {
        /**
         * Create a Thumb branch instruction
         * @param from Source address
         * @param to Target address
         * @return Encoded instruction(s)
         */
        inline void CreateThumbBranch(uintptr_t from, uintptr_t to, uint8_t* buffer)
        {
            // Use LDR PC, [PC, #0] + address for long jump
            // This works for any distance
            // LDR.W PC, [PC, #0]  = 0xF000F8DF
            // <target address>

            buffer[0] = 0xDF;
            buffer[1] = 0xF8;
            buffer[2] = 0x00;
            buffer[3] = 0xF0;

            // Target address (with Thumb bit)
            uint32_t target = (to & ~1) | 1;
            memcpy(buffer + 4, &target, 4);
        }

        /**
         * Create Thumb NOP instructions
         */
        constexpr uint16_t THUMB_NOP = 0xBF00;

        /**
         * Install a branch hook at the given offset
         * @param offset Offset from libGTASA base
         * @param hookFunc Address of hook function
         * @param origFunc Optional: store original function pointer
         * @return true if successful
         */
        inline bool InstallBranchHook(uint32_t offset, uintptr_t hookFunc, uintptr_t* origFunc = nullptr)
        {
            uintptr_t addr = GetAbsoluteAddress(offset);

            // Store original if requested
            if (origFunc)
            {
                // For Thumb, the function pointer has bit 0 set
                *origFunc = addr | 1;
            }

            uint8_t hookCode[8];
            CreateThumbBranch(addr, hookFunc, hookCode);

            if (!UnprotectMemory(addr, 8))
                return false;

            memcpy((void*)addr, hookCode, 8);
            ClearCache(addr, 8);

            LOGI("Installed ARM32 hook at 0x%lx -> 0x%lx", (unsigned long)addr, (unsigned long)hookFunc);
            return true;
        }

        /**
         * Install a hook that preserves the original function
         * Creates a trampoline for calling the original
         */
        inline bool InstallHookWithTrampoline(uint32_t offset, uintptr_t hookFunc,
                                               uintptr_t* trampoline, size_t prologSize)
        {
            uintptr_t addr = GetAbsoluteAddress(offset);

            // Allocate trampoline memory (executable)
            void* trampolineMem = mmap(nullptr, g_pageSize,
                                       PROT_READ | PROT_WRITE | PROT_EXEC,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (trampolineMem == MAP_FAILED)
            {
                LOGE("Failed to allocate trampoline memory");
                return false;
            }

            uint8_t* tramp = (uint8_t*)trampolineMem;

            // Copy original prologue to trampoline
            memcpy(tramp, (void*)addr, prologSize);

            // Add jump back to original function (after prologue)
            CreateThumbBranch((uintptr_t)(tramp + prologSize), addr + prologSize, tramp + prologSize);

            // Set trampoline pointer (with Thumb bit)
            *trampoline = ((uintptr_t)trampolineMem) | 1;

            // Install hook at original location
            return InstallBranchHook(offset, hookFunc, nullptr);
        }

    } // namespace ARM32

    //=========================================================================
    // ARM64 Hook Implementation
    //=========================================================================

    namespace ARM64
    {
        /**
         * Create an ARM64 branch instruction sequence
         * Uses LDR X16, #8; BR X16; <address> pattern
         */
        inline void CreateBranch(uintptr_t from, uintptr_t to, uint8_t* buffer)
        {
            // LDR X16, #8    = 0x58000050
            // BR X16         = 0xD61F0200
            // <64-bit address>

            uint32_t ldr = 0x58000050;  // LDR X16, [PC, #8]
            uint32_t br = 0xD61F0200;   // BR X16

            memcpy(buffer, &ldr, 4);
            memcpy(buffer + 4, &br, 4);
            memcpy(buffer + 8, &to, 8);
        }

        /**
         * ARM64 NOP instruction
         */
        constexpr uint32_t ARM64_NOP = 0xD503201F;

        /**
         * Install a branch hook at the given offset
         */
        inline bool InstallBranchHook(uint32_t offset, uintptr_t hookFunc, uintptr_t* origFunc = nullptr)
        {
            uintptr_t addr = GetAbsoluteAddress(offset);

            if (origFunc)
            {
                *origFunc = addr;
            }

            uint8_t hookCode[16];
            CreateBranch(addr, hookFunc, hookCode);

            if (!UnprotectMemory(addr, 16))
                return false;

            memcpy((void*)addr, hookCode, 16);
            ClearCache(addr, 16);

            LOGI("Installed ARM64 hook at 0x%lx -> 0x%lx", (unsigned long)addr, (unsigned long)hookFunc);
            return true;
        }

        /**
         * Install hook with trampoline for calling original
         */
        inline bool InstallHookWithTrampoline(uint32_t offset, uintptr_t hookFunc,
                                               uintptr_t* trampoline, size_t prologSize)
        {
            uintptr_t addr = GetAbsoluteAddress(offset);

            // Allocate trampoline
            void* trampolineMem = mmap(nullptr, g_pageSize,
                                       PROT_READ | PROT_WRITE | PROT_EXEC,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (trampolineMem == MAP_FAILED)
            {
                LOGE("Failed to allocate trampoline memory");
                return false;
            }

            uint8_t* tramp = (uint8_t*)trampolineMem;

            // Copy prologue
            memcpy(tramp, (void*)addr, prologSize);

            // Add jump back
            CreateBranch((uintptr_t)(tramp + prologSize), addr + prologSize, tramp + prologSize);

            *trampoline = (uintptr_t)trampolineMem;

            return InstallBranchHook(offset, hookFunc, nullptr);
        }

    } // namespace ARM64

    //=========================================================================
    // Architecture-Independent API (matches MTA's multiplayer_hooksystem.cpp)
    //=========================================================================

    /**
     * Install a hook - equivalent to MTA's HookInstall()
     * @param offset Offset from libGTASA base (from ARMAddressMap.h)
     * @param hookFunc Address of hook function
     * @param hookSize Ignored on ARM (for API compatibility)
     * @return true if successful
     */
    inline bool ARMHookInstall(uint32_t offset, uintptr_t hookFunc, int hookSize = 0)
    {
        #if defined(__aarch64__)
            return ARM64::InstallBranchHook(offset, hookFunc, nullptr);
        #else
            return ARM32::InstallBranchHook(offset, hookFunc, nullptr);
        #endif
    }

    /**
     * Install a hook with trampoline for calling original
     * @param offset Offset from libGTASA base
     * @param hookFunc Address of hook function
     * @param trampoline Output: pointer to call original function
     * @param prologSize Size of function prologue to copy
     * @return true if successful
     */
    inline bool ARMHookInstallWithOriginal(uint32_t offset, uintptr_t hookFunc,
                                            uintptr_t* trampoline, size_t prologSize)
    {
        #if defined(__aarch64__)
            return ARM64::InstallHookWithTrampoline(offset, hookFunc, trampoline, prologSize);
        #else
            return ARM32::InstallHookWithTrampoline(offset, hookFunc, trampoline, prologSize);
        #endif
    }

    /**
     * Install a method/vtable hook - equivalent to MTA's HookInstallMethod()
     * @param offset Offset of vtable entry from libGTASA base
     * @param hookFunc New function pointer
     * @param origFunc Optional: store original function pointer
     * @return true if successful
     */
    inline bool ARMHookInstallMethod(uint32_t offset, uintptr_t hookFunc, uintptr_t* origFunc = nullptr)
    {
        uintptr_t addr = GetAbsoluteAddress(offset);

        if (!UnprotectMemory(addr, sizeof(uintptr_t)))
            return false;

        if (origFunc)
        {
            *origFunc = *reinterpret_cast<uintptr_t*>(addr);
        }

        *reinterpret_cast<uintptr_t*>(addr) = hookFunc;
        ClearCache(addr, sizeof(uintptr_t));

        LOGI("Installed method hook at 0x%lx -> 0x%lx", (unsigned long)addr, (unsigned long)hookFunc);
        return true;
    }

    /**
     * Write NOP instructions - equivalent to MTA's MemSet with 0x90
     * @param offset Offset from libGTASA base
     * @param count Number of instructions to NOP
     */
    inline void ARMNop(uint32_t offset, size_t count)
    {
        uintptr_t addr = GetAbsoluteAddress(offset);

        #if defined(__aarch64__)
            for (size_t i = 0; i < count; i++)
            {
                MemWrite<uint32_t>(addr + i * 4, ARM64::ARM64_NOP);
            }
        #else
            for (size_t i = 0; i < count; i++)
            {
                MemWrite<uint16_t>(addr + i * 2, ARM32::THUMB_NOP);
            }
        #endif
    }

    /**
     * Write a return instruction - make function return immediately
     * @param offset Offset from libGTASA base
     */
    inline void ARMRet(uint32_t offset)
    {
        uintptr_t addr = GetAbsoluteAddress(offset);

        #if defined(__aarch64__)
            // RET = 0xD65F03C0
            MemWrite<uint32_t>(addr, 0xD65F03C0);
        #else
            // BX LR = 0x4770 (Thumb)
            MemWrite<uint16_t>(addr, 0x4770);
        #endif
    }

    //=========================================================================
    // PLT Hook Support (for hooking imported functions)
    //=========================================================================

    /**
     * Hook a PLT (Procedure Linkage Table) entry
     * Used for hooking functions imported from other libraries
     * @param pltOffset Offset of PLT entry
     * @param hookFunc New function
     * @param origFunc Store original function
     */
    inline bool ARMHookPLT(uint32_t pltOffset, uintptr_t hookFunc, uintptr_t* origFunc = nullptr)
    {
        return ARMHookInstallMethod(pltOffset, hookFunc, origFunc);
    }

} // namespace MTA::Android::Hooks

#endif // ARM_HOOK_INSTALLER_H
