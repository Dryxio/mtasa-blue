/*
 * MTA:SA Android - Main Activity
 *
 * This is the main entry point for the MTA:SA Android application.
 * Handles the game surface, input events, and lifecycle management.
 */

package com.mtasa.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.Uri;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.os.Vibrator;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MTAActivity extends Activity implements GLSurfaceView.Renderer {
    private static final String TAG = "MTA-Activity";

    // Native library name
    private static final String NATIVE_LIB = "mta_android";

    // Views
    private GLSurfaceView mGLSurfaceView;
    private View mLoadingView;
    private ProgressBar mProgressBar;
    private TextView mStatusText;

    // State
    private boolean mNativeInitialized = false;
    private boolean mSurfaceReady = false;
    private long mLastFrameTime = 0;

    // Network monitoring
    private ConnectivityManager mConnectivityManager;
    private ConnectivityManager.NetworkCallback mNetworkCallback;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "onCreate");

        // Keep screen on
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Fullscreen immersive mode
        setImmersiveMode();

        // Load native library
        try {
            System.loadLibrary(NATIVE_LIB);
            Log.i(TAG, "Native library loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage());
            showErrorAndExit("Failed to load MTA native library");
            return;
        }

        // Create GL surface view
        mGLSurfaceView = new GLSurfaceView(this);
        mGLSurfaceView.setEGLContextClientVersion(3); // OpenGL ES 3.0
        mGLSurfaceView.setRenderer(this);
        mGLSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

        // Set content view
        setContentView(mGLSurfaceView);

        // Setup network monitoring
        setupNetworkMonitoring();

        // Initialize native code
        initializeNative();
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume");

        setImmersiveMode();

        if (mGLSurfaceView != null) {
            mGLSurfaceView.onResume();
        }

        if (mNativeInitialized) {
            MTANative.nativeResume();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause");

        if (mGLSurfaceView != null) {
            mGLSurfaceView.onPause();
        }

        if (mNativeInitialized) {
            MTANative.nativePause();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy");

        // Cleanup network callback
        if (mConnectivityManager != null && mNetworkCallback != null) {
            mConnectivityManager.unregisterNetworkCallback(mNetworkCallback);
        }

        // Shutdown native
        if (mNativeInitialized) {
            MTANative.nativeShutdown();
            mNativeInitialized = false;
        }
    }

    // =========================================================================
    // GLSurfaceView.Renderer Implementation
    // =========================================================================

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        Log.i(TAG, "onSurfaceCreated");
        mSurfaceReady = true;
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        Log.i(TAG, "onSurfaceChanged: " + width + "x" + height);

        if (mNativeInitialized) {
            MTANative.nativeSurfaceChanged(width, height);
        }
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        if (!mNativeInitialized || !mSurfaceReady) {
            return;
        }

        // Calculate delta time
        long currentTime = System.nanoTime();
        float deltaTime = 0.016f; // Default 60 FPS

        if (mLastFrameTime != 0) {
            deltaTime = (currentTime - mLastFrameTime) / 1_000_000_000.0f;
        }
        mLastFrameTime = currentTime;

        // Cap delta time to prevent physics issues
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }

        // Update and render
        MTANative.nativeUpdate(deltaTime);
        MTANative.nativeRender();
    }

    // =========================================================================
    // Input Handling
    // =========================================================================

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!mNativeInitialized) {
            return super.onTouchEvent(event);
        }

        int action = event.getActionMasked();
        int pointerIndex = event.getActionIndex();
        int pointerId = event.getPointerId(pointerIndex);

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                MTANative.nativeTouchEvent(0, pointerId,
                    event.getX(pointerIndex), event.getY(pointerIndex),
                    event.getPressure(pointerIndex));
                break;

            case MotionEvent.ACTION_MOVE:
                // Handle all pointers
                for (int i = 0; i < event.getPointerCount(); i++) {
                    MTANative.nativeTouchEvent(2, event.getPointerId(i),
                        event.getX(i), event.getY(i), event.getPressure(i));
                }
                break;

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                MTANative.nativeTouchEvent(1, pointerId,
                    event.getX(pointerIndex), event.getY(pointerIndex), 0);
                break;

            case MotionEvent.ACTION_CANCEL:
                MTANative.nativeTouchEvent(3, 0, 0, 0, 0);
                break;
        }

        return true;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!mNativeInitialized) {
            return super.onKeyDown(keyCode, event);
        }

        // Handle back button specially
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            // TODO: Show exit confirmation dialog
            return true;
        }

        MTANative.nativeKeyEvent(keyCode, 0, event.getMetaState());
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (!mNativeInitialized) {
            return super.onKeyUp(keyCode, event);
        }

        MTANative.nativeKeyEvent(keyCode, 1, event.getMetaState());
        return true;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (!mNativeInitialized) {
            return super.onGenericMotionEvent(event);
        }

        // Handle gamepad input
        if ((event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
            (event.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {

            int deviceId = event.getDeviceId();

            // Left stick
            MTANative.nativeGamepadAxis(deviceId, 0, event.getAxisValue(MotionEvent.AXIS_X));
            MTANative.nativeGamepadAxis(deviceId, 1, event.getAxisValue(MotionEvent.AXIS_Y));

            // Right stick
            MTANative.nativeGamepadAxis(deviceId, 11, event.getAxisValue(MotionEvent.AXIS_Z));
            MTANative.nativeGamepadAxis(deviceId, 14, event.getAxisValue(MotionEvent.AXIS_RZ));

            // Triggers
            MTANative.nativeGamepadAxis(deviceId, 17, event.getAxisValue(MotionEvent.AXIS_LTRIGGER));
            MTANative.nativeGamepadAxis(deviceId, 18, event.getAxisValue(MotionEvent.AXIS_RTRIGGER));

            return true;
        }

        return super.onGenericMotionEvent(event);
    }

    // =========================================================================
    // Initialization
    // =========================================================================

    private void initializeNative() {
        Log.i(TAG, "Initializing native code...");

        AssetManager assetManager = getAssets();

        boolean result = MTANative.nativeInit(this, assetManager);
        if (!result) {
            Log.e(TAG, "Failed to initialize native code");
            showErrorAndExit("Failed to initialize MTA");
            return;
        }

        mNativeInitialized = true;
        Log.i(TAG, "Native initialization complete");

        // Log version info
        String version = MTANative.nativeGetVersion();
        String buildDate = MTANative.nativeGetBuildDate();
        Log.i(TAG, "MTA Version: " + version + " (" + buildDate + ")");
    }

    // =========================================================================
    // Network Monitoring
    // =========================================================================

    private void setupNetworkMonitoring() {
        mConnectivityManager = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            mNetworkCallback = new ConnectivityManager.NetworkCallback() {
                @Override
                public void onAvailable(Network network) {
                    updateNetworkState();
                }

                @Override
                public void onLost(Network network) {
                    updateNetworkState();
                }

                @Override
                public void onCapabilitiesChanged(Network network, NetworkCapabilities caps) {
                    updateNetworkState();
                }
            };

            NetworkRequest request = new NetworkRequest.Builder()
                .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .build();

            mConnectivityManager.registerNetworkCallback(request, mNetworkCallback);
        }

        // Initial state update
        updateNetworkState();
    }

    private void updateNetworkState() {
        if (!mNativeInitialized) return;

        boolean available = false;
        int type = -1;
        boolean metered = true;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Network network = mConnectivityManager.getActiveNetwork();
            if (network != null) {
                NetworkCapabilities caps = mConnectivityManager.getNetworkCapabilities(network);
                if (caps != null) {
                    available = caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);

                    if (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
                        type = 1; // WiFi
                    } else if (caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) {
                        type = 0; // Mobile
                    } else if (caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) {
                        type = 9; // Ethernet
                    }

                    metered = !caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED);
                }
            }
        }

        MTANative.nativeNetworkStateChanged(available, type, metered);
    }

    // =========================================================================
    // UI Helpers
    // =========================================================================

    private void setImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            );
        }
    }

    private void showErrorAndExit(String message) {
        new AlertDialog.Builder(this)
            .setTitle("Error")
            .setMessage(message)
            .setPositiveButton("Exit", (dialog, which) -> finish())
            .setCancelable(false)
            .show();
    }

    // =========================================================================
    // Server Connection (called from UI)
    // =========================================================================

    public void connectToServer(String host, int port, String nick, String password) {
        if (!mNativeInitialized) {
            Toast.makeText(this, "Not initialized", Toast.LENGTH_SHORT).show();
            return;
        }

        boolean result = MTANative.nativeConnect(host, port, nick, password);
        if (!result) {
            Toast.makeText(this, "Failed to connect", Toast.LENGTH_SHORT).show();
        }
    }

    public void disconnect() {
        if (mNativeInitialized) {
            MTANative.nativeDisconnect();
        }
    }

    public int getConnectionState() {
        if (!mNativeInitialized) return 0;
        return MTANative.nativeGetConnectionState();
    }
}
