/*
 * MTA:SA Android - Test Interface
 *
 * Java interface for running native subsystem tests.
 */

package com.mtasa.android.test;

import android.util.Log;
import java.util.ArrayList;
import java.util.List;

public class MTATest {
    private static final String TAG = "MTA-Test";

    // =========================================================================
    // Native Methods
    // =========================================================================

    /**
     * Get the number of registered tests
     */
    public static native int nativeGetTestCount();

    /**
     * Get list of test categories
     */
    public static native String[] nativeGetCategories();

    /**
     * Run all tests and return results
     */
    public static native TestResult[] nativeRunAllTests();

    /**
     * Run tests in a specific category
     */
    public static native TestResult[] nativeRunCategory(String category);

    /**
     * Get test report as string
     */
    public static native String nativeGetReport(TestResult[] results);

    /**
     * Quick self-test - returns true if basic systems work
     */
    public static native boolean nativeQuickTest();

    /**
     * Get device info string
     */
    public static native String nativeGetDeviceInfo();

    // =========================================================================
    // Java Interface
    // =========================================================================

    /**
     * Run all tests and return summary
     */
    public static TestSummary runAllTests() {
        Log.i(TAG, "Starting test suite...");

        TestResult[] results = nativeRunAllTests();
        if (results == null) {
            Log.e(TAG, "No test results returned");
            return new TestSummary(new TestResult[0]);
        }

        TestSummary summary = new TestSummary(results);
        Log.i(TAG, summary.toString());

        return summary;
    }

    /**
     * Run tests in a category
     */
    public static TestSummary runCategory(String category) {
        Log.i(TAG, "Running category: " + category);

        TestResult[] results = nativeRunCategory(category);
        if (results == null) {
            return new TestSummary(new TestResult[0]);
        }

        return new TestSummary(results);
    }

    /**
     * Get available categories
     */
    public static String[] getCategories() {
        return nativeGetCategories();
    }

    /**
     * Quick health check
     */
    public static boolean quickTest() {
        return nativeQuickTest();
    }

    /**
     * Get device information
     */
    public static String getDeviceInfo() {
        return nativeGetDeviceInfo();
    }

    // =========================================================================
    // Test Summary
    // =========================================================================

    public static class TestSummary {
        private TestResult[] results;
        private int passed;
        private int failed;
        private int skipped;
        private int errors;
        private float totalDurationMs;

        public TestSummary(TestResult[] results) {
            this.results = results;
            calculateStats();
        }

        private void calculateStats() {
            passed = 0;
            failed = 0;
            skipped = 0;
            errors = 0;
            totalDurationMs = 0;

            for (TestResult result : results) {
                totalDurationMs += result.getDurationMs();

                switch (result.getStatus()) {
                    case TestResult.STATUS_PASSED:
                        passed++;
                        break;
                    case TestResult.STATUS_FAILED:
                        failed++;
                        break;
                    case TestResult.STATUS_SKIPPED:
                        skipped++;
                        break;
                    case TestResult.STATUS_ERROR:
                        errors++;
                        break;
                }
            }
        }

        public TestResult[] getResults() { return results; }
        public int getTotal() { return results.length; }
        public int getPassed() { return passed; }
        public int getFailed() { return failed; }
        public int getSkipped() { return skipped; }
        public int getErrors() { return errors; }
        public float getTotalDurationMs() { return totalDurationMs; }

        public boolean isAllPassed() {
            return failed == 0 && errors == 0;
        }

        public List<TestResult> getFailedTests() {
            List<TestResult> list = new ArrayList<>();
            for (TestResult r : results) {
                if (r.isFailed() || r.isError()) {
                    list.add(r);
                }
            }
            return list;
        }

        @Override
        public String toString() {
            return String.format(
                "Tests: %d total, %d passed, %d failed, %d skipped, %d errors (%.1fms)",
                getTotal(), passed, failed, skipped, errors, totalDurationMs
            );
        }
    }
}
