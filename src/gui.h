//=============================================================================
// gui.h — GUI thread: Win32 native UI for hotkey/hotstring management
// Hotkey & Hotstring Manager
//=============================================================================
#pragma once

#include <windows.h>

namespace Gui {

    //-------------------------------------------------------------------------
    // GUI thread entry point. Called via CreateThread.
    //
    //   lpParam - HANDLE to the engine-ready event (manual-reset)
    //
    // This function:
    //   1. Waits for the engine HWND to be valid (hReadyEvent)
    //   2. Initializes common controls
    //   3. Creates the main GUI window with tabs
    //   4. Loads existing hotkeys/hotstrings from file into ListViews
    //   5. Enters its own message loop
    //   6. On exit, posts WM_ENGINE_SHUTDOWN to the engine thread
    //
    // Returns 0 on clean exit.
    //-------------------------------------------------------------------------
    DWORD WINAPI ThreadProc(LPVOID lpParam);

} // namespace Gui
