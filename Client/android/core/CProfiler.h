/*
 * MTA:SA Android - Performance Profiler
 *
 * Simple profiling system for measuring performance of various subsystems.
 * Useful for identifying bottlenecks during device testing.
 */

#ifndef CPROFILER_H
#define CPROFILER_H

#include <cstdint>
#include <string>
#include <array>
#include <chrono>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace MTA::Android
{

//=============================================================================
// Profiler Categories
//=============================================================================

enum class ProfileCategory
{
    Frame,          // Total frame time
    Input,          // Input processing
    Network,        // Network operations
    GameLogic,      // Game state updates
    Render,         // Rendering
    RenderSetup,    // Render state setup
    RenderWorld,    // World rendering
    RenderUI,       // UI rendering
    Physics,        // Physics simulation
    Scripts,        // Lua script execution
    Streaming,      // Resource streaming
    Audio,          // Audio processing

    COUNT
};

//=============================================================================
// Profile Sample
//=============================================================================

struct ProfileSample
{
    float currentMs;      // Current frame time in ms
    float averageMs;      // Running average
    float minMs;          // Minimum recorded
    float maxMs;          // Maximum recorded
    uint32_t callCount;   // Number of calls this frame
    float totalMs;        // Total time this frame

    ProfileSample()
        : currentMs(0), averageMs(0), minMs(999999), maxMs(0), callCount(0), totalMs(0) {}

    void Reset()
    {
        currentMs = totalMs;
        callCount = 0;
        totalMs = 0;

        // Update running average (exponential moving average)
        if (averageMs == 0)
            averageMs = currentMs;
        else
            averageMs = averageMs * 0.95f + currentMs * 0.05f;

        // Update min/max
        if (currentMs > 0)
        {
            if (currentMs < minMs) minMs = currentMs;
            if (currentMs > maxMs) maxMs = currentMs;
        }
    }
};

//=============================================================================
// Scoped Timer
//=============================================================================

class ScopedProfiler
{
public:
    ScopedProfiler(ProfileCategory category);
    ~ScopedProfiler();

private:
    ProfileCategory m_category;
    std::chrono::high_resolution_clock::time_point m_startTime;
};

//=============================================================================
// CProfiler - Main Profiler Class
//=============================================================================

class CProfiler
{
public:
    static CProfiler& Instance();

    // Enable/disable profiling
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Frame management
    void BeginFrame();
    void EndFrame();

    // Manual timing
    void BeginSection(ProfileCategory category);
    void EndSection(ProfileCategory category);

    // Get results
    const ProfileSample& GetSample(ProfileCategory category) const;
    float GetFrameTime() const { return m_samples[static_cast<int>(ProfileCategory::Frame)].currentMs; }
    float GetFPS() const { return m_fps; }

    // Logging
    void LogResults();
    void LogCategory(ProfileCategory category);

    // Get category name
    static const char* GetCategoryName(ProfileCategory category);

private:
    CProfiler();

    bool m_enabled;
    std::array<ProfileSample, static_cast<int>(ProfileCategory::COUNT)> m_samples;
    std::array<std::chrono::high_resolution_clock::time_point, static_cast<int>(ProfileCategory::COUNT)> m_startTimes;

    // Frame timing
    std::chrono::high_resolution_clock::time_point m_frameStartTime;
    float m_fps;
    float m_fpsAccumulator;
    int m_fpsFrameCount;

    // Frame count
    uint64_t m_totalFrames;
};

//=============================================================================
// Profiler Macros
//=============================================================================

#ifdef MTA_PROFILER_ENABLED
    #define PROFILE_SCOPE(category) \
        MTA::Android::ScopedProfiler _profiler_##category(MTA::Android::ProfileCategory::category)
    #define PROFILE_BEGIN(category) \
        MTA::Android::CProfiler::Instance().BeginSection(MTA::Android::ProfileCategory::category)
    #define PROFILE_END(category) \
        MTA::Android::CProfiler::Instance().EndSection(MTA::Android::ProfileCategory::category)
#else
    #define PROFILE_SCOPE(category)
    #define PROFILE_BEGIN(category)
    #define PROFILE_END(category)
#endif

//=============================================================================
// Inline Implementations
//=============================================================================

inline CProfiler& CProfiler::Instance()
{
    static CProfiler instance;
    return instance;
}

inline CProfiler::CProfiler()
    : m_enabled(true)
    , m_fps(0)
    , m_fpsAccumulator(0)
    , m_fpsFrameCount(0)
    , m_totalFrames(0)
{
}

inline void CProfiler::BeginFrame()
{
    if (!m_enabled) return;

    m_frameStartTime = std::chrono::high_resolution_clock::now();
    BeginSection(ProfileCategory::Frame);
}

inline void CProfiler::EndFrame()
{
    if (!m_enabled) return;

    EndSection(ProfileCategory::Frame);

    // Reset all samples for next frame
    for (auto& sample : m_samples)
    {
        sample.Reset();
    }

    // Update FPS
    m_fpsFrameCount++;
    m_fpsAccumulator += m_samples[static_cast<int>(ProfileCategory::Frame)].currentMs;

    if (m_fpsAccumulator >= 1000.0f)
    {
        m_fps = m_fpsFrameCount * 1000.0f / m_fpsAccumulator;
        m_fpsFrameCount = 0;
        m_fpsAccumulator = 0;
    }

    m_totalFrames++;
}

inline void CProfiler::BeginSection(ProfileCategory category)
{
    if (!m_enabled) return;

    int idx = static_cast<int>(category);
    m_startTimes[idx] = std::chrono::high_resolution_clock::now();
}

inline void CProfiler::EndSection(ProfileCategory category)
{
    if (!m_enabled) return;

    int idx = static_cast<int>(category);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - m_startTimes[idx]).count();

    float ms = duration / 1000.0f;
    m_samples[idx].totalMs += ms;
    m_samples[idx].callCount++;
}

