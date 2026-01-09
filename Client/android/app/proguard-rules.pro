# MTA:SA Android - ProGuard Rules
#
# Rules for code shrinking and obfuscation in release builds.

# Keep the native interface classes
-keep class com.mtasa.android.MTANative { *; }
-keep class com.mtasa.android.MTABridge { *; }
-keep class com.mtasa.android.MTAActivity { *; }

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep classes used via JNI
-keepclassmembers class * {
    @android.webkit.JavascriptInterface <methods>;
}

# Keep NetworkReceiver
-keep class com.mtasa.android.NetworkReceiver { *; }

# Keep Parcelables
-keepclassmembers class * implements android.os.Parcelable {
    public static final android.os.Parcelable$Creator *;
}

# Keep Serializable
-keepclassmembers class * implements java.io.Serializable {
    static final long serialVersionUID;
    private static final java.io.ObjectStreamField[] serialPersistentFields;
    private void writeObject(java.io.ObjectOutputStream);
    private void readObject(java.io.ObjectInputStream);
    java.lang.Object writeReplace();
    java.lang.Object readResolve();
}

# Keep enums
-keepclassmembers enum * {
    public static **[] values();
    public static ** valueOf(java.lang.String);
}

# Remove logging in release
-assumenosideeffects class android.util.Log {
    public static int v(...);
    public static int d(...);
    public static int i(...);
}

# Preserve line numbers for crash reporting
-keepattributes SourceFile,LineNumberTable

# Hide original source file names
-renamesourcefileattribute SourceFile
