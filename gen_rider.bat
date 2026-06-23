@echo off
chcp 65001 >nul

REM ============================================================
REM  Godot Engine - Rider All-in-One Setup
REM ============================================================
REM  One-shot setup that wires Godot's SCons build into JetBrains Rider.
REM
REM  Why this script is so flat (no if/else blocks, no setlocal):
REM    The error "Microsoft was unexpected at this time." comes from
REM    cmd's parser choking on a ( ) block whose content contains a
REM    token it doesn't recognize as a command. It happens when
REM    certain command hosts (PowerShell -Command, ConEmu, etc.) wrap
REM    the script in a setlocal context that breaks parsing inside
REM    if (...) blocks. We side-step the whole class by using only
REM    flat if + goto, never ( ) blocks.
REM
REM  Steps:
REM    1. Locate vcvars64.bat
REM    2. Detect scons (scons or python -m SCons)
REM    3. Run vcvars + scons in a clean cmd /D /C child to produce
REM       godot.sln, godot.vcxproj, .props, and compile_commands.json
REM    4. Write Rider workspace.xml (2 Run/Debug configs) and
REM       tools/External Tools.xml (4 scons invocations)
REM    5. Write .idea/.gitignore
REM
REM  Prerequisites:
REM    - Python 3.9+ in PATH
REM    - SCons 4.4+ (pip install scons) - optional, python -m SCons works
REM    - Visual Studio 2022 with "Desktop development with C++"
REM    - JetBrains Rider (any recent version)
REM
REM  Usage:
REM    gen_rider.bat              REM Debug default
REM    gen_rider.bat release      REM Release default
REM ============================================================

REM ---- Locate engine dir ----
set "GDN_DIR=%~dp0"
if "%GDN_DIR:~-1%"=="\" set "GDN_DIR=%GDN_DIR:~0,-1%"

REM ---- Parse argument ----
set "PROFILE=%~1"
if /I "%PROFILE%"=="release" goto :profile_release
set "DEFAULT_PROFILE=debug"
goto :profile_done
:profile_release
set "DEFAULT_PROFILE=release"
:profile_done

echo.
echo ============================================
echo  Godot Engine - Rider All-in-One Setup
echo ============================================
echo  Engine dir : %GDN_DIR%
echo  Default    : %DEFAULT_PROFILE%
echo.

REM ============================================================
REM  Step 1: Locate vcvars64.bat
REM ============================================================
echo [1/5] Locating vcvars64.bat...
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" goto :step1_notfound
echo       OK - %VCVARS%
goto :step1_done

:step1_notfound
echo [ERROR] vcvars64.bat not found at "%VCVARS%"
echo         Edit VCVARS in this script to match your VS install.
echo         Common alternatives:
echo           C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat
echo           C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat
echo           C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat
exit /b 1

:step1_done

REM ============================================================
REM  Step 2: Detect scons
REM ============================================================
echo [2/5] Detecting SCons...
where scons >nul 2>&1
if %ERRORLEVEL% EQU 0 goto :step2_have_scons
echo       [WARN] 'scons' not on PATH; will use 'python -m SCons'.
set "INNER_SCONS=python -m SCons"
goto :step2_done
:step2_have_scons
echo       OK
set "INNER_SCONS=scons"
:step2_done

REM ============================================================
REM  Step 3: Run vcvars + scons in a clean cmd child process
REM
REM  The cmd /D /C wrapper forces a fresh cmd with no AutoRun and
REM  no inherited setlocal quirks. Inside, we call vcvars then
REM  scons; both run with the MSVC env (INCLUDE, LIB, PATH) live.
REM
REM  Flags:
REM    vsproj=yes vsproj_gen_only=yes  -> generate project files only, no build
REM    compiledb=yes                    -> emit compile_commands.json
REM    progress=no                      -> quiet
REM    -j31                             -> parallel build (use whatever your CPU count is)
REM
REM  Output is piped through powershell to strip ANSI color codes
REM  so it reads cleanly in cmd.
REM ============================================================
echo [3/5] Generating godot.sln / godot.vcxproj / compile_commands.json...
echo       (This may take 30-90s, but does NOT compile - project generation only.)
echo.

