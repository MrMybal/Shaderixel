@echo off
rem Configure + build Shaderixel with the VS 2022 toolchain (MSVC, bundled CMake & Ninja).
rem Usage: build.bat [Debug|Release]   (default Release)
setlocal
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if not defined VSDIR (
    echo Visual Studio with C++ tools not found.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set BUILD_DIR=%~dp0build\%CONFIG%
cmake -S "%~dp0." -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1
cmake --build "%BUILD_DIR%" || exit /b 1
echo.
echo Built: %BUILD_DIR%\Shaderixel.exe
