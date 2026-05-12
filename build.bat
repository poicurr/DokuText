@echo off
setlocal

if defined VCPKG_ROOT (
    if not exist "%VCPKG_ROOT%" (
        echo ERROR: VCPKG_ROOT is set but not found: %VCPKG_ROOT%
        exit /b 1
    )
) else (
    set "VCPKG_ROOT=C:\dev\vcpkg"
    if not exist "%VCPKG_ROOT%" (
        set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
    )
)

set TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

if not exist "%VCPKG_ROOT%" (
    echo Cloning vcpkg...
    git clone https://github.com/microsoft/vcpkg "%VCPKG_ROOT%"
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat"
)

echo Installing dependencies via manifest...
"%VCPKG_ROOT%\vcpkg.exe" install --triplet x64-windows

echo Configuring CMake...
cmake -B build -S . -G "Ninja Multi-Config" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release

echo Building...
cmake --build build --config Release

echo Done.
endlocal
