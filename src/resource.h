//=============================================================================
// resource.h — Control IDs, message constants, and resource identifiers
// Hotkey & Hotstring Manager
//=============================================================================
#pragma once

//-----------------------------------------------------------------------------
// Application-level constants
//-----------------------------------------------------------------------------
#define APP_NAME            L"Hotkey Manager"
#define APP_CLASS_ENGINE    L"HotkeyMgrEngine"
#define APP_CLASS_GUI       L"HotkeyMgrGUI"
#define SCRIPT_FILENAME     L"hotkeys.dat"

//-----------------------------------------------------------------------------
// Custom window messages (WM_APP range: 0x8000+)
//-----------------------------------------------------------------------------
#define WM_ENGINE_RELOAD    (WM_APP + 1)   // GUI → Engine: re-register hotkeys + hotstrings
#define WM_ENGINE_SHUTDOWN  (WM_APP + 2)   // GUI → Engine: clean shutdown

//-----------------------------------------------------------------------------
// Main GUI window
//-----------------------------------------------------------------------------
#define IDC_TAB_CONTROL         1001

//-----------------------------------------------------------------------------
// Tab 1: Hotkeys
//-----------------------------------------------------------------------------
#define IDC_HOTKEY_INPUT        2001   // MSCTLS_HOTKEY32 control
#define IDC_HOTKEY_TARGET       2002   // EDIT — target file/app path
#define IDC_HOTKEY_BROWSE       2003   // BUTTON — browse for file
#define IDC_HOTKEY_ADD          2004   // BUTTON — add hotkey
#define IDC_HOTKEY_DELETE       2005   // BUTTON — delete selected hotkey
#define IDC_HOTKEY_LIST         2006   // WC_LISTVIEW — active hotkeys

// Static labels for hotkey tab
#define IDC_HOTKEY_LBL_COMBO    2010
#define IDC_HOTKEY_LBL_TARGET   2011

//-----------------------------------------------------------------------------
// Tab 2: Hotstrings
//-----------------------------------------------------------------------------
#define IDC_HS_ABBREV           3001   // EDIT — abbreviation trigger
#define IDC_HS_EXPAND           3002   // EDIT (multiline) — expansion text
#define IDC_HS_ADD              3003   // BUTTON — add hotstring
#define IDC_HS_DELETE           3004   // BUTTON — delete selected hotstring
#define IDC_HS_LIST             3005   // WC_LISTVIEW — active hotstrings

// Static labels for hotstring tab
#define IDC_HS_LBL_ABBREV       3010
#define IDC_HS_LBL_EXPAND       3011

//-----------------------------------------------------------------------------
// Resource IDs
//-----------------------------------------------------------------------------
#define IDI_APP_ICON            101
#define IDR_MANIFEST            102

//-----------------------------------------------------------------------------
// System Tray
//-----------------------------------------------------------------------------
#define WM_TRAY_ICON            (WM_APP + 3)
#define IDC_TRAY_OPEN           4001
#define IDC_TRAY_EXIT           4002

