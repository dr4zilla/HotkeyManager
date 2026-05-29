//=============================================================================
// gui.cpp — GUI thread: Win32 native UI with tab control
// Hotkey & Hotstring Manager
//
// Two-tab interface: Hotkeys (MSCTLS_HOTKEY32 + ListView) and
// Hotstrings (Edit controls + ListView). All add/delete operations
// write to hotkeys.dat and signal the engine thread for reload.
//=============================================================================

#include "gui.h"
#include "engine.h"
#include "script_writer.h"
#include "conflict.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")

//-----------------------------------------------------------------------------
// Module state
//-----------------------------------------------------------------------------
namespace {
    HINSTANCE g_hGuiInstance = nullptr;
    HWND      g_hMainWnd    = nullptr;
    HWND      g_hTabCtrl    = nullptr;
    bool      g_bTrayIconVisible = false;

    // Hotkey tab controls
    HWND g_hHotkeyInput     = nullptr;
    HWND g_hHotkeyTarget    = nullptr;
    HWND g_hHotkeyBrowse    = nullptr;
    HWND g_hHotkeyAdd       = nullptr;
    HWND g_hHotkeyDelete    = nullptr;
    HWND g_hHotkeyList      = nullptr;
    HWND g_hHotkeyLblCombo  = nullptr;
    HWND g_hHotkeyLblTarget = nullptr;

    // Hotstring tab controls
    HWND g_hHsAbbrev        = nullptr;
    HWND g_hHsExpand        = nullptr;
    HWND g_hHsAdd           = nullptr;
    HWND g_hHsDelete        = nullptr;
    HWND g_hHsList          = nullptr;
    HWND g_hHsLblAbbrev     = nullptr;
    HWND g_hHsLblExpand     = nullptr;

    // Layout constants
    constexpr int MARGIN        = 16;
    constexpr int LABEL_H       = 18;
    constexpr int CONTROL_H     = 28;
    constexpr int BUTTON_W      = 100;
    constexpr int BUTTON_H      = 32;
    constexpr int BROWSE_W      = 36;
    constexpr int TAB_TOP       = 8;
    constexpr int TAB_CONTENT_Y = 40;

    // Fonts
    HFONT g_hFontUI     = nullptr;
    HFONT g_hFontHeader = nullptr;

    //=========================================================================
    // Helper: Get text from an edit control
    //=========================================================================
    std::wstring GetEditText(HWND hEdit)
    {
        int len = GetWindowTextLengthW(hEdit);
        if (len <= 0) return {};
        std::wstring text(len + 1, L'\0');
        GetWindowTextW(hEdit, &text[0], len + 1);
        text.resize(len);
        return text;
    }

    //=========================================================================
    // Populate the hotkey ListView from in-memory data
    //=========================================================================
    void RefreshHotkeyList()
    {
        ListView_DeleteAllItems(g_hHotkeyList);

        const auto hotkeys = ScriptWriter::GetHotkeys();
        for (size_t i = 0; i < hotkeys.size(); ++i) {
            const auto& hk = hotkeys[i];

            std::wstring display = ScriptWriter::HotkeyToDisplayString(hk.vk, hk.modifiers);

            LVITEMW lvi = {};
            lvi.mask     = LVIF_TEXT;
            lvi.iItem    = (int)i;
            lvi.iSubItem = 0;
            lvi.pszText  = const_cast<LPWSTR>(display.c_str());
            ListView_InsertItem(g_hHotkeyList, &lvi);

            ListView_SetItemText(g_hHotkeyList, (int)i, 1,
                                 const_cast<LPWSTR>(hk.target.c_str()));
        }
    }

