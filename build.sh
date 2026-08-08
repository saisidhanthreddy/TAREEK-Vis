#!/bin/bash
# SimVis Build Script
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

# Create build directory if needed
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure if needed
if [ ! -f "Makefile" ]; then
    echo "Configuring CMake..."
    cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE="$CONFIG" ..
    if [ $? -ne 0 ]; then
        echo "CMake configuration failed!"
        exit 1
    fi
fi

# Build
echo "Building SimVis ($CONFIG) with $JOBS jobs..."
mingw32-make -j"$JOBS"

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "Executable: $BUILD_DIR/SimVis.exe"
else
    echo ""
    echo "Build failed!"
    exit 1
fi
