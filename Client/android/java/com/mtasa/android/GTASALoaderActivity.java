/*
 * MTA:SA Android - GTA:SA Loader Activity
 *
 * Phase 6: This activity handles launching GTA:SA with MTA integration
 *
 * There are two modes of operation:
 * 1. Modified APK: Our library is already in GTA:SA's APK (recommended)
 * 2. Wrapper Mode: We launch GTA:SA and inject our library via system mechanisms
 *
 * This activity serves as a launcher and configuration UI for MTA features.
 */

package com.mtasa.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

/**
 * Launcher activity for GTA:SA integration
 * Provides UI for configuring MTA features before launching the game
 */
public class GTASALoaderActivity extends Activity {

    private static final String TAG = "MTA-Loader";

    // GTA:SA package name
    private static final String GTASA_PACKAGE = "com.rockstargames.gtasa";
    private static final String GTASA_ACTIVITY = "com.rockstargames.gtasa.GTA";

    // UI Elements
    private TextView statusText;
    private TextView versionText;
    private Button launchButton;
    private Button godModeButton;
    private Button settingsButton;

    // State
    private boolean gtasaInstalled = false;
    private String gtasaVersion = "Unknown";
    private boolean godModeEnabled = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Build UI programmatically (no XML needed)
        setContentView(createUI());

        // Check GTA:SA installation
        checkGTASAInstallation();

