/*
 * MTA:SA Android - Signature Scanner
 *
 * Used to find function addresses in the Android GTA:SA binary by matching
 * byte patterns, string references, and structural signatures.
 *
 * This is critical for mapping x86 addresses from MTA:SA Windows to ARM addresses
 * in the Android version. GTA-Reversed provides the function names and logic,
 * this scanner finds them in the ARM binary.
 */

#ifndef SIGNATURE_SCANNER_H
#define SIGNATURE_SCANNER_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>
#include <algorithm>

namespace MTA::Android::Signatures
{
    //=========================================================================
    // Pattern Byte Structure
    //=========================================================================

    struct PatternByte
    {
        uint8_t value;
        bool    wildcard;   // True if this byte should match anything

        PatternByte() : value(0), wildcard(true) {}
        PatternByte(uint8_t v) : value(v), wildcard(false) {}

        static PatternByte Wildcard() { return PatternByte(); }
        static PatternByte Exact(uint8_t v) { return PatternByte(v); }
    };

    //=========================================================================
    // Signature Definition
    //=========================================================================

    struct Signature
    {
        std::string              name;           // Function name (from GTA-Reversed)
        std::vector<PatternByte> pattern;        // Byte pattern to search
        int32_t                  offset;         // Offset from match to actual address
        uintptr_t                x86Address;     // Original x86 address (for reference)
        uintptr_t                foundAddress;   // Found ARM address (0 if not found)
        bool                     isThumb;        // ARM mode flag

        Signature() : offset(0), x86Address(0), foundAddress(0), isThumb(false) {}

        Signature(const char* n, uintptr_t x86Addr)
            : name(n), offset(0), x86Address(x86Addr), foundAddress(0), isThumb(false) {}
    };

    //=========================================================================
    // Pattern Parsing Utilities
    //=========================================================================

    /**
     * Parse IDA-style pattern string
     * Format: "48 8B 05 ?? ?? ?? ?? 48 85 C0"
     * Where ?? is a wildcard byte
     */
    inline std::vector<PatternByte> ParsePattern(const char* patternStr)
    {
        std::vector<PatternByte> result;
        const char* p = patternStr;

        while (*p)
        {
            // Skip whitespace
            while (*p == ' ' || *p == '\t') p++;
            if (!*p) break;

            // Check for wildcard
            if (p[0] == '?' && p[1] == '?')
            {
                result.push_back(PatternByte::Wildcard());
                p += 2;
            }
            // Parse hex byte
            else if ((p[0] >= '0' && p[0] <= '9') ||
                     (p[0] >= 'A' && p[0] <= 'F') ||
                     (p[0] >= 'a' && p[0] <= 'f'))
            {
                char hex[3] = {p[0], p[1], 0};
                uint8_t byte = static_cast<uint8_t>(strtol(hex, nullptr, 16));
                result.push_back(PatternByte::Exact(byte));
                p += 2;
            }
            else
            {
                p++;  // Skip unknown character
            }
        }

        return result;
    }

    /**
     * Parse mask-style pattern
     * Pattern: "\x48\x8B\x05\x00\x00\x00\x00\x48\x85\xC0"
     * Mask:    "xxx????xxx"  (x = match, ? = wildcard)
     */
    inline std::vector<PatternByte> ParsePatternWithMask(const char* bytes, const char* mask)
    {
        std::vector<PatternByte> result;
        size_t len = strlen(mask);

        for (size_t i = 0; i < len; i++)
        {
            if (mask[i] == '?')
            {
                result.push_back(PatternByte::Wildcard());
            }
            else
            {
                result.push_back(PatternByte::Exact(static_cast<uint8_t>(bytes[i])));
            }
        }

        return result;
    }

    //=========================================================================
    // Memory Region Definition
    //=========================================================================

    struct MemoryRegion
    {
        uintptr_t   base;
        size_t      size;
        const char* name;   // Optional name (e.g., ".text", "libGTASA.so")

        MemoryRegion() : base(0), size(0), name(nullptr) {}
        MemoryRegion(uintptr_t b, size_t s, const char* n = nullptr)
            : base(b), size(s), name(n) {}
    };

