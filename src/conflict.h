//=============================================================================
// conflict.h — Global hotkey conflict detection
// Hotkey & Hotstring Manager
//=============================================================================
#pragma once

#include <windows.h>

namespace Conflict {

    //-------------------------------------------------------------------------
    // Check a proposed hotkey combination against the hardcoded safety matrix.
    //
    // Parameters:
    //   vk        - Virtual key code (from HKM_GETHOTKEY LOBYTE)
    //   modifiers - Modifier flags (HOTKEYF_CONTROL, HOTKEYF_ALT,
    //               HOTKEYF_SHIFT, HOTKEYF_EXT for Win key)
    //
    // Returns:
    //   nullptr   - No conflict, safe to register.
    //   non-null  - Pointer to a static description string explaining
    //               the conflicting system/app binding.
    //-------------------------------------------------------------------------
    const wchar_t* Check(WORD vk, WORD modifiers);

} // namespace Conflict
