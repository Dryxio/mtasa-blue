/*
 * MTA:SA Android - Test Harness
 *
 * Framework for running subsystem tests without requiring GTA:SA.
 * Validates that each component works correctly on the target device.
 */

#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <sstream>

#ifdef __ANDROID__
#include <android/log.h>
#define TEST_LOG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "MTA-Test", fmt, ##__VA_ARGS__)
#define TEST_ERR(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, "MTA-Test", fmt, ##__VA_ARGS__)
#else
#include <cstdio>
#define TEST_LOG(fmt, ...) printf("[TEST] " fmt "\n", ##__VA_ARGS__)
#define TEST_ERR(fmt, ...) fprintf(stderr, "[TEST-ERR] " fmt "\n", ##__VA_ARGS__)
#endif

namespace MTA::Android::Test
{

//=============================================================================
// Test Result
//=============================================================================

enum class TestStatus
{
    Passed,
    Failed,
    Skipped,
    Error
};

struct TestResult
{
    std::string name;
    TestStatus status;
    std::string message;
    float durationMs;

    TestResult()
        : status(TestStatus::Skipped), durationMs(0) {}

    TestResult(const std::string& n, TestStatus s, const std::string& msg = "", float dur = 0)
        : name(n), status(s), message(msg), durationMs(dur) {}

