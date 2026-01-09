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

        // =====================================================================
        // Core Game Functions
        // =====================================================================

        // CEntity::Render - Main entity rendering
        mapper.Register("CEntity::Render", 0x534310);

        // CGame::Process - Main game loop
        mapper.Register("CGame::Process", 0x53BEE0);

        // CGame::Initialise - Game initialization
        mapper.Register("CGame::Initialise", 0x5BF3A0);

        // =====================================================================
        // Vehicle Functions
        // =====================================================================

        // CAutomobile::ProcessControl - Vehicle physics/control
        mapper.Register("CAutomobile::ProcessControl", 0x6B1880);

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
        mapper.Register("CPed::ProcessControl", 0x60EA90);

        // CPlayerPed::ProcessControl - Player-specific control
        mapper.Register("CPlayerPed::ProcessControl", 0x60EA90);

        // CPed::DoFootLanded - Footstep detection
        mapper.Register("CPed::DoFootLanded", 0x5E3D60);

        // =====================================================================
        // Rendering Functions
        // =====================================================================

        // CVisibilityPlugins::RenderWeaponPedsForPC
        mapper.Register("CVisibilityPlugins::RenderWeaponPedsForPC", 0x733080);

        // CRenderer::RenderOneNonRoad - Non-road entity rendering
        mapper.Register("CRenderer::RenderOneNonRoad", 0x553260);

        // =====================================================================
        // Collision Functions
        // =====================================================================

        // CPhysical::ProcessCollision
        mapper.Register("CPhysical::ProcessCollision", 0x54DFB0);

        // CWorld::ProcessLineOfSight
        mapper.Register("CWorld::ProcessLineOfSight", 0x56BA00);

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
        mapper.Register("CCamera::Process", 0x52B730);

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
        mapper.Register("CStreaming::RequestModel", 0x4087E0);

        // CStreaming::LoadAllRequestedModels
        mapper.Register("CStreaming::LoadAllRequestedModels", 0x40EA10);

        // =====================================================================
        // Task Functions (for multiplayer sync)
        // =====================================================================

        // CTaskSimplePlayerOnFoot::MakeAbortable
        mapper.Register("CTaskSimplePlayerOnFoot::MakeAbortable", 0x68584D);

        // CTaskManager::GetActiveTask
        mapper.Register("CTaskManager::GetActiveTask", 0x681720);

        // Note: Add more signatures as needed during hook migration
        // Each signature should be verified against GTA-Reversed
    }

} // namespace MTA::Android::Signatures

#endif // SIGNATURE_SCANNER_H
