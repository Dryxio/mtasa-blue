@echo off
echo ========================================
echo  MTA:SA Build Sync Script
echo ========================================
echo.

set SOURCE=C:\Mac\Home\Documents\GitHub\mtasa-blue
set DEST=C:\mtasa-blue

echo [1/2] Syncing files to local drive...
robocopy "%SOURCE%" "%DEST%" /MIR /XD .git Build .vs /XF *.user *.suo /MT:16 /NFL /NDL /NJH /NJS /NC /NS

if %ERRORLEVEL% GEQ 8 (
    echo ERROR: Sync failed!
    pause
    exit /b 1
)

echo.
echo [2/2] Sync complete!
echo.
echo Ready to build from: %DEST%\Build\MTASA.sln
echo.
echo Options:
echo   1. Open Visual Studio manually
echo   2. Or run: msbuild "%DEST%\Build\MTASA.sln" /p:Configuration=Release /m
echo.
pause
