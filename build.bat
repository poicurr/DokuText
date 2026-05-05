@echo off
setlocal
set VCPKG_ROOT=C:\dev\vcpkg
set TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

if not exist "%VCPKG_ROOT%" (
    echo Cloning vcpkg...
    git clone https://github.com/microsoft/vcpkg "%VCPKG_ROOT%"
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat"
)

echo Installing dependencies...
"%VCPKG_ROOT%\vcpkg.exe" install freetype harfbuzz libpng --triplet x64-windows

echo Configuring CMake...
cmake -B build -S . -G "Ninja Multi-Config" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release

echo Building...
cmake --build build --config Release

echo Done.
endlocal
