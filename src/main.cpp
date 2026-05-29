//=============================================================================
// main.cpp — Application entry point and thread orchestration
// Hotkey & Hotstring Manager
//
// Architecture:
//   Thread 1 (main):  Engine — hidden HWND, keyboard hook, message loop
//   Thread 2 (child): GUI    — Win32 native UI, waits for engine readiness
//
// Startup sequence:
//   1. Create synchronization event (manual-reset)
//   2. Initialize script writer with the application directory
//   3. Load any existing hotkey/hotstring data from the script file
//   4. Spawn the GUI thread, passing the event handle
//   5. Run the engine on the main thread (blocks on message loop)
//   6. Cleanup and exit
//=============================================================================

#include "engine.h"
#include "gui.h"
#include "script_writer.h"
#include "resource.h"

#include <windows.h>
#include <string>

//=============================================================================
// WinMain — Application entry point
//=============================================================================
int WINAPI wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_     LPWSTR    /*lpCmdLine*/,
    _In_     int       /*nCmdShow*/)
{
    // ---- Enforce Single Instance ----
    // Configure a security descriptor with a NULL DACL to allow access across different privilege levels (Admin vs Standard)
    SECURITY_DESCRIPTOR sd = {};
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    HANDLE hMutex = CreateMutexW(&sa, TRUE, L"Local\\HotkeyManagerSingleInstanceMutex");
    if (hMutex == nullptr) {
        // If creation failed (e.g. due to access rights), try to open the existing mutex
        hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\HotkeyManagerSingleInstanceMutex");
        if (hMutex != nullptr) {
            HWND hExistingWnd = FindWindowW(APP_CLASS_GUI, APP_NAME);
            if (hExistingWnd) {
                ShowWindow(hExistingWnd, SW_RESTORE);
                SetForegroundWindow(hExistingWnd);
            }
            CloseHandle(hMutex);
            return 0;
        }
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExistingWnd = FindWindowW(APP_CLASS_GUI, APP_NAME);
        if (hExistingWnd) {
            ShowWindow(hExistingWnd, SW_RESTORE);
            SetForegroundWindow(hExistingWnd);
        }
        CloseHandle(hMutex);
        return 0;
    }

    // ---- Determine the application directory ----
    // The data file (hotkeys.dat) lives next to the .exe
    wchar_t exePath[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return 1;
    }

    // Strip the filename to get the directory
    std::wstring appDir(exePath);
    size_t lastSlash = appDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        appDir = appDir.substr(0, lastSlash);
    }

    // ---- Initialize the script writer ----
    ScriptWriter::Initialize(appDir);
    ScriptWriter::Load();  // Load existing data (no-op if file doesn't exist)

    // ---- Create the synchronization event ----
    // Manual-reset: once signaled, it stays signaled for all waiters
    HANDLE hEngineReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (hEngineReady == nullptr) {
        MessageBoxW(nullptr,
            L"Failed to create synchronization event.\nThe application cannot start.",
            L"Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // ---- Spawn the GUI thread ----
    HANDLE hGuiThread = CreateThread(
        nullptr,            // Default security
        0,                  // Default stack size
        Gui::ThreadProc,    // Thread function
        hEngineReady,       // Parameter: the event handle
        0,                  // Start immediately
        nullptr             // We don't need the thread ID
    );

    if (hGuiThread == nullptr) {
        MessageBoxW(nullptr,
            L"Failed to create the GUI thread.\nThe application cannot start.",
            L"Fatal Error", MB_OK | MB_ICONERROR);
        CloseHandle(hEngineReady);
        return 2;
    }

    // ---- Run the engine on the main thread ----
    // This BLOCKS until the engine receives WM_QUIT
    int exitCode = Engine::Initialize(hInstance, hEngineReady);

    // ---- Wait for the GUI thread to finish (timeout: 5 seconds) ----
    WaitForSingleObject(hGuiThread, 5000);

    // ---- Cleanup ----
    CloseHandle(hGuiThread);
    CloseHandle(hEngineReady);
    CloseHandle(hMutex);

    return exitCode;
}