cd /d "%GDN_DIR%"

cmd /D /C "call \"%VCVARS%\" x64 >nul 2>&1 && %INNER_SCONS% platform=windows target=editor arch=x86_64 tests=yes d3d12=yes vsproj=yes vsproj_gen_only=yes compiledb=yes progress=no -j31" 2>&1 | powershell -NoProfile -Command "$input | ForEach-Object { $_ -replace '\e\[[0-9;]*m', '' }"

if %ERRORLEVEL% NEQ 0 goto :step3_failed

if not exist "%GDN_DIR%\godot.sln" goto :step3_no_sln
if not exist "%GDN_DIR%\compile_commands.json" goto :step3_no_compdb

echo       OK
goto :step3_done

:step3_failed
echo.
echo [ERROR] scons exited with error code %ERRORLEVEL%. See output above.
exit /b 1

:step3_no_sln
echo [ERROR] godot.sln was not generated. Check scons output above.
exit /b 1

:step3_no_compdb
echo [WARN] compile_commands.json was not generated. Rider intellisense will be limited.
echo       OK (degraded)

:step3_done

REM ============================================================
REM  Step 4: Write Rider configuration
REM ============================================================
echo [4/5] Writing Rider configuration...

call :write_rider_workspace "%GDN_DIR%"
if %ERRORLEVEL% NEQ 0 goto :step4_ws_failed
echo       - workspace.xml   (Run/Debug configurations)

call :write_external_tools "%GDN_DIR%"
if %ERRORLEVEL% NEQ 0 goto :step4_et_failed
echo       - tools/External Tools.xml   (4 External Tools: Build/Clean x Debug/Release)
echo       OK
goto :step4_done

:step4_ws_failed
echo [ERROR] Failed to write Rider workspace.
exit /b 1

:step4_et_failed
echo [ERROR] Failed to write External Tools config.
exit /b 1

:step4_done

REM ============================================================
REM  Step 5: Write .idea/.gitignore
REM ============================================================
echo [5/5] Writing .idea/.gitignore...
call :write_idea_gitignore "%GDN_DIR%"
if %ERRORLEVEL% NEQ 0 goto :step5_failed
echo       OK
goto :step5_done

:step5_failed
echo [ERROR] Failed to write .idea/.gitignore.
exit /b 1

:step5_done

echo.
echo ============================================
echo  Setup complete!
echo ============================================
echo.
echo  Next steps:
echo    1. Open in Rider:  File -^> Open -^> %GDN_DIR%\godot.vcxproj
echo       (NOT godot.sln - the project entry point is the .vcxproj)
echo    2. Wait for indexing to finish (~1-2 min on first open)
echo    3. Pick a run config from the top-right dropdown:
echo         - godot.editor.dev     (default, F5 launches Debug build)
echo         - godot.editor.release
echo    4. To rebuild: Tools -^> External Tools -^> Build Debug / Build Release
echo.
echo  Generated files:
echo    %GDN_DIR%\godot.sln
echo    %GDN_DIR%\godot.vcxproj
echo    %GDN_DIR%\godot.vcxproj.filters
echo    %GDN_DIR%\compile_commands.json
echo    %GDN_DIR%\.idea\.idea.godot\.idea\workspace.xml
echo    %GDN_DIR%\.idea\.idea.godot\.idea\tools\External Tools.xml
echo    %GDN_DIR%\.idea\.idea.godot\.idea\.gitignore
echo.

exit /b 0


REM ============================================================
REM  Function: write_rider_workspace
REM  Writes .idea/.idea.godot/.idea/workspace.xml with:
REM    - ProjectId, ProjectViewState, VcsManagerConfiguration
REM    - RunManager: 2 configurations
REM        godot.editor.dev     (AUTO_SELECT_PRIORITY=100, default)
REM        godot.editor.release (AUTO_SELECT_PRIORITY=50)
REM ============================================================
:write_rider_workspace
set "IDEA_DIR=%~1\.idea\.idea.godot\.idea"
if not exist "%IDEA_DIR%" mkdir "%IDEA_DIR%" >nul 2>&1
if errorlevel 1 exit /b 1

