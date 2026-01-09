/*
 * MTA:SA Android - Test Result
 *
 * Java class representing a single test result from native code.
 */

package com.mtasa.android.test;

public class TestResult {
    // Status constants matching native TestStatus enum
    public static final int STATUS_PASSED = 0;
    public static final int STATUS_FAILED = 1;
    public static final int STATUS_SKIPPED = 2;
    public static final int STATUS_ERROR = 3;

    private String name;
    private int status;
    private String message;
    private float durationMs;

    /**
     * Constructor called from native code
     */
    public TestResult(String name, int status, String message, float durationMs) {
        this.name = name;
        this.status = status;
        this.message = message;
        this.durationMs = durationMs;
    }

    public String getName() {
        return name;
    }

    public int getStatus() {
        return status;
    }

    public String getMessage() {
        return message;
    }

    public float getDurationMs() {
        return durationMs;
    }

    public boolean isPassed() {
        return status == STATUS_PASSED;
    }

    public boolean isFailed() {
        return status == STATUS_FAILED;
    }

    public boolean isSkipped() {
        return status == STATUS_SKIPPED;
    }

    public boolean isError() {
        return status == STATUS_ERROR;
    }

    public String getStatusString() {
        switch (status) {
            case STATUS_PASSED:  return "PASS";
            case STATUS_FAILED:  return "FAIL";
            case STATUS_SKIPPED: return "SKIP";
            case STATUS_ERROR:   return "ERROR";
            default: return "UNKNOWN";
        }
    }

    public int getStatusColor() {
        switch (status) {
            case STATUS_PASSED:  return 0xFF4CAF50; // Green
            case STATUS_FAILED:  return 0xFFF44336; // Red
            case STATUS_SKIPPED: return 0xFFFF9800; // Orange
            case STATUS_ERROR:   return 0xFF9C27B0; // Purple
            default: return 0xFF9E9E9E; // Gray
        }
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("[").append(getStatusString()).append("] ");
        sb.append(name);
        sb.append(" (").append(String.format("%.2f", durationMs)).append("ms)");
        if (message != null && !message.isEmpty()) {
            sb.append(" - ").append(message);
        }
        return sb.toString();
    }
}
