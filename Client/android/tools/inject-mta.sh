#!/bin/bash
#
# MTA:SA Android - APK Injection Script
# Phase 6: Inject MTA library into GTA:SA APK
#
# This script:
# 1. Decompiles GTA:SA APK using apktool
# 2. Adds MTA native library
# 3. Patches smali code to load our library
# 4. Repackages and signs the APK
#
# Requirements:
# - apktool (https://ibotpeaches.github.io/Apktool/)
# - Java JDK
# - zipalign (from Android SDK)
# - apksigner (from Android SDK) or jarsigner
#

set -e

#=============================================================================
# Configuration
#=============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MTA_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${MTA_DIR}/output"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

#=============================================================================
# Functions
#=============================================================================

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_requirements() {
    log_info "Checking requirements..."

    # Check apktool
    if ! command -v apktool &> /dev/null; then
        log_error "apktool not found. Install from: https://ibotpeaches.github.io/Apktool/"
        exit 1
    fi

    # Check Java
    if ! command -v java &> /dev/null; then
        log_error "Java not found. Install JDK 11+"
        exit 1
    fi

    # Check for zipalign (optional but recommended)
    if ! command -v zipalign &> /dev/null; then
        log_warn "zipalign not found. APK may not be optimized."
        ZIPALIGN=""
    else
        ZIPALIGN="zipalign"
    fi

    # Check for apksigner or jarsigner
    if command -v apksigner &> /dev/null; then
        SIGNER="apksigner"
    elif command -v jarsigner &> /dev/null; then
        SIGNER="jarsigner"
    else
        log_error "Neither apksigner nor jarsigner found. Install Android SDK build-tools."
        exit 1
    fi

    log_info "Requirements check passed"
}

show_usage() {
    echo "Usage: $0 <gtasa.apk> [options]"
    echo ""
    echo "Options:"
    echo "  -o, --output <file>     Output APK path (default: gtasa-mta.apk)"
    echo "  -k, --keystore <file>   Keystore for signing (creates debug key if not specified)"
    echo "  -a, --abi <abi>         Target ABI: arm64-v8a, armeabi-v7a (default: both)"
    echo "  -h, --help              Show this help"
    echo ""
    echo "Example:"
    echo "  $0 GTA_SA_2.11.apk -o gtasa-mta.apk"
}

create_debug_keystore() {
    local keystore="$1"

    if [ -f "$keystore" ]; then
        return 0
    fi

    log_info "Creating debug keystore..."

    keytool -genkey -v \
        -keystore "$keystore" \
        -alias mta-debug \
        -keyalg RSA \
        -keysize 2048 \
        -validity 10000 \
        -storepass android \
        -keypass android \
        -dname "CN=MTA Debug, OU=MTA, O=MTA, L=City, ST=State, C=US"

    log_info "Debug keystore created: $keystore"
}

decompile_apk() {
    local apk="$1"
    local output="$2"

    log_info "Decompiling APK..."

    if [ -d "$output" ]; then
        log_warn "Removing existing decompiled directory..."
        rm -rf "$output"
    fi

    apktool d -f "$apk" -o "$output"

    log_info "APK decompiled to: $output"
}

inject_native_library() {
    local decompiled="$1"
    local abi="$2"

    log_info "Injecting MTA native library for ABI: $abi"

    # Check if MTA library exists
    local mta_lib="${MTA_DIR}/app/build/intermediates/cmake/debug/obj/${abi}/libmta_android.so"

    if [ ! -f "$mta_lib" ]; then
        # Try release build
        mta_lib="${MTA_DIR}/app/build/intermediates/cmake/release/obj/${abi}/libmta_android.so"
    fi

    if [ ! -f "$mta_lib" ]; then
        log_error "MTA library not found for $abi. Build the project first."
        log_error "Expected: $mta_lib"
        return 1
    fi

    # Create lib directory if not exists
    local lib_dir="${decompiled}/lib/${abi}"
    mkdir -p "$lib_dir"

    # Copy library
    cp "$mta_lib" "$lib_dir/"
    log_info "Copied: libmta_android.so -> $lib_dir/"

    return 0
}

patch_smali_code() {
    local decompiled="$1"

    log_info "Patching smali code to load MTA library..."

    # Find the main GTA activity class
    local gta_activity=$(find "$decompiled/smali" -name "*.smali" -exec grep -l "Lcom/rockstargames/gtasa" {} \; | head -1)

    if [ -z "$gta_activity" ]; then
        # Try alternate paths
        gta_activity=$(find "$decompiled/smali" -path "*/rockstargames/gtasa/*.smali" | head -1)
    fi

    if [ -z "$gta_activity" ]; then
        log_warn "Could not find GTA activity class. Manual patching may be needed."
        log_warn "Add this to the static initializer of the main activity:"
        log_warn '  System.loadLibrary("mta_android");'
        return 0
    fi

    log_info "Found GTA activity: $gta_activity"

    # Create smali code to load our library
    # This gets injected into the static initializer or onCreate

    # Check if there's a static initializer
    if grep -q "\.method static constructor <clinit>" "$gta_activity"; then
        log_info "Patching static initializer..."

        # Find the line after .locals in <clinit>
        # Insert: const-string v0, "mta_android"
        #         invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

        # Create a backup
        cp "$gta_activity" "${gta_activity}.bak"

        # This is a simplified patch - real implementation would need proper smali manipulation
        log_warn "Auto-patch not fully implemented. See docs/PHASE6_INTEGRATION.md for manual steps."

    else
        log_warn "No static initializer found. Manual patching required."
    fi

    # Create MTA loader smali file
    create_mta_loader_smali "$decompiled"

    return 0
}