    //=========================================================================
    // Signature Scanner Class
    //=========================================================================

    class SignatureScanner
    {
    public:
        SignatureScanner() = default;

        /**
         * Add a memory region to scan
         */
        void AddRegion(const MemoryRegion& region)
        {
            m_regions.push_back(region);
        }

        /**
         * Add a memory region from library base and size
         */
        void AddRegion(uintptr_t base, size_t size, const char* name = nullptr)
        {
            m_regions.emplace_back(base, size, name);
        }

        /**
         * Scan for a single pattern in all regions
         * Returns first match or 0 if not found
         */
        uintptr_t FindPattern(const std::vector<PatternByte>& pattern, int32_t offset = 0) const
        {
            for (const auto& region : m_regions)
            {
                uintptr_t result = ScanRegion(region, pattern);
                if (result != 0)
                {
                    return result + offset;
                }
            }
            return 0;
        }

        /**
         * Scan for a pattern string (IDA format)
         */
        uintptr_t FindPattern(const char* patternStr, int32_t offset = 0) const
        {
            return FindPattern(ParsePattern(patternStr), offset);
        }

        /**
         * Find all occurrences of a pattern
         */
        std::vector<uintptr_t> FindAllPatterns(const std::vector<PatternByte>& pattern) const
        {
            std::vector<uintptr_t> results;

            for (const auto& region : m_regions)
            {
                ScanRegionAll(region, pattern, results);
            }

            return results;
        }

        /**
         * Find a string reference in the binary
         * Searches for the string, then finds code that references it
         */
        uintptr_t FindStringReference(const char* str) const
        {
            // First, find the string itself
            size_t strLen = strlen(str) + 1;
            std::vector<PatternByte> strPattern;
            for (size_t i = 0; i < strLen; i++)
            {
                strPattern.push_back(PatternByte::Exact(static_cast<uint8_t>(str[i])));
            }

            uintptr_t strAddr = FindPattern(strPattern);
            if (strAddr == 0) return 0;

            // Now find code that references this address
            // This is architecture-specific
            return FindAddressReference(strAddr);
        }

        /**
         * Resolve a signature and store the found address
         */
        bool ResolveSignature(Signature& sig) const
        {
            if (sig.pattern.empty())
            {
                sig.foundAddress = 0;
                return false;
            }

            sig.foundAddress = FindPattern(sig.pattern, sig.offset);
            return sig.foundAddress != 0;
        }

        /**
         * Resolve multiple signatures
         */
        size_t ResolveSignatures(std::vector<Signature>& signatures) const
        {
            size_t resolved = 0;
            for (auto& sig : signatures)
            {
                if (ResolveSignature(sig))
                {
                    resolved++;
                }
            }
            return resolved;
        }

        /**
         * Find address that references another address (for xrefs)
         */
        uintptr_t FindAddressReference(uintptr_t targetAddr) const
        {
            // For ARM32, look for LDR Rd, =targetAddr patterns
            // This typically appears as: LDR Rd, [PC, #offset] where [PC+offset] contains targetAddr

            for (const auto& region : m_regions)
            {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(region.base);
                size_t size = region.size;

                // Search for the target address as a literal in the code
                for (size_t i = 0; i < size - 4; i += 4)
                {
                    uint32_t value = *reinterpret_cast<const uint32_t*>(data + i);
                    if (value == static_cast<uint32_t>(targetAddr))
                    {
                        // Found the literal, now search backwards for LDR that uses it
                        uintptr_t literalAddr = region.base + i;

                        // Check previous instructions for LDR PC-relative
                        for (int j = 4; j <= 4096; j += 4)
                        {
                            if (i < static_cast<size_t>(j)) break;

                            uintptr_t instrAddr = region.base + i - j;
                            uint32_t instr = *reinterpret_cast<const uint32_t*>(data + i - j);

                            // Check for LDR Rd, [PC, #imm] pattern
                            // Format: cond(4) 01(2) I(1) P(1) U(1) B(1) W(1) L(1) 1111(4) Rd(4) imm12(12)
                            if ((instr & 0x0F7F0000) == 0x051F0000)  // LDR Rd, [PC, #-imm]
                            {
                                uint32_t imm = instr & 0xFFF;
                                bool isUp = (instr & 0x00800000) != 0;
                                uintptr_t pcAddr = instrAddr + 8;  // PC is 2 instructions ahead

                                uintptr_t loadAddr = isUp ? (pcAddr + imm) : (pcAddr - imm);
                                if (loadAddr == literalAddr)
                                {
                                    return instrAddr;
                                }
                            }
                        }
                    }
                }
            }

            return 0;
        }

