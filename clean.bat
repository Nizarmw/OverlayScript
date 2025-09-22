@echo off
echo ==========================================
echo      Script Cleanup Tool
echo ==========================================
echo.

echo [1] Killing Script.exe process...
taskkill /f /im Script.exe 2>nul
if %errorlevel%==0 (
    echo    ✓ Process killed successfully
) else (
    echo    ✗ Process not running or already killed
)

echo.
echo [2] Removing from startup registry...
reg delete "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v "OverlayApp" /f 2>nul
if %errorlevel%==0 (
    echo    ✓ Registry entry removed successfully
) else (
    echo    ✗ Registry entry not found or already removed
)

echo.
echo [3] Cleanup complete!
echo ==========================================
echo Press any key to exit...
pause >nul