/*
 * MTA:SA Android - Test Activity
 *
 * UI for running subsystem tests and viewing results.
 * Can be launched independently to verify device compatibility.
 */

package com.mtasa.android.test;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import com.mtasa.android.MTANative;

public class TestActivity extends Activity {
    private static final String TAG = "MTA-TestActivity";

    private TextView statusText;
    private TextView deviceInfoText;
    private TextView resultsText;
    private LinearLayout categoryButtons;
    private Button runAllButton;
    private ProgressBar progressBar;
    private ScrollView scrollView;

    private ExecutorService executor;
    private Handler mainHandler;

    // Load native library
    static {
        try {
            System.loadLibrary("mta_android");
            Log.i(TAG, "Native library loaded");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage());
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        executor = Executors.newSingleThreadExecutor();
        mainHandler = new Handler(Looper.getMainLooper());

        createUI();
        loadDeviceInfo();
        loadCategories();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (executor != null) {
            executor.shutdown();
        }
    }

    private void createUI() {
        // Root layout
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(0xFF1A1A2E);
        root.setPadding(32, 48, 32, 32);

        // Title
        TextView title = new TextView(this);
        title.setText("MTA:SA Android Tests");
        title.setTextColor(Color.WHITE);
        title.setTextSize(24);
        title.setTypeface(null, Typeface.BOLD);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, 0, 0, 16);
        root.addView(title);

        // Device info
        deviceInfoText = new TextView(this);
        deviceInfoText.setTextColor(0xFFB0B0B0);
        deviceInfoText.setTextSize(12);
        deviceInfoText.setPadding(0, 0, 0, 24);
        root.addView(deviceInfoText);

        // Status text
        statusText = new TextView(this);
        statusText.setTextColor(0xFF4CAF50);
        statusText.setTextSize(14);
        statusText.setText("Ready to run tests");
        statusText.setPadding(0, 0, 0, 16);
        root.addView(statusText);