        /**
         * Find function prologue pattern (for function start detection)
         * ARM32: PUSH {r4-rx, lr} is common
         */
        uintptr_t FindFunctionStart(uintptr_t addressInFunction) const
        {
            // Search backwards for common prologue patterns
            for (int offset = 0; offset < 4096; offset += 4)
            {
                uintptr_t addr = addressInFunction - offset;
                uint32_t instr = *reinterpret_cast<uint32_t*>(addr);

                // Check for PUSH {regs, lr} - STMFD SP!, {regs}
                // Pattern: 0xE92D???? where bit 14 (LR) is typically set
                if ((instr & 0xFFFF0000) == 0xE92D0000)
                {
                    // Check if LR is in the register list (common for function start)
                    if (instr & (1 << 14))
                    {
                        return addr;
                    }
                }

                // Check for alternative prologue: SUB SP, SP, #imm
                // Pattern: 0xE24DD??? (SUB SP, SP, #imm)
                if ((instr & 0xFFFFF000) == 0xE24DD000)
                {
                    return addr;
                }
            }

            return 0;
        }

    private:
        std::vector<MemoryRegion> m_regions;

        /**
         * Scan a single region for a pattern
         */
        uintptr_t ScanRegion(const MemoryRegion& region,
                             const std::vector<PatternByte>& pattern) const
        {
            if (pattern.empty() || region.size < pattern.size())
            {
                return 0;
            }

            const uint8_t* data = reinterpret_cast<const uint8_t*>(region.base);
            size_t scanSize = region.size - pattern.size();

            for (size_t i = 0; i <= scanSize; i++)
            {
                if (MatchPattern(data + i, pattern))
                {
                    return region.base + i;
                }
            }

            return 0;
        }

        /**
         * Scan region for all occurrences
         */
        void ScanRegionAll(const MemoryRegion& region,
                          const std::vector<PatternByte>& pattern,
                          std::vector<uintptr_t>& results) const
        {
            if (pattern.empty() || region.size < pattern.size())
            {
                return;
            }

            const uint8_t* data = reinterpret_cast<const uint8_t*>(region.base);
            size_t scanSize = region.size - pattern.size();

            for (size_t i = 0; i <= scanSize; i++)
            {
                if (MatchPattern(data + i, pattern))
                {
                    results.push_back(region.base + i);
                }
            }
        }

        /**
         * Check if pattern matches at given position
         */
        bool MatchPattern(const uint8_t* data, const std::vector<PatternByte>& pattern) const
        {
            for (size_t i = 0; i < pattern.size(); i++)
            {
                if (!pattern[i].wildcard && data[i] != pattern[i].value)
                {
                    return false;
                }
            }
            return true;
        }
    };

    //=========================================================================
    // Address Mapping Database
    //=========================================================================

    /**
     * Maps x86 Windows addresses to ARM Android addresses
     * This is the core translation layer that enables porting hooks
     */
    class AddressMapper
    {
    public:
        struct AddressEntry
        {
            const char* name;           // Function/hook name
            uintptr_t   x86Address;     // Original Windows x86 address
            uintptr_t   armAddress;     // Resolved Android ARM address
            const char* signature;      // Pattern used to find it (optional)
            bool        verified;       // Has been manually verified

            AddressEntry() : name(nullptr), x86Address(0), armAddress(0),
                            signature(nullptr), verified(false) {}
        };

        static AddressMapper& Instance()
        {
            static AddressMapper instance;
            return instance;
        }

        /**
         * Register an x86 address that needs mapping
         */
        void Register(const char* name, uintptr_t x86Addr, const char* signature = nullptr)
        {
            AddressEntry entry;
            entry.name = name;
            entry.x86Address = x86Addr;
            entry.signature = signature;
            entry.armAddress = 0;
            entry.verified = false;

            m_entries[name] = entry;
        }

