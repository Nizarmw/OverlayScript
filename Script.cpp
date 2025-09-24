#include <windows.h>
#include <gdiplus.h>
#include <mmsystem.h> 
#include <cstdio>
#include <thread>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

using Gdiplus::Image;
using Gdiplus::Graphics;
using Gdiplus::GdiplusStartupInput;
using Gdiplus::GdiplusStartup;
using Gdiplus::GdiplusShutdown;

ULONG_PTR gdiplusToken = 0;
Image* openingGif = nullptr;
Image* idleGif = nullptr;
bool playingOpening = true;
DWORD startTime = 0;
UINT frameIndex = 0;
UINT frameDelay = 100; 

const DWORD OPENING_DURATION = 12290; 

HHOOK keyboardHook = NULL;
bool altPressed = false;

// Function to add program to startup
void AddToStartup() {
    HKEY hKey;
    const wchar_t* czStartName = L"OverlayApp";
    
    // Get current executable path
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    
    // Add startup argument using wsprintfW (Windows API - no include needed)
    wchar_t szStartupPath[MAX_PATH + 20];
    wsprintfW(szStartupPath, L"\"%s\" -startup", szPath);
    
    LONG lnRes = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_WRITE, &hKey);
    
    if (lnRes == ERROR_SUCCESS) {
        RegSetValueExW(hKey, czStartName, 0, REG_SZ, 
            (LPBYTE)szStartupPath, (wcslen(szStartupPath) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
}

// Function to remove program from startup
void RemoveFromStartup() {
    HKEY hKey;
    const wchar_t* czStartName = L"OverlayApp";
    
    LONG lnRes = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_WRITE, &hKey);
    
    if (lnRes == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, czStartName);
        RegCloseKey(hKey);
    }
}

// Launch watchdog process
void LaunchWatchdog() {
    if (GetFileAttributesW(L"watchdog.exe") != INVALID_FILE_ATTRIBUTES) {
        STARTUPINFOW si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        if (CreateProcessW(L"watchdog.exe", NULL, NULL, NULL, FALSE, 
                          CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
}

// Windows key blocker with exit keys
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // Block Windows keys (Left and Right)
            if (pKeyboard->vkCode == VK_LWIN || pKeyboard->vkCode == VK_RWIN) {
                return 1; 
            }
            
            // Block Alt+Tab, Alt+F4, Ctrl+Shift+Esc
            if (pKeyboard->vkCode == VK_TAB && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
                return 1; // Block Alt+Tab
            }
            if (pKeyboard->vkCode == VK_F4 && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
                return 1; // Block Alt+F4
            }
            if (pKeyboard->vkCode == VK_ESCAPE && (GetAsyncKeyState(VK_LCONTROL) & 0x8000) && (GetAsyncKeyState(VK_LSHIFT) & 0x8000)) {
                return 1; // Block Ctrl+Shift+Esc (Task Manager)
            }
            if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_MENU) & 0x8000) && pKeyboard->vkCode == VK_DELETE) {
                return 1; // Try to block Ctrl+Alt+Del
            }
            
            // Track Alt key for exit combinations
            if (pKeyboard->vkCode == VK_MENU) {
                altPressed = true;
            }
            
            // SAFETY EXIT: Alt+Esc to exit
            if (pKeyboard->vkCode == VK_ESCAPE && altPressed) {
                PostQuitMessage(0);
                return 1;
            }
            
            // UNINSTALL: Alt+Shift+U to remove from startup and exit
            if (pKeyboard->vkCode == 'U' && altPressed && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                RemoveFromStartup();
                MessageBoxW(NULL, L"Removed from startup! The spell has been broken.", L"Liberation", MB_OK | MB_ICONINFORMATION);
                PostQuitMessage(0);
                return 1;
            }
        }
        
        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            // Reset Alt tracking when Alt is released
            if (pKeyboard->vkCode == VK_MENU) {
                altPressed = false;
            }
        }
    }
    
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// Draw current frame of the GIF
void DrawGif(Graphics& graphics, Image* gif, UINT frameIdx, int width, int height) {
    if (!gif) return;

    UINT count = gif->GetFrameDimensionsCount();
    if (count == 0) return;

    GUID dim;
    gif->GetFrameDimensionsList(&dim, 1);
    UINT frameCount = gif->GetFrameCount(&dim);
    if (frameCount > 1) {
        // select active frame (GDI+ handles animation timing if we pick frames periodically)
        gif->SelectActiveFrame(&dim, frameIdx % frameCount);
    }

    graphics.DrawImage(gif, 0, 0, width, height);
}

