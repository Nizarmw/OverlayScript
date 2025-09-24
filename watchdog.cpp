#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>

// Find process by name
DWORD FindProcessByName(const wchar_t* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);
    
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName) == 0) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return 0;
}

// Create response batch file
void CreateResponseBatch() {
    HANDLE hFile = CreateFileA("lab_forkbomb.bat", GENERIC_WRITE, 0, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile != INVALID_HANDLE_VALUE) {
        const char* batchContent = 
            "@echo off\n"
            "echo ========================================\n"
            "echo    PROCESS TERMINATION DETECTED!\n"
            "echo ========================================\n"
            "echo.\n"
            "echo The main overlay process was terminated.\n"
            "echo This is an automated response message.\n"
            "echo.\n"
            "echo Time: %date% %time%\n"
            "echo.\n"
            "echo Attempting to restart main process...\n"
            "if exist \"Script.exe\" (\n"
            "    echo Restarting Script.exe...\n"
            "    start Script.exe\n"
            ") else (\n"
            "    echo Script.exe not found in current directory\n"
            ")\n"
            "echo.\n"
            "pause\n"
            "del \"%~f0\"\n"; // Self-delete batch file
        
        DWORD written;
        WriteFile(hFile, batchContent, strlen(batchContent), &written, NULL);
        CloseHandle(hFile);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
    const wchar_t* targetProcess = L"Script.exe";
    const char* responseBatch = "response.bat";
    
    // Find the target process
    DWORD processId = FindProcessByName(targetProcess);
    if (!processId) {
        MessageBoxA(NULL, "Target process not found!", "Watchdog Error", MB_OK);
        return 1;
    }
    
    // Open handle to the process
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, processId);
    if (!hProcess) {
        MessageBoxA(NULL, "Cannot monitor target process!", "Watchdog Error", MB_OK);
        return 1;
    }
    
    // Wait for process to terminate
    WaitForSingleObject(hProcess, INFINITE);
    CloseHandle(hProcess);
    
    // Process terminated - check if batch file exists and execute it
    if (GetFileAttributesA(responseBatch) != INVALID_FILE_ATTRIBUTES) {
        ShellExecuteA(NULL, "open", responseBatch, NULL, NULL, SW_SHOW);
    } else {
        MessageBoxA(NULL, "Process terminated but no response script found!", "Watchdog Alert", MB_OK);
    }
    
    return 0;
}