        /**
         * Set the ARM address for a registered entry
         */
        bool SetARMAddress(const char* name, uintptr_t armAddr, bool verified = false)
        {
            auto it = m_entries.find(name);
            if (it == m_entries.end()) return false;

            it->second.armAddress = armAddr;
            it->second.verified = verified;
            return true;
        }

        /**
         * Get ARM address by name
         */
        uintptr_t GetARMAddress(const char* name) const
        {
            auto it = m_entries.find(name);
            if (it == m_entries.end()) return 0;
            return it->second.armAddress;
        }

        /**
         * Get ARM address by x86 address
         */
        uintptr_t TranslateX86ToARM(uintptr_t x86Addr) const
        {
            for (const auto& pair : m_entries)
            {
                if (pair.second.x86Address == x86Addr)
                {
                    return pair.second.armAddress;
                }
            }
            return 0;
        }

        /**
         * Resolve all registered addresses using a scanner
         */
        size_t ResolveAll(const SignatureScanner& scanner)
        {
            size_t resolved = 0;

            for (auto& pair : m_entries)
            {
                if (pair.second.armAddress != 0) continue;  // Already resolved
                if (pair.second.signature == nullptr) continue;  // No signature

                auto pattern = ParsePattern(pair.second.signature);
                uintptr_t addr = scanner.FindPattern(pattern);

                if (addr != 0)
                {
                    pair.second.armAddress = addr;
                    resolved++;
                }
            }

            return resolved;
        }

        /**
         * Export mappings to JSON for debugging/verification
         */
        std::string ExportJSON() const
        {
            std::string json = "{\n  \"mappings\": [\n";
            bool first = true;

            for (const auto& pair : m_entries)
            {
                if (!first) json += ",\n";
                first = false;

                char buf[512];
                snprintf(buf, sizeof(buf),
                    "    {\n"
                    "      \"name\": \"%s\",\n"
                    "      \"x86\": \"0x%08lX\",\n"
                    "      \"arm\": \"0x%08lX\",\n"
                    "      \"verified\": %s\n"
                    "    }",
                    pair.second.name,
                    static_cast<unsigned long>(pair.second.x86Address),
                    static_cast<unsigned long>(pair.second.armAddress),
                    pair.second.verified ? "true" : "false");
                json += buf;
            }

            json += "\n  ]\n}";
            return json;
        }

        /**
         * Get statistics
         */
        void GetStats(size_t& total, size_t& resolved, size_t& verified) const
        {
            total = m_entries.size();
            resolved = 0;
            verified = 0;

            for (const auto& pair : m_entries)
            {
                if (pair.second.armAddress != 0) resolved++;
                if (pair.second.verified) verified++;
            }
        }

        /**
         * Get all entries for iteration
         */
        const std::unordered_map<std::string, AddressEntry>& GetEntries() const
        {
            return m_entries;
        }

    private:
        std::unordered_map<std::string, AddressEntry> m_entries;

        AddressMapper() = default;
    };

    //=========================================================================
    // Predefined GTA:SA Signatures
    //=========================================================================