inline const ProfileSample& CProfiler::GetSample(ProfileCategory category) const
{
    return m_samples[static_cast<int>(category)];
}

inline const char* CProfiler::GetCategoryName(ProfileCategory category)
{
    static const char* names[] = {
        "Frame",
        "Input",
        "Network",
        "GameLogic",
        "Render",
        "RenderSetup",
        "RenderWorld",
        "RenderUI",
        "Physics",
        "Scripts",
        "Streaming",
        "Audio"
    };

    int idx = static_cast<int>(category);
    if (idx >= 0 && idx < static_cast<int>(ProfileCategory::COUNT))
        return names[idx];
    return "Unknown";
}

inline void CProfiler::LogResults()
{
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "MTA-Profiler",
        "=== Frame Profile (%.1f FPS, %.2fms) ===",
        m_fps, m_samples[static_cast<int>(ProfileCategory::Frame)].currentMs);

    for (int i = 0; i < static_cast<int>(ProfileCategory::COUNT); i++)
    {
        const auto& sample = m_samples[i];
        if (sample.currentMs > 0.01f)
        {
            __android_log_print(ANDROID_LOG_INFO, "MTA-Profiler",
                "  %s: %.2fms (avg: %.2f, min: %.2f, max: %.2f)",
                GetCategoryName(static_cast<ProfileCategory>(i)),
                sample.currentMs, sample.averageMs, sample.minMs, sample.maxMs);
        }
    }
#endif
}

inline void CProfiler::LogCategory(ProfileCategory category)
{
#ifdef __ANDROID__
    const auto& sample = GetSample(category);
    __android_log_print(ANDROID_LOG_INFO, "MTA-Profiler",
        "%s: %.2fms", GetCategoryName(category), sample.currentMs);
#endif
}

//=============================================================================
// ScopedProfiler Implementation
//=============================================================================

inline ScopedProfiler::ScopedProfiler(ProfileCategory category)
    : m_category(category)
{
    CProfiler::Instance().BeginSection(category);
}

inline ScopedProfiler::~ScopedProfiler()
{
    CProfiler::Instance().EndSection(m_category);
}

} // namespace MTA::Android

#endif // CPROFILER_H
