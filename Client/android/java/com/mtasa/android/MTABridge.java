/*
 * MTA:SA Android - Java Bridge
 *
 * Provides utility methods that can be called from native C++ code.
 * Used for Android-specific features that require Java APIs.
 */

package com.mtasa.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.Log;
import android.view.inputmethod.InputMethodManager;
import android.widget.Toast;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class MTABridge {
    private static final String TAG = "MTA-Bridge";

    // =========================================================================
    // Toast Messages
    // =========================================================================

    /**
     * Show a toast message
     * @param context Application context
     * @param message Message to display
     */
    public static void showToast(Context context, String message) {
        if (context == null) return;

        // Must run on UI thread
        if (context instanceof Activity) {
            ((Activity) context).runOnUiThread(() -> {
                Toast.makeText(context, message, Toast.LENGTH_SHORT).show();
            });
        }
    }

    /**
     * Show a long toast message
     */
    public static void showToastLong(Context context, String message) {
        if (context == null) return;

        if (context instanceof Activity) {
            ((Activity) context).runOnUiThread(() -> {
                Toast.makeText(context, message, Toast.LENGTH_LONG).show();
            });
        }
    }

    // =========================================================================
    // Dialogs
    // =========================================================================

    /**
     * Show an alert dialog
     * @param context Context (should be Activity)
     * @param title Dialog title
     * @param message Dialog message
     */
    public static void showDialog(Context context, String title, String message) {
        if (!(context instanceof Activity)) return;

        Activity activity = (Activity) context;
        activity.runOnUiThread(() -> {
            new AlertDialog.Builder(context)
                .setTitle(title)
                .setMessage(message)
                .setPositiveButton("OK", null)
                .show();
        });
    }

    /**
     * Show a confirmation dialog
     * @param context Context (should be Activity)
     * @param title Dialog title
     * @param message Dialog message
     * @param callback Native callback ID
     */
    public static void showConfirmDialog(Context context, String title, String message,
                                         final long callback) {
        if (!(context instanceof Activity)) return;

        Activity activity = (Activity) context;
        activity.runOnUiThread(() -> {
            new AlertDialog.Builder(context)
                .setTitle(title)
                .setMessage(message)
                .setPositiveButton("Yes", (dialog, which) -> {
                    // TODO: Call native callback with result
                })
                .setNegativeButton("No", (dialog, which) -> {
                    // TODO: Call native callback with result
                })
                .show();
        });
    }

    // =========================================================================
    // Permissions
    // =========================================================================

    /**
     * Request a runtime permission
     * @param activity Activity instance
     * @param permission Permission string (e.g., Manifest.permission.READ_EXTERNAL_STORAGE)
     */
    public static void requestPermission(Activity activity, String permission) {
        if (activity == null || permission == null) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (ContextCompat.checkSelfPermission(activity, permission)
                    != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(activity, new String[]{permission}, 1);
            }
        }
    }

    /**
     * Check if a permission is granted
     * @param context Context
     * @param permission Permission string
     * @return true if granted
     */
    public static boolean hasPermission(Context context, String permission) {
        if (context == null || permission == null) return false;

        return ContextCompat.checkSelfPermission(context, permission)
            == PackageManager.PERMISSION_GRANTED;
    }

    // =========================================================================
    // Intents
    // =========================================================================

    /**
     * Open a URL in the browser
     * @param context Context
     * @param url URL to open
     */
    public static void openURL(Context context, String url) {
        if (context == null || url == null) return;

        try {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
        } catch (Exception e) {
            Log.e(TAG, "Failed to open URL: " + e.getMessage());
        }
    }

    // =========================================================================
    // Clipboard
    // =========================================================================

    /**
     * Copy text to clipboard
     * @param context Context
     * @param text Text to copy
     */
    public static void setClipboardText(Context context, String text) {
        if (context == null) return;

        ClipboardManager clipboard = (ClipboardManager)
            context.getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null) {
            ClipData clip = ClipData.newPlainText("MTA", text);
            clipboard.setPrimaryClip(clip);
        }
    }

    /**
     * Get text from clipboard
     * @param context Context
     * @return Clipboard text or empty string
     */
    public static String getClipboardText(Context context) {
        if (context == null) return "";

        ClipboardManager clipboard = (ClipboardManager)
            context.getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null && clipboard.hasPrimaryClip()) {
            ClipData clip = clipboard.getPrimaryClip();
            if (clip != null && clip.getItemCount() > 0) {
                CharSequence text = clip.getItemAt(0).getText();
                if (text != null) {
                    return text.toString();
                }
            }
        }
        return "";
    }

    // =========================================================================
    // Keyboard
    // =========================================================================

    /**
     * Show or hide the soft keyboard
     * @param context Context
     * @param show True to show, false to hide
     */
    public static void showKeyboard(Context context, boolean show) {
        if (!(context instanceof Activity)) return;

        Activity activity = (Activity) context;
        InputMethodManager imm = (InputMethodManager)
            context.getSystemService(Context.INPUT_METHOD_SERVICE);

        if (imm == null) return;

        activity.runOnUiThread(() -> {
            if (show) {
                imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, 0);
            } else {
                if (activity.getCurrentFocus() != null) {
                    imm.hideSoftInputFromWindow(
                        activity.getCurrentFocus().getWindowToken(), 0);
                }
            }
        });
    }

    // =========================================================================
    // Vibration
    // =========================================================================

    /**
     * Vibrate the device
     * @param context Context
     * @param milliseconds Duration in milliseconds
     */
    public static void vibrate(Context context, int milliseconds) {
        if (context == null) return;

        Vibrator vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        if (vibrator == null || !vibrator.hasVibrator()) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(VibrationEffect.createOneShot(milliseconds,
                VibrationEffect.DEFAULT_AMPLITUDE));
        } else {
            vibrator.vibrate(milliseconds);
        }
    }

    // =========================================================================
    // Device Info
    // =========================================================================

    /**
     * Get device model name
     * @return Device model string
     */
    public static String getDeviceModel() {
        return Build.MODEL;
    }

    /**
     * Get device manufacturer
     * @return Manufacturer string
     */
    public static String getDeviceManufacturer() {
        return Build.MANUFACTURER;
    }

    /**
     * Get Android version
     * @return Android version string
     */
    public static String getAndroidVersion() {
        return Build.VERSION.RELEASE;
    }

    /**
     * Get Android SDK level
     * @return SDK integer
     */
    public static int getSDKLevel() {
        return Build.VERSION.SDK_INT;
    }
}
