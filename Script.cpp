#include <windows.h>
#include <gdiplus.h>
#include <mmsystem.h> 
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

// Windows key blocker
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // Block Windows keys (Left and Right)
            if (pKeyboard->vkCode == VK_LWIN || pKeyboard->vkCode == VK_RWIN) {
                return 1; 
            }
            
            // Block Alt+Tab, Alt+F4, Ctrl+Alt+Del, Ctrl+Shift+Esc
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
                return 1; // Try to block (not working lmao)
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
        gif->SelectActiveFrame(&dim, frameIdx % frameCount);
    }

    graphics.DrawImage(gif, 0, 0, width, height);
}
// some window proc stuff
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

    case WM_TIMER:
        // advance animation frame
        frameIndex++;

        // change state opening -> idle GIF
        if (playingOpening && (GetTickCount() - startTime >= OPENING_DURATION)) {
            playingOpening = false;
            frameIndex = 0; // reset for idle animation
        }
        InvalidateRect(hwnd, NULL, TRUE);
        break;

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
// entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Init GDI+
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Gdiplus::Ok) {
        MessageBoxW(NULL, L"Failed to initialize GDI+.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Popup message 
    MessageBoxW(NULL, L"Your device is filled with Determination!", L"Notice", MB_OK | MB_ICONINFORMATION);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OverlayWindowClass";
    RegisterClassW(&wc);

    // generate fullscreen window
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,  // Prevents appearing in Alt-Tab
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

    // cleanup
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