set "WORKSPACE_XML=%IDEA_DIR%\workspace.xml"
(
    echo ^<?xml version="1.0" encoding="UTF-8"?^>
    echo ^<project version="4"^>
    echo   ^<component name="AutoGeneratedRunConfigurationManager"^>
    echo     ^<projectFile^>godot.vcxproj^</projectFile^>
    echo   ^</component^>
    echo   ^<component name="AutoImportSettings"^>
    echo     ^<option name="autoReloadType" value="SELECTIVE" /^>
    echo   ^</component^>
    echo   ^<component name="ChangeListManager"^>
    echo     ^<list default="true" id="fa4d4f75-3c92-460a-bac4-f95e1fff2a1c" name="Changes" comment="" /^>
    echo     ^<option name="SHOW_DIALOG" value="false" /^>
    echo     ^<option name="HIGHLIGHT_CONFLICTS" value="true" /^>
    echo     ^<option name="HIGHLIGHT_NON_ACTIVE_CHANGELIST" value="false" /^>
    echo     ^<option name="LAST_RESOLUTION" value="IGNORE" /^>
    echo   ^</component^>
    echo   ^<component name="Git.Settings"^>
    echo     ^<option name="RECENT_GIT_ROOT_PATH" value="$PROJECT_DIR$" /^>
    echo   ^</component^>
    echo   ^<component name="ProjectId" id="3Ej596vM1f8XmgMwXGPxotTVwLz" /^>
    echo   ^<component name="ProjectViewState"^>
    echo     ^<option name="hideEmptyMiddlePackages" value="true" /^>
    echo     ^<option name="showLibraryContents" value="true" /^>
    echo   ^</component^>
    echo   ^<component name="PropertiesComponent"^>{^&quot;keyToString^&quot;: {^&quot;RunOnceActivity.ShowReadmeOnStart^&quot;: ^&quot;true^&quot;, ^&quot;RunOnceActivity.git.unshallow^&quot;: ^&quot;true^&quot;, ^&quot;git-widget-placeholder^&quot;: ^&quot;main^&quot;, ^&quot;settings.editor.selected.configurable^&quot;: ^&quot;preferences.pluginManager^&quot;}}^</component^>
    echo   ^<component name="RunManager"^>
    echo     ^<configuration name="godot.editor.dev" type="CppProject" factoryName="C++ Project" nameIsGenerated="false"^>
    echo       ^<configuration_1 setup="1"^>
    echo         ^<option name="CONFIGURATION" value="editor" /^>
    echo         ^<option name="PLATFORM" value="x64" /^>
    echo         ^<option name="CURRENT_LAUNCH_PROFILE" value="Local" /^>
    echo         ^<option name="EXE_PATH" value="%GDN_DIR%\bin\godot.windows.editor.dev.x86_64.exe" /^>
    echo         ^<option name="PROGRAM_PARAMETERS" value="" /^>
    echo         ^<option name="WORKING_DIRECTORY" value="%GDN_DIR%" /^>
    echo         ^<option name="PASS_PARENT_ENVS" value="1" /^>
    echo         ^<option name="USE_EXTERNAL_CONSOLE" value="0" /^>
    echo         ^<option name="TERMINAL_INTERACTION_BEHAVIOR" value="AUTO_DETECT" /^>
    echo         ^<option name="PROJECT_FILE_PATH" value="$PROJECT_DIR$/godot.vcxproj" /^>
    echo       ^</configuration_1^>
    echo       ^<option name="DEFAULT_PROJECT_PATH" value="$PROJECT_DIR$/godot.vcxproj" /^>
    echo       ^<option name="PROJECT_FILE_PATH" value="$PROJECT_DIR$/godot.vcxproj" /^>
    echo       ^<option name="AUTO_SELECT_PRIORITY" value="100" /^>
    echo       ^<method v="2"^>^<option name="Build" /^>^</method^>
    echo     ^</configuration^>
    echo     ^<configuration name="godot.editor.release" type="CppProject" factoryName="C++ Project" nameIsGenerated="false"^>
    echo       ^<configuration_1 setup="1"^>
    echo         ^<option name="CONFIGURATION" value="editor" /^>
    echo         ^<option name="PLATFORM" value="x64" /^>
    echo         ^<option name="CURRENT_LAUNCH_PROFILE" value="Local" /^>
    echo         ^<option name="EXE_PATH" value="%GDN_DIR%\bin\godot.windows.editor.x86_64.exe" /^>
    echo         ^<option name="PROGRAM_PARAMETERS" value="" /^>
    echo         ^<option name="WORKING_DIRECTORY" value="%GDN_DIR%" /^>
    echo         ^<option name="PASS_PARENT_ENVS" value="1" /^>
    echo         ^<option name="USE_EXTERNAL_CONSOLE" value="0" /^>
    echo         ^<option name="TERMINAL_INTERACTION_BEHAVIOR" value="AUTO_DETECT" /^>
    echo         ^<option name="PROJECT_FILE_PATH" value="$PROJECT_DIR$/godot.vcxproj" /^>
    echo       ^</configuration_1^>
    echo       ^<option name="DEFAULT_PROJECT_PATH" value="$PROJECT_DIR$/godot.vcxproj" /^>
    echo       ^<option name="PROJECT_FILE_PATH" value="$PROJECT_DIR$/godot.vcxproj" /^>
    echo       ^<option name="AUTO_SELECT_PRIORITY" value="50" /^>
    echo       ^<method v="2"^>^<option name="Build" /^>^</method^>
    echo     ^</configuration^>
    echo   ^</component^>
    echo   ^<component name="SpellCheckerSettings" RuntimeDictionaries="0" Folders="0" CustomDictionaries="0" DefaultDictionary="application-level" UseSingleDictionary="true" transferred="true" /^>
    echo   ^<component name="TaskManager"^>
    echo     ^<task active="true" id="Default" summary="Default task"^>
    echo       ^<changelist id="fa4d4f75-3c92-460a-bac4-f95e1fff2a1c" name="Changes" comment="" /^>
    echo       ^<created^>1780678983410^</created^>
    echo       ^<option name="number" value="Default" /^>
    echo       ^<option name="presentableId" value="Default" /^>
    echo       ^<updated^>1780678983410^</updated^>
    echo     ^</task^>
    echo     ^<servers /^>
    echo   ^</component^>
    echo   ^<component name="TypeScriptGeneratedFilesManager"^>
    echo     ^<option name="version" value="3" /^>
    echo   ^</component^>
    echo   ^<component name="UnityProjectConfiguration" hasMinimizedUI="false" /^>
    echo   ^<component name="VcsManagerConfiguration"^>
    echo     ^<option name="CLEAR_INITIAL_COMMIT_MESSAGE" value="true" /^>
    echo   ^</component^>
    echo ^</project^>
) > "%WORKSPACE_XML%"
if not exist "%WORKSPACE_XML%" exit /b 1
exit /b 0


