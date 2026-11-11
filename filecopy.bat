@echo off
setlocal EnableDelayedExpansion

rem Set BRIG_GAME_DIR to the installed Half-Life mod directory before building.
rem Example: set "BRIG_GAME_DIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life\brig"
if not defined BRIG_GAME_DIR (
    echo BRIG_GAME_DIR is not set; skipping automatic BRIG installation.
    exit /b 0
)

set "mod_directory=%BRIG_GAME_DIR%"
set source=%~1
set source=%source:\=\\%
set destination=%mod_directory%

if NOT "%~2" == "" (
    set destination=!destination!/%~2
)

set destination=!destination:\=\\!
set filename=%~3

echo Installing "!destination!/%filename%"
@robocopy "%source%" "!destination!" "%filename%" /njh /njs /ndl /nc /ns /np > nul

rem Robocopy uses codes below 8 for success and non-fatal conditions.
if %ERRORLEVEL% GEQ 8 exit /b %ERRORLEVEL%
exit /b 0
