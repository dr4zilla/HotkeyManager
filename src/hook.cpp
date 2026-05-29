//=============================================================================
// hook.cpp — Low-level keyboard hook: panic intercept + hotstring engine
// Hotkey & Hotstring Manager
//
// Hotstring logic:
//   - A circular character buffer tracks recently typed printable characters.
//   - A match fires immediately when the last character of an abbreviation is
//     typed (no terminator required), but only when the character immediately
//     before the abbreviation in the buffer is a non-word character or the
//     abbreviation is at the start of the buffer. This prevents mid-word
//     firing when a letter directly precedes the abbreviation (e.g. typing
//     "testbtw" does not trigger "btw"), while still firing at the start of
//     a new word or after punctuation.
//   - On match: after passing the triggering key through via CallNextHookEx,
//     SendInput injects abbrev_len backspaces then the expansion text as
//     KEYEVENTF_UNICODE events.
//   - Injected events are identified by LLKHF_INJECTED and skipped entirely
//     so they do not corrupt the buffer or trigger recursion.
//=============================================================================

#include "hook.h"
#include "script_writer.h"
#include <vector>
#include <string>

//-----------------------------------------------------------------------------
// Module state
//-----------------------------------------------------------------------------
namespace {
    HHOOK g_hKeyboardHook = nullptr;

    // Typed-character buffer — stores the most recent BUFFER_MAX printable chars
    constexpr int BUFFER_MAX = 128;
    std::wstring g_buffer;

    // Hotstring list cached from ScriptWriter (refreshed on UpdateHotstrings)
    std::vector<HotstringEntry> g_hotstrings;

    std::wstring g_pendingExpansion;
    int          g_pendingDeleteCount = 0;

    // Track the virtual key code of the triggering key so we can block its corresponding keyup event
    DWORD        g_blockedVkCode = 0;

    // Magic stamp placed in dwExtraInfo on every INPUT we inject.
    // LLKHF_INJECTED is unreliable for KEYEVENTF_UNICODE events on some
    // Windows builds, so we use our own marker to skip our own keystrokes.
    constexpr ULONG_PTR INJECTION_MAGIC = 0x484B4D47; // 'HKMG'

    //-------------------------------------------------------------------------
    // Inject backspaces to erase the typed abbreviation, then insert the
    // expansion as Unicode keystrokes.
    // All injected INPUTs are stamped with INJECTION_MAGIC so the hook
    // callback can identify and skip them without relying on LLKHF_INJECTED.
    //-------------------------------------------------------------------------
    static void InjectExpansion(int deleteCount, const std::wstring& text, DWORD triggerVk)
    {
        std::vector<INPUT> inputs;
        inputs.reserve((triggerVk != 0 ? 1 : 0) + deleteCount * 2 + text.size() * 2);

        if (triggerVk != 0) {
            INPUT in = {};
            in.type             = INPUT_KEYBOARD;
            in.ki.wVk           = (WORD)triggerVk;
            in.ki.dwFlags       = KEYEVENTF_KEYUP;
            in.ki.dwExtraInfo   = INJECTION_MAGIC;
            inputs.push_back(in);
        }

        for (int i = 0; i < deleteCount; ++i) {
            INPUT in = {};
            in.type             = INPUT_KEYBOARD;
            in.ki.wVk           = VK_BACK;
            in.ki.dwExtraInfo   = INJECTION_MAGIC;
            inputs.push_back(in);
            in.ki.dwFlags       = KEYEVENTF_KEYUP;
            inputs.push_back(in);
        }

        for (wchar_t ch : text) {
            INPUT in = {};
            in.type             = INPUT_KEYBOARD;
            in.ki.wScan         = ch;
            in.ki.dwFlags       = KEYEVENTF_UNICODE;
            in.ki.dwExtraInfo   = INJECTION_MAGIC;
            inputs.push_back(in);
            in.ki.dwFlags       = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            inputs.push_back(in);
        }

        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }

} // anonymous namespace

//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

//=============================================================================
// Public API
//=============================================================================

bool Hook::Install(HINSTANCE hInstance)
{
    if (g_hKeyboardHook != nullptr) return true;

    // Cache initial hotstring list
    const auto& hs = ScriptWriter::GetHotstrings();
    g_hotstrings.assign(hs.begin(), hs.end());

    g_hKeyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        hInstance,
        0   // global hook
    );
    return (g_hKeyboardHook != nullptr);
}

void Hook::Uninstall()
{
    if (g_hKeyboardHook != nullptr) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = nullptr;
    }
}

void Hook::UpdateHotstrings()
{
    const auto& hs = ScriptWriter::GetHotstrings();
    g_hotstrings.assign(hs.begin(), hs.end());
    g_buffer.clear();
}

[[noreturn]] void Hook::EmergencyShutdown()
{
    if (g_hKeyboardHook != nullptr) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = nullptr;
    }
    ExitProcess(0);
}

