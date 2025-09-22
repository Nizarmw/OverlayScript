# Fullscreen Overlay Application

A Windows application that creates a fullscreen overlay with animated GIF support and keyboard input control.

## Features
- Fullscreen animated GIF overlay
- Automatic startup integration
- Keyboard shortcut blocking (Windows key, Alt+Tab, etc.)
- Hidden exit combinations
- Audio playback support

## Usage
1. Compile: `g++ Script.cpp -o Script.exe -lgdiplus -lwinmm -mwindows`
2. Run: `Script.exe`
3. Exit: `Alt+Esc`
4. Uninstall: `Alt+Shift+U`

## File Structure
├── Script.cpp          # Main application
├── cleanup.bat         # Cleanup utility
├── assets/
│   ├── opening.gif     # Opening animation
│   ├── idle.gif        # Idle animation
│   └── audio.wav       # Background audio
└── README.md

## Disclaimer
Educational purposes only. Test in safe environment.