    //=========================================================================
    // Populate the hotstring ListView from in-memory data
    //=========================================================================
    void RefreshHotstringList()
    {
        ListView_DeleteAllItems(g_hHsList);

        const auto hotstrings = ScriptWriter::GetHotstrings();
        for (size_t i = 0; i < hotstrings.size(); ++i) {
            const auto& hs = hotstrings[i];

            LVITEMW lvi = {};
            lvi.mask     = LVIF_TEXT;
            lvi.iItem    = (int)i;
            lvi.iSubItem = 0;
            lvi.pszText  = const_cast<LPWSTR>(hs.abbreviation.c_str());
            ListView_InsertItem(g_hHsList, &lvi);

            ListView_SetItemText(g_hHsList, (int)i, 1,
                                 const_cast<LPWSTR>(hs.expansion.c_str()));
        }
    }

    //=========================================================================
    // Show/hide controls based on the active tab
    //=========================================================================
    void SwitchTab(int tabIndex)
    {
        // Tab 0 = Hotkeys, Tab 1 = Hotstrings
        int showHK = (tabIndex == 0) ? SW_SHOW : SW_HIDE;
        int showHS = (tabIndex == 1) ? SW_SHOW : SW_HIDE;

        ShowWindow(g_hHotkeyLblCombo,  showHK);
        ShowWindow(g_hHotkeyInput,     showHK);
        ShowWindow(g_hHotkeyLblTarget, showHK);
        ShowWindow(g_hHotkeyTarget,    showHK);
        ShowWindow(g_hHotkeyBrowse,    showHK);
        ShowWindow(g_hHotkeyAdd,       showHK);
        ShowWindow(g_hHotkeyDelete,    showHK);
        ShowWindow(g_hHotkeyList,      showHK);

        ShowWindow(g_hHsLblAbbrev,     showHS);
        ShowWindow(g_hHsAbbrev,        showHS);
        ShowWindow(g_hHsLblExpand,     showHS);
        ShowWindow(g_hHsExpand,        showHS);
        ShowWindow(g_hHsAdd,           showHS);
        ShowWindow(g_hHsDelete,        showHS);
        ShowWindow(g_hHsList,          showHS);
    }

    //=========================================================================
    // Signal the engine thread to reload hotkeys and hotstrings
    //=========================================================================
    void SignalReload()
    {
        HWND hEngine = Engine::GetHWND();
        if (hEngine != nullptr) {
            PostMessageW(hEngine, WM_ENGINE_RELOAD, 0, 0);
        }
    }

    //=========================================================================
    // Add/Remove System Tray Icon
    //=========================================================================
    bool AddTrayIcon(HWND hWnd)
    {
        if (g_bTrayIconVisible) return true;

        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAY_ICON;
        nid.hIcon = (HICON)LoadImageW(g_hGuiInstance, MAKEINTRESOURCEW(IDI_APP_ICON), 
                                      IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), 
                                      GetSystemMetrics(SM_CYSMICON), LR_SHARED);
        if (!nid.hIcon) {
            nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        }
        wcscpy_s(nid.szTip, APP_NAME);

