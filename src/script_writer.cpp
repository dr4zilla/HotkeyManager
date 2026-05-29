//=============================================================================
// script_writer.cpp — hotkeys.dat file I/O and data model
// Hotkey & Hotstring Manager
//
// File format (tab-separated, UTF-8):
//   [hotkeys]
//   <vk_decimal> TAB <modifiers_decimal> TAB <target_path>
//
//   [hotstrings]
//   <abbreviation> TAB <expansion>
//
// Backslash escapes in field values: \\ \n \r \t
//=============================================================================

#include "script_writer.h"
#include "resource.h"
#include <commctrl.h>
#include <sstream>
#include <mutex>

//-----------------------------------------------------------------------------
// Module state
//-----------------------------------------------------------------------------
namespace {
    std::wstring                s_scriptDir;
    std::wstring                s_scriptPath;
    std::vector<HotkeyEntry>    s_hotkeys;
    std::vector<HotstringEntry> s_hotstrings;
    std::recursive_mutex        s_writerMutex;

    //-------------------------------------------------------------------------
    // VK display name table
    //-------------------------------------------------------------------------
    struct VkDisplayEntry {
        WORD vk;
        const wchar_t* displayName;
    };

    static const VkDisplayEntry s_vkDisplay[] = {
        { VK_F1,       L"F1"         }, { VK_F2,       L"F2"         },
        { VK_F3,       L"F3"         }, { VK_F4,       L"F4"         },
        { VK_F5,       L"F5"         }, { VK_F6,       L"F6"         },
        { VK_F7,       L"F7"         }, { VK_F8,       L"F8"         },
        { VK_F9,       L"F9"         }, { VK_F10,      L"F10"        },
        { VK_F11,      L"F11"        }, { VK_F12,      L"F12"        },
        { VK_RETURN,   L"Enter"      }, { VK_ESCAPE,   L"Esc"        },
        { VK_TAB,      L"Tab"        }, { VK_SPACE,    L"Space"      },
        { VK_BACK,     L"Backspace"  }, { VK_DELETE,   L"Delete"     },
        { VK_INSERT,   L"Insert"     }, { VK_HOME,     L"Home"       },
        { VK_END,      L"End"        }, { VK_PRIOR,    L"Page Up"    },
        { VK_NEXT,     L"Page Down"  }, { VK_UP,       L"Up"         },
        { VK_DOWN,     L"Down"       }, { VK_LEFT,     L"Left"       },
        { VK_RIGHT,    L"Right"      }, { VK_NUMPAD0,  L"Num 0"      },
        { VK_NUMPAD1,  L"Num 1"      }, { VK_NUMPAD2,  L"Num 2"      },
        { VK_NUMPAD3,  L"Num 3"      }, { VK_NUMPAD4,  L"Num 4"      },
        { VK_NUMPAD5,  L"Num 5"      }, { VK_NUMPAD6,  L"Num 6"      },
        { VK_NUMPAD7,  L"Num 7"      }, { VK_NUMPAD8,  L"Num 8"      },
        { VK_NUMPAD9,  L"Num 9"      }, { VK_MULTIPLY, L"Num *"      },
        { VK_ADD,      L"Num +"      }, { VK_SUBTRACT, L"Num -"      },
        { VK_DECIMAL,  L"Num ."      }, { VK_DIVIDE,   L"Num /"      },
        { VK_SNAPSHOT, L"PrtScn"     }, { VK_SCROLL,   L"Scroll Lock"},
        { VK_PAUSE,    L"Pause"      }, { VK_CAPITAL,  L"Caps Lock"  },
        { VK_NUMLOCK,  L"Num Lock"   },
        { VK_OEM_1,    L";"          }, { VK_OEM_PLUS, L"="          },
        { VK_OEM_COMMA,L","          }, { VK_OEM_MINUS,L"-"          },
        { VK_OEM_PERIOD,L"."         }, { VK_OEM_2,    L"/"          },
        { VK_OEM_3,    L"`"          }, { VK_OEM_4,    L"["          },
        { VK_OEM_5,    L"\\"         }, { VK_OEM_6,    L"]"          },
        { VK_OEM_7,    L"'"          },
    };
    static const int s_vkDisplayCount = sizeof(s_vkDisplay) / sizeof(s_vkDisplay[0]);

