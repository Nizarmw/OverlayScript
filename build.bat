
@echo off
echo ==========================================
echo    Building Overlay Application
echo ==========================================

echo [1] Compiling resource file...
windres resources.rc -o resources.o
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile resources
    pause
    exit /b 1
)

echo [2] Compiling main application...
g++ Script.cpp resources.o -o Script.exe -lgdiplus -lwinmm -mwindows -static-libgcc -static-libstdc++
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile application
    pause
    exit /b 1
)

echo [3] Cleaning up temporary files...
del resources.o

echo.
echo ✓ Build successful! Script.exe created.
echo ✓ All assets embedded - no external files needed.
echo.
echo Usage: Script.exe (standalone)
echo Exit: Alt+Esc
echo Uninstall: Alt+Shift+U
echo ==========================================
pause