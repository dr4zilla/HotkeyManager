//=============================================================================
// conflict.cpp — Hardcoded safety matrix for global shortcut conflict detection
// Hotkey & Hotstring Manager
//=============================================================================

#include "conflict.h"
#include <commctrl.h>  // HOTKEYF_* constants

//-----------------------------------------------------------------------------
// Internal types
//-----------------------------------------------------------------------------
namespace {

struct ConflictEntry {
    WORD modifiers;        // HOTKEYF_CONTROL, HOTKEYF_ALT, HOTKEYF_SHIFT, HOTKEYF_EXT (Win)
    WORD vk;               // Virtual key code
    const wchar_t* desc;   // Human-readable description
};

//-----------------------------------------------------------------------------
// Safety matrix — hardcoded entries that must NEVER be overridden.
//
// Modifier mapping:
//   HOTKEYF_CONTROL = 0x02  → Ctrl
//   HOTKEYF_ALT     = 0x04  → Alt
//   HOTKEYF_SHIFT   = 0x01  → Shift
//   HOTKEYF_EXT     = 0x08  → Win key
//-----------------------------------------------------------------------------
static const ConflictEntry s_matrix[] = {

    // =====================================================================
    // Windows OS — Critical system shortcuts
    // =====================================================================
    { HOTKEYF_EXT,                              'L',            L"Windows: Lock Workstation (Win+L)" },
    { HOTKEYF_EXT,                              'E',            L"Windows: File Explorer (Win+E)" },
    { HOTKEYF_EXT,                              'D',            L"Windows: Show Desktop (Win+D)" },
    { HOTKEYF_EXT,                              'R',            L"Windows: Run Dialog (Win+R)" },
    { HOTKEYF_EXT,                              'I',            L"Windows: Settings (Win+I)" },
    { HOTKEYF_EXT,                              'S',            L"Windows: Search (Win+S)" },
    { HOTKEYF_EXT,                              'X',            L"Windows: Quick Link Menu (Win+X)" },
    { HOTKEYF_EXT,                              'P',            L"Windows: Project Display (Win+P)" },
    { HOTKEYF_EXT,                              'A',            L"Windows: Action Center (Win+A)" },
    { HOTKEYF_EXT,                              'G',            L"Windows: Game Bar (Win+G)" },
    { HOTKEYF_EXT,                              'H',            L"Windows: Dictation (Win+H)" },
    { HOTKEYF_EXT,                              'K',            L"Windows: Connect (Win+K)" },
    { HOTKEYF_EXT,                              'M',            L"Windows: Minimize All (Win+M)" },
    { HOTKEYF_EXT,                              'V',            L"Windows: Clipboard History (Win+V)" },
    { HOTKEYF_EXT,                              VK_TAB,         L"Windows: Task View (Win+Tab)" },
    { HOTKEYF_EXT,                              VK_SNAPSHOT,    L"Windows: Screenshot (Win+PrtScn)" },
    { HOTKEYF_EXT | HOTKEYF_SHIFT,              'S',            L"Windows: Snipping Tool (Win+Shift+S)" },
    { HOTKEYF_EXT,                              VK_UP,          L"Windows: Maximize Window (Win+Up)" },
    { HOTKEYF_EXT,                              VK_DOWN,        L"Windows: Minimize Window (Win+Down)" },
    { HOTKEYF_EXT,                              VK_LEFT,        L"Windows: Snap Left (Win+Left)" },
    { HOTKEYF_EXT,                              VK_RIGHT,       L"Windows: Snap Right (Win+Right)" },
    { HOTKEYF_EXT,                              VK_OEM_PERIOD,  L"Windows: Emoji Panel (Win+.)" },

    // Alt-based system shortcuts
    { HOTKEYF_ALT,                              VK_TAB,         L"Windows: Switch Window (Alt+Tab)" },
    { HOTKEYF_ALT,                              VK_F4,          L"Windows: Close Window (Alt+F4)" },
    { HOTKEYF_ALT,                              VK_RETURN,      L"Windows: Window Properties (Alt+Enter)" },
    { HOTKEYF_ALT,                              VK_SPACE,       L"Windows: Window System Menu (Alt+Space)" },

    // Ctrl+Alt+Del and Ctrl+Shift+Esc
    { HOTKEYF_CONTROL | HOTKEYF_ALT,            VK_DELETE,      L"Windows: Security Screen (Ctrl+Alt+Del)" },
    { HOTKEYF_CONTROL | HOTKEYF_SHIFT,           VK_ESCAPE,      L"Windows: Task Manager (Ctrl+Shift+Esc)" },

    // =====================================================================
    // Universal application shortcuts (browsers, editors, productivity)
    // =====================================================================

    // Clipboard
    { HOTKEYF_CONTROL,                          'C',            L"Universal: Copy (Ctrl+C)" },
    { HOTKEYF_CONTROL,                          'V',            L"Universal: Paste (Ctrl+V)" },
    { HOTKEYF_CONTROL,                          'X',            L"Universal: Cut (Ctrl+X)" },

    // Undo / Redo
    { HOTKEYF_CONTROL,                          'Z',            L"Universal: Undo (Ctrl+Z)" },
    { HOTKEYF_CONTROL,                          'Y',            L"Universal: Redo (Ctrl+Y)" },
    { HOTKEYF_CONTROL | HOTKEYF_SHIFT,          'Z',            L"Universal: Redo (Ctrl+Shift+Z)" },

    // Selection
    { HOTKEYF_CONTROL,                          'A',            L"Universal: Select All (Ctrl+A)" },

    // File operations
    { HOTKEYF_CONTROL,                          'S',            L"Universal: Save (Ctrl+S)" },
    { HOTKEYF_CONTROL | HOTKEYF_SHIFT,          'S',            L"Universal: Save As (Ctrl+Shift+S)" },
    { HOTKEYF_CONTROL,                          'O',            L"Universal: Open (Ctrl+O)" },
    { HOTKEYF_CONTROL,                          'N',            L"Universal: New (Ctrl+N)" },
    { HOTKEYF_CONTROL,                          'P',            L"Universal: Print (Ctrl+P)" },

    // Find / Replace
    { HOTKEYF_CONTROL,                          'F',            L"Universal: Find (Ctrl+F)" },
    { HOTKEYF_CONTROL,                          'H',            L"Universal: Replace (Ctrl+H)" },

    // Browser tab management
    { HOTKEYF_CONTROL,                          'W',            L"Browser: Close Tab (Ctrl+W)" },
    { HOTKEYF_CONTROL,                          'T',            L"Browser: New Tab (Ctrl+T)" },
    { HOTKEYF_CONTROL | HOTKEYF_SHIFT,          'T',            L"Browser: Reopen Tab (Ctrl+Shift+T)" },
    { HOTKEYF_CONTROL,                          VK_TAB,         L"Browser: Next Tab (Ctrl+Tab)" },
    { HOTKEYF_CONTROL | HOTKEYF_SHIFT,          VK_TAB,         L"Browser: Previous Tab (Ctrl+Shift+Tab)" },
    { HOTKEYF_CONTROL,                          'L',            L"Browser: Address Bar (Ctrl+L)" },
    { HOTKEYF_CONTROL,                          'R',            L"Browser: Refresh (Ctrl+R)" },
    { HOTKEYF_CONTROL | HOTKEYF_SHIFT,          'N',            L"Browser: Private Window (Ctrl+Shift+N)" },

    // Function keys
    { 0,                                        VK_F1,          L"Universal: Help (F1)" },
    { 0,                                        VK_F2,          L"Universal: Rename (F2)" },
    { 0,                                        VK_F3,          L"Universal: Find Next (F3)" },
    { 0,                                        VK_F5,          L"Universal: Refresh (F5)" },
    { 0,                                        VK_F11,         L"Universal: Fullscreen (F11)" },
    { HOTKEYF_CONTROL | HOTKEYF_SHIFT,          'I',            L"Browser: Developer Tools (Ctrl+Shift+I)" },
};

static const int s_matrixCount = sizeof(s_matrix) / sizeof(s_matrix[0]);

} // anonymous namespace

