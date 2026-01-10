###############################################################################
#
#  PROJECT:     MTA:SA Server - Android Network Module
#  LICENSE:     See LICENSE in the top level directory
#  FILE:        Server/net-android/CMakeLists.txt
#  PURPOSE:     Build configuration for net_android.so
#
#  This builds a network module for Android clients without Anti-Cheat.
#
#  Usage:
#    mkdir build && cd build
#    cmake .. -DCMAKE_BUILD_TYPE=Release
#    make
#
#  Output: libnet_android.so (or net_android.dll on Windows)
#
###############################################################################

cmake_minimum_required(VERSION 3.10)
project(net_android VERSION 1.0.0 LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform detection
if(WIN32)
    set(LIB_PREFIX "")
    set(LIB_SUFFIX ".dll")
    add_definitions(-D_WIN32 -DWIN32)
else()
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".so")
    add_definitions(-DLINUX)
endif()

# Source files
set(SOURCE_FILES
    CNetServerAndroid.cpp
    CNetBitStreamAndroid.cpp
)

set(HEADER_FILES
    CNetServerAndroid.h
    CNetBitStreamAndroid.h
)

# Create shared library
add_library(net_android SHARED ${SOURCE_FILES} ${HEADER_FILES})

# Include directories
target_include_directories(net_android PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler flags
if(MSVC)
    target_compile_options(net_android PRIVATE
        /W4
        /WX-
        /wd4100  # unreferenced formal parameter
        /wd4458  # declaration hides class member
    )
else()
    target_compile_options(net_android PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -fvisibility=hidden
        -fPIC
    )
endif()

# Link libraries
if(WIN32)
    target_link_libraries(net_android PRIVATE ws2_32)
else()
    target_link_libraries(net_android PRIVATE pthread)
endif()

# Output name
set_target_properties(net_android PROPERTIES
    OUTPUT_NAME "net_android"
    PREFIX ""
    SUFFIX ${LIB_SUFFIX}
)

# Export symbols
if(WIN32)
    # Windows exports via __declspec(dllexport)
elseif(APPLE)
    # macOS - use exported_symbols_list
    set_target_properties(net_android PROPERTIES
        LINK_FLAGS "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/exports_macos.txt"
    )
else()
    # Linux exports using version script
    set_target_properties(net_android PROPERTIES
        LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map"
    )
endif()

# Install target
install(TARGETS net_android
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Print configuration
message(STATUS "")
message(STATUS "=== net_android Configuration ===")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Output: net_android${LIB_SUFFIX}")
message(STATUS "================================")
message(STATUS "")
