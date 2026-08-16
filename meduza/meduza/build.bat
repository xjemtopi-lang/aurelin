rem ---------- ХУЕТЕНЬ КОРОЧЕ МЕНЯЙ ПУТЬ К ADB/NDK ИНАЧЕ НИКАК ----------
@echo off
setlocal EnableDelayedExpansion

set "PROJECT=%~dp0"

rem ---------- find NDK ----------
set "NDK="
if defined ANDROID_NDK_HOME if exist "%ANDROID_NDK_HOME%\ndk-build.cmd" set "NDK=%ANDROID_NDK_HOME%"
if not defined NDK if defined ANDROID_NDK_ROOT if exist "%ANDROID_NDK_ROOT%\ndk-build.cmd" set "NDK=%ANDROID_NDK_ROOT%"
if not defined NDK if exist "C:\Users\realt\Desktop\android-ndk-r27d\ndk-build.cmd" set "NDK=C:\Users\realt\Desktop\android-ndk-r27d"
if not defined NDK for /d %%d in ("%USERPROFILE%\Desktop\android-ndk-*") do if exist "%%d\ndk-build.cmd" set "NDK=%%d"
if not defined NDK for /d %%d in ("%USERPROFILE%\Downloads\android-ndk-*") do if exist "%%d\ndk-build.cmd" set "NDK=%%d"

if not defined NDK (
    echo [ERROR] NDK not found!
    echo 1. Download Android NDK: https://developer.android.com/ndk/downloads
    echo 2. Extract it to Desktop or Downloads
    echo 3. Or set variable NDK in this script manually
    pause
    exit /b 1
)

echo ============================================
echo   Building with NDK: %NDK%
echo   Project: %PROJECT%
echo   ABIs:    arm64-v8a, x86_64  (BlueStacks = x86_64)
echo ============================================
echo.

cd /d "%PROJECT%"

if "%~1"=="" (
    call "%NDK%\ndk-build.cmd" APP_ABI="arm64-v8a x86_64"
) else (
    call "%NDK%\ndk-build.cmd" %*
)

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed.
    pause
    exit /b %errorlevel%
)

echo.
echo [OK] Build finished.
echo Output:
if exist "%PROJECT%libs\arm64-v8a\magaisanware" echo   %PROJECT%libs\arm64-v8a\magaisanware
if exist "%PROJECT%libs\x86_64\magaisanware"   echo   %PROJECT%libs\x86_64\magaisanware
pause
