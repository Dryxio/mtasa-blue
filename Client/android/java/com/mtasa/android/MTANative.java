/*
 * MTA:SA Android - Native Interface
 *
 * Declares native methods implemented in C++ (MTANative.cpp).
 * This class provides the Java-side interface to the native MTA code.
 */

package com.mtasa.android;

import android.content.Context;
import android.content.res.AssetManager;

public class MTANative {
    // =========================================================================
    // Core Lifecycle
    // =========================================================================

    /**
     * Initialize native MTA client
     * @param context Application context
     * @param assetManager Asset manager for reading APK resources
     * @return true if initialization succeeded
     */
    public static native boolean nativeInit(Context context, AssetManager assetManager);

    /**
     * Shutdown native client and release resources
     */
    public static native void nativeShutdown();

    /**
     * Main game loop update
     * @param deltaTime Time since last frame in seconds
     */
    public static native void nativeUpdate(float deltaTime);

    /**
     * Render current frame
     */
    public static native void nativeRender();

    /**
     * Called when GL surface is created or resized
     * @param width Surface width in pixels
     * @param height Surface height in pixels
     */
    public static native void nativeSurfaceChanged(int width, int height);

    /**
     * Called when GL surface is destroyed
     */
    public static native void nativeSurfaceDestroyed();

    /**
     * Called when activity is paused
     */
    public static native void nativePause();

    /**
     * Called when activity is resumed
     */
    public static native void nativeResume();

    // =========================================================================
    // Input Events
    // =========================================================================

    /**
     * Touch screen event
     * @param action Motion event action (0=down, 1=up, 2=move, 3=cancel)
     * @param pointerId Touch pointer ID
     * @param x X coordinate
     * @param y Y coordinate
     * @param pressure Touch pressure (0-1)
     */
    public static native void nativeTouchEvent(int action, int pointerId,
                                               float x, float y, float pressure);

    /**
     * Keyboard event
     * @param keyCode Android key code
     * @param action Key action (0=down, 1=up)
     * @param metaState Meta key state flags
     */
    public static native void nativeKeyEvent(int keyCode, int action, int metaState);

    /**
     * Gamepad connected
     * @param deviceId Device ID
     * @param name Device name
     */
    public static native void nativeGamepadConnected(int deviceId, String name);

    /**
     * Gamepad disconnected
     * @param deviceId Device ID
     */
    public static native void nativeGamepadDisconnected(int deviceId);

    /**
     * Gamepad button event
     * @param deviceId Device ID
     * @param button Button code
     * @param pressed True if pressed
     */
    public static native void nativeGamepadButton(int deviceId, int button, boolean pressed);

    /**
     * Gamepad axis event
     * @param deviceId Device ID
     * @param axis Axis code
     * @param value Axis value (-1 to 1)
     */
    public static native void nativeGamepadAxis(int deviceId, int axis, float value);

    // =========================================================================
    // Network
    // =========================================================================

    /**
     * Network connectivity state changed
     * @param available True if network is available
     * @param type Network type (0=mobile, 1=wifi, 9=ethernet)
     * @param metered True if connection is metered
     */
    public static native void nativeNetworkStateChanged(boolean available, int type, boolean metered);

    // =========================================================================
    // Server Connection
    // =========================================================================

    /**
     * Connect to a server
     * @param host Server hostname or IP
     * @param port Server port
     * @param nick Player nickname
     * @param password Server password (empty string if none)
     * @return true if connection initiated
     */
    public static native boolean nativeConnect(String host, int port,
                                               String nick, String password);

    /**
     * Disconnect from current server
     */
    public static native void nativeDisconnect();

    /**
     * Get current connection state
     * @return 0=disconnected, 1=connecting, 2=connected
     */
    public static native int nativeGetConnectionState();

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * Set path to GTA:SA game data
     * @param path Absolute path to game data directory
     */
    public static native void nativeSetGameDataPath(String path);

    /**
     * Get MTA version string
     * @return Version string (e.g., "1.6.0-android")
     */
    public static native String nativeGetVersion();

    /**
     * Get build date
     * @return Build date string
     */
    public static native String nativeGetBuildDate();

    // =========================================================================
    // Simplified API (wraps native methods)
    // =========================================================================

    /**
     * Initialize the MTA native library
     * This is called automatically in JNI_OnLoad, but can be called manually
     * @return true if initialization succeeded
     */
    public static native boolean initialize();

    /**
     * Check if MTA native library is initialized
     * @return true if initialized
     */
    public static native boolean isInitialized();

    /**
     * Get MTA version string
     * @return Version string
     */
    public static native String getVersion();

    /**
     * Get address mappings as JSON (for debugging)
     * @return JSON string with address mappings
     */
    public static native String getAddressMappings();

    // =========================================================================
    // GTA:SA Integration (Phase 6)
    // =========================================================================

    /**
     * Initialize GTA:SA integration
     * Call this after libGTASA.so is loaded
     * @return true if integration initialized
     */
    public static native boolean initGTASAIntegration();

    /**
     * Get integration status as JSON
     * @return JSON string with integration status
     */
    public static native String getIntegrationStatus();

    /**
     * Enable god mode (player invincibility)
     * @return true if enabled successfully
     */
    public static native boolean enableGodMode();

    /**
     * Disable god mode
     */
    public static native void disableGodMode();

    /**
     * Toggle god mode
     * @return new state (true = enabled)
     */
    public static native boolean toggleGodMode();

    /**
     * Check if god mode is currently enabled
     * @return true if god mode is on
     */
    public static native boolean isGodModeEnabled();

    /**
     * Get detected GTA:SA version
     * @return Version string or "Unknown"
     */
    public static native String getGTASAVersion();

    /**
     * Check if GTA:SA library is loaded and valid
     * @return true if game library is ready
     */
    public static native boolean isGameLibraryReady();
}
