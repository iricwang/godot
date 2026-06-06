@echo off
setlocal

REM ============================================================
REM  Godot Engine - Generate Visual Studio Solution (.sln)
REM ============================================================
REM  This script generates the Visual Studio solution and project
REM  files for the Godot Engine (Windows platform).
REM
REM  Prerequisites:
REM    - Python 3.9+ installed and in PATH
REM    - SCons 4.4+ installed (pip install scons)
REM    - Visual Studio 2022 with "Desktop development with C++"
REM ============================================================
REM  Usage:  godot\generate_solution.bat
REM          (or cd godot && generate_solution.bat)
REM ============================================================

REM ---- Find the Godot engine directory (where this script lives) ----
set GDN_DIR=%~dp0
REM Remove trailing backslash
if "%GDN_DIR:~-1%"=="\" set GDN_DIR=%GDN_DIR:~0,-1%

echo.
echo ============================================
echo  Godot Engine - Generate VS Solution
echo ============================================
echo  Engine dir: %GDN_DIR%
echo.

REM ---- Setup Visual Studio environment ----
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %VCVARS% (
    echo [ERROR] Visual Studio 2022 Community not found at expected path.
    echo         Please modify VCVARS in this script to point to your VS installation.
    exit /b 1
)
call %VCVARS% x64 >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to initialize Visual Studio environment.
    exit /b 1
)
echo [INFO] Visual Studio environment initialized (x64).

REM ---- Check if scons is available ----
where scons >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] scons not found in PATH.
    echo         Please install SCons: pip install scons
    exit /b 1
)

echo [INFO] Generating Visual Studio solution and project files...
echo [INFO] Command: scons -C "%GDN_DIR%" platform=windows vsproj=yes accesskit=no d3d12=no
echo.

scons -C "%GDN_DIR%" platform=windows vsproj=yes vsproj_gen_only=yes accesskit=no d3d12=no

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================
    echo  Solution generated successfully!
    echo ============================================
    echo.
    echo  Files generated in %GDN_DIR%:
    echo    - godot.sln
    echo    - godot.vcxproj
    echo    - godot.vcxproj.filters
    echo    - godot.props
    echo.
    echo  Open godot.sln with Visual Studio to start coding.
    echo.
) else (
    echo.
    echo [ERROR] Failed to generate solution. Check the output above.
    exit /b 1
)

endlocal