//=============================================================================
// Hook callback — must return quickly
//=============================================================================

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        const KBDLLHOOKSTRUCT* pKbd = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);

        // Skip events we injected.
        // Primary guard: LLKHF_INJECTED (set by SendInput for all injected events).
        // Secondary guard: our dwExtraInfo magic stamp, because KEYEVENTF_UNICODE
        // events don't always receive LLKHF_INJECTED on all Windows builds.
        bool isOurs = (pKbd->flags & LLKHF_INJECTED) ||
                      (pKbd->dwExtraInfo == INJECTION_MAGIC);
        if (!isOurs) {
            // Block the keyup event corresponding to the triggered hotstring keydown
            if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                if (pKbd->vkCode == g_blockedVkCode) {
                    g_blockedVkCode = 0;
                    return 1; // Block key up
                }
            }

            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                DWORD vk = pKbd->vkCode;

                // ---- Panic: Ctrl+Shift+Esc ----
                if (vk == VK_ESCAPE &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                    (GetAsyncKeyState(VK_SHIFT)   & 0x8000))
                {
                    Hook::EmergencyShutdown();
                }

                // ---- Hotstring buffer ----
                // Pure modifier key-downs do not affect the typed text stream.
                bool isPureModifier =
                    (vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT   ||
                     vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
                     vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU    ||
                     vk == VK_LWIN   || vk == VK_RWIN      || vk == VK_APPS);

                if (!isPureModifier) {
                    if (vk == VK_BACK) {
                        if (!g_buffer.empty()) g_buffer.pop_back();
                    } else {
                        // Translate VK → character using the foreground window's layout
                        HWND  hFg  = GetForegroundWindow();
                        DWORD tid  = GetWindowThreadProcessId(hFg, nullptr);
                        HKL   layout = GetKeyboardLayout(tid);

                        // Build modifier state from async key state (more reliable in LL hooks)
                        BYTE keyState[256] = {};
                        if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) keyState[VK_SHIFT]   = 0x80;
                        if (GetAsyncKeyState(VK_LSHIFT)  & 0x8000) keyState[VK_LSHIFT]  = 0x80;
                        if (GetAsyncKeyState(VK_RSHIFT)  & 0x8000) keyState[VK_RSHIFT]  = 0x80;
                        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) keyState[VK_CONTROL] = 0x80;
                        if (GetAsyncKeyState(VK_MENU)    & 0x8000) keyState[VK_MENU]    = 0x80;
                        if (GetKeyState(VK_CAPITAL) & 0x0001)      keyState[VK_CAPITAL] = 0x01;

                        wchar_t chars[4] = {};
                        int count = ToUnicodeEx(vk, pKbd->scanCode, keyState,
                                                chars, 4, 0, layout);

                        if (count < 0) {
                            // Dead key — call again with same args to flush the pending
                            // dead-key state on this thread, then break the buffer context.
                            wchar_t dummy[4] = {};
                            ToUnicodeEx(vk, pKbd->scanCode, keyState, dummy, 4, 0, layout);
                            g_buffer.clear();
                        } else if (count == 1 && chars[0] >= L' ') {
                            if ((int)g_buffer.size() >= BUFFER_MAX) {
                                g_buffer.erase(0, 1);
                            }
                            g_buffer += chars[0];

                            // Check every abbreviation against the buffer tail.
                            // Fires immediately on the last character of the abbreviation.
                            for (const auto& hs : g_hotstrings) {
                                const std::wstring& abbrev = hs.abbreviation;
                                if (g_buffer.size() < abbrev.size()) continue;

                                size_t offset = g_buffer.size() - abbrev.size();

                                // Word-boundary guard: the character directly before the
                                // abbreviation must be a non-word character (or the
                                // abbreviation starts at the beginning of the buffer).
                                // This prevents "testbtw" from firing the "btw" hotstring.
                                if (offset > 0 &&
                                    (iswalnum(g_buffer[offset - 1]) ||
                                     g_buffer[offset - 1] == L'_'))
                                    continue;

                                if (g_buffer.compare(offset, abbrev.size(), abbrev) == 0) {
                                    g_pendingExpansion   = hs.expansion;
                                    g_pendingDeleteCount = (int)abbrev.size() - 1; // Last char is blocked
                                    g_blockedVkCode      = vk;                     // Block this key's up event
                                    g_buffer.resize(offset);
                                    break;
                                }
                            }
                        } else {
                            // Non-printable (Ctrl+key, function key, arrow, etc.) — break context
                            g_buffer.clear();
                        }
                    }
                }
            }
        }
    }

    // Expand if we matched.
    // If expanding, we block the triggering keydown event from reaching the target window.
    bool blockKey = false;
    if (!g_pendingExpansion.empty()) {
        blockKey = true;
    }

    LRESULT result = 0;
    if (blockKey) {
        result = 1; // Block the triggering keydown event
    } else {
        result = CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
    }

    if (blockKey) {
        std::wstring expansion = g_pendingExpansion;
        int deleteCount = g_pendingDeleteCount;
        DWORD triggerVk = g_blockedVkCode;

        // Clear the pending state BEFORE calling InjectExpansion to prevent recursive triggers
        g_pendingExpansion.clear();
        g_pendingDeleteCount = 0;

        InjectExpansion(deleteCount, expansion, triggerVk);
    }

    return result;
}
