#!/bin/bash

# Rina Build Script
# Usage: $ bash build.sh [clean|debug|release]

PROJECT_ROOT=$(pwd)
BUILD_DIR="build"
BUILD_TYPE="Release"
CLEAN_BUILD=false

# --- Argument Parsing ---
for arg in "$@"; do
    case $arg in
        clean)
            CLEAN_BUILD=true
            ;;
        debug)
            BUILD_TYPE="Debug"
            ;;
        release)
            BUILD_TYPE="Release"
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: ./build.sh [clean|debug|release]"
            exit 1
            ;;
    esac
done

# --- Dependency Check (Basic) ---
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is not installed."
    exit 1
fi

if ! command -v ninja &> /dev/null; then
    echo "Warning: ninja is not found. Falling back to make."
    GENERATOR="Unix Makefiles"
else
    GENERATOR="Ninja"
fi

# --- Build Process ---
echo "=== Building Rina ($BUILD_TYPE) ==="

if [ "$CLEAN_BUILD" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || exit 1

echo "Configuring with CMake ($GENERATOR)..."
cmake -G "$GENERATOR" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ..

if [ $? -ne 0 ]; then
    echo "CMake configuration failed."
    exit 1
fi

echo "Compiling..."
cmake --build . --config "$BUILD_TYPE" -- -j$(nproc)

if [ $? -eq 0 ]; then
    echo "=== Build Success! ==="
    echo "Executable is located at: $BUILD_DIR/Rina"
    
    # Optional: Check if run is requested? No, keep it simple.
    echo "Run with: ./$BUILD_DIR/Rina"
else
    echo "=== Build Failed ==="
    exit 1
fi
