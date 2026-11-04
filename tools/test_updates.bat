@echo off
rem Update policy/integrity tests. Add --live to download and verify a GitHub installer without running it.
setlocal
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if not defined VSDIR exit /b 1
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set CHECK_DIR=%~dp0..\build\CyUiCheck
cmake -S "%~dp0.." -B "%CHECK_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSX_BUILD_UPDATE_TESTS=ON || exit /b 1
cmake --build "%CHECK_DIR%" --target ShaderixelUpdateTests --parallel 6 || exit /b 1
ctest --test-dir "%CHECK_DIR%" -R "^shaderixel_updates$" --output-on-failure || exit /b 1
if /i "%~1"=="--live" "%CHECK_DIR%\ShaderixelUpdateTests.exe" --live
exit /b %ERRORLEVEL%
