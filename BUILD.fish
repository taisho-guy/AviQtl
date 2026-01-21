#!/usr/bin/env fish

# Rina Build Script
# Usage: ./build.fish [clean|debug|release]

set PROJECT_ROOT (pwd)
set BUILD_DIR "build"
set BUILD_TYPE "Release"
set CLEAN_BUILD false

# --- Argument Parsing ---

for arg in $argv
    switch $arg
        case clean
            set CLEAN_BUILD true
        case debug
            set BUILD_TYPE "Debug"
        case release
            set BUILD_TYPE "Release"
        case '*'
            echo "Unknown argument: $arg"
            echo "Usage: ./build.fish [clean|debug|release]"
            exit 1
    end
end

# --- Dependency Check (Basic) ---

if not type -q cmake
    echo "Error: cmake is not installed."
    exit 1
end

if not type -q ninja
    echo "Warning: ninja is not found. Falling back to make."
    set GENERATOR "Unix Makefiles"
else
    set GENERATOR "Ninja"
end

# --- Build Process ---

echo "=== Building Rina ($BUILD_TYPE) ==="

if test "$CLEAN_BUILD" = true
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
end

mkdir -p "$BUILD_DIR"

# ディレクトリ移動に失敗したら終了
cd "$BUILD_DIR"; or exit 1

echo "Configuring with CMake ($GENERATOR)..."

cmake -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ..

if test $status -ne 0
    echo "CMake configuration failed."
    exit 1
end

echo "Compiling..."

# nprocの結果を展開して引数に渡す
cmake --build . --config "$BUILD_TYPE" -- -j(nproc)

if test $status -eq 0
    echo "=== Build Success! ==="
    echo "Executable is located at: $BUILD_DIR/Rina"
    echo "Run with: ./$BUILD_DIR/Rina"
else
    echo "=== Build Failed ==="
    exit 1
end