        // Initialize native library if available
        initializeNative();
    }

    /**
     * Create the UI layout programmatically
     */
    private View createUI() {
        // Root scroll view
        ScrollView scrollView = new ScrollView(this);
        scrollView.setLayoutParams(new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        ));

        // Main container
        LinearLayout container = new LinearLayout(this);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(32, 32, 32, 32);

        // Title
        TextView title = new TextView(this);
        title.setText("MTA:SA Android");
        title.setTextSize(28);
        title.setPadding(0, 0, 0, 16);
        container.addView(title);

        // Subtitle
        TextView subtitle = new TextView(this);
        subtitle.setText("Phase 6: GTA:SA Integration");
        subtitle.setTextSize(16);
        subtitle.setPadding(0, 0, 0, 32);
        container.addView(subtitle);

        // Status section
        TextView statusLabel = new TextView(this);
        statusLabel.setText("Status");
        statusLabel.setTextSize(18);
        statusLabel.setPadding(0, 16, 0, 8);
        container.addView(statusLabel);

        statusText = new TextView(this);
        statusText.setText("Checking GTA:SA installation...");
        statusText.setTextSize(14);
        statusText.setPadding(16, 8, 16, 8);
        statusText.setBackgroundColor(0x20000000);
        container.addView(statusText);

        // Version info
        versionText = new TextView(this);
        versionText.setText("Version: Detecting...");
        versionText.setTextSize(14);
        versionText.setPadding(16, 8, 16, 16);
        container.addView(versionText);

        // Launch button
        launchButton = new Button(this);
        launchButton.setText("Launch GTA:SA with MTA");
        launchButton.setEnabled(false);
        launchButton.setPadding(16, 16, 16, 16);
        LinearLayout.LayoutParams buttonParams = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        );
        buttonParams.setMargins(0, 16, 0, 8);
        launchButton.setLayoutParams(buttonParams);
        launchButton.setOnClickListener(v -> launchGTASA());
        container.addView(launchButton);

        // God mode button
        godModeButton = new Button(this);
        updateGodModeButton();
        godModeButton.setLayoutParams(buttonParams);
        godModeButton.setOnClickListener(v -> toggleGodMode());
        container.addView(godModeButton);

        // Settings button
        settingsButton = new Button(this);
        settingsButton.setText("Settings");
        settingsButton.setLayoutParams(buttonParams);
        settingsButton.setOnClickListener(v -> openSettings());
        container.addView(settingsButton);

        // Install instructions
        TextView instructions = new TextView(this);
        instructions.setText("\nInstructions:\n\n" +
            "1. GTA:SA Android must be installed\n" +
            "2. Use modified APK with MTA library\n" +
            "3. Launch game through this app\n\n" +
            "See docs/PHASE6_INTEGRATION.md for details.");
        instructions.setTextSize(12);
        instructions.setPadding(0, 32, 0, 16);
        container.addView(instructions);

        // Debug info button
        Button debugButton = new Button(this);
        debugButton.setText("Show Debug Info");
        debugButton.setLayoutParams(buttonParams);
        debugButton.setOnClickListener(v -> showDebugInfo());
        container.addView(debugButton);

        scrollView.addView(container);
        return scrollView;
    }

    /**
     * Check if GTA:SA is installed and get version info
     */
    private void checkGTASAInstallation() {
        try {
            PackageManager pm = getPackageManager();
            PackageInfo info = pm.getPackageInfo(GTASA_PACKAGE, 0);

            gtasaInstalled = true;
            gtasaVersion = info.versionName;

            statusText.setText("GTA:SA is INSTALLED");
            statusText.setBackgroundColor(0x2000FF00);  // Green tint
            versionText.setText("Version: " + gtasaVersion + " (code: " + info.versionCode + ")");

            launchButton.setEnabled(true);

            Log.i(TAG, "GTA:SA found: version " + gtasaVersion);

        } catch (PackageManager.NameNotFoundException e) {
            gtasaInstalled = false;

            statusText.setText("GTA:SA is NOT INSTALLED");
            statusText.setBackgroundColor(0x20FF0000);  // Red tint
            versionText.setText("Please install GTA:SA from Play Store");

            launchButton.setEnabled(false);

            Log.w(TAG, "GTA:SA not found");
        }
    }

    /**
     * Initialize native library
     */
    private void initializeNative() {
        try {
            System.loadLibrary("mta_android");
            Log.i(TAG, "Native library loaded");

            // Get native status
            String version = MTANative.getVersion();
            Log.i(TAG, "MTA version: " + version);

        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage());
        }
    }

    /**
     * Launch GTA:SA with MTA integration
     */
    private void launchGTASA() {
        if (!gtasaInstalled) {
            Toast.makeText(this, "GTA:SA is not installed", Toast.LENGTH_SHORT).show();
            return;
        }

        Log.i(TAG, "Launching GTA:SA...");

        try {
            // Option 1: Launch via package manager (standard way)
            Intent launchIntent = getPackageManager().getLaunchIntentForPackage(GTASA_PACKAGE);

            if (launchIntent != null) {
                // Add MTA flags (these would be read by our injected code)
                launchIntent.putExtra("mta_enabled", true);
                launchIntent.putExtra("mta_god_mode", godModeEnabled);

                Toast.makeText(this, "Launching GTA:SA...", Toast.LENGTH_SHORT).show();
                startActivity(launchIntent);
            } else {
                // Option 2: Launch via explicit component
                Intent explicit = new Intent();
                explicit.setComponent(new ComponentName(GTASA_PACKAGE, GTASA_ACTIVITY));
                explicit.putExtra("mta_enabled", true);
                explicit.putExtra("mta_god_mode", godModeEnabled);
                startActivity(explicit);
            }

        } catch (Exception e) {
            Log.e(TAG, "Failed to launch GTA:SA: " + e.getMessage());
            Toast.makeText(this, "Failed to launch: " + e.getMessage(), Toast.LENGTH_LONG).show();

            showLaunchFailedDialog();
        }
    }

    /**
     * Toggle god mode feature
     */
    private void toggleGodMode() {
        godModeEnabled = !godModeEnabled;
        updateGodModeButton();

        // Update native state
        try {
            if (godModeEnabled) {
                MTANative.enableGodMode();
                Toast.makeText(this, "God Mode ENABLED", Toast.LENGTH_SHORT).show();
            } else {
                MTANative.disableGodMode();
                Toast.makeText(this, "God Mode DISABLED", Toast.LENGTH_SHORT).show();
            }
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Native not available for god mode toggle");
        }
    }

    private void updateGodModeButton() {
        if (godModeEnabled) {
            godModeButton.setText("God Mode: ON");
            godModeButton.setBackgroundColor(0xFF4CAF50);  // Green
        } else {
            godModeButton.setText("God Mode: OFF");
            godModeButton.setBackgroundColor(0xFF9E9E9E);  // Gray
        }
    }

    /**
     * Open settings (placeholder)
     */
    private void openSettings() {
        Toast.makeText(this, "Settings coming soon", Toast.LENGTH_SHORT).show();
    }

    /**
     * Show debug information
     */
    private void showDebugInfo() {
        StringBuilder info = new StringBuilder();

        info.append("=== MTA:SA Android Debug Info ===\n\n");

        // GTA:SA info
        info.append("GTA:SA Installed: ").append(gtasaInstalled).append("\n");
        info.append("GTA:SA Version: ").append(gtasaVersion).append("\n\n");

        // Device info
        info.append("Device: ").append(android.os.Build.MODEL).append("\n");
        info.append("Android: ").append(android.os.Build.VERSION.RELEASE).append("\n");
        info.append("SDK: ").append(android.os.Build.VERSION.SDK_INT).append("\n");
        info.append("ABI: ").append(android.os.Build.SUPPORTED_ABIS[0]).append("\n\n");

        // MTA native info
        try {
            String mtaVersion = MTANative.getVersion();
            info.append("MTA Version: ").append(mtaVersion).append("\n");
            info.append("MTA Initialized: ").append(MTANative.isInitialized()).append("\n");

            // Get native status JSON if available
            try {
                String statusJson = MTANative.getIntegrationStatus();
                info.append("\nNative Status:\n").append(statusJson).append("\n");
            } catch (UnsatisfiedLinkError e) {
                info.append("(Native status not available)\n");
            }

        } catch (UnsatisfiedLinkError e) {
            info.append("MTA Native: NOT LOADED\n");
            info.append("Error: ").append(e.getMessage()).append("\n");
        }

        // Show dialog
        new AlertDialog.Builder(this)
            .setTitle("Debug Information")
            .setMessage(info.toString())
            .setPositiveButton("OK", null)
            .setNeutralButton("Copy", (dialog, which) -> {
                android.content.ClipboardManager clipboard =
                    (android.content.ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
                android.content.ClipData clip =
                    android.content.ClipData.newPlainText("MTA Debug Info", info.toString());
                clipboard.setPrimaryClip(clip);
                Toast.makeText(this, "Copied to clipboard", Toast.LENGTH_SHORT).show();
            })
            .show();
    }

    /**
     * Show dialog when launch fails
     */
    private void showLaunchFailedDialog() {
        new AlertDialog.Builder(this)
            .setTitle("Launch Failed")
            .setMessage("Could not launch GTA:SA.\n\n" +
                "This may be because:\n" +
                "1. GTA:SA APK is not modified\n" +
                "2. MTA library is not injected\n" +
                "3. Permissions issue\n\n" +
                "See docs/PHASE6_INTEGRATION.md for APK modification instructions.")
            .setPositiveButton("OK", null)
            .setNeutralButton("View Docs", (dialog, which) -> {
                // Open documentation (if we had a web view or file viewer)
                Toast.makeText(this, "Check docs/PHASE6_INTEGRATION.md", Toast.LENGTH_LONG).show();
            })
            .show();
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Refresh status when returning from GTA:SA
        checkGTASAInstallation();
    }
}
