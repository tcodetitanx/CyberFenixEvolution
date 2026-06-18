@echo off
REM ============================================================================
REM  EvolutionWar - compile C++ then relaunch the editor.
REM  Run this from an EXTERNAL command prompt (NOT the editor's embedded
REM  terminal), then close the Unreal editor. This script waits for the editor
REM  to fully release the module DLL, compiles the EvolutionWarEditor target,
REM  and relaunches the editor when done.
REM ============================================================================

setlocal
set ENGINE=D:\UE_5.8
set PROJECT=D:\UEprojects\EvolutionWar\EvolutionWar.uproject
set EDITOR=%ENGINE%\Engine\Binaries\Win64\UnrealEditor.exe
set BUILD=%ENGINE%\Engine\Build\BatchFiles\Build.bat
set LOG=D:\UEprojects\EvolutionWar\Saved\build_and_relaunch.log

echo [build_and_relaunch] Waiting for UnrealEditor.exe to close...
:waitloop
tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>nul | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
    timeout /t 2 /nobreak >nul
    goto waitloop
)

echo [build_and_relaunch] Editor closed. Compiling EvolutionWarEditor...
echo === Build started %DATE% %TIME% === > "%LOG%"
call "%BUILD%" EvolutionWarEditor Win64 Development -project="%PROJECT%" -waitmutex >> "%LOG%" 2>&1
set BUILDRESULT=%ERRORLEVEL%
echo === Build finished with exit code %BUILDRESULT% === >> "%LOG%"

if %BUILDRESULT% NEQ 0 (
    echo [build_and_relaunch] BUILD FAILED ^(exit %BUILDRESULT%^). See %LOG%
    echo [build_and_relaunch] Not relaunching. Fix errors and re-run.
    pause
    exit /b %BUILDRESULT%
)

echo [build_and_relaunch] Build OK. Relaunching editor...
start "" "%EDITOR%" "%PROJECT%"
echo [build_and_relaunch] Done. You can close this window.
exit /b 0
