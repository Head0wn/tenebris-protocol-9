@echo off
setlocal
cd /d "%~dp0"

echo TENEBRIS 0.7.0 - Site 47 Visual Validation
echo.
echo Controls:
echo   Right mouse       Look around
echo   WASD / arrows     Move
echo   Q/E               Down / up
echo   Shift             Fast movement
echo   Mouse wheel       Dolly
echo   O                 Cinematic orbit
echo   R                 Reset camera
echo   Escape            Release mouse, then quit
echo.

if not exist "TENEBRIS.exe" (
    echo ERROR: TENEBRIS.exe is missing from this folder.
    pause
    exit /b 1
)

"TENEBRIS.exe" --auto-orbit
set "TENEBRIS_EXIT=%ERRORLEVEL%"

if not "%TENEBRIS_EXIT%"=="0" (
    echo.
    echo TENEBRIS stopped with error code %TENEBRIS_EXIT%.
    echo Keep this window open and copy the diagnostic text.
    pause
)

exit /b %TENEBRIS_EXIT%
