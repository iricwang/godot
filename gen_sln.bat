@echo off
chcp 65001 >nul
echo ==========================================
echo Godot Visual Studio Solution Generator
echo ==========================================
echo.

set GODOT_DIR=%~dp0
set PYTHON=python

echo [1/3] Checking environment...
%PYTHON% --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found in PATH
    pause
    exit /b 1
)

echo [2/3] Generating Visual Studio solution...
echo    Platform : windows
echo    Target   : editor
echo    Modules  : (psd_ui)
echo.
cd /d "%GODOT_DIR%"

REM Generate VS solution only (skip actual build)
REM This allows the solution to be regenerated even if there are build errors.
REM d3d12=no is added to avoid requiring the Direct3D 12 SDK during generation.
REM If you need D3D12 support, install it first with:
REM   python misc\scripts\install_d3d12_sdk_windows.py
%PYTHON% -m SCons vsproj=yes vsproj_gen_only=yes platform=windows target=editor tests=yes d3d12=yes debug_symbols=yes -j31 2>&1 | powershell -Command "$input | ForEach-Object { $_ -replace '\e\[[0-9;]*m', '' }"

set GEN_RESULT=%errorlevel%
echo.

if %GEN_RESULT% neq 0 (
    echo [3/3] GENERATION FAILED with error code %GEN_RESULT%
    pause
    exit /b %GEN_RESULT%
)

echo [3/3] GENERATION SUCCESS!
echo.
echo Output: godot.sln (and related .vcxproj / .props files)
echo.
echo You can now open godot.sln in Visual Studio.
pause