    //-------------------------------------------------------------------------
    // Escape / unescape field values for the data file
    //-------------------------------------------------------------------------
    std::wstring EscapeField(const std::wstring& s)
    {
        std::wstring out;
        out.reserve(s.size());
        for (wchar_t c : s) {
            switch (c) {
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n";  break;
            case L'\r': out += L"\\r";  break;
            case L'\t': out += L"\\t";  break;
            default:    out += c;       break;
            }
        }
        return out;
    }

    std::wstring UnescapeField(const std::wstring& s)
    {
        std::wstring out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == L'\\' && i + 1 < s.size()) {
                switch (s[i + 1]) {
                case L'\\': out += L'\\'; ++i; break;
                case L'n':  out += L'\n'; ++i; break;
                case L'r':  out += L'\r'; ++i; break;
                case L't':  out += L'\t'; ++i; break;
                default:    out += s[i];  break;
                }
            } else {
                out += s[i];
            }
        }
        return out;
    }

    //-------------------------------------------------------------------------
    // Convert a wide string to UTF-8 bytes (no BOM).
    //-------------------------------------------------------------------------
    std::string WideToUtf8(const std::wstring& wide)
    {
        if (wide.empty()) return {};
        int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(),
                                         nullptr, 0, nullptr, nullptr);
        if (needed <= 0) return {};
        std::string utf8(needed, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(),
                            &utf8[0], needed, nullptr, nullptr);
        return utf8;
    }

    //-------------------------------------------------------------------------
    // Read an entire file as a wide string (assuming UTF-8 input).
    //-------------------------------------------------------------------------
    std::wstring ReadFileAsWide(const std::wstring& path)
    {
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return {};

        DWORD fileSize = GetFileSize(hFile, nullptr);
        if (fileSize == 0 || fileSize == INVALID_FILE_SIZE || fileSize > 10 * 1024 * 1024) {
            CloseHandle(hFile);
            return {};
        }

        std::string utf8(fileSize, '\0');
        DWORD bytesRead = 0;
        ReadFile(hFile, &utf8[0], fileSize, &bytesRead, nullptr);
        CloseHandle(hFile);

        // Skip BOM if present
        const char* start = utf8.c_str();
        if (bytesRead >= 3 &&
            (unsigned char)start[0] == 0xEF &&
            (unsigned char)start[1] == 0xBB &&
            (unsigned char)start[2] == 0xBF)
        {
            start += 3;
            bytesRead -= 3;
        }

        int needed = MultiByteToWideChar(CP_UTF8, 0, start, (int)bytesRead, nullptr, 0);
        if (needed <= 0) return {};
        std::wstring wide(needed, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, start, (int)bytesRead, &wide[0], needed);
        return wide;
    }

} // anonymous namespace

//=============================================================================
// Public API
//=============================================================================

void ScriptWriter::Initialize(const std::wstring& scriptDir)
{
    s_scriptDir  = scriptDir;
    s_scriptPath = scriptDir + L"\\" + SCRIPT_FILENAME;
}

const std::wstring& ScriptWriter::GetScriptPath()
{
    return s_scriptPath;
}

//-----------------------------------------------------------------------------
// HotkeyToDisplayString — Human-readable format for ListView
//-----------------------------------------------------------------------------
std::wstring ScriptWriter::HotkeyToDisplayString(WORD vk, WORD modifiers)
{
    std::wstring result;

    if (modifiers & HOTKEYF_CONTROL) result += L"Ctrl + ";
    if (modifiers & HOTKEYF_ALT)     result += L"Alt + ";
    if (modifiers & HOTKEYF_SHIFT)   result += L"Shift + ";
    if (modifiers & HOTKEYF_EXT)     result += L"Win + ";

    if (vk >= 'A' && vk <= 'Z') {
        result += static_cast<wchar_t>(vk);
    } else if (vk >= '0' && vk <= '9') {
        result += static_cast<wchar_t>(vk);
    } else {
        for (int i = 0; i < s_vkDisplayCount; ++i) {
            if (s_vkDisplay[i].vk == vk) {
                result += s_vkDisplay[i].displayName;
                return result;
            }
        }
        wchar_t buf[16];
        wsprintfW(buf, L"VK 0x%02X", vk);
        result += buf;
    }

    return result;
}

