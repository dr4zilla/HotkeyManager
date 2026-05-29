# Hotkey Manager

A lightweight, zero-dependency Win32 application for managing global keyboard hotkeys and hotstrings on Windows.

---

## Features

**Global Hotkeys**
- Bind any key combination (Ctrl, Alt, Shift, Win + key) to launch an application or open a file
- Hotkeys fire system-wide regardless of which window has focus
- Built-in conflict detection blocks dangerous bindings (system shortcuts, universal clipboard keys, browser shortcuts) before they can lock up your input

**Hotstrings (Text Expansion)**
- Define abbreviation → expansion pairs that fire as you type
- Expansions trigger immediately on the last character of the abbreviation — no terminator key required
- Word-boundary detection prevents mid-word false positives (typing `testbtw` does not trigger `btw`)
- Works in any application: browsers, editors, terminals, chat apps

**System Tray**
- Minimises to the system tray instead of the taskbar
- Double-click or right-click → Open to restore; right-click → Exit to quit
- Single-instance enforced across privilege levels (standard and elevated processes share the mutex)

**Emergency Exit**
- Press `Ctrl + Shift + Esc` at any time to immediately terminate the hook and exit the process — useful if a misbehaving hotstring gets stuck

---

## Requirements

- Windows 10 or Windows 11 (x64)
- Visual Studio 2022 with the **Desktop development with C++** workload
- Windows SDK 10.0

---

## Building

1. Open `HotkeyManager.sln` in Visual Studio 2022
2. Select **Release | x64** from the configuration drop-down
3. Build → Build Solution (`Ctrl+Shift+B`)
4. The output binary is written to `build\Release\HotkeyManager.exe`

The project has no external dependencies. Everything links against the standard Windows SDK libraries (`comctl32`, `shell32`, `comdlg32`).

---

## Usage

**Adding a hotkey**
1. Click inside the *Key Combination* field and press the desired key combination
2. Type the target path directly or click **...** to browse for an executable or file
3. Click **Add Hotkey** — the binding activates immediately, no restart required

**Adding a hotstring**
1. Switch to the *Hotstrings* tab
2. Enter the abbreviation (e.g. `btw`) and the expansion text (e.g. `by the way`)
3. Click **Add Hotstring** — from this point, typing `btw` anywhere on the system replaces it with the expansion

**Deleting an entry**
- Select the row in the list and click **Delete**

---

## Data File

All hotkeys and hotstrings are persisted in `hotkeys.dat`, stored next to the executable. The file format is plain UTF-8 text and can be inspected manually:

```
[hotkeys]
<vk_decimal>  <modifiers_decimal>  <target_path>

[hotstrings]
<abbreviation>  <expansion>
```

Backslash escape sequences (`\\`, `\n`, `\r`, `\t`) are used for special characters in field values.

---

## Architecture

| Component | Description |
|---|---|
| `main.cpp` | Entry point. Creates the sync event, initialises the script writer, spawns the GUI thread, runs the engine on the main thread. |
| `engine.cpp` | Hidden message-only window (`HWND_MESSAGE`) on the main thread. Owns `RegisterHotKey` / `UnregisterHotKey` and responds to `WM_HOTKEY`, `WM_ENGINE_RELOAD`, `WM_ENGINE_SHUTDOWN`. |
| `hook.cpp` | System-wide low-level keyboard hook (`WH_KEYBOARD_LL`). Manages the hotstring character buffer, word-boundary checking, and `SendInput` injection. |
| `gui.cpp` | GUI thread. Native Win32 tab control with two pages (Hotkeys, Hotstrings). Sends `WM_ENGINE_RELOAD` after each mutation. |
| `script_writer.cpp` | In-memory data model and `hotkeys.dat` I/O. Handles UTF-8 encoding, field escaping, and atomic file writes. |
| `conflict.cpp` | Static safety matrix of reserved system and application shortcuts. Called before any `RegisterHotKey` to prevent input lockouts. |

The engine and GUI run on separate threads. All coordination goes through posted window messages — no shared locks required outside of the `HANDLE` event used at startup.

---

## License

MIT License — see [LICENSE](LICENSE) for details.
