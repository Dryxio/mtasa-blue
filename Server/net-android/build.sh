#!/bin/bash
###############################################################################
#
#  MTA:SA Server - Android Network Module Build Script
#
#  Usage:
#    ./build.sh              # Build in Release mode
#    ./build.sh debug        # Build in Debug mode
#    ./build.sh clean        # Clean build directory
#
###############################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Parse arguments
BUILD_TYPE="Release"
if [ "$1" = "debug" ]; then
    BUILD_TYPE="Debug"
elif [ "$1" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
    echo "Done."
    exit 0
fi

echo "=================================="
echo "Building net_android.so"
echo "Build type: ${BUILD_TYPE}"
echo "=================================="

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

# Build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Show output
echo ""
echo "=================================="
echo "Build complete!"
echo "Output: ${BUILD_DIR}/net_android.so"
echo "=================================="

# Verify exports
if [ -f "net_android.so" ]; then
    echo ""
    echo "Exported symbols:"
    nm -D net_android.so 2>/dev/null | grep -E "InitNetServerInterface|ReleaseNetServerInterface" || \
    nm net_android.so 2>/dev/null | grep -E "InitNetServerInterface|ReleaseNetServerInterface" || \
    echo "(Could not list symbols)"
fi