//-----------------------------------------------------------------------------
// Load — Parse hotkeys.dat and populate in-memory vectors
//-----------------------------------------------------------------------------
bool ScriptWriter::Load()
{
    std::lock_guard<std::recursive_mutex> lock(s_writerMutex);
    s_hotkeys.clear();
    s_hotstrings.clear();

    std::wstring content = ReadFileAsWide(s_scriptPath);
    if (content.empty()) return false;

    enum class Section { None, Hotkeys, Hotstrings };
    Section section = Section::None;

    std::wistringstream stream(content);
    std::wstring line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty() || line[0] == L';') continue;

        if (line == L"[hotkeys]")    { section = Section::Hotkeys;    continue; }
        if (line == L"[hotstrings]") { section = Section::Hotstrings; continue; }

        if (section == Section::Hotkeys) {
            // <vk_dec> TAB <mod_dec> TAB <target>
            size_t t1 = line.find(L'\t');
            if (t1 == std::wstring::npos) continue;
            size_t t2 = line.find(L'\t', t1 + 1);
            if (t2 == std::wstring::npos) continue;

            WORD vk  = (WORD)_wtoi(line.substr(0, t1).c_str());
            WORD mod = (WORD)_wtoi(line.substr(t1 + 1, t2 - t1 - 1).c_str());
            std::wstring target = UnescapeField(line.substr(t2 + 1));

            if (vk != 0 && !target.empty()) {
                s_hotkeys.push_back({ vk, mod, target });
            }
        } else if (section == Section::Hotstrings) {
            // <abbreviation> TAB <expansion>
            size_t t1 = line.find(L'\t');
            if (t1 == std::wstring::npos) continue;

            std::wstring abbrev    = line.substr(0, t1);
            std::wstring expansion = UnescapeField(line.substr(t1 + 1));

            if (!abbrev.empty() && !expansion.empty()) {
                s_hotstrings.push_back({ abbrev, expansion });
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
// Save — Rewrite the entire data file from in-memory state
//-----------------------------------------------------------------------------
bool ScriptWriter::Save()
{
    std::lock_guard<std::recursive_mutex> lock(s_writerMutex);
    std::wstring content;
    content += L"; Hotkey Manager data file — do not edit manually\r\n";
    content += L"\r\n";

    content += L"[hotkeys]\r\n";
    for (const auto& hk : s_hotkeys) {
        content += std::to_wstring(hk.vk);
        content += L'\t';
        content += std::to_wstring(hk.modifiers);
        content += L'\t';
        content += EscapeField(hk.target);
        content += L"\r\n";
    }
    content += L"\r\n";

    content += L"[hotstrings]\r\n";
    for (const auto& hs : s_hotstrings) {
        content += hs.abbreviation;
        content += L'\t';
        content += EscapeField(hs.expansion);
        content += L"\r\n";
    }

    std::string utf8 = WideToUtf8(content);

    HANDLE hFile = CreateFileW(
        s_scriptPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, utf8.c_str(), (DWORD)utf8.size(), &written, nullptr);
    FlushFileBuffers(hFile);
    CloseHandle(hFile);

    return (ok && written == (DWORD)utf8.size());
}

//-----------------------------------------------------------------------------
// Mutation operations
//-----------------------------------------------------------------------------

bool ScriptWriter::AddHotkey(WORD vk, WORD modifiers, const std::wstring& target)
{
    std::lock_guard<std::recursive_mutex> lock(s_writerMutex);
    s_hotkeys.push_back({ vk, modifiers, target });
    return Save();
}

bool ScriptWriter::RemoveHotkey(size_t index)
{
    std::lock_guard<std::recursive_mutex> lock(s_writerMutex);
    if (index >= s_hotkeys.size()) return false;
    s_hotkeys.erase(s_hotkeys.begin() + index);
    return Save();
}

bool ScriptWriter::AddHotstring(const std::wstring& abbreviation, const std::wstring& expansion)
{
    std::lock_guard<std::recursive_mutex> lock(s_writerMutex);
    s_hotstrings.push_back({ abbreviation, expansion });
    return Save();
}

bool ScriptWriter::RemoveHotstring(size_t index)
{
    std::lock_guard<std::recursive_mutex> lock(s_writerMutex);
    if (index >= s_hotstrings.size()) return false;
    s_hotstrings.erase(s_hotstrings.begin() + index);
    return Save();
}

//-----------------------------------------------------------------------------
// Read-only accessors
//-----------------------------------------------------------------------------

std::vector<HotkeyEntry> ScriptWriter::GetHotkeys()
{
    std::lock_guard<std::recursive_mutex> lock(s_writerMutex);
    return s_hotkeys;
}

std::vector<HotstringEntry> ScriptWriter::GetHotstrings()
{
    std::lock_guard<std::recursive_mutex> lock(s_writerMutex);
    return s_hotstrings;
}