//=============================================================================
// Public API
//=============================================================================

const wchar_t* Conflict::Check(WORD vk, WORD modifiers)
{
    // Normalize: uppercase the VK code for letter keys so comparisons are
    // consistent regardless of how the hotkey control reports them.
    WORD normalizedVk = vk;
    if (normalizedVk >= 'a' && normalizedVk <= 'z') {
        normalizedVk -= 32;  // to uppercase
    }

    // Protect against modifier-free standard keyboard keys (letters, numbers, space, arrows, etc.)
    // to prevent the user from accidentally locking themselves out of basic typing.
    // We only allow Function keys (F1-F24) and Media/Browser keys without modifiers.
    if (modifiers == 0) {
        bool isAllowedNoMod = (normalizedVk >= VK_F1 && normalizedVk <= VK_F24) ||
                              (normalizedVk >= 0xA6 && normalizedVk <= 0xB7);
        if (!isAllowedNoMod) {
            return L"Standard typing keys (letters, numbers, symbols, space, arrows, etc.) cannot be registered as hotkeys without at least one modifier key (Ctrl, Alt, Shift, or Win).";
        }
    }

    for (int i = 0; i < s_matrixCount; ++i) {
        if (s_matrix[i].vk == normalizedVk && s_matrix[i].modifiers == modifiers) {
            return s_matrix[i].desc;
        }
    }

    return nullptr;  // No conflict — safe to register
}