        if (Shell_NotifyIconW(NIM_ADD, &nid)) {
            g_bTrayIconVisible = true;
            return true;
        }
        return false;
    }

    bool RemoveTrayIcon(HWND hWnd)
    {
        if (!g_bTrayIconVisible) return true;

        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hWnd;
        nid.uID = 1;

        if (Shell_NotifyIconW(NIM_DELETE, &nid)) {
            g_bTrayIconVisible = false;
            return true;
        }
        return false;
    }

    void ShowTrayContextMenu(HWND hWnd)
    {
        HMENU hMenu = CreatePopupMenu();
        if (hMenu) {
            AppendMenuW(hMenu, MF_STRING, IDC_TRAY_OPEN, L"Open Hotkey Manager");
            SetMenuDefaultItem(hMenu, IDC_TRAY_OPEN, FALSE);
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, IDC_TRAY_EXIT, L"Exit");

            POINT pt;
            GetCursorPos(&pt);

            SetForegroundWindow(hWnd);

            int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, nullptr);
            DestroyMenu(hMenu);

            if (cmd == IDC_TRAY_OPEN) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
                RemoveTrayIcon(hWnd);
            }
            else if (cmd == IDC_TRAY_EXIT) {
                RemoveTrayIcon(hWnd);
                DestroyWindow(hWnd);
            }
        }
    }

    //=========================================================================
    // Handle "Add Hotkey" button click
    //=========================================================================
    void OnAddHotkey()
    {
        // 1. Get the hotkey from the native control
        LRESULT hkResult = SendMessageW(g_hHotkeyInput, HKM_GETHOTKEY, 0, 0);
        WORD vk        = LOBYTE(LOWORD(hkResult));
        WORD modifiers = HIBYTE(LOWORD(hkResult));

        if (vk == 0) {
            MessageBoxW(g_hMainWnd,
                L"Please press a key combination in the hotkey input field.",
                L"No Hotkey", MB_OK | MB_ICONWARNING);
            return;
        }

        // 2. Get the target path
        std::wstring target = GetEditText(g_hHotkeyTarget);
        if (target.empty()) {
            MessageBoxW(g_hMainWnd,
                L"Please enter or browse for a target application or file path.",
                L"No Target", MB_OK | MB_ICONWARNING);
            return;
        }

        // 3. Conflict detection
        const wchar_t* conflict = Conflict::Check(vk, modifiers);
        if (conflict != nullptr) {
            std::wstring msg = L"This shortcut conflicts with a reserved system binding:\n\n";
            msg += conflict;
            msg += L"\n\nRegistration has been blocked to prevent system-wide input lockouts.";
            MessageBoxW(g_hMainWnd, msg.c_str(), L"Shortcut Conflict Detected",
                        MB_OK | MB_ICONERROR);
            return;
        }

        // 4. Check for duplicate hotkey
        const auto existing = ScriptWriter::GetHotkeys();
        for (const auto& hk : existing) {
            if (hk.vk == vk && hk.modifiers == modifiers) {
                std::wstring display = ScriptWriter::HotkeyToDisplayString(vk, modifiers);
                std::wstring msg = L"The hotkey " + display + L" is already registered.\n"
                                   L"Please delete the existing one first or choose a different combination.";
                MessageBoxW(g_hMainWnd, msg.c_str(), L"Duplicate Hotkey",
                            MB_OK | MB_ICONWARNING);
                return;
            }
        }

        // 5. Save and reload
        if (ScriptWriter::AddHotkey(vk, modifiers, target)) {
            RefreshHotkeyList();
            SignalReload();

            // Clear inputs
            SendMessageW(g_hHotkeyInput, HKM_SETHOTKEY, 0, 0);
            SetWindowTextW(g_hHotkeyTarget, L"");
        } else {
            MessageBoxW(g_hMainWnd,
                L"Failed to write hotkeys.dat. Check file permissions.",
                L"Write Error", MB_OK | MB_ICONERROR);
        }
    }

    //=========================================================================
    // Handle "Delete Hotkey" button click
    //=========================================================================
    void OnDeleteHotkey()
    {
        int sel = ListView_GetNextItem(g_hHotkeyList, -1, LVNI_SELECTED);
        if (sel < 0) {
            MessageBoxW(g_hMainWnd,
                L"Please select a hotkey from the list to delete.",
                L"No Selection", MB_OK | MB_ICONINFORMATION);
            return;
        }

        if (ScriptWriter::RemoveHotkey((size_t)sel)) {
            RefreshHotkeyList();
            SignalReload();
        }
    }

    //=========================================================================
    // Handle "Add Hotstring" button click
    //=========================================================================
    void OnAddHotstring()
    {
        std::wstring abbrev = GetEditText(g_hHsAbbrev);
        std::wstring expand = GetEditText(g_hHsExpand);

        if (abbrev.empty()) {
            MessageBoxW(g_hMainWnd,
                L"Please enter an abbreviation (trigger text).",
                L"No Abbreviation", MB_OK | MB_ICONWARNING);
            return;
        }

        if (expand.empty()) {
            MessageBoxW(g_hMainWnd,
                L"Please enter the expansion text.",
                L"No Expansion", MB_OK | MB_ICONWARNING);
            return;
        }

        // Check for duplicate abbreviation
        const auto existing = ScriptWriter::GetHotstrings();
        for (const auto& hs : existing) {
            if (hs.abbreviation == abbrev) {
                MessageBoxW(g_hMainWnd,
                    L"This abbreviation is already registered.\n"
                    L"Please delete the existing one first or use a different abbreviation.",
                    L"Duplicate Abbreviation", MB_OK | MB_ICONWARNING);
                return;
            }
        }

        if (ScriptWriter::AddHotstring(abbrev, expand)) {
            RefreshHotstringList();
            SignalReload();

            // Clear inputs
            SetWindowTextW(g_hHsAbbrev, L"");
            SetWindowTextW(g_hHsExpand, L"");
        } else {
            MessageBoxW(g_hMainWnd,
                L"Failed to write hotkeys.dat. Check file permissions.",
                L"Write Error", MB_OK | MB_ICONERROR);
        }
    }

    //=========================================================================
    // Handle "Delete Hotstring" button click
    //=========================================================================
    void OnDeleteHotstring()
    {
        int sel = ListView_GetNextItem(g_hHsList, -1, LVNI_SELECTED);
        if (sel < 0) {
            MessageBoxW(g_hMainWnd,
                L"Please select a hotstring from the list to delete.",
                L"No Selection", MB_OK | MB_ICONINFORMATION);
            return;
        }

        if (ScriptWriter::RemoveHotstring((size_t)sel)) {
            RefreshHotstringList();
            SignalReload();
        }
    }

    //=========================================================================
    // Handle "Browse" button click — file open dialog
    //=========================================================================
    void OnBrowseTarget()
    {
        wchar_t filename[MAX_PATH] = {};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = g_hMainWnd;
        ofn.lpstrFile    = filename;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = L"All Files (*.*)\0*.*\0"
                           L"Applications (*.exe)\0*.exe\0"
                           L"Shortcuts (*.lnk)\0*.lnk\0";
        ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileNameW(&ofn)) {
            SetWindowTextW(g_hHotkeyTarget, filename);
        }
    }

    //=========================================================================
    // Resize handler — dynamically layout all controls
    //=========================================================================
    void OnResize(int clientW, int clientH)
    {
        if (g_hTabCtrl == nullptr) return;

        // Tab control fills the window
        MoveWindow(g_hTabCtrl, 0, 0, clientW, clientH, TRUE);

        // Content area (inside tab control)
        int contentX = MARGIN;
        int contentY = TAB_CONTENT_Y;
        int contentW = clientW - MARGIN * 2;

        int y = contentY;

        // ---- Hotkey tab layout ----
        // Label: "Key Combination"
        MoveWindow(g_hHotkeyLblCombo, contentX, y, contentW, LABEL_H, TRUE);
        y += LABEL_H + 4;

        // Hotkey input control
        MoveWindow(g_hHotkeyInput, contentX, y, contentW, CONTROL_H, TRUE);
        y += CONTROL_H + 12;

        // Label: "Target (File / Application)"
        MoveWindow(g_hHotkeyLblTarget, contentX, y, contentW, LABEL_H, TRUE);
        y += LABEL_H + 4;

        // Target edit + Browse button
        int editW = contentW - BROWSE_W - 8;
        MoveWindow(g_hHotkeyTarget, contentX, y, editW, CONTROL_H, TRUE);
        MoveWindow(g_hHotkeyBrowse, contentX + editW + 8, y, BROWSE_W, CONTROL_H, TRUE);
        y += CONTROL_H + 16;

        // Add + Delete buttons
        MoveWindow(g_hHotkeyAdd, contentX, y, BUTTON_W, BUTTON_H, TRUE);
        MoveWindow(g_hHotkeyDelete, contentX + BUTTON_W + 12, y, BUTTON_W, BUTTON_H, TRUE);
        y += BUTTON_H + 12;

        // ListView fills remaining space
        int listH = clientH - y - MARGIN;
        if (listH < 60) listH = 60;
        MoveWindow(g_hHotkeyList, contentX, y, contentW, listH, TRUE);

        // ---- Hotstring tab layout (same Y positions) ----
        y = contentY;

        MoveWindow(g_hHsLblAbbrev, contentX, y, contentW, LABEL_H, TRUE);
        y += LABEL_H + 4;

        MoveWindow(g_hHsAbbrev, contentX, y, contentW, CONTROL_H, TRUE);
        y += CONTROL_H + 12;

        MoveWindow(g_hHsLblExpand, contentX, y, contentW, LABEL_H, TRUE);
        y += LABEL_H + 4;

        // Expansion edit (taller for multiline)
        int expandH = CONTROL_H * 3;
        MoveWindow(g_hHsExpand, contentX, y, contentW, expandH, TRUE);
        y += expandH + 16;

        MoveWindow(g_hHsAdd, contentX, y, BUTTON_W, BUTTON_H, TRUE);
        MoveWindow(g_hHsDelete, contentX + BUTTON_W + 12, y, BUTTON_W, BUTTON_H, TRUE);
        y += BUTTON_H + 12;

        listH = clientH - y - MARGIN;
        if (listH < 60) listH = 60;
        MoveWindow(g_hHsList, contentX, y, contentW, listH, TRUE);
    }

    //=========================================================================
    // Main window procedure
    //=========================================================================
    LRESULT CALLBACK GuiWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg) {

        case WM_CREATE:
        {
            g_hMainWnd = hWnd;

            // ---- Create fonts ----
            g_hFontUI = CreateFontW(
                -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
            );
            g_hFontHeader = CreateFontW(
                -13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
            );

            HINSTANCE hInst = ((LPCREATESTRUCTW)lParam)->hInstance;

            // ---- Tab control ----
            g_hTabCtrl = CreateWindowExW(
                0, WC_TABCONTROLW, nullptr,
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                0, 0, 0, 0,
                hWnd, (HMENU)IDC_TAB_CONTROL, hInst, nullptr
            );
            SendMessageW(g_hTabCtrl, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            // Insert tabs
            TCITEMW tci = {};
            tci.mask = TCIF_TEXT;
            tci.pszText = const_cast<LPWSTR>(L"  Hotkeys  ");
            TabCtrl_InsertItem(g_hTabCtrl, 0, &tci);
            tci.pszText = const_cast<LPWSTR>(L"  Hotstrings  ");
            TabCtrl_InsertItem(g_hTabCtrl, 1, &tci);

            // ==== HOTKEY TAB CONTROLS ====

            g_hHotkeyLblCombo = CreateWindowExW(
                0, L"STATIC", L"Key Combination:",
                WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HOTKEY_LBL_COMBO, hInst, nullptr
            );

            g_hHotkeyInput = CreateWindowExW(
                WS_EX_CLIENTEDGE, HOTKEY_CLASSW, nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HOTKEY_INPUT, hInst, nullptr
            );

            g_hHotkeyLblTarget = CreateWindowExW(
                0, L"STATIC", L"Target (File / Application):",
                WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HOTKEY_LBL_TARGET, hInst, nullptr
            );

            g_hHotkeyTarget = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HOTKEY_TARGET, hInst, nullptr
            );

            g_hHotkeyBrowse = CreateWindowExW(
                0, L"BUTTON", L"...",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HOTKEY_BROWSE, hInst, nullptr
            );

            g_hHotkeyAdd = CreateWindowExW(
                0, L"BUTTON", L"Add Hotkey",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HOTKEY_ADD, hInst, nullptr
            );

            g_hHotkeyDelete = CreateWindowExW(
                0, L"BUTTON", L"Delete",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HOTKEY_DELETE, hInst, nullptr
            );

            g_hHotkeyList = CreateWindowExW(
                WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HOTKEY_LIST, hInst, nullptr
            );
            ListView_SetExtendedListViewStyle(g_hHotkeyList,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            // ListView columns
            LVCOLUMNW lvc = {};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
            lvc.fmt = LVCFMT_LEFT;

            lvc.cx = 180;
            lvc.pszText = const_cast<LPWSTR>(L"Shortcut");
            ListView_InsertColumn(g_hHotkeyList, 0, &lvc);

            lvc.cx = 380;
            lvc.pszText = const_cast<LPWSTR>(L"Target Path");
            ListView_InsertColumn(g_hHotkeyList, 1, &lvc);

            // ==== HOTSTRING TAB CONTROLS ====

            g_hHsLblAbbrev = CreateWindowExW(
                0, L"STATIC", L"Abbreviation (trigger text):",
                WS_CHILD,  // Hidden initially
                0, 0, 0, 0, hWnd, (HMENU)IDC_HS_LBL_ABBREV, hInst, nullptr
            );

            g_hHsAbbrev = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HS_ABBREV, hInst, nullptr
            );

            g_hHsLblExpand = CreateWindowExW(
                0, L"STATIC", L"Expansion (replacement text):",
                WS_CHILD,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HS_LBL_EXPAND, hInst, nullptr
            );

            g_hHsExpand = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HS_EXPAND, hInst, nullptr
            );

            g_hHsAdd = CreateWindowExW(
                0, L"BUTTON", L"Add Hotstring",
                WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HS_ADD, hInst, nullptr
            );

            g_hHsDelete = CreateWindowExW(
                0, L"BUTTON", L"Delete",
                WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HS_DELETE, hInst, nullptr
            );

            g_hHsList = CreateWindowExW(
                WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
                WS_CHILD | WS_TABSTOP |
                LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
                0, 0, 0, 0, hWnd, (HMENU)IDC_HS_LIST, hInst, nullptr
            );
            ListView_SetExtendedListViewStyle(g_hHsList,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            lvc.cx = 160;
            lvc.pszText = const_cast<LPWSTR>(L"Abbreviation");
            ListView_InsertColumn(g_hHsList, 0, &lvc);

            lvc.cx = 400;
            lvc.pszText = const_cast<LPWSTR>(L"Expansion");
            ListView_InsertColumn(g_hHsList, 1, &lvc);

            // ---- Apply fonts to all controls ----
            HWND allControls[] = {
                g_hHotkeyLblCombo, g_hHotkeyInput, g_hHotkeyLblTarget,
                g_hHotkeyTarget, g_hHotkeyBrowse, g_hHotkeyAdd, g_hHotkeyDelete,
                g_hHotkeyList,
                g_hHsLblAbbrev, g_hHsAbbrev, g_hHsLblExpand, g_hHsExpand,
                g_hHsAdd, g_hHsDelete, g_hHsList
            };
            for (HWND hCtrl : allControls) {
                SendMessageW(hCtrl, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
            }

            // Apply header font to labels
            HWND labels[] = {
                g_hHotkeyLblCombo, g_hHotkeyLblTarget,
                g_hHsLblAbbrev, g_hHsLblExpand
            };
            for (HWND hLbl : labels) {
                SendMessageW(hLbl, WM_SETFONT, (WPARAM)g_hFontHeader, TRUE);
            }

            // ---- Load existing data ----
            RefreshHotkeyList();
            RefreshHotstringList();

            // Show hotkey tab by default
            SwitchTab(0);

            return 0;
        }

        case WM_SIZE:
        {
            OnResize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        }

        case WM_NOTIFY:
        {
            LPNMHDR pnm = (LPNMHDR)lParam;
            if (pnm->hwndFrom == g_hTabCtrl && pnm->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(g_hTabCtrl);
                SwitchTab(sel);
            }
            return 0;
        }

        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);

            switch (wmId) {
            case IDC_HOTKEY_ADD:    OnAddHotkey();     break;
            case IDC_HOTKEY_DELETE: OnDeleteHotkey();  break;
            case IDC_HOTKEY_BROWSE: OnBrowseTarget();  break;
            case IDC_HS_ADD:       OnAddHotstring();   break;
            case IDC_HS_DELETE:    OnDeleteHotstring(); break;
            }
            return 0;
        }

        case WM_SYSCOMMAND:
        {
            UINT cmd = (wParam & 0xFFF0);
            if (cmd == SC_MINIMIZE || cmd == SC_CLOSE) {
                ShowWindow(hWnd, SW_HIDE);
                AddTrayIcon(hWnd);
                return 0;
            }
            break;
        }

        case WM_TRAY_ICON:
        {
            if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
                RemoveTrayIcon(hWnd);
                return 0;
            }
            else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                ShowTrayContextMenu(hWnd);
                return 0;
            }
            break;
        }

        case WM_SHOWWINDOW:
        {
            if (wParam) {
                RemoveTrayIcon(hWnd);
            }
            break;
        }

        case WM_GETMINMAXINFO:
        {
            // Enforce minimum window size
            LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;
            mmi->ptMinTrackSize.x = 520;
            mmi->ptMinTrackSize.y = 480;
            return 0;
        }

        case WM_DESTROY:
        {
            RemoveTrayIcon(hWnd);
            if (g_hFontUI)     { DeleteObject(g_hFontUI);     g_hFontUI = nullptr; }
            if (g_hFontHeader) { DeleteObject(g_hFontHeader); g_hFontHeader = nullptr; }
            PostQuitMessage(0);
            return 0;
        }

        } // switch

        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

} // anonymous namespace

