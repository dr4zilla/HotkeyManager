//=============================================================================
// engine.cpp — Engine thread: hidden HWND, message loop, hotkey registration
// Hotkey & Hotstring Manager
//
// The engine runs on the main thread. It owns a hidden message-only window
// that receives WM_HOTKEY events and reload/shutdown commands from the GUI.
//=============================================================================

#include "engine.h"
#include "hook.h"
#include "script_writer.h"
#include "resource.h"
#include <shellapi.h>
#include <commctrl.h>

//-----------------------------------------------------------------------------
// Module state
//-----------------------------------------------------------------------------
namespace {
    HWND      g_hEngineWnd = nullptr;
    HINSTANCE g_hInstance  = nullptr;
    int       g_registeredCount = 0;  // Number of currently registered hotkeys

    //-------------------------------------------------------------------------
    // Convert HOTKEYF_* modifier flags to MOD_* flags for RegisterHotKey.
    // HOTKEYF_EXT is used throughout this app to represent the Win key.
    //-------------------------------------------------------------------------
    static UINT HotkeyFlagsToMod(WORD modifiers)
    {
        UINT mod = MOD_NOREPEAT;  // Prevent repeated WM_HOTKEY while key is held
        if (modifiers & HOTKEYF_ALT)     mod |= MOD_ALT;
        if (modifiers & HOTKEYF_CONTROL) mod |= MOD_CONTROL;
        if (modifiers & HOTKEYF_SHIFT)   mod |= MOD_SHIFT;
        if (modifiers & HOTKEYF_EXT)     mod |= MOD_WIN;
        return mod;
    }

    //-------------------------------------------------------------------------
    // Unregister all currently registered hotkeys.
    //-------------------------------------------------------------------------
    static void UnregisterAllHotkeys()
    {
        for (int i = 0; i < g_registeredCount; ++i) {
            UnregisterHotKey(g_hEngineWnd, i);
        }
        g_registeredCount = 0;
    }

    //-------------------------------------------------------------------------
    // Register all hotkeys from the current in-memory state.
    // Uses the vector index as the hotkey ID so WM_HOTKEY wParam maps directly.
    //-------------------------------------------------------------------------
    static void RegisterAllHotkeys()
    {
        const auto& hotkeys = ScriptWriter::GetHotkeys();
        g_registeredCount = 0;

        for (int i = 0; i < (int)hotkeys.size(); ++i) {
            const auto& hk = hotkeys[i];
            UINT mod = HotkeyFlagsToMod(hk.modifiers);
            if (RegisterHotKey(g_hEngineWnd, i, mod, hk.vk)) {
                g_registeredCount = i + 1;
            }
            // Silent skip on failure — another app may have grabbed the combo.
        }
    }

    //-------------------------------------------------------------------------
    // Engine window procedure
    //-------------------------------------------------------------------------
    LRESULT CALLBACK EngineWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg) {

        case WM_HOTKEY:
        {
            int id = (int)wParam;
            const auto& hotkeys = ScriptWriter::GetHotkeys();
            if (id >= 0 && id < (int)hotkeys.size()) {
                ShellExecuteW(nullptr, L"open",
                              hotkeys[id].target.c_str(),
                              nullptr, nullptr, SW_SHOWNORMAL);
            }
            return 0;
        }

        case WM_ENGINE_RELOAD:
            // GUI has updated the data file; re-register hotkeys and refresh
            // the hotstring buffer in the hook.
            UnregisterAllHotkeys();
            RegisterAllHotkeys();
            Hook::UpdateHotstrings();
            return 0;

        case WM_ENGINE_SHUTDOWN:
            UnregisterAllHotkeys();
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

} // anonymous namespace

//=============================================================================
// Public API
//=============================================================================

int Engine::Initialize(HINSTANCE hInstance, HANDLE hReadyEvent)
{
    g_hInstance = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize       = sizeof(wc);
    wc.lpfnWndProc  = EngineWndProc;
    wc.hInstance    = hInstance;
    wc.lpszClassName = APP_CLASS_ENGINE;

    if (!RegisterClassExW(&wc)) return 1;

    g_hEngineWnd = CreateWindowExW(
        0, APP_CLASS_ENGINE, L"HotkeyMgrEngine",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr
    );
    if (g_hEngineWnd == nullptr) return 2;

    // Install the keyboard hook (panic + hotstring state machine)
    Hook::Install(hInstance);

    // Register all hotkeys from data loaded at startup
    RegisterAllHotkeys();

    // Signal the GUI thread that the engine is ready
    if (hReadyEvent != nullptr) SetEvent(hReadyEvent);

    // Block on the message loop until WM_QUIT
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Hook::Uninstall();
    UnregisterAllHotkeys();

    return (int)msg.wParam;
}

HWND Engine::GetHWND()
{
    return g_hEngineWnd;
}

void Engine::RequestShutdown()
{
    if (g_hEngineWnd != nullptr) {
        PostMessageW(g_hEngineWnd, WM_ENGINE_SHUTDOWN, 0, 0);
    }
}