// Window procedure for handling messages
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Graphics graphics(hdc);

        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);

        if (playingOpening) {
            DrawGif(graphics, openingGif, frameIndex, w, h);
        } else {
            DrawGif(graphics, idleGif, frameIndex, w, h);
        }

        EndPaint(hwnd, &ps);
    } break;

    // Timer for frame updates
    case WM_TIMER:
        // advance animation frame
        frameIndex++;

        // change state opening -> idle GIF
        if (playingOpening && (GetTickCount() - startTime >= OPENING_DURATION)) {
            playingOpening = false;
            frameIndex = 0; 
        }
        InvalidateRect(hwnd, NULL, TRUE);
        break; 

    // Cleanup on destroy
    case WM_DESTROY:
        // Unhook keyboard before destroying
        if (keyboardHook) {
            UnhookWindowsHookEx(keyboardHook);
            keyboardHook = NULL;
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
    // Init GDI+
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Gdiplus::Ok) {
        MessageBoxW(NULL, L"Failed to initialize GDI+.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Check if running from startup or manual launch
    bool isStartupRun = (strstr(lpCmdLine, "-startup") != NULL);
    
    if (isStartupRun) {
        // Startup launch message - auto close in 2 seconds
        std::thread([]() {
            MessageBoxW(NULL, L"I'm BAAAACK", L"", MB_OK | MB_ICONINFORMATION);
        }).detach();
        
        std::thread([]() {
            Sleep(2000); // 2 seconds
            HWND msgBox = FindWindowW(L"#32770", L"");
            if (msgBox) {
                PostMessage(msgBox, WM_COMMAND, IDOK, 0);
            }
        }).detach();
        
        Sleep(2500); // Wait for message to close before continuing
        
    } else {
        // Manual Launch - auto close in 3 seconds
        std::thread([]() {
            MessageBoxW(NULL, L"Your Device is now filled with Determination!", L"Notice", MB_OK | MB_ICONINFORMATION);
        }).detach();
        
        std::thread([]() {
            Sleep(3000); // 3 seconds
            HWND msgBox = FindWindowW(L"#32770", L"Notice");
            if (msgBox) {
                PostMessage(msgBox, WM_COMMAND, IDOK, 0);
            }
        }).detach();
        
        Sleep(3500); // Wait before continuing
        
        // Add to startup on first manual run
        AddToStartup();
        
        // Launch watchdog process to monitor for termination
        LaunchWatchdog();
    }

    // Register window class (wide version)
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OverlayWindowClass";
    RegisterClassW(&wc);

    // Create fullscreen window - hidden from taskbar
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,  // Prevents appearing in Alt-Tab and taskbar
        wc.lpszClassName,
        L"Overlay",
        WS_POPUP,
        0, 0, screenW, screenH,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create window.", L"Error", MB_OK | MB_ICONERROR);
        GdiplusShutdown(gdiplusToken);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Install low-level keyboard hook to disable Windows key and other shortcuts
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    if (!keyboardHook) {
        MessageBoxW(NULL, L"Failed to install keyboard hook.", L"Warning", MB_OK | MB_ICONWARNING);
    }

    // Load GIFs
    openingGif = Image::FromFile(L"assets\\opening.gif", FALSE);
    idleGif    = Image::FromFile(L"assets\\idle.gif", FALSE);

    // Check if GIFs loaded successfully
    if (!openingGif || !idleGif) {
        MessageBoxW(NULL, L"Cannot load GIF files. Ensure assets\\opening.gif and assets\\idle.gif exist.", L"Error", MB_OK | MB_ICONERROR);
        if (keyboardHook) {
            UnhookWindowsHookEx(keyboardHook);
            keyboardHook = NULL;
        }
        DestroyWindow(hwnd);
        GdiplusShutdown(gdiplusToken);
        return 1;
    }

    // Play background music
    PlaySoundW(L"assets\\audio.wav", NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);

    // Start timer for frame updates
    startTime = GetTickCount();
    SetTimer(hwnd, 1, frameDelay, NULL);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    KillTimer(hwnd, 1);
    if (keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = NULL;
    }
    if (openingGif) { delete openingGif; openingGif = nullptr; }
    if (idleGif)    { delete idleGif; idleGif = nullptr; }
    GdiplusShutdown(gdiplusToken);

    return 0;
}