    /**
     * Register known GTA:SA function signatures
     * These are derived from GTA-Reversed and MTA:SA x86 hooks
     *
     * Call this during initialization to populate the address mapper
     */
    inline void RegisterGTASASignatures()
    {
        auto& mapper = AddressMapper::Instance();

        // Signature patterns extracted from SA-MP 2.10 libGTASA.so
#if defined(__aarch64__)
        const char* sig_CEntity_Render =
            "ff 43 01 d1 f8 5f 01 a9 f6 57 02 a9 f4 4f 03 a9 "
            "fd 7b 04 a9 fd 03 01 91 f3 03 00 aa 68 12 40 f9";
        const char* sig_CGame_Process =
            "e8 0f 1b fc f9 07 00 f9 f8 5f 01 a9 f6 57 02 a9 "
            "f4 4f 03 a9 fd 7b 04 a9 fd 03 01 91 2c 29 f5 97";
        const char* sig_CPed_ProcessControl =
            "ff 03 02 d1 ed 33 01 6d eb 2b 02 6d e9 23 03 6d "
            "f7 23 00 f9 f6 57 05 a9 f4 4f 06 a9 fd 7b 07 a9 "
            "fd c3 01 91 f3 03 00 aa 60 62 06 91 d1 3e f2 97 "
            "68 22 47 b9 1f 05 00 71 68 07 00 54 74 72 a3 39 "
            "68 16 14 8b 08 31 47 b9 1f 49 00 71 c1 06 00 54";
        const char* sig_CAutomobile_ProcessControl =
            "ff 83 05 d1 ef 3b 0c 6d ed 33 0d 6d eb 2b 0e 6d "
            "e9 23 0f 6d fc 6f 10 a9 fa 67 11 a9 f8 5f 12 a9 "
            "f6 57 13 a9 f4 4f 14 a9 fd 7b 15 a9 fd 43 05 91 "
            "f3 03 00 aa 68 b6 42 f9 15 e5 79 d3 b5 c3 16 b8";
        const char* sig_CWeapon_Fire =
            "ff 43 02 d1 e9 23 02 6d fc 6f 03 a9 fa 67 04 a9 "
            "f8 5f 05 a9 f6 57 06 a9 f4 4f 07 a9 fd 7b 08 a9 "
            "fd 03 02 91 48 33 93 52 28 e3 a7 72 f3 03 00 aa "
            "ff 0b 00 f9 e8 1b 00 b9 60 02 40 b9 f5 03 01 aa "
            "e1 03 00 32 f9 03 06 aa f7 03 05 aa f6 03 04 aa "
            "f8 03 03 aa fa 03 02 aa 61 77 ec 97 f4 03 00 aa";
        const char* sig_CCamera_Process =
            "ff 83 05 d1 ef 3b 0c 6d ed 33 0d 6d eb 2b 0e 6d "
            "e9 23 0f 6d fc 6f 10 a9 fa 67 11 a9 f8 5f 12 a9 "
            "f6 57 13 a9 f4 4f 14 a9 fd 7b 15 a9 fd 43 05 91 "
            "f3 03 00 aa cd bc f5 97 e8 1b 09 32 e8 6f 00 b9";
        const char* sig_CStreaming_RequestModel =
            "f8 5f bc a9 f6 57 01 a9 f4 4f 02 a9 fd 7b 03 a9 "
            "fd c3 00 91 d6 25 00 b0 d6 ea 45 f9 88 02 80 52";
        const char* sig_CWorld_ProcessLineOfSight =
            "ff c3 04 d1 ea 5b 00 fd e9 23 0c 6d fc 6f 0d a9 "
            "fa 67 0e a9 f8 5f 0f a9 f6 57 10 a9 f4 4f 11 a9";
        const char* sig_CPlayerPed_SetupPlayerPed =
            "ea 0f 1b fc e9 23 01 6d f6 57 02 a9 f4 4f 03 a9 "
            "fd 7b 04 a9 fd 03 01 91 f4 03 00 2a 00 33 81 52";
        const char* sig_CPlayerPed_SetInitialState =
            "f7 0f 1c f8 f6 57 01 a9 f4 4f 02 a9 fd 7b 03 a9 "
            "fd c3 00 91 f4 03 01 2a f3 03 00 aa d1 a6 f1 97";
        const char* sig_CPlayerPed_DeactivatePlayerPed =
            "69 14 00 b0 29 d5 43 f9 08 3b 80 52 08 7c 28 9b "
            "20 69 68 f8 53 b5 f1 17";
        const char* sig_CPlayerPed_ReactivatePlayerPed =
            "69 14 00 b0 29 d5 43 f9 08 3b 80 52 08 7c 28 9b "
            "20 69 68 f8 1d a8 f1 17";
        const char* sig_CPed_ClearWeapons =
            "f3 0f 1e f8 fd 7b 01 a9 fd 43 00 91 01 00 80 12 "
            "f3 03 00 aa 27 17 00 94 e0 03 13 aa 78 17 00 94";
        const char* sig_CPed_SetModelIndex =
            "f4 4f be a9 fd 7b 01 a9 fd 43 00 91 f3 03 00 aa "
            "68 16 40 f9 08 01 79 b2 68 16 00 f9 af 6d f2 97";
        const char* sig_CPed_GetWeaponSkill =
            "e8 0f 1d fc f5 07 00 f9 f4 4f 01 a9 fd 7b 02 a9 "
            "fd 83 00 91 f3 03 01 2a 68 5a 00 51 1f 29 00 71";
        const char* sig_CRenderer_RenderOneNonRoad =
            "f4 4f be a9 fd 7b 01 a9 fd 43 00 91 f3 03 00 aa "
            "68 6a 41 39 08 09 00 12 1f 0d 00 71 e1 00 00 54";
        const char* sig_CEntity_CreateRwObject =
            "f5 0f 1d f8 f4 4f 01 a9 fd 7b 02 a9 fd 83 00 91 "
            "f3 03 00 aa 68 16 40 f9 a8 0e 38 36 2a 1c 00 b0";
        const char* sig_CEntity_DeleteRwObject =
            "f5 0f 1d f8 f4 4f 01 a9 fd 7b 02 a9 fd 83 00 91 "
            "f3 03 00 aa 74 12 40 f9 94 01 00 b4 88 02 40 39";
#else
        const char* sig_CEntity_Render =
            "f0 b5 03 af 2d e9 00 07 82 b0 80 46 d8 f8 18 00 "
            "00 28 7a d0 00 78 01 28 0d d1 40 46 aa f5 12 e8";
        const char* sig_CGame_Process =
            "f0 b5 03 af 4d f8 04 bd 2d ed 02 8b 9d f5 b0 e8 "
            "a4 f5 e0 eb a4 f5 0a ea df f8 7c 03 78 44 00 68";
        const char* sig_CPed_ProcessControl =
            "f0 b5 03 af 2d e9 00 0f 81 b0 2d ed 04 8b 8e b0 "
            "04 46 04 f5 9e 70 f6 f4 f2 e8 d4 f8 9c 05 01 28";
        const char* sig_CAutomobile_ProcessControl =
            "f0 b5 03 af 2d e9 00 0f 81 b0 2d ed 10 8b b8 b0 "
            "04 46 d4 f8 30 04 c0 f3 40 65 20 46 37 95 40 f4 "
            "88 ec c6 6a 00 23 94 f8 7c 08 d4 f8 2c 14 d4 f8";
        const char* sig_CWeapon_Fire =
            "f0 b5 03 af 2d e9 00 0f 81 b0 2d ed 04 8b 8e b0 "
            "81 46 49 f6 9a 10 4f f0 00 0b c3 f6 19 70 cd e9";
        const char* sig_CCamera_Process =
            "f0 b5 03 af 2d e9 00 0f 81 b0 2d ed 10 8b ae b0 "
            "04 46 bd f5 fa ec 4f f0 7e 50 04 f1 04 0a 19 90 "
            "00 21 60 69 84 f8 28 10 51 46 00 28 18 bf 00 f1";
        const char* sig_CStreaming_RequestModel =
            "f0 b5 03 af 2d e9 00 07 82 46 83 48 0a eb 8a 06 "
            "0c 46 78 44 00 68 00 eb 86 09 4d 46 15 f8 10 0f";
        const char* sig_CWorld_ProcessLineOfSight =
            "f0 b5 03 af 2d e9 00 0f 81 b0 2d ed 0c 8b a0 b0 "
            "04 46 df f8 c0 0d 0e 46 98 46 78 44 4f f6 ff 71";
        const char* sig_CPlayerPed_SetupPlayerPed =
            "f0 b5 03 af 4d f8 04 bd 2d ed 06 8b 04 46 40 f2 "
            "ac 70 c6 f4 44 ec 21 46 00 22 05 46 dc f4 70 e9";
        const char* sig_CPlayerPed_SetInitialState =
            "f0 b5 03 af 2d e9 00 07 88 46 04 46 d7 f4 98 e9 "
            "4d 48 4f f0 7e 51 a1 46 78 44 00 68 01 60 59 f8";
        const char* sig_CPlayerPed_DeactivatePlayerPed =
            "04 49 4f f4 ca 72 50 43 79 44 09 68 08 58 da f4 "
            "81 bd 00 bf e0 48 1b 00";
        const char* sig_CPlayerPed_ReactivatePlayerPed =
            "04 49 4f f4 ca 72 50 43 79 44 09 68 08 58 d7 f4 "
            "73 bc 00 bf c8 48 1b 00";
        const char* sig_CPed_ClearWeapons =
            "d0 b5 02 af 4f f0 ff 31 04 46 fd f4 6c e9 20 46 "
            "f2 f4 36 ef 04 f2 a4 50 ef f4 60 ea 04 f5 b8 60";
        const char* sig_CPed_SetModelIndex =
            "b0 b5 02 af 04 46 e0 69 40 f0 80 00 e0 61 20 46 "
            "01 f5 70 e9 a0 69 fc f4 de e9 a0 69 04 f2 94 41";
        const char* sig_CPed_GetWeaponSkill =
            "f0 b5 03 af 4d f8 04 8d 0c 46 a4 f1 16 01 0a 29 "
            "32 d8 d0 f8 9c 15 01 29 30 d8 20 46 f7 f4 5e e9";
        const char* sig_CRenderer_RenderOneNonRoad =
            "f0 b5 03 af 4d f8 04 bd 04 46 94 f8 3a 00 00 f0 "
            "07 00 03 28 04 bf d4 f8 4c 04 32 28 64 d0 20 68";
        const char* sig_CEntity_CreateRwObject =
            "f0 b5 03 af 2d e9 00 0b 04 46 26 46 56 f8 1c 0f "
            "10 f0 80 0f 00 f0 a1 80 51 49 80 05 b4 f9 26 20";
        const char* sig_CEntity_DeleteRwObject =
            "f0 b5 03 af 4d f8 04 bd 04 46 a5 69 65 b1 28 78 "
            "02 28 0c d0 01 28 1c d1 28 46 6e 68 9e f5 ca eb";
#endif

        // =====================================================================
        // Core Game Functions
        // =====================================================================

        // CEntity::Render - Main entity rendering
        mapper.Register("CEntity::Render", 0x534310, sig_CEntity_Render);

        // CGame::Process - Main game loop
        mapper.Register("CGame::Process", 0x53BEE0, sig_CGame_Process);

        // CGame::Initialise - Game initialization
        mapper.Register("CGame::Initialise", 0x5BF3A0);

        // =====================================================================
        // Vehicle Functions
        // =====================================================================

        // CAutomobile::ProcessControl - Vehicle physics/control
        mapper.Register("CAutomobile::ProcessControl", 0x6B1880, sig_CAutomobile_ProcessControl);

        // CVehicle::BurstTyre - Tire burst handling
        mapper.Register("CVehicle::BurstTyre", 0x6A32B0);

        // CAutomobile::ProcessControl_VehicleDamage - Collision damage
        mapper.Register("CAutomobile::ProcessControl_VehicleDamage", 0x6B1F3B);

        // CVehicle::DoHeadLightBeam - Headlight rendering
        mapper.Register("CVehicle::DoHeadLightBeam", 0x6E0E20);

        // =====================================================================
        // Pedestrian Functions
        // =====================================================================

        // CPed::ProcessControl - Ped AI/physics
        mapper.Register("CPed::ProcessControl", 0x60EA90, sig_CPed_ProcessControl);

        // CPlayerPed::ProcessControl - Player-specific control
        mapper.Register("CPlayerPed::ProcessControl", 0x60EA90);

        // CPlayerPed::SetupPlayerPed - player initialization
        mapper.Register("CPlayerPed::SetupPlayerPed", 0x60D790, sig_CPlayerPed_SetupPlayerPed);

        // CPlayerPed::SetInitialState - initial ped state setup
        mapper.Register("CPlayerPed::SetInitialState", 0x60CD20, sig_CPlayerPed_SetInitialState);

        // CPlayerPed::DeactivatePlayerPed - temporary disable
        mapper.Register("CPlayerPed::DeactivatePlayerPed", 0x609520, sig_CPlayerPed_DeactivatePlayerPed);

        // CPlayerPed::ReactivatePlayerPed - restore from deactivate
        mapper.Register("CPlayerPed::ReactivatePlayerPed", 0x609540, sig_CPlayerPed_ReactivatePlayerPed);

        // CPed::ClearWeapons - remove ped weapons
        mapper.Register("CPed::ClearWeapons", 0x5E6320, sig_CPed_ClearWeapons);

        // CPed::SetModelIndex - change ped model
        mapper.Register("CPed::SetModelIndex", 0x5E4880, sig_CPed_SetModelIndex);

        // CPed::GetWeaponSkill - weapon skill lookup
        mapper.Register("CPed::GetWeaponSkill", 0x5E3B60, sig_CPed_GetWeaponSkill);

        // CPed::DoFootLanded - Footstep detection
        mapper.Register("CPed::DoFootLanded", 0x5E3D60);

        // =====================================================================
        // Rendering Functions
        // =====================================================================

        // CVisibilityPlugins::RenderWeaponPedsForPC
        mapper.Register("CVisibilityPlugins::RenderWeaponPedsForPC", 0x733080);

        // CRenderer::RenderOneNonRoad - Non-road entity rendering
        mapper.Register("CRenderer::RenderOneNonRoad", 0x553260, sig_CRenderer_RenderOneNonRoad);

        // =====================================================================
        // Collision Functions
        // =====================================================================

        // CPhysical::ProcessCollision
        mapper.Register("CPhysical::ProcessCollision", 0x54DFB0);

        // CWorld::ProcessLineOfSight
        mapper.Register("CWorld::ProcessLineOfSight", 0x56BA00, sig_CWorld_ProcessLineOfSight);

        // =====================================================================
        // Animation Functions
        // =====================================================================

        // CAnimManager::BlendAnimation
        mapper.Register("CAnimManager::BlendAnimation", 0x4D6150);

        // RpAnimBlendClumpGetAssociation
        mapper.Register("RpAnimBlendClumpGetAssociation", 0x4D68B0);

        // =====================================================================
        // Camera Functions
        // =====================================================================

        // CCamera::Process - Camera update
        mapper.Register("CCamera::Process", 0x52B730, sig_CCamera_Process);

        // CCamera::CamControl - Camera control logic
        mapper.Register("CCamera::CamControl", 0x527FA0);

        // =====================================================================
        // Audio Functions
        // =====================================================================

        // CAudioEngine::PreloadMissionAudio
        mapper.Register("CAudioEngine::PreloadMissionAudio", 0x507290);

        // =====================================================================
        // Streaming Functions
        // =====================================================================

        // CStreaming::RequestModel
        mapper.Register("CStreaming::RequestModel", 0x4087E0, sig_CStreaming_RequestModel);

        // CStreaming::LoadAllRequestedModels
        mapper.Register("CStreaming::LoadAllRequestedModels", 0x40EA10);

        // =====================================================================
        // RenderWare Entity Functions
        // =====================================================================

        // CEntity::CreateRwObject - build RW object for entity
        mapper.Register("CEntity::CreateRwObject", 0x533D30, sig_CEntity_CreateRwObject);

        // CEntity::DeleteRwObject - destroy RW object for entity
        mapper.Register("CEntity::DeleteRwObject", 0x534030, sig_CEntity_DeleteRwObject);

        // =====================================================================
        // Task Functions (for multiplayer sync)
        // =====================================================================

        // CTaskSimplePlayerOnFoot::MakeAbortable
        mapper.Register("CTaskSimplePlayerOnFoot::MakeAbortable", 0x68584D);

        // CTaskManager::GetActiveTask
        mapper.Register("CTaskManager::GetActiveTask", 0x681720);

        // =====================================================================
        // Weapon Functions
        // =====================================================================

        // CWeapon::Fire - Core weapon firing path
        mapper.Register("CWeapon::Fire", 0x742300, sig_CWeapon_Fire);

        // Note: Add more signatures as needed during hook migration
        // Each signature should be verified against GTA-Reversed
    }

} // namespace MTA::Android::Signatures

#endif // SIGNATURE_SCANNER_H