    const char* GetStatusString() const
    {
        switch (status)
        {
            case TestStatus::Passed:  return "PASS";
            case TestStatus::Failed:  return "FAIL";
            case TestStatus::Skipped: return "SKIP";
            case TestStatus::Error:   return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

//=============================================================================
// Test Case
//=============================================================================

class TestCase
{
public:
    using TestFunc = std::function<TestResult()>;

    TestCase(const std::string& name, const std::string& category, TestFunc func)
        : m_name(name), m_category(category), m_func(func) {}

    TestResult Run()
    {
        auto start = std::chrono::high_resolution_clock::now();

        TestResult result;
        try
        {
            result = m_func();
            result.name = m_name;
        }
        catch (const std::exception& e)
        {
            result.name = m_name;
            result.status = TestStatus::Error;
            result.message = std::string("Exception: ") + e.what();
        }
        catch (...)
        {
            result.name = m_name;
            result.status = TestStatus::Error;
            result.message = "Unknown exception";
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.durationMs = std::chrono::duration<float, std::milli>(end - start).count();

        return result;
    }

    const std::string& GetName() const { return m_name; }
    const std::string& GetCategory() const { return m_category; }

private:
    std::string m_name;
    std::string m_category;
    TestFunc m_func;
};

//=============================================================================
// Test Harness
//=============================================================================

class TestHarness
{
public:
    static TestHarness& Instance()
    {
        static TestHarness instance;
        return instance;
    }

    // Register a test
    void RegisterTest(const std::string& name, const std::string& category,
                      TestCase::TestFunc func)
    {
        m_tests.emplace_back(name, category, func);
    }

    // Run all tests
    std::vector<TestResult> RunAll()
    {
        std::vector<TestResult> results;
        results.reserve(m_tests.size());

        TEST_LOG("========================================");
        TEST_LOG("  MTA:SA Android Test Suite");
        TEST_LOG("========================================");
        TEST_LOG("Running %zu tests...", m_tests.size());
        TEST_LOG("");

        for (auto& test : m_tests)
        {
            TEST_LOG("[%s] %s...", test.GetCategory().c_str(), test.GetName().c_str());

            TestResult result = test.Run();
            results.push_back(result);

            TEST_LOG("  -> %s (%.2fms) %s",
                     result.GetStatusString(),
                     result.durationMs,
                     result.message.c_str());
        }

        // Summary
        int passed = 0, failed = 0, skipped = 0, errors = 0;
        for (const auto& r : results)
        {
            switch (r.status)
            {
                case TestStatus::Passed:  passed++; break;
                case TestStatus::Failed:  failed++; break;
                case TestStatus::Skipped: skipped++; break;
                case TestStatus::Error:   errors++; break;
            }
        }

        TEST_LOG("");
        TEST_LOG("========================================");
        TEST_LOG("  Results: %d passed, %d failed, %d skipped, %d errors",
                 passed, failed, skipped, errors);
        TEST_LOG("========================================");

        return results;
    }

    // Run tests by category
    std::vector<TestResult> RunCategory(const std::string& category)
    {
        std::vector<TestResult> results;

        TEST_LOG("Running tests for category: %s", category.c_str());

        for (auto& test : m_tests)
        {
            if (test.GetCategory() == category)
            {
                TEST_LOG("[%s] %s...", test.GetCategory().c_str(), test.GetName().c_str());

                TestResult result = test.Run();
                results.push_back(result);

                TEST_LOG("  -> %s (%.2fms) %s",
                         result.GetStatusString(),
                         result.durationMs,
                         result.message.c_str());
            }
        }

        return results;
    }

    // Get test count
    size_t GetTestCount() const { return m_tests.size(); }

    // Get categories
    std::vector<std::string> GetCategories() const
    {
        std::vector<std::string> categories;
        for (const auto& test : m_tests)
        {
            bool found = false;
            for (const auto& cat : categories)
            {
                if (cat == test.GetCategory())
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                categories.push_back(test.GetCategory());
        }
        return categories;
    }

    // Generate report string
    std::string GenerateReport(const std::vector<TestResult>& results)
    {
        std::stringstream ss;

        ss << "MTA:SA Android Test Report\n";
        ss << "==========================\n\n";

        int passed = 0, failed = 0, skipped = 0, errors = 0;
        float totalTime = 0;

        for (const auto& r : results)
        {
            ss << "[" << r.GetStatusString() << "] " << r.name;
            ss << " (" << r.durationMs << "ms)";
            if (!r.message.empty())
                ss << " - " << r.message;
            ss << "\n";

            totalTime += r.durationMs;

            switch (r.status)
            {
                case TestStatus::Passed:  passed++; break;
                case TestStatus::Failed:  failed++; break;
                case TestStatus::Skipped: skipped++; break;
                case TestStatus::Error:   errors++; break;
            }
        }

        ss << "\n";
        ss << "Summary\n";
        ss << "-------\n";
        ss << "Total:   " << results.size() << "\n";
        ss << "Passed:  " << passed << "\n";
        ss << "Failed:  " << failed << "\n";
        ss << "Skipped: " << skipped << "\n";
        ss << "Errors:  " << errors << "\n";
        ss << "Time:    " << totalTime << "ms\n";

        return ss.str();
    }

private:
    TestHarness() = default;
    std::vector<TestCase> m_tests;
};

//=============================================================================
// Test Registration Macro
//=============================================================================

#define REGISTER_TEST(category, name) \
    static struct TestRegistrar_##category##_##name { \
        TestRegistrar_##category##_##name() { \
            MTA::Android::Test::TestHarness::Instance().RegisterTest( \
                #name, #category, Test_##category##_##name); \
        } \
    } g_testRegistrar_##category##_##name; \
    static MTA::Android::Test::TestResult Test_##category##_##name()

//=============================================================================
// Assertion Helpers
//=============================================================================

inline TestResult TestPass(const std::string& msg = "")
{
    return TestResult("", TestStatus::Passed, msg);
}

inline TestResult TestFail(const std::string& msg)
{
    return TestResult("", TestStatus::Failed, msg);
}

inline TestResult TestSkip(const std::string& msg)
{
    return TestResult("", TestStatus::Skipped, msg);
}

#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) return TestFail(msg)

#define ASSERT_FALSE(cond, msg) \
    if (cond) return TestFail(msg)

#define ASSERT_EQ(a, b, msg) \
    if ((a) != (b)) return TestFail(msg)

#define ASSERT_NE(a, b, msg) \
    if ((a) == (b)) return TestFail(msg)

#define ASSERT_GT(a, b, msg) \
    if (!((a) > (b))) return TestFail(msg)

#define ASSERT_GE(a, b, msg) \
    if (!((a) >= (b))) return TestFail(msg)

#define ASSERT_LT(a, b, msg) \
    if (!((a) < (b))) return TestFail(msg)

#define ASSERT_LE(a, b, msg) \
    if (!((a) <= (b))) return TestFail(msg)

} // namespace MTA::Android::Test

#endif // TEST_HARNESS_H