        // Progress bar
        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setIndeterminate(false);
        progressBar.setMax(100);
        progressBar.setProgress(0);
        progressBar.setVisibility(View.GONE);
        LinearLayout.LayoutParams progressParams = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, 8);
        progressParams.setMargins(0, 0, 0, 16);
        progressBar.setLayoutParams(progressParams);
        root.addView(progressBar);

        // Run All button
        runAllButton = createButton("Run All Tests", 0xFF4CAF50);
        runAllButton.setOnClickListener(v -> runAllTests());
        root.addView(runAllButton);

        // Quick Test button
        Button quickTestButton = createButton("Quick Test", 0xFF2196F3);
        quickTestButton.setOnClickListener(v -> runQuickTest());
        root.addView(quickTestButton);

        // Server Connection Test button
        Button serverTestButton = createButton("Test Server Connection", 0xFFFF9800);
        serverTestButton.setOnClickListener(v -> runServerConnectionTest());
        root.addView(serverTestButton);

        // Category buttons container
        TextView categoryLabel = new TextView(this);
        categoryLabel.setText("Run by Category:");
        categoryLabel.setTextColor(0xFFB0B0B0);
        categoryLabel.setTextSize(12);
        categoryLabel.setPadding(0, 24, 0, 8);
        root.addView(categoryLabel);

        categoryButtons = new LinearLayout(this);
        categoryButtons.setOrientation(LinearLayout.HORIZONTAL);
        categoryButtons.setPadding(0, 0, 0, 16);

        // Wrap in horizontal scroll for category buttons
        android.widget.HorizontalScrollView hScroll = new android.widget.HorizontalScrollView(this);
        hScroll.addView(categoryButtons);
        LinearLayout.LayoutParams catParams = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        hScroll.setLayoutParams(catParams);
        root.addView(hScroll);

        // Results scroll view
        scrollView = new ScrollView(this);
        scrollView.setBackgroundColor(0xFF0F0F1A);
        LinearLayout.LayoutParams scrollParams = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0f);
        scrollParams.setMargins(0, 16, 0, 0);
        scrollView.setLayoutParams(scrollParams);

        resultsText = new TextView(this);
        resultsText.setTextColor(0xFFE0E0E0);
        resultsText.setTextSize(12);
        resultsText.setTypeface(Typeface.MONOSPACE);
        resultsText.setPadding(16, 16, 16, 16);
        resultsText.setText("Test results will appear here...");
        scrollView.addView(resultsText);

        root.addView(scrollView);

        setContentView(root);
    }

    private Button createButton(String text, int color) {
        Button button = new Button(this);
        button.setText(text);
        button.setTextColor(Color.WHITE);
        button.setBackgroundColor(color);
        button.setPadding(32, 16, 32, 16);

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.setMargins(0, 8, 0, 8);
        button.setLayoutParams(params);

        return button;
    }

    private Button createSmallButton(String text, int color) {
        Button button = new Button(this);
        button.setText(text);
        button.setTextColor(Color.WHITE);
        button.setBackgroundColor(color);
        button.setTextSize(11);
        button.setPadding(16, 8, 16, 8);

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.setMargins(4, 0, 4, 0);
        button.setLayoutParams(params);

        return button;
    }

    private void loadDeviceInfo() {
        executor.execute(() -> {
            try {
                String info = MTATest.getDeviceInfo();
                mainHandler.post(() -> deviceInfoText.setText(info));
            } catch (Exception e) {
                mainHandler.post(() -> deviceInfoText.setText("Device info unavailable"));
            }
        });
    }

    private void loadCategories() {
        executor.execute(() -> {
            try {
                String[] categories = MTATest.getCategories();
                mainHandler.post(() -> {
                    categoryButtons.removeAllViews();
                    if (categories != null) {
                        for (String category : categories) {
                            Button btn = createSmallButton(category, 0xFF607D8B);
                            btn.setOnClickListener(v -> runCategory(category));
                            categoryButtons.addView(btn);
                        }
                    }
                });
            } catch (Exception e) {
                Log.e(TAG, "Failed to load categories", e);
            }
        });
    }

    private void runAllTests() {
        setRunning(true);
        statusText.setText("Running all tests...");
        statusText.setTextColor(0xFFFFEB3B);
        resultsText.setText("");
        progressBar.setProgress(0);
        progressBar.setVisibility(View.VISIBLE);

        executor.execute(() -> {
            try {
                MTATest.TestSummary summary = MTATest.runAllTests();
                mainHandler.post(() -> displayResults(summary));
            } catch (Exception e) {
                mainHandler.post(() -> {
                    setRunning(false);
                    statusText.setText("Error: " + e.getMessage());
                    statusText.setTextColor(0xFFF44336);
                });
            }
        });
    }

    private void runCategory(String category) {
        setRunning(true);
        statusText.setText("Running " + category + " tests...");
        statusText.setTextColor(0xFFFFEB3B);
        resultsText.setText("");

        executor.execute(() -> {
            try {
                MTATest.TestSummary summary = MTATest.runCategory(category);
                mainHandler.post(() -> displayResults(summary));
            } catch (Exception e) {
                mainHandler.post(() -> {
                    setRunning(false);
                    statusText.setText("Error: " + e.getMessage());
                    statusText.setTextColor(0xFFF44336);
                });
            }
        });
    }

    private void runQuickTest() {
        setRunning(true);
        statusText.setText("Running quick test...");
        statusText.setTextColor(0xFFFFEB3B);

        executor.execute(() -> {
            try {
                boolean passed = MTATest.quickTest();
                mainHandler.post(() -> {
                    setRunning(false);
                    if (passed) {
                        statusText.setText("Quick test PASSED - Basic systems OK");
                        statusText.setTextColor(0xFF4CAF50);
                        resultsText.setText("All basic subsystems are working:\n" +
                            "  - Memory allocation\n" +
                            "  - Threading\n" +
                            "  - File access\n\n" +
                            "Run full tests for detailed results.");
                    } else {
                        statusText.setText("Quick test FAILED");
                        statusText.setTextColor(0xFFF44336);
                        resultsText.setText("Some basic systems are not working.\n" +
                            "Check logcat for details.");
                    }
                });
            } catch (Exception e) {
                mainHandler.post(() -> {
                    setRunning(false);
                    statusText.setText("Quick test error: " + e.getMessage());
                    statusText.setTextColor(0xFFF44336);
                });
            }
        });
    }

    private void displayResults(MTATest.TestSummary summary) {
        setRunning(false);
        progressBar.setProgress(100);

        if (summary.isAllPassed()) {
            statusText.setText("All tests passed!");
            statusText.setTextColor(0xFF4CAF50);
        } else {
            statusText.setText(String.format("%d test(s) failed",
                summary.getFailed() + summary.getErrors()));
            statusText.setTextColor(0xFFF44336);
        }

        StringBuilder sb = new StringBuilder();
        sb.append("=== Test Results ===\n\n");
        sb.append(summary.toString()).append("\n\n");

        for (TestResult result : summary.getResults()) {
            sb.append(result.toString()).append("\n");
        }

        resultsText.setText(sb.toString());

        // Scroll to top
        scrollView.post(() -> scrollView.fullScroll(View.FOCUS_UP));
    }

    private void setRunning(boolean running) {
        runAllButton.setEnabled(!running);
        for (int i = 0; i < categoryButtons.getChildCount(); i++) {
            categoryButtons.getChildAt(i).setEnabled(!running);
        }
        progressBar.setVisibility(running ? View.VISIBLE : View.GONE);
    }

    private void runServerConnectionTest() {
        // Test server: VPS with MTA server
        final String serverHost = "37.59.101.35";
        final int serverPort = 22004;

        setRunning(true);
        statusText.setText("Testing server connection...");
        statusText.setTextColor(0xFFFFEB3B);
        resultsText.setText("Connecting to " + serverHost + ":" + serverPort + "...\n");

        executor.execute(() -> {
            StringBuilder results = new StringBuilder();
            results.append("=== Server Connection Test ===\n\n");
            results.append("Target: ").append(serverHost).append(":").append(serverPort).append("\n\n");

            try {
                // Test 1: DNS Resolution
                results.append("1. DNS Resolution:\n");
                long startTime = System.currentTimeMillis();
                String resolvedIP = MTANative.testDNSResolution(serverHost);
                long dnsTime = System.currentTimeMillis() - startTime;
                if (resolvedIP != null && !resolvedIP.isEmpty()) {
                    results.append("   ✓ Resolved to: ").append(resolvedIP).append(" (").append(dnsTime).append("ms)\n\n");
                } else {
                    results.append("   ✗ DNS resolution failed\n\n");
                }

                // Test 2: UDP Connectivity
                results.append("2. UDP Connectivity:\n");
                startTime = System.currentTimeMillis();
                boolean reachable = MTANative.testServerConnectivity(serverHost, serverPort, 5000);
                long connectTime = System.currentTimeMillis() - startTime;
                if (reachable) {
                    results.append("   ✓ Server reachable (").append(connectTime).append("ms)\n\n");
                } else {
                    results.append("   ✗ Server not reachable (timeout after ").append(connectTime).append("ms)\n\n");
                }

                // Test 3: Get detailed results
                results.append("3. Detailed Results:\n");
                String jsonResults = MTANative.getConnectionTestResults();
                if (jsonResults != null && !jsonResults.isEmpty()) {
                    results.append("   ").append(jsonResults.replace(",", ",\n   ")).append("\n\n");
                }

                // Test 4: Try actual connection
                results.append("4. MTA Protocol Connection:\n");
                boolean connected = MTANative.connectToServer(serverHost, serverPort, "AndroidTest", "");
                if (connected) {
                    results.append("   ✓ Connection initiated\n");

                    // Wait for connection with timeout
                    int maxWait = 10;
                    for (int i = 0; i < maxWait; i++) {
                        Thread.sleep(1000);
                        MTANative.processServerConnection();
                        int state = MTANative.getServerConnectionState();
                        results.append("   State: ").append(getStateName(state)).append("\n");

                        if (state == 7) { // CONNECTED
                            results.append("   ✓ Successfully connected to server!\n");
                            break;
                        } else if (state == 9) { // ERROR_STATE
                            results.append("   ✗ Connection error\n");
                            break;
                        }
                    }

                    // Disconnect
                    MTANative.disconnectFromServer();
                    results.append("   Disconnected\n");
                } else {
                    results.append("   ✗ Failed to initiate connection\n");
                }

                final String finalResults = results.toString();
                final boolean success = reachable;
                mainHandler.post(() -> {
                    setRunning(false);
                    if (success) {
                        statusText.setText("Server connection test completed");
                        statusText.setTextColor(0xFF4CAF50);
                    } else {
                        statusText.setText("Server connection test failed");
                        statusText.setTextColor(0xFFF44336);
                    }
                    resultsText.setText(finalResults);
                });

            } catch (Exception e) {
                final String error = e.getMessage();
                results.append("\nError: ").append(error).append("\n");
                final String finalResults = results.toString();
                mainHandler.post(() -> {
                    setRunning(false);
                    statusText.setText("Test error: " + error);
                    statusText.setTextColor(0xFFF44336);
                    resultsText.setText(finalResults);
                });
            }
        });
    }

    private String getStateName(int state) {
        switch (state) {
            case 0: return "DISCONNECTED";
            case 1: return "RESOLVING_DNS";
            case 2: return "CONNECTING";
            case 3: return "WAIT_MOD_NAME";
            case 4: return "SENDING_JOIN";
            case 5: return "WAIT_JOIN_COMPLETE";
            case 6: return "WAIT_JOINED_GAME";
            case 7: return "CONNECTED";
            case 8: return "DISCONNECTING";
            case 9: return "ERROR_STATE";
            default: return "UNKNOWN(" + state + ")";
        }
    }
}
