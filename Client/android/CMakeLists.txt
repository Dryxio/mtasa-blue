# MTA:SA Android - Main CMake Build Configuration
#
# This is the root CMake file for the MTA:SA Android port.
# Builds native libraries that integrate with the Android GTA:SA.
#
# Usage:
#   mkdir build && cd build
#   cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
#         -DANDROID_ABI=armeabi-v7a \
#         -DANDROID_PLATFORM=android-21 \
#         ..
#   make -j$(nproc)

cmake_minimum_required(VERSION 3.18)

project(MTA_SA_Android
    VERSION 1.6.0
    DESCRIPTION "Multi Theft Auto: San Andreas - Android Port"
    LANGUAGES C CXX ASM
)

# =============================================================================
# Build Configuration
# =============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Android-specific settings
if(ANDROID)
    message(STATUS "Building for Android")
    message(STATUS "  ABI: ${ANDROID_ABI}")
    message(STATUS "  Platform: ${ANDROID_PLATFORM}")
    message(STATUS "  NDK: ${ANDROID_NDK}")

    # Enable position-independent code (required for shared libraries)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    # Optimize for size on release builds
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os -DNDEBUG")

    # Debug symbols for debugging
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")

    # ARM-specific flags
    if(ANDROID_ABI STREQUAL "armeabi-v7a")
        message(STATUS "  Mode: ARM32 (ARMv7-A)")
        add_compile_definitions(MTA_ARM32=1)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp -mfpu=neon")
    elseif(ANDROID_ABI STREQUAL "arm64-v8a")
        message(STATUS "  Mode: ARM64 (AArch64)")
        add_compile_definitions(MTA_ARM64=1)
    endif()
else()
    message(WARNING "Not building for Android - cross-compilation required")
endif()

# =============================================================================
# Project Directories
# =============================================================================

