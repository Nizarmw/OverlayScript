@echo off
title Script Manager
color 0A

:menu
cls
echo ==========================================
echo           Script Manager v1.0
echo ==========================================
echo.
echo 1. Kill Script Process
echo 2. Remove from Startup
echo 3. Full Cleanup (Kill + Remove)
echo 4. Check if Running
echo 5. Exit
echo.
set /p choice="Choose option (1-5): "

if "%choice%"=="1" goto kill
if "%choice%"=="2" goto remove
if "%choice%"=="3" goto cleanup
if "%choice%"=="4" goto check
if "%choice%"=="5" goto exit
goto menu

:kill
echo Killing Script.exe...
taskkill /f /im Script.exe
pause
goto menu

:remove
echo Removing from startup...
reg delete "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v "OverlayApp" /f
pause
goto menu

:cleanup
echo Full cleanup...
taskkill /f /im Script.exe 2>nul
reg delete "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v "OverlayApp" /f 2>nul
echo Done!
pause
goto menu

:check
echo Checking if Script.exe is running...
tasklist /fi "imagename eq Script.exe" 2>nul | find /i "Script.exe" >nul
if %errorlevel%==0 (
    echo Script.exe is RUNNING
) else (
    echo Script.exe is NOT running
)
pause
goto menu

:exit
exit