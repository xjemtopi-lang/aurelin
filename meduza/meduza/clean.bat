@echo off
setlocal

set "PROJECT=%~dp0"

cd /d "%PROJECT%"

echo Cleaning build artifacts...
if exist "obj"           rmdir /s /q "obj"
if exist "libs"          rmdir /s /q "libs"
if exist ".cxx"          rmdir /s /q ".cxx"
if exist "local.properties" del /q "local.properties"

echo [OK] Cleaned.
pause