set(MTA_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(MTA_CLIENT_DIR "${MTA_ROOT_DIR}/Client")
set(MTA_SHARED_DIR "${MTA_ROOT_DIR}/Shared")
set(MTA_VENDOR_DIR "${MTA_ROOT_DIR}/vendor")

# Android-specific directories
set(ANDROID_HOOKS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/hooks")
set(ANDROID_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core")
set(ANDROID_GRAPHICS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/graphics")
set(ANDROID_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform")
set(ANDROID_SIGNATURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/signatures")
set(ANDROID_JNI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/jni")
set(ANDROID_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test")
set(ANDROID_NETWORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/network")

# =============================================================================
# Include Directories
# =============================================================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ANDROID_HOOKS_DIR}
    ${ANDROID_CORE_DIR}
    ${ANDROID_GRAPHICS_DIR}
    ${ANDROID_PLATFORM_DIR}
    ${ANDROID_SIGNATURES_DIR}
    ${ANDROID_JNI_DIR}
    ${ANDROID_TEST_DIR}
    ${ANDROID_NETWORK_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/multiplayer
    ${CMAKE_CURRENT_SOURCE_DIR}/game_sa

    # Shared MTA headers
    ${MTA_SHARED_DIR}/sdk
    ${MTA_CLIENT_DIR}/sdk
    ${MTA_CLIENT_DIR}/sdk/game

    # Third-party (cross-platform)
    ${MTA_VENDOR_DIR}/lua/src
    ${MTA_VENDOR_DIR}/tinyxml
    ${MTA_VENDOR_DIR}/json-c
    ${MTA_VENDOR_DIR}/zlib
    ${MTA_VENDOR_DIR}/curl/include
    ${MTA_VENDOR_DIR}/cryptopp
)

# =============================================================================
# Source Files
# =============================================================================

# Hook System
set(HOOK_SOURCES
    ${ANDROID_HOOKS_DIR}/ARMHookSystem.h
    ${ANDROID_HOOKS_DIR}/ARMHookInstaller.h
)

# Signature Scanner & Address Mapping
set(SIGNATURE_SOURCES
    ${ANDROID_SIGNATURES_DIR}/SignatureScanner.h
    ${ANDROID_SIGNATURES_DIR}/AddressDatabase.h
    ${ANDROID_SIGNATURES_DIR}/ARMAddressMap.h
)

# Multiplayer Hooks (ARM port of multiplayer_sa)
set(ANDROID_MULTIPLAYER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/multiplayer")
set(MULTIPLAYER_SOURCES
    ${ANDROID_MULTIPLAYER_DIR}/CMultiplayerSA_ARM.h
    ${ANDROID_MULTIPLAYER_DIR}/CMultiplayerSA_ARM.cpp
)

# Platform Abstraction (Phase 4)
set(PLATFORM_SOURCES
    ${ANDROID_PLATFORM_DIR}/AndroidInput.h
    ${ANDROID_PLATFORM_DIR}/AndroidInput.cpp
    ${ANDROID_PLATFORM_DIR}/AndroidFileSystem.h
    ${ANDROID_PLATFORM_DIR}/AndroidFileSystem.cpp
    ${ANDROID_PLATFORM_DIR}/AndroidNetwork.h
    ${ANDROID_PLATFORM_DIR}/AndroidNetwork.cpp
)

# Graphics Backend (OpenGL ES)
set(GRAPHICS_SOURCES
    ${ANDROID_GRAPHICS_DIR}/GLESGraphics.h
    ${ANDROID_GRAPHICS_DIR}/GLESGraphics.cpp
    ${ANDROID_GRAPHICS_DIR}/GLESShaders.h
    ${ANDROID_GRAPHICS_DIR}/RenderWareBridge.h
    ${ANDROID_GRAPHICS_DIR}/RenderWareBridge.cpp
)

# Game SA Interface Layer (ARM port)
set(ANDROID_GAMESA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game_sa")
set(GAMESA_SOURCES
    ${ANDROID_GAMESA_DIR}/GameSA_Platform.h
    ${ANDROID_GAMESA_DIR}/GameSA_Platform.cpp
    ${ANDROID_GAMESA_DIR}/CEntitySA_ARM.h
    ${ANDROID_GAMESA_DIR}/CPedSA_ARM.h
    ${ANDROID_GAMESA_DIR}/CVehicleSA_ARM.h
    ${ANDROID_GAMESA_DIR}/CWorldSA_ARM.h
)

# Core Module (Phase 5)
set(CORE_SOURCES
    ${ANDROID_CORE_DIR}/CAndroidCore.h
    ${ANDROID_CORE_DIR}/CAndroidCore.cpp
    ${ANDROID_CORE_DIR}/CProfiler.h
)

# JNI Bridge (Phase 4)
set(JNI_SOURCES
    ${ANDROID_JNI_DIR}/MTANative.h
    ${ANDROID_JNI_DIR}/MTANative.cpp
)

# Test Harness
set(TEST_SOURCES
    ${ANDROID_TEST_DIR}/TestHarness.h
    ${ANDROID_TEST_DIR}/SubsystemTests.cpp
    ${ANDROID_TEST_DIR}/TestNative.cpp
)

# Network Module (Phase 7)
set(NETWORK_SOURCES
    ${ANDROID_NETWORK_DIR}/CNetAndroid.h
    ${ANDROID_NETWORK_DIR}/CNetAndroid.cpp
    ${ANDROID_NETWORK_DIR}/CPacketHandler.h
    ${ANDROID_NETWORK_DIR}/CPacketHandler.cpp
    ${ANDROID_NETWORK_DIR}/SyncStructures.h
)

# =============================================================================
# Libraries
# =============================================================================

# Main MTA Android library
add_library(mta_android SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/MTAAndroidMain.cpp
    ${MULTIPLAYER_SOURCES}
    ${GRAPHICS_SOURCES}
    ${GAMESA_SOURCES}
    ${PLATFORM_SOURCES}
    ${JNI_SOURCES}
    ${CORE_SOURCES}
    ${TEST_SOURCES}
    ${NETWORK_SOURCES}
)

target_compile_definitions(mta_android PRIVATE
    MTA_CLIENT
    MTA_ANDROID
    $<$<CONFIG:Debug>:MTA_DEBUG>
)

# Link Android system libraries
if(ANDROID)
    find_library(LOG_LIB log)
    find_library(ANDROID_LIB android)
    find_library(GLES3_LIB GLESv3)
    find_library(EGL_LIB EGL)

    target_link_libraries(mta_android
        ${LOG_LIB}
        ${ANDROID_LIB}
        ${GLES3_LIB}
        ${EGL_LIB}
    )
endif()

# =============================================================================
# Hook Test Library (for development)
# =============================================================================

add_library(mta_hooks_test SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/hooks/HookTest.cpp
)

target_compile_definitions(mta_hooks_test PRIVATE
    MTA_HOOKS_TEST
)

if(ANDROID)
    target_link_libraries(mta_hooks_test ${LOG_LIB})
endif()

# =============================================================================
# Signature Scanner Test
# =============================================================================

add_library(mta_scanner_test SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/signatures/ScannerTest.cpp
)

target_compile_definitions(mta_scanner_test PRIVATE
    MTA_SCANNER_TEST
)

if(ANDROID)
    target_link_libraries(mta_scanner_test ${LOG_LIB})
endif()

# =============================================================================
# Installation
# =============================================================================

install(TARGETS mta_android mta_hooks_test mta_scanner_test
    LIBRARY DESTINATION lib/${ANDROID_ABI}
    ARCHIVE DESTINATION lib/${ANDROID_ABI}
)

# =============================================================================
# Custom Targets
# =============================================================================

# Generate address mapping report
add_custom_target(address_report
    COMMAND ${CMAKE_COMMAND} -E echo "Generating address mapping report..."
    COMMENT "This will generate a report of mapped x86 -> ARM addresses"
)

# =============================================================================
# Print Configuration Summary
# =============================================================================

message(STATUS "")
message(STATUS "MTA:SA Android Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
if(ANDROID)
    message(STATUS "  Android ABI:  ${ANDROID_ABI}")
    message(STATUS "  API Level:    ${ANDROID_PLATFORM}")
endif()
message(STATUS "")
