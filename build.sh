#!/usr/bin/env bash
set -e

if [ -n "${VCPKG_ROOT+x}" ]; then
  if [ -z "$VCPKG_ROOT" ] || [ ! -d "$VCPKG_ROOT" ]; then
    echo "ERROR: VCPKG_ROOT is set but not valid: ${VCPKG_ROOT-<empty>}"
    exit 1
  fi
elif [ -d "$HOME/dev/vcpkg" ]; then
  VCPKG_ROOT="$HOME/dev/vcpkg"
else
  VCPKG_ROOT=~/dev/vcpkg
  echo "Cloning vcpkg..."
  git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
fi

if [ -z "$VCPKG_ROOT" ]; then
  VCPKG_ROOT=~/dev/vcpkg
  echo "Cloning vcpkg..."
  git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
fi

if [ ! -d "$VCPKG_ROOT" ]; then
  echo "Cloning vcpkg..."
  git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
fi
TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
if [ ! -x "$VCPKG_ROOT/vcpkg" ]; then
  (cd "$VCPKG_ROOT" && ./bootstrap-vcpkg.sh)
fi

if [ ! -f "$TOOLCHAIN" ]; then
  echo "vcpkg toolchain file not found: $TOOLCHAIN"
  exit 1
fi

echo "Installing dependencies via manifest..."
"$VCPKG_ROOT/vcpkg" install --triplet x64-linux

echo "Configuring CMake..."
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build build --config Release

echo "Done."
