# FetchContentConsumer

A minimal example of integrating `DokuText` into an external project using
CMake `FetchContent`.

Example for Windows with vcpkg:

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

This example uses the recommended `TextSystem` / `TextAtlas` API path to
demonstrate:

- Registering a font fallback chain
- Seeding the atlas
- Calling `prepareLine(...)`

The lower-level `DokuTextContext`, `DokuText`, and `TextLayoutContext` APIs
are intended for direct use when more control is required.