//=============================================================================
// GUI thread entry point
//=============================================================================

DWORD WINAPI Gui::ThreadProc(LPVOID lpParam)
{
    HANDLE hEngineReady = (HANDLE)lpParam;

    // ---- Wait for the engine thread to be ready ----
    DWORD waitResult = WaitForSingleObject(hEngineReady, 10000);
    if (waitResult != WAIT_OBJECT_0) {
        MessageBoxW(nullptr,
            L"Engine thread failed to initialize within 10 seconds.\nThe application will exit.",
            L"Startup Error", MB_OK | MB_ICONERROR);
        ExitProcess(3);
    }

    g_hGuiInstance = GetModuleHandleW(nullptr);

    // ---- Initialize common controls ----
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_HOTKEY_CLASS | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    // ---- Register GUI window class ----
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = GuiWndProc;
    wc.hInstance      = g_hGuiInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName  = APP_CLASS_GUI;
    wc.hIcon         = LoadIconW(g_hGuiInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!wc.hIcon) {
        wc.hIcon     = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wc.hIconSm       = (HICON)LoadImageW(g_hGuiInstance, MAKEINTRESOURCEW(IDI_APP_ICON), 
                                         IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), 
                                         GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (!wc.hIconSm) {
        wc.hIconSm   = LoadIconW(nullptr, IDI_APPLICATION);
    }

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register GUI window class.",
                    L"Error", MB_OK | MB_ICONERROR);
        Engine::RequestShutdown();
        return 1;
    }

    // ---- Create the main window ----
    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        APP_CLASS_GUI,
        APP_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        640, 560,
        nullptr, nullptr,
        g_hGuiInstance, nullptr
    );

    if (hWnd == nullptr) {
        MessageBoxW(nullptr, L"Failed to create the main window.",
                    L"Error", MB_OK | MB_ICONERROR);
        Engine::RequestShutdown();
        return 2;
    }

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    // ---- GUI message loop ----
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        // Allow Tab/Enter navigation between controls
        if (!IsDialogMessageW(hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // ---- GUI closed — tell the engine to shut down ----
    Engine::RequestShutdown();

    return 0;
}
