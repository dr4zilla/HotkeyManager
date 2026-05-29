//=============================================================================
// engine.h — Engine thread: hidden HWND, message loop, hotkey registration
// Hotkey & Hotstring Manager
//=============================================================================
#pragma once

#include <windows.h>

namespace Engine {

    //-------------------------------------------------------------------------
    // Initialize the engine on the CALLING thread (must be the main thread).
    //
    //   hInstance    - Application instance handle
    //   hReadyEvent - Manual-reset event to signal when engine HWND is valid
    //
    // This function:
    //   1. Registers the engine window class
    //   2. Creates the hidden message-only window
    //   3. Installs the panic keyboard hook
    //   4. Signals hReadyEvent
    //   5. Enters the message loop (BLOCKS until WM_QUIT)
    //
    // Returns the exit code from the message loop.
    //-------------------------------------------------------------------------
    int Initialize(HINSTANCE hInstance, HANDLE hReadyEvent);

    //-------------------------------------------------------------------------
    // Get the engine's hidden window handle.
    // Only valid AFTER hReadyEvent has been signaled.
    //-------------------------------------------------------------------------
    HWND GetHWND();

    //-------------------------------------------------------------------------
    // Request a clean shutdown of the engine thread.
    // Can be called from any thread.
    //-------------------------------------------------------------------------
    void RequestShutdown();

} // namespace Engine
