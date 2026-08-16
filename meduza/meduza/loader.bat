@echo off
setlocal EnableDelayedExpansion

set "PROJECT=%~dp0"

rem ---------- find adb ----------
set "ADB="
if exist "C:\Program Files\BlueStacks_nxt\HD-Adb.exe" set "ADB=C:\Program Files\BlueStacks_nxt\HD-Adb.exe"
if not defined ADB if exist "C:\Program Files\BlueStacks\HD-Adb.exe" set "ADB=C:\Program Files\BlueStacks\HD-Adb.exe"
if not defined ADB if exist "%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe" set "ADB=%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe"
if not defined ADB if exist "C:\Android\platform-tools\adb.exe" set "ADB=C:\Android\platform-tools\adb.exe"
if not defined ADB for /f "delims=" %%a in ('where adb 2^>nul') do if not defined ADB set "ADB=%%a"

if not defined ADB (
    echo [ERROR] adb not found.
    echo Install BlueStacks or Android platform-tools.
    pause
    exit /b 1
)

set "REMOTE=/data/local/tmp/magaisanware"

echo ============================================
echo   magaisanware.wtf loader
echo   adb:     %ADB%
echo ============================================
echo.

echo [1/5] Checking device...
"%ADB%" get-state >nul 2>&1
if errorlevel 1 (
    echo [ERROR] No device connected. Start BlueStacks first.
    pause
    exit /b 1
)

rem ---------- multiple devices check ----------
set "COUNT=0"
for /f "skip=1 tokens=1" %%d in ('"%ADB%" devices') do set /a COUNT+=1
if %COUNT% GTR 1 (
    echo.
    echo [ERROR] More than one device connected ^(%COUNT%^).
    echo Unplug extra devices, or run "adb devices" to see the list.
    pause
    exit /b 1
)
if %COUNT% EQU 0 (
    echo [ERROR] No devices in "adb devices" output.
    pause
    exit /b 1
)

rem ---------- ABI detection (strip trailing \r from device output!) ----------
for /f "delims=" %%a in ('"%ADB%" shell getprop ro.product.cpu.abi 2^>nul') do set "ABI=%%a"

set "BIN="
if "%ABI:~0,6%"=="x86_64" if exist "%PROJECT%libs\x86_64\magaisanware" set "BIN=%PROJECT%libs\x86_64\magaisanware"
if not defined BIN if exist "%PROJECT%libs\arm64-v8a\magaisanware" set "BIN=%PROJECT%libs\arm64-v8a\magaisanware"
if not defined BIN for /d %%d in ("%PROJECT%libs\*") do if exist "%%d\magaisanware" set "BIN=%%d\magaisanware"

if not defined BIN (
    echo [ERROR] Binary not found in libs\. Run build.bat first.
    pause
    exit /b 1
)

echo [2/5] Device ABI: %ABI%
echo        Binary:   %BIN%

rem ---------- kill previous instance ----------
"%ADB%" shell "su -c 'pkill -f magaisanware'" >nul 2>&1

echo [3/5] Checking root...
echo    If a Magisk window appears on the emulator - tap ALLOW
for /l %%i in (1,1,10) do (
    "%ADB%" shell "su -c id" >nul 2>&1
    if not errorlevel 1 goto root_ok
    ping 127.0.0.1 -n 2 >nul
)
echo.
echo [ERROR] NO ROOT ACCESS on the emulator!
echo The cheat cannot read the game memory without root.
echo.
echo How to fix:
echo   1. BlueStacks -^> gear icon -^> Settings -^> Advanced
echo   2. Enable "Root access" (turn ON) and RESTART the instance
echo   3. Or open Magisk -^> Superuser and grant access to "shell"
echo   4. Run this script again
echo.
pause
exit /b 1
:root_ok
echo        Root OK.

echo [4/5] Pushing...
"%ADB%" push "%BIN%" "%REMOTE%"
if errorlevel 1 (
    echo [ERROR] Push failed.
    pause
    exit /b 1
)

"%ADB%" shell chmod +x "%REMOTE%"

echo [5/5] Running...
echo Start Standoff yourself - the cheat will wait for it.
echo Press Ctrl+C to stop.
echo.
:run_loop
"%ADB%" shell su -c "%REMOTE%"
set "EXIT=%errorlevel%"
echo.
echo [i] Cheat exited with code %EXIT%.
echo     If it crashed - restart automatically.
choice /C YN /M "Restart cheat" /T 3 /D Y >nul
if errorlevel 2 goto done
echo.
goto run_loop
:done
pause
