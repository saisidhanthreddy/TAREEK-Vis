#!/bin/bash
# TAREEK-Vis Build Script
# Ensures correct MSYS2 UCRT64 environment for building

# Set UCRT64 as primary in PATH (required for Qt tools and runtime)
export PATH="/c/msys64/ucrt64/bin:$PATH"

# Project directory
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

# Parse arguments
CLEAN=false
CONFIG="Release"
JOBS=4

for arg in "$@"; do
    case $arg in
        --clean)
            CLEAN=true
            ;;
        --debug)
            CONFIG="Debug"
            ;;
        -j*)
            JOBS="${arg#-j}"
            ;;
        --help)
            echo "Usage: ./build.sh [options]"
            echo ""
            echo "Options:"
            echo "  --clean    Remove build directory and rebuild from scratch"
            echo "  --debug    Build in Debug mode (default: Release)"
            echo "  -jN        Use N parallel jobs (default: 4)"
            echo "  --help     Show this help message"
            exit 0
            ;;
    esac
done

# Clean build if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# A CMake cache records the absolute source path it was generated for. If the
# checkout has since been moved or renamed, CMake aborts with "The source
# directory ... does not exist" instead of reconfiguring. Detect the mismatch
# and discard the stale cache so the build recovers on its own.
#
# The two paths come from different worlds: CMake writes a Windows path
# (E:/foo/bar) while MSYS2's pwd yields a POSIX one (/e/foo/bar). Normalize
# both to lowercase POSIX form before comparing, or every run would look like
# a mismatch and wipe the build directory.
normalize_path() {
    # Backslashes -> forward slashes, drive letter "E:/" -> "/e/", strip any
    # trailing slash, then lowercase (Windows paths are case-insensitive).
    echo "$1" | tr '\134' '/' \
              | sed -e 's|^\([A-Za-z]\):|/\1|' -e 's|/$||' \
              | tr '[:upper:]' '[:lower:]'
}

if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    CACHED_SOURCE_DIR=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' \
        "$BUILD_DIR/CMakeCache.txt")
    if [ -n "$CACHED_SOURCE_DIR" ] && \
       [ "$(normalize_path "$CACHED_SOURCE_DIR")" != "$(normalize_path "$PROJECT_DIR")" ]; then
        echo "Build directory was configured for a different source path:"
        echo "  cached: $CACHED_SOURCE_DIR"
        echo "  actual: $PROJECT_DIR"
        echo "Discarding stale CMake cache and reconfiguring..."
        rm -rf "$BUILD_DIR"
    fi
fi

# Create build directory if needed
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure if needed
if [ ! -f "Makefile" ]; then
    echo "Configuring CMake..."
    cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE="$CONFIG" "$PROJECT_DIR"
    if [ $? -ne 0 ]; then
        echo "CMake configuration failed!"
        exit 1
    fi
fi

# Build
echo "Building TAREEK-Vis ($CONFIG) with $JOBS jobs..."
mingw32-make -j"$JOBS"

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "Executable: $BUILD_DIR/TAREEK-Vis.exe"
else
    echo ""
    echo "Build failed!"
    exit 1
fi