create_mta_loader_smali() {
    local decompiled="$1"

    log_info "Creating MTA loader smali class..."

    # Create directory
    local smali_dir="${decompiled}/smali/com/mtasa/android"
    mkdir -p "$smali_dir"

    # Create MTALoader.smali
    cat > "${smali_dir}/MTALoader.smali" << 'EOF'
.class public Lcom/mtasa/android/MTALoader;
.super Ljava/lang/Object;
.source "MTALoader.java"

# Static initializer - loads MTA library
.method static constructor <clinit>()V
    .registers 1

    const-string v0, "mta_android"

    invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

    return-void
.end method

# Constructor
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

# Initialize MTA integration
.method public static initialize()Z
    .registers 1

    # Call native initialize
    invoke-static {}, Lcom/mtasa/android/MTALoader;->nativeInit()Z

    move-result v0

    return v0
.end method

# Native method declaration
.method public static native nativeInit()Z
.end method
EOF

    log_info "Created MTALoader.smali"
}

recompile_apk() {
    local decompiled="$1"
    local output="$2"

    log_info "Recompiling APK..."

    apktool b "$decompiled" -o "$output"

    log_info "APK recompiled: $output"
}

align_apk() {
    local input="$1"
    local output="$2"

    if [ -z "$ZIPALIGN" ]; then
        cp "$input" "$output"
        return 0
    fi

    log_info "Aligning APK..."

    zipalign -f -p 4 "$input" "$output"

    log_info "APK aligned: $output"
}

sign_apk() {
    local apk="$1"
    local keystore="$2"

    log_info "Signing APK..."

    if [ "$SIGNER" == "apksigner" ]; then
        apksigner sign \
            --ks "$keystore" \
            --ks-pass pass:android \
            --key-pass pass:android \
            "$apk"
    else
        # jarsigner
        jarsigner -verbose \
            -sigalg SHA256withRSA \
            -digestalg SHA-256 \
            -keystore "$keystore" \
            -storepass android \
            -keypass android \
            "$apk" \
            mta-debug
    fi

    log_info "APK signed: $apk"
}

#=============================================================================
# Main
#=============================================================================

main() {
    # Parse arguments
    INPUT_APK=""
    OUTPUT_APK=""
    KEYSTORE=""
    TARGET_ABI="both"

    while [[ $# -gt 0 ]]; do
        case $1 in
            -o|--output)
                OUTPUT_APK="$2"
                shift 2
                ;;
            -k|--keystore)
                KEYSTORE="$2"
                shift 2
                ;;
            -a|--abi)
                TARGET_ABI="$2"
                shift 2
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                if [ -z "$INPUT_APK" ]; then
                    INPUT_APK="$1"
                else
                    log_error "Unknown argument: $1"
                    show_usage
                    exit 1
                fi
                shift
                ;;
        esac
    done

    # Validate input
    if [ -z "$INPUT_APK" ]; then
        log_error "No input APK specified"
        show_usage
        exit 1
    fi

    if [ ! -f "$INPUT_APK" ]; then
        log_error "Input APK not found: $INPUT_APK"
        exit 1
    fi

    # Set defaults
    if [ -z "$OUTPUT_APK" ]; then
        OUTPUT_APK="${OUTPUT_DIR}/gtasa-mta.apk"
    fi

    if [ -z "$KEYSTORE" ]; then
        KEYSTORE="${OUTPUT_DIR}/debug.keystore"
    fi

    # Create output directory
    mkdir -p "$OUTPUT_DIR"

    # Show configuration
    echo ""
    echo "==========================================="
    echo "MTA:SA Android - APK Injection"
    echo "==========================================="
    echo "Input APK:  $INPUT_APK"
    echo "Output APK: $OUTPUT_APK"
    echo "Target ABI: $TARGET_ABI"
    echo "Keystore:   $KEYSTORE"
    echo "==========================================="
    echo ""

    # Check requirements
    check_requirements

    # Create debug keystore if needed
    create_debug_keystore "$KEYSTORE"

    # Decompile
    WORK_DIR="${OUTPUT_DIR}/decompiled"
    decompile_apk "$INPUT_APK" "$WORK_DIR"

    # Inject native libraries
    if [ "$TARGET_ABI" == "both" ]; then
        inject_native_library "$WORK_DIR" "arm64-v8a" || true
        inject_native_library "$WORK_DIR" "armeabi-v7a" || true
    else
        inject_native_library "$WORK_DIR" "$TARGET_ABI"
    fi

    # Patch smali code
    patch_smali_code "$WORK_DIR"

    # Recompile
    UNSIGNED_APK="${OUTPUT_DIR}/unsigned.apk"
    recompile_apk "$WORK_DIR" "$UNSIGNED_APK"

    # Align
    ALIGNED_APK="${OUTPUT_DIR}/aligned.apk"
    align_apk "$UNSIGNED_APK" "$ALIGNED_APK"

    # Sign
    cp "$ALIGNED_APK" "$OUTPUT_APK"
    sign_apk "$OUTPUT_APK" "$KEYSTORE"

    # Cleanup
    rm -f "$UNSIGNED_APK" "$ALIGNED_APK"

    echo ""
    echo "==========================================="
    log_info "APK injection complete!"
    echo "==========================================="
    echo "Output: $OUTPUT_APK"
    echo ""
    echo "Install with:"
    echo "  adb install -r $OUTPUT_APK"
    echo ""
    echo "Note: You may need to uninstall the original GTA:SA first:"
    echo "  adb uninstall com.rockstargames.gtasa"
    echo ""
}

main "$@"
