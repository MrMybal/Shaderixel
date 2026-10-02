@echo off
rem Build one target: build_target.bat <Shaderixel|ShaderixelVk> [Debug|Release]
setlocal
set TARGET=%1
set CONFIG=%2
if "%CONFIG%"=="" set CONFIG=Release
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set BUILD_DIR=%~dp0build\%CONFIG%
cmake -S "%~dp0." -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1
cmake --build "%BUILD_DIR%" --target %TARGET% || exit /b 1
echo Built: %BUILD_DIR%\%TARGET%.exe
