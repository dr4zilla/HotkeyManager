//=============================================================================
// script_writer.h — hotkeys.dat file I/O and in-memory data model
// Hotkey & Hotstring Manager
//=============================================================================
#pragma once

#include <windows.h>
#include <string>
#include <vector>

//-----------------------------------------------------------------------------
// Data structures
//-----------------------------------------------------------------------------

struct HotkeyEntry {
    WORD vk;                 // Virtual key code
    WORD modifiers;          // HOTKEYF_CONTROL | HOTKEYF_ALT | HOTKEYF_SHIFT | HOTKEYF_EXT
    std::wstring target;     // File path or application to launch
};

struct HotstringEntry {
    std::wstring abbreviation;   // Trigger text (e.g., "syn")
    std::wstring expansion;      // Replacement text (e.g., "see you now")
};

namespace ScriptWriter {

    //-------------------------------------------------------------------------
    // Initialize with the directory where hotkeys.dat will be stored.
    //-------------------------------------------------------------------------
    void Initialize(const std::wstring& scriptDir);

    //-------------------------------------------------------------------------
    // Load existing hotkeys and hotstrings from hotkeys.dat.
    // Populates the in-memory vectors. Safe to call if file doesn't exist.
    //-------------------------------------------------------------------------
    bool Load();

    //-------------------------------------------------------------------------
    // Rewrite hotkeys.dat from in-memory state.
    // Returns true on success.
    //-------------------------------------------------------------------------
    bool Save();

    //-------------------------------------------------------------------------
    // Hotkey operations — each calls Save() internally.
    //-------------------------------------------------------------------------
    bool AddHotkey(WORD vk, WORD modifiers, const std::wstring& target);
    bool RemoveHotkey(size_t index);

    //-------------------------------------------------------------------------
    // Hotstring operations — each calls Save() internally.
    //-------------------------------------------------------------------------
    bool AddHotstring(const std::wstring& abbreviation, const std::wstring& expansion);
    bool RemoveHotstring(size_t index);

    //-------------------------------------------------------------------------
    // Read-only access to current state.
    //-------------------------------------------------------------------------
    const std::vector<HotkeyEntry>& GetHotkeys();
    const std::vector<HotstringEntry>& GetHotstrings();

    //-------------------------------------------------------------------------
    // Get the full path to the data file.
    //-------------------------------------------------------------------------
    const std::wstring& GetScriptPath();

    //-------------------------------------------------------------------------
    // Convert hotkey modifiers + VK to a human-readable display string.
    // e.g., HOTKEYF_CONTROL + VK_N → "Ctrl + N"
    //-------------------------------------------------------------------------
    std::wstring HotkeyToDisplayString(WORD vk, WORD modifiers);

} // namespace ScriptWriter
