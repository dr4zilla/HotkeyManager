//=============================================================================
// hook.h — Low-level keyboard hook: panic intercept + hotstring state machine
// Hotkey & Hotstring Manager
//=============================================================================
#pragma once

#include <windows.h>

namespace Hook {

    //-------------------------------------------------------------------------
    // Install the global low-level keyboard hook.
    // Must be called from a thread that runs a message pump.
    // Returns true on success.
    //-------------------------------------------------------------------------
    bool Install(HINSTANCE hInstance);

    //-------------------------------------------------------------------------
    // Cleanly uninstall the keyboard hook (if installed).
    //-------------------------------------------------------------------------
    void Uninstall();

    //-------------------------------------------------------------------------
    // Refresh the hotstring list cached inside the hook from ScriptWriter.
    // Call after any hotstring add/remove (i.e., on WM_ENGINE_RELOAD).
    // Also clears the typed-character buffer.
    //-------------------------------------------------------------------------
    void UpdateHotstrings();

    //-------------------------------------------------------------------------
    // Emergency shutdown: uninstall the hook and ExitProcess(0).
    // Called from the hook callback on Ctrl+Shift+Esc. Does NOT return.
    //-------------------------------------------------------------------------
    [[noreturn]] void EmergencyShutdown();

} // namespace Hook
