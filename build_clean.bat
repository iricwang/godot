@echo off
chcp 65001 >nul
echo ==========================================
echo Godot Clean Build Script
echo ==========================================
echo.

set GODOT_DIR=%~dp0
set PYTHON=python

echo [1/4] Checking environment...
%PYTHON% --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found in PATH
    pause
    exit /b 1
)

echo [2/4] Cleaning previous build...
cd /d "%GODOT_DIR%"
%PYTHON% -m SCons --clean platform=windows target=editor tests=yes -j31 2>&1 | powershell -Command "$input | ForEach-Object { $_ -replace '\e\[[0-9;]*m', '' }"

echo.
echo [3/4] Starting fresh build...
%PYTHON% -m SCons platform=windows target=editor tests=yes -j31 2>&1 | powershell -Command "$input | ForEach-Object { $_ -replace '\e\[[0-9;]*m', '' }"

set BUILD_RESULT=%errorlevel%
echo.

if %BUILD_RESULT% neq 0 (
    echo [4/4] BUILD FAILED with error code %BUILD_RESULT%
    pause
    exit /b %BUILD_RESULT%
)

echo [4/4] BUILD SUCCESS!
echo.
echo Output: bin\godot.windows.editor.x86_64.exe
pause
