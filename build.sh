#!/usr/bin/env bash
set -e

VCPKG_ROOT=~/dev/vcpkg
TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

if [ ! -d "$VCPKG_ROOT" ]; then
  echo "Cloning vcpkg..."
  git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
  (cd "$VCPKG_ROOT" && ./bootstrap-vcpkg.sh)
fi

echo "Installing dependencies..."
"$VCPKG_ROOT/vcpkg" install freetype harfbuzz libpng --triplet x64-linux

echo "Configuring CMake..."
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build build --config Release

echo "Done."
