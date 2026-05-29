# Hotkey Manager

![Build](https://github.com/dr4zilla/HotkeyManager/actions/workflows/msbuild.yml/badge.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D4?logo=windows&logoColor=white)
![Language](https://img.shields.io/badge/C%2B%2B17-Win32-00599C?logo=cplusplus&logoColor=white)

A native Win32 utility for binding global keyboard shortcuts and text expansions on Windows — no runtime, no interpreter, no background service. One `.exe`, pure Win32 API.

---

## What it does

**Global Hotkeys** — Bind any modifier + key combination to instantly launch an application or open a file, system-wide, regardless of which window has focus.

**Text Expansion (Hotstrings)** — Define abbreviation → expansion pairs that fire as you type. Type `btw` anywhere and it replaces with `by the way`. Works in every application: browsers, terminals, editors, chat apps.

**Conflict detection** — A built-in safety matrix blocks registrations that would override critical system shortcuts (Win+L, Ctrl+Alt+Del, Alt+F4, etc.) before they can lock up your input.

**System tray** — Runs quietly in the tray. Double-click to open, right-click to exit. Minimising or closing the window sends it back to the tray instead of quitting.

**Single instance** — Only one copy runs at a time, even across different privilege levels (standard user vs. elevated). A second launch restores the existing window.

**Emergency exit** — `Ctrl+Shift+Esc` immediately unhooks and exits the process if anything goes wrong.

---

## Known Limitations

**Elevated windows** — When running as a standard user, the keyboard hook cannot inject text into applications running as Administrator (e.g. an elevated terminal). Run `HotkeyManager.exe` as Administrator if you need hotstrings to work everywhere.

**Antivirus flags** — A global keyboard hook that monitors input and injects keystrokes matches the heuristic signature of a keylogger. Some AV engines will flag or quarantine the binary. The source is fully open — build from source if you need to verify it.

---

## Building

**Requirements**
- Windows 10 or 11 (x64)
- Visual Studio 2022 or later with the *Desktop development with C++* workload
- Windows SDK 10.0

**Steps**
1. Open `HotkeyManager.sln`
2. Set configuration to **Release | x64**
3. `Ctrl+Shift+B` → Build Solution
4. Binary is output to `build\Release\HotkeyManager.exe`

No external packages. Links only against `comctl32`, `shell32`, and `comdlg32` from the Windows SDK.

---

## Usage

**Adding a hotkey**
1. Click in the *Key Combination* field and press the desired shortcut
2. Enter the target path or click **...** to browse for an executable or file
3. Click **Add Hotkey** — the binding is active immediately

**Adding a hotstring**
1. Switch to the *Hotstrings* tab
2. Enter the abbreviation and the expansion text
3. Click **Add Hotstring** — typing the abbreviation anywhere replaces it inline

**Removing an entry** — select the row and click **Delete**

**Hotstring matching** fires on the last character of the abbreviation with no terminator key. A word-boundary check prevents false positives — typing `testbtw` will not trigger a `btw` hotstring.

---

## Data

Everything is stored in `hotkeys.dat` next to the executable. Plain UTF-8, human-readable:

```
; Hotkey Manager data file
[hotkeys]
78	6	C:\Windows\notepad.exe

[hotstrings]
btw	by the way
omw	on my way
```

Columns for hotkeys are `vk_decimal`, `modifiers_decimal`, `target_path`. Backslash escapes (`\\`, `\n`, `\t`) are used in field values.

---

## Architecture

The application uses two threads with no shared locks — all coordination goes through posted window messages.

```
Main thread                    GUI thread
───────────────                ────────────────────────────
Engine (hidden HWND)           Win32 window (Tab control)
├─ WH_KEYBOARD_LL hook         ├─ Hotkeys tab
│   ├─ Hotstring buffer        │   ├─ MSCTLS_HOTKEY32 input
│   └─ SendInput injection     │   └─ ListView
├─ RegisterHotKey              ├─ Hotstrings tab
└─ WM_HOTKEY → ShellExecute    │   └─ ListView
                               └─ Posts WM_ENGINE_RELOAD
                                  on every add/delete
```

| File | Responsibility |
|---|---|
| `engine.cpp` | Hidden message window, hotkey registration, `WM_HOTKEY` dispatch |
| `hook.cpp` | Low-level keyboard hook, typed-character buffer, expansion injection |
| `gui.cpp` | Tab UI, tray icon, user input, file browser |
| `script_writer.cpp` | In-memory data model, `hotkeys.dat` read/write, UTF-8 I/O |
| `conflict.cpp` | Static safety matrix of reserved system and app shortcuts |
| `main.cpp` | Entry point, single-instance mutex, thread orchestration |

---

## License

[MIT](LICENSE)
