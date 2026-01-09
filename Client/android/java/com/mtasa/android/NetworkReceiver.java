/*
 * MTA:SA Android - Network State Receiver
 *
 * Broadcasts network connectivity changes to native code.
 */

package com.mtasa.android;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.os.Build;
import android.util.Log;

public class NetworkReceiver extends BroadcastReceiver {
    private static final String TAG = "MTA-Network";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (context == null) return;

        ConnectivityManager cm = (ConnectivityManager)
            context.getSystemService(Context.CONNECTIVITY_SERVICE);

        if (cm == null) {
            notifyNative(false, 0, false);
            return;
        }

        boolean available = false;
        int type = 0;
        boolean metered = false;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Network network = cm.getActiveNetwork();
            if (network != null) {
                NetworkCapabilities caps = cm.getNetworkCapabilities(network);
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
        } else {
            NetworkInfo info = cm.getActiveNetworkInfo();
            if (info != null) {
                available = info.isConnected();
                type = info.getType();
                metered = cm.isActiveNetworkMetered();
            }
        }

        Log.d(TAG, "Network state changed: available=" + available +
                   ", type=" + type + ", metered=" + metered);

        notifyNative(available, type, metered);
    }

    private void notifyNative(boolean available, int type, boolean metered) {
        try {
            MTANative.nativeNetworkStateChanged(available, type, metered);
        } catch (UnsatisfiedLinkError e) {
            // Native library not loaded yet, ignore
            Log.w(TAG, "Native library not ready");
        }
    }
}