REM ============================================================
REM  Function: write_external_tools
REM  Writes .idea/.idea.godot/.idea/tools/External Tools.xml
REM  with 4 tools (Build/Clean x Debug/Release).
REM
REM  We always use "python -m SCons" because scons is not on PATH
REM  on this machine (we already verified that in Step 2 - the
REM  variable INNER_SCONS was set, but we don't pass it across the
REM  function boundary; the embedded flag set is what matters).
REM ============================================================
:write_external_tools
set "TOOLS_DIR=%~1\.idea\.idea.godot\.idea\tools"
if not exist "%TOOLS_DIR%" mkdir "%TOOLS_DIR%" >nul 2>&1
if errorlevel 1 exit /b 1

set "TOOLS_XML=%TOOLS_DIR%\External Tools.xml"
(
    echo ^<?xml version="1.0" encoding="UTF-8"?^>
    echo ^<toolSet name="External Tools"^>
    echo   ^<tool name="Build Debug" description="Compile Godot editor in Debug mode (dev_build=yes debug_symbols=yes optimize=debug). Produces bin\godot.windows.editor.dev.x86_64.exe" showInMainMenu="true" showInEditor="false" createShortcut="false" useConsole="true" showConsoleOnStdOut="true" showConsoleOnStdErr="true" synchronizeAfterRun="true"^>
    echo     ^<exec^>
    echo       ^<option name="COMMAND" value="python" /^>
    echo       ^<option name="PARAMETERS" value="-m SCons platform=windows target=editor arch=x86_64 tests=yes d3d12=yes dev_build=yes debug_symbols=yes optimize=debug -j31" /^>
    echo       ^<option name="WORKING_DIRECTORY" value="$ProjectFileDir$" /^>
    echo     ^</exec^>
    echo   ^</tool^>
    echo   ^<tool name="Build Release" description="Compile Godot editor in Release mode. Produces bin\godot.windows.editor.x86_64.exe" showInMainMenu="true" showInEditor="false" createShortcut="false" useConsole="true" showConsoleOnStdOut="true" showConsoleOnStdErr="true" synchronizeAfterRun="true"^>
    echo     ^<exec^>
    echo       ^<option name="COMMAND" value="python" /^>
    echo       ^<option name="PARAMETERS" value="-m SCons platform=windows target=editor arch=x86_64 tests=yes d3d12=yes -j31" /^>
    echo       ^<option name="WORKING_DIRECTORY" value="$ProjectFileDir$" /^>
    echo     ^</exec^>
    echo   ^</tool^>
    echo   ^<tool name="Clean Debug" description="Remove Debug build artifacts. Run before a full Debug rebuild." showInMainMenu="false" showInEditor="false" createShortcut="false" useConsole="true" showConsoleOnStdOut="true" showConsoleOnStdErr="true" synchronizeAfterRun="true"^>
    echo     ^<exec^>
    echo       ^<option name="COMMAND" value="python" /^>
    echo       ^<option name="PARAMETERS" value="-m SCons --clean platform=windows target=editor arch=x86_64 tests=yes d3d12=yes dev_build=yes debug_symbols=yes optimize=debug -j31" /^>
    echo       ^<option name="WORKING_DIRECTORY" value="$ProjectFileDir$" /^>
    echo     ^</exec^>
    echo   ^</tool^>
    echo   ^<tool name="Clean Release" description="Remove Release build artifacts. Run before a full Release rebuild." showInMainMenu="false" showInEditor="false" createShortcut="false" useConsole="true" showConsoleOnStdOut="true" showConsoleOnStdErr="true" synchronizeAfterRun="true"^>
    echo     ^<exec^>
    echo       ^<option name="COMMAND" value="python" /^>
    echo       ^<option name="PARAMETERS" value="-m SCons --clean platform=windows target=editor arch=x86_64 tests=yes d3d12=yes -j31" /^>
    echo       ^<option name="WORKING_DIRECTORY" value="$ProjectFileDir$" /^>
    echo     ^</exec^>
    echo   ^</tool^>
    echo ^</toolSet^>
) > "%TOOLS_XML%"
if not exist "%TOOLS_XML%" exit /b 1
exit /b 0


REM ============================================================
REM  Function: write_idea_gitignore
REM  Prevents Rider's transient files from being committed.
REM ============================================================
:write_idea_gitignore
set "IDEA_GITIGNORE=%~1\.idea\.idea.godot\.idea\.gitignore"
(
    echo # Rider / IntelliJ transient files
    echo # Generated by gen_rider.bat - do not commit.
    echo.
    echo # Caches and indices
    echo caches/
    echo index/
    echo workspace.xml.tasks
    echo.
    echo # Shelf / local history (uncomment if you want to keep these)
    echo # shelf/
    echo # localHistory/
) > "%IDEA_GITIGNORE%"
exit /b 0
