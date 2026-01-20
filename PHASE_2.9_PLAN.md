# Phase 2.9 Implementation Plan: Search Features Suite

**Version:** v2.9.0 - v2.9.5  
**Goal:** Add comprehensive search/navigation features: Find, Replace, Bookmarks, Go to Line, Translation Tables  
**Current Binary Size:** 253 KB (MSVC), 308 KB (MinGW)  
**Target Binary Size:** <290 KB (MSVC)  
**Estimated Size Impact:** +35 KB (MSVC)  

---

## Overview

Phase 2.9 adds a new **Search** menu before the View menu, containing all search and navigation features. This keeps the codebase organized and provides room for future enhancements.

### Version Sequence

1. **v2.9.1:** Find Dialog (core search functionality)
2. **v2.9.2:** Replace (extends Find dialog)
3. **v2.9.3:** Bookmarks (line markers with keyboard navigation)
4. **v2.9.4:** Go to Line (simple line jump dialog)
5. **v2.9.5:** Translation Tables (bulk find/replace from TSV files)

---

## Menu Structure: New Search Menu

Insert **Search** menu before View menu (after Edit):

```
&Search
├── &Find...                    Ctrl+F
├── Find &Next                  F3
├── Find &Previous              Shift+F3
├── ──────────────
├── &Replace...                 Ctrl+H
├── ──────────────
├── &Go to Line...              Ctrl+G
├── ──────────────
├── Toggle &Bookmark            Ctrl+F2
├── Next Bookmark               F2
├── Previous Bookmark           Shift+F2
├── Clear All Bookmarks
├── ──────────────
└── Translation Tables    ▶
    ├── Apply from File...
    └── ──────────────
        [Recent translation files...]
```

### Resource ID Allocation

**Update resource.h with new ID range:**

```cpp
// Search menu (NEW - Phase 2.9)
#define ID_SEARCH_FIND                1300
#define ID_SEARCH_FIND_NEXT           1301
#define ID_SEARCH_FIND_PREVIOUS       1302
#define ID_SEARCH_REPLACE             1310
#define ID_SEARCH_REPLACE_NEXT        1311  // For Replace dialog
#define ID_SEARCH_REPLACE_ALL         1312  // For Replace dialog
#define ID_SEARCH_GOTO_LINE           1320
#define ID_SEARCH_BOOKMARK_TOGGLE     1330
#define ID_SEARCH_BOOKMARK_NEXT       1331
#define ID_SEARCH_BOOKMARK_PREVIOUS   1332
#define ID_SEARCH_BOOKMARK_CLEAR_ALL  1333
#define ID_SEARCH_TRANSLATION_TABLES  1340
#define ID_SEARCH_TRANSLATION_APPLY   1341

// Dialog IDs
#define IDD_FIND                      301
#define IDD_GOTO                      302

// Find/Replace dialog controls (3001-3049)
#define IDC_FIND_WHAT                 3001
#define IDC_REPLACE_WITH              3002
#define IDC_MATCH_CASE                3010
#define IDC_WHOLE_WORD                3011
#define IDC_USE_ESCAPES               3012
// Note: Match case, Whole word, and Use escapes options are saved to INI file
// and restored on next dialog open (user preference persistence)
#define IDC_FIND_NEXT_BTN             3020
#define IDC_FIND_PREV_BTN             3021
#define IDC_REPLACE_BTN               3022
#define IDC_REPLACE_ALL_BTN           3023
#define IDC_CLOSE_BTN                 3024

// Go to Line dialog controls (3050-3059)
#define IDC_GOTO_LINE                 3050
#define IDC_GOTO_CURRENT_LINE         3051

// String IDs for Search features (2140-2199)
#define IDS_FIND_TITLE                2140
#define IDS_FIND_WHAT                 2141
#define IDS_REPLACE_WITH              2142
#define IDS_MATCH_CASE                2143
#define IDS_WHOLE_WORD                2144
#define IDS_USE_ESCAPES               2145
#define IDS_FIND_NEXT_BTN             2146
#define IDS_FIND_PREV_BTN             2147
#define IDS_REPLACE_BTN               2148
#define IDS_REPLACE_ALL_BTN           2149
#define IDS_FIND_NOTFOUND_PREFIX      2150  // "Cannot find \"" (prefix only, use wcscpy/wcscat)
#define IDS_FIND_NOTFOUND_TITLE       2151
#define IDS_REPLACE_COMPLETE          2152  // "Replaced X occurrences"
#define IDS_REPLACE_TITLE             2153
#define IDS_GOTO_TITLE                2154
#define IDS_GOTO_LINE_LABEL           2155
#define IDS_GOTO_INVALID_LINE         2156
#define IDS_BOOKMARK_TOGGLED          2157
#define IDS_NO_BOOKMARKS              2158
#define IDS_TRANSLATION_TITLE         2159
#define IDS_TRANSLATION_ERROR         2160
```

---

## Phase 2.9.1: Find Dialog (Core)

**Estimated:** 730 lines, +14 KB, 6-8 hours  
**Commit Message:** "Add Find dialog with escape sequences and history (Phase 2.9.1)"

### Features

- Modeless dialog (stays open during searches)
- Search forward/backward with easy button switching
- Match case, whole word options
- Escape sequence support: `\n`, `\t`, `\r`, `\xNN`, `\uNNNN`
- Find history (20 entries, saved on dialog close)
- Configurable: Select found text or just move cursor
- MessageBox when not found
- Close on Escape key

### Global Variables (Add to main.cpp ~line 70)

```cpp
//============================================================================
// Search System (Phase 2.9)
//============================================================================
#define MAX_FIND_HISTORY 20
#define MAX_SEARCH_TEXT 256

// Find dialog state
HWND g_hDlgFind = NULL;                          // Find dialog handle (modeless)
WCHAR g_szFindWhat[MAX_SEARCH_TEXT] = L"";       // Current search term
BOOL g_bFindMatchCase = FALSE;                   // Case-sensitive search
BOOL g_bFindWholeWord = FALSE;                   // Whole word search
BOOL g_bFindUseEscapes = FALSE;                  // Parse escape sequences
BOOL g_bSearchDown = TRUE;                       // Search direction (TRUE=down/forward)
BOOL g_bSelectAfterFind = TRUE;                  // Select found text (configurable)

// Find history
WCHAR g_szFindHistory[MAX_FIND_HISTORY][MAX_SEARCH_TEXT];
int g_nFindHistoryCount = 0;
```

### Functions to Implement

#### 1. ParseEscapeSequences() - Convert escape codes to actual characters

**Note:** Replace existing `UnescapeTemplateString()` function (line ~708) with this enhanced version.
The new function adds `\xNN` and `\uNNNN` support, benefiting templates as well.

```cpp
//============================================================================
// ParseEscapeSequences - Convert C-style escape sequences to actual characters
// Input: pszInput - String with escape sequences (\n, \t, \xNN, \uNNNN)
// Returns: Allocated WCHAR* with parsed string (caller must free!)
// 
// Supported escapes:
//   \n  -> LF (0x0A)
//   \r  -> CR (0x0D)
//   \t  -> TAB (0x09)
//   \\  -> Backslash
//   \xNN -> Hex byte (e.g., \x41 = 'A')
//   \uNNNN -> Unicode codepoint (e.g., \u00E9 = 'é')
//
// Note: \r\n is treated as two separate escapes (\r then \n) and will
// search for CRLF literally. RichEdit uses LF internally for line breaks.
//
// This replaces UnescapeTemplateString() with enhanced functionality.
//============================================================================
LPWSTR ParseEscapeSequences(LPCWSTR pszInput)
{
    if (!pszInput) return NULL;
    
    size_t len = wcslen(pszInput);
    // Worst case: no escapes, same length + null terminator
    LPWSTR pszResult = (LPWSTR)malloc((len + 1) * sizeof(WCHAR));
    if (!pszResult) return NULL;
    
    LPWSTR pDest = pszResult;
    LPCWSTR pSrc = pszInput;
    
    while (*pSrc) {
        if (*pSrc == L'\\' && *(pSrc + 1)) {
            pSrc++;  // Skip backslash
            switch (*pSrc) {
                case L'n':  *pDest++ = L'\n'; pSrc++; break;
                case L'r':  *pDest++ = L'\r'; pSrc++; break;
                case L't':  *pDest++ = L'\t'; pSrc++; break;
                case L'\\': *pDest++ = L'\\'; pSrc++; break;
                
                case L'x': {
                    // \xNN - Hex byte
                    pSrc++;
                    WCHAR hex[3] = {0};
                    int count = 0;
                    while (count < 2 && iswxdigit(*pSrc)) {
                        hex[count++] = *pSrc++;
                    }
                    if (count > 0) {
                        *pDest++ = (WCHAR)wcstoul(hex, NULL, 16);
                    }
                    break;
                }
                
                case L'u': {
                    // \uNNNN - Unicode codepoint
                    pSrc++;
                    WCHAR hex[5] = {0};
                    int count = 0;
                    while (count < 4 && iswxdigit(*pSrc)) {
                        hex[count++] = *pSrc++;
                    }
                    if (count > 0) {
                        *pDest++ = (WCHAR)wcstoul(hex, NULL, 16);
                    }
                    break;
                }
                
                default:
                    // Unknown escape - keep backslash and character
                    *pDest++ = L'\\';
                    *pDest++ = *pSrc++;
                    break;
            }
        } else {
            *pDest++ = *pSrc++;
        }
    }
    
    *pDest = L'\0';
    return pszResult;
}
```

#### 2. FindTextInDocument() - Wrapper around EM_FINDTEXTEX

```cpp
//============================================================================
// FindTextInDocument - Search for text in RichEdit control
// Returns: Character position of match, or -1 if not found
//============================================================================
LONG FindTextInDocument(LPCWSTR pszSearchText, BOOL bMatchCase, BOOL bWholeWord, 
                       BOOL bSearchDown, LONG nStartPos)
{
    if (!pszSearchText || !pszSearchText[0]) return -1;
    
    // Get document length for search range
    GETTEXTLENGTHEX gtl = {GTL_NUMCHARS, 1200};  // UTF-16LE
    LONG nDocLen = (LONG)SendMessage(g_hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    
    // Set up search structure
    FINDTEXTEX ft;
    ft.lpstrText = pszSearchText;
    
    if (bSearchDown) {
        ft.chrg.cpMin = nStartPos;
        ft.chrg.cpMax = nDocLen;
    } else {
        ft.chrg.cpMin = nStartPos;
        ft.chrg.cpMax = 0;  // Search backwards
    }
    
    // Build search flags
    DWORD dwFlags = 0;
    if (bMatchCase) dwFlags |= FR_MATCHCASE;
    if (bWholeWord) dwFlags |= FR_WHOLEWORD;
    if (bSearchDown) dwFlags |= FR_DOWN;
    
    // Execute search (use W version for Unicode support)
    LONG nPos = (LONG)SendMessage(g_hWndEdit, EM_FINDTEXTEXW, dwFlags, (LPARAM)&ft);
    if (nPos != -1) {
        // Found - ft.chrgText contains the match range
        CHARRANGE cr;
        cr.cpMin = ft.chrgText.cpMin;
        cr.cpMax = ft.chrgText.cpMax;
        
        if (g_bSelectAfterFind) {
            // Select the found text
            SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        } else {
            // Just move cursor to start of match
            cr.cpMax = cr.cpMin;
            SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        }
        
        // Scroll into view
        SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
    }
    
    return nPos;
}
```

#### 3. DoFind() - Main find logic

```cpp
//============================================================================
// DoFind - Perform find operation with current settings
// Called by Find Next/Previous buttons and F3/Shift+F3 shortcuts
// Returns: TRUE if found, FALSE if not found
//============================================================================
BOOL DoFind(BOOL bSearchDown)
{
    // Update search direction
    g_bSearchDown = bSearchDown;
    
    if (g_szFindWhat[0] == L'\0') {
        // No search term - show Find dialog
        if (g_hDlgFind) {
            SetFocus(g_hDlgFind);
        } else {
            SendMessage(g_hWndMain, WM_COMMAND, ID_SEARCH_FIND, 0);
        }
        return FALSE;
    }
    
    // Parse escape sequences if enabled
    LPWSTR pszSearchText = NULL;
    if (g_bFindUseEscapes) {
        pszSearchText = ParseEscapeSequences(g_szFindWhat);
        if (!pszSearchText) {
            pszSearchText = _wcsdup(g_szFindWhat);  // Fallback
        }
    } else {
        pszSearchText = _wcsdup(g_szFindWhat);
    }
    
    // Get current selection to start search after/before it
    CHARRANGE cr;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    
    LONG nStartPos = bSearchDown ? cr.cpMax : cr.cpMin;
    
    // Search
    LONG nPos = FindTextInDocument(pszSearchText, g_bFindMatchCase, g_bFindWholeWord, 
                                   bSearchDown, nStartPos);
    
    free(pszSearchText);
    
    if (nPos == -1) {
        // Not found - show message
        // Note: Don't use swprintf() with user-provided Unicode text
        // Use wcscpy/wcscat pattern to avoid UTF-16 issues
        WCHAR szMsg[512], szTitle[64];
        WCHAR szPrefix[256];
        LoadStringResource(IDS_FIND_NOTFOUND_PREFIX, szPrefix, 256);  // "Cannot find \""
        LoadStringResource(IDS_FIND_NOTFOUND_TITLE, szTitle, 64);
        
        wcscpy(szMsg, szPrefix);
        wcscat(szMsg, g_szFindWhat);
        wcscat(szMsg, L"\"");
        
        MessageBox(g_hDlgFind ? g_hDlgFind : g_hWndMain, szMsg, szTitle, 
                  MB_ICONINFORMATION);
        return FALSE;
    }
    
    return TRUE;
}
```

#### 4. Find Dialog Procedure

```cpp
//============================================================================
// DlgFindProc - Find dialog procedure (modeless)
//============================================================================
INT_PTR CALLBACK DlgFindProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_INITDIALOG:
        {
            // Load history into combo box
            HWND hCombo = GetDlgItem(hDlg, IDC_FIND_WHAT);
            for (int i = 0; i < g_nFindHistoryCount; i++) {
                SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)g_szFindHistory[i]);
            }
            
            // If no current search term, use most recent from history
            if (g_szFindWhat[0] == L'\0' && g_nFindHistoryCount > 0) {
                wcscpy(g_szFindWhat, g_szFindHistory[0]);
            }
            
            // Set current search term (or most recent from history)
            SetDlgItemText(hDlg, IDC_FIND_WHAT, g_szFindWhat);
            
            // Set checkbox states (restored from saved preferences)
            CheckDlgButton(hDlg, IDC_MATCH_CASE, g_bFindMatchCase ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_WHOLE_WORD, g_bFindWholeWord ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_USE_ESCAPES, g_bFindUseEscapes ? BST_CHECKED : BST_UNCHECKED);
            
            // Focus on search box and select all text (for easy overtyping)
            SetFocus(hCombo);
            SendMessage(hCombo, CB_SETEDITSEL, 0, MAKELPARAM(0, -1));  // Select all
            
            return FALSE;  // We set focus manually
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FIND_NEXT_BTN:
                {
                    // Get search term from combo box
                    GetDlgItemText(hDlg, IDC_FIND_WHAT, g_szFindWhat, MAX_SEARCH_TEXT);
                    
                    // Get checkbox states
                    g_bFindMatchCase = (IsDlgButtonChecked(hDlg, IDC_MATCH_CASE) == BST_CHECKED);
                    g_bFindWholeWord = (IsDlgButtonChecked(hDlg, IDC_WHOLE_WORD) == BST_CHECKED);
                    g_bFindUseEscapes = (IsDlgButtonChecked(hDlg, IDC_USE_ESCAPES) == BST_CHECKED);
                    
                    // Perform search
                    DoFind(TRUE);  // Search down
                    return TRUE;
                }
                
                case IDC_FIND_PREV_BTN:
                {
                    // Get search term
                    GetDlgItemText(hDlg, IDC_FIND_WHAT, g_szFindWhat, MAX_SEARCH_TEXT);
                    
                    // Get checkbox states
                    g_bFindMatchCase = (IsDlgButtonChecked(hDlg, IDC_MATCH_CASE) == BST_CHECKED);
                    g_bFindWholeWord = (IsDlgButtonChecked(hDlg, IDC_WHOLE_WORD) == BST_CHECKED);
                    g_bFindUseEscapes = (IsDlgButtonChecked(hDlg, IDC_USE_ESCAPES) == BST_CHECKED);
                    
                    // Perform search
                    DoFind(FALSE);  // Search up
                    return TRUE;
                }
                
                case IDC_CLOSE_BTN:
                case IDCANCEL:
                    // Save history before closing
                    SaveFindHistory();
                    DestroyWindow(hDlg);
                    g_hDlgFind = NULL;
                    return TRUE;
            }
            break;
        
        case WM_CLOSE:
            SaveFindHistory();
            DestroyWindow(hDlg);
            g_hDlgFind = NULL;
            return TRUE;
    }
    
    return FALSE;
}
```

#### 5. History Management Functions

```cpp
//============================================================================
// LoadFindHistory - Load find history from INI file
//============================================================================
void LoadFindHistory()
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    // Read count
    g_nFindHistoryCount = ReadINIInt(szIniPath, L"FindHistory", L"Count", 0);
    if (g_nFindHistoryCount > MAX_FIND_HISTORY) {
        g_nFindHistoryCount = MAX_FIND_HISTORY;
    }
    
    // Read each item
    for (int i = 0; i < g_nFindHistoryCount; i++) {
        WCHAR szKey[32];
        swprintf(szKey, 32, L"Item%d", i + 1);  // OK: numeric formatting only
        ReadINIValue(szIniPath, L"FindHistory", szKey, 
                    g_szFindHistory[i], MAX_SEARCH_TEXT, L"");
    }
}

//============================================================================
// SaveFindHistory - Save find history to INI file
//============================================================================
void SaveFindHistory()
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    // Add current search term to history if not empty
    if (g_szFindWhat[0] != L'\0') {
        AddToFindHistory(g_szFindWhat);
    }
    
    // Write count
    WCHAR szCount[16];
    swprintf(szCount, 16, L"%d", g_nFindHistoryCount);
    WriteINIValue(szIniPath, L"FindHistory", L"Count", szCount);
    
    // Write each item
    for (int i = 0; i < g_nFindHistoryCount; i++) {
        WCHAR szKey[32];
        swprintf(szKey, 32, L"Item%d", i + 1);
        WriteINIValue(szIniPath, L"FindHistory", szKey, g_szFindHistory[i]);
    }
    
    // Save checkbox states to Settings section
    WriteINIValue(szIniPath, L"Settings", L"FindMatchCase", 
                 g_bFindMatchCase ? L"1" : L"0");
    WriteINIValue(szIniPath, L"Settings", L"FindWholeWord", 
                 g_bFindWholeWord ? L"1" : L"0");
    WriteINIValue(szIniPath, L"Settings", L"FindUseEscapes", 
                 g_bFindUseEscapes ? L"1" : L"0");
}

//============================================================================
// AddToFindHistory - Add item to history (most recent first)
//============================================================================
void AddToFindHistory(LPCWSTR pszText)
{
    if (!pszText || !pszText[0]) return;
    
    // Check if already in history
    for (int i = 0; i < g_nFindHistoryCount; i++) {
        if (wcscmp(g_szFindHistory[i], pszText) == 0) {
            // Already exists - move to top
            WCHAR szTemp[MAX_SEARCH_TEXT];
            wcscpy(szTemp, g_szFindHistory[i]);
            
            // Shift items down
            for (int j = i; j > 0; j--) {
                wcscpy(g_szFindHistory[j], g_szFindHistory[j - 1]);
            }
            
            // Put at top
            wcscpy(g_szFindHistory[0], szTemp);
            return;
        }
    }
    
    // Not in history - add to top
    if (g_nFindHistoryCount < MAX_FIND_HISTORY) {
        g_nFindHistoryCount++;
    }
    
    // Shift items down
    for (int i = g_nFindHistoryCount - 1; i > 0; i--) {
        wcscpy(g_szFindHistory[i], g_szFindHistory[i - 1]);
    }
    
    // Add new item at top
    wcscpy(g_szFindHistory[0], pszText);
}
```

### Dialog Resource (resource.rc)

**English (LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US):**

```rc
// Find Dialog
IDD_FIND DIALOGEX 0, 0, 280, 120
STYLE DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Find"
FONT 8, "MS Shell Dlg", 400, 0
BEGIN
    LTEXT           "Fi&nd what:", IDC_STATIC, 10, 12, 50, 8
    COMBOBOX        IDC_FIND_WHAT, 70, 10, 200, 100, CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
    
    GROUPBOX        "Options", IDC_STATIC, 10, 35, 200, 50
    AUTOCHECKBOX    "Match &case", IDC_MATCH_CASE, 20, 48, 80, 10
    AUTOCHECKBOX    "&Whole word", IDC_WHOLE_WORD, 20, 62, 80, 10
    AUTOCHECKBOX    "&Use escape sequences (\\n, \\t, \\xNN, \\uNNNN)", IDC_USE_ESCAPES, 20, 76, 180, 10
    
    DEFPUSHBUTTON   "Find Next →", IDC_FIND_NEXT_BTN, 220, 35, 50, 14
    PUSHBUTTON      "← Find Previous", IDC_FIND_PREV_BTN, 220, 52, 50, 14
    PUSHBUTTON      "Close", IDC_CLOSE_BTN, 220, 95, 50, 14
END
```

**Czech (LANGUAGE LANG_CZECH, SUBLANG_DEFAULT):**

```rc
IDD_FIND DIALOGEX 0, 0, 280, 120
STYLE DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Najít"
FONT 8, "MS Shell Dlg", 400, 0
BEGIN
    LTEXT           "&Najít:", IDC_STATIC, 10, 12, 50, 8
    COMBOBOX        IDC_FIND_WHAT, 70, 10, 200, 100, CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
    
    GROUPBOX        "Možnosti", IDC_STATIC, 10, 35, 200, 50
    AUTOCHECKBOX    "Rozlišovat &velikost písmen", IDC_MATCH_CASE, 20, 48, 100, 10
    AUTOCHECKBOX    "Celá &slova", IDC_WHOLE_WORD, 20, 62, 80, 10
    AUTOCHECKBOX    "&Escape sekvence (\\n, \\t, \\xNN, \\uNNNN)", IDC_USE_ESCAPES, 20, 76, 180, 10
    
    DEFPUSHBUTTON   "Najít další →", IDC_FIND_NEXT_BTN, 220, 35, 50, 14
    PUSHBUTTON      "← Najít předchozí", IDC_FIND_PREV_BTN, 220, 52, 50, 14
    PUSHBUTTON      "Zavřít", IDC_CLOSE_BTN, 220, 95, 50, 14
END
```

### String Resources

**English (STRINGTABLE, LANG_ENGLISH):**

```rc
IDS_FIND_TITLE              "Find"
IDS_FIND_NOTFOUND_PREFIX    "Cannot find \""
IDS_FIND_NOTFOUND_TITLE     "Find"
```

**Czech (STRINGTABLE, LANG_CZECH):**

```rc
IDS_FIND_TITLE              "Najít"
IDS_FIND_NOTFOUND_PREFIX    "Nelze najít \""
IDS_FIND_NOTFOUND_TITLE     "Najít"
```

### Menu Resources

**English (update IDR_MENU_MAIN):**

Insert new Search menu between Edit and View:

```rc
POPUP "&Search"
BEGIN
    MENUITEM "&Find...\tCtrl+F", ID_SEARCH_FIND
    MENUITEM "Find &Next\tF3", ID_SEARCH_FIND_NEXT
    MENUITEM "Find &Previous\tShift+F3", ID_SEARCH_FIND_PREVIOUS
END
```

**Czech:**

```rc
POPUP "&Hledat"
BEGIN
    MENUITEM "&Najít...\tCtrl+F", ID_SEARCH_FIND
    MENUITEM "Najít &další\tF3", ID_SEARCH_FIND_NEXT
    MENUITEM "Najít &předchozí\tShift+F3", ID_SEARCH_FIND_PREVIOUS
END
```

### Integration Points

#### 1. Update BuildAcceleratorTable() (~line 882)

```cpp
const int BUILTIN_COUNT = 18;  // Was 15, now 18 (add F3, Shift+F3, Ctrl+F)

// Add search shortcuts after line 917 (after Ctrl+Shift+T):
pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'F'; pAccel[idx++].cmd = ID_SEARCH_FIND;
pAccel[idx].fVirt = FVIRTKEY; pAccel[idx].key = VK_F3; pAccel[idx++].cmd = ID_SEARCH_FIND_NEXT;
pAccel[idx].fVirt = FSHIFT | FVIRTKEY; pAccel[idx].key = VK_F3; pAccel[idx++].cmd = ID_SEARCH_FIND_PREVIOUS;
```

#### 2. Update Reserved Shortcuts (~line 175)

```cpp
const ReservedShortcut g_ReservedShortcuts[] = {
    // ... existing shortcuts ...
    { 'F', FCONTROL | FVIRTKEY, L"Ctrl+F (Find)" },
    { VK_F3, FVIRTKEY, L"F3 (Find Next)" },
    { VK_F3, FSHIFT | FVIRTKEY, L"Shift+F3 (Find Previous)" },
    { 0, 0, NULL }  // Sentinel
};
```

#### 3. Update WM_COMMAND in WndProc (~line 1698)

```cpp
// After ID_EDIT_TIMEDATE case (line ~1738):

// Search menu
case ID_SEARCH_FIND:
    if (g_hDlgFind) {
        // Already open - bring to front
        SetFocus(g_hDlgFind);
    } else {
        // Create modeless dialog
        g_hDlgFind = CreateDialog(GetModuleHandle(NULL), 
                                  MAKEINTRESOURCE(IDD_FIND),
                                  hwnd, DlgFindProc);
        ShowWindow(g_hDlgFind, SW_SHOW);
    }
    break;

case ID_SEARCH_FIND_NEXT:
    DoFind(TRUE);  // Search down
    break;

case ID_SEARCH_FIND_PREVIOUS:
    DoFind(FALSE);  // Search up
    break;
```

#### 4. Update Message Loop in WinMain (~line 607)

```cpp
// Message loop
MSG msg;
while (GetMessage(&msg, NULL, 0, 0)) {
    // Handle modeless Find dialog
    if (g_hDlgFind && IsDialogMessage(g_hDlgFind, &msg)) {
        continue;
    }
    
    if (!TranslateAccelerator(g_hWndMain, g_hAccel, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
```

#### 5. Update LoadSettings() (~line 5958)

```cpp
// After TabSize loading (around line 6058):

// SelectAfterFind (Phase 2.9.1)
ReadINIValue(szIniPath, L"Settings", L"SelectAfterFind", szValue, 256, L"");
if (szValue[0] == L'\0') {
    WriteINIValue(szIniPath, L"Settings", L"SelectAfterFind", L"1");
    g_bSelectAfterFind = TRUE;
} else {
    g_bSelectAfterFind = ReadINIInt(szIniPath, L"Settings", L"SelectAfterFind", 1);
}

// FindMatchCase (Phase 2.9.1) - Persist checkbox state
ReadINIValue(szIniPath, L"Settings", L"FindMatchCase", szValue, 256, L"");
if (szValue[0] == L'\0') {
    WriteINIValue(szIniPath, L"Settings", L"FindMatchCase", L"0");
    g_bFindMatchCase = FALSE;
} else {
    g_bFindMatchCase = ReadINIInt(szIniPath, L"Settings", L"FindMatchCase", 0);
}

// FindWholeWord (Phase 2.9.1) - Persist checkbox state
ReadINIValue(szIniPath, L"Settings", L"FindWholeWord", szValue, 256, L"");
if (szValue[0] == L'\0') {
    WriteINIValue(szIniPath, L"Settings", L"FindWholeWord", L"0");
    g_bFindWholeWord = FALSE;
} else {
    g_bFindWholeWord = ReadINIInt(szIniPath, L"Settings", L"FindWholeWord", 0);
}

// FindUseEscapes (Phase 2.9.1) - Persist checkbox state
ReadINIValue(szIniPath, L"Settings", L"FindUseEscapes", szValue, 256, L"");
if (szValue[0] == L'\0') {
    WriteINIValue(szIniPath, L"Settings", L"FindUseEscapes", L"0");
    g_bFindUseEscapes = FALSE;
} else {
    g_bFindUseEscapes = ReadINIInt(szIniPath, L"Settings", L"FindUseEscapes", 0);
}

// Load find history
LoadFindHistory();
```

#### 6. Update WM_DESTROY to cleanup dialog

```cpp
case WM_DESTROY:
    // ... existing cleanup ...
    
    // Destroy Find dialog if open
    if (g_hDlgFind) {
        SaveFindHistory();  // Save before destroying
        DestroyWindow(g_hDlgFind);
        g_hDlgFind = NULL;
    }
    
    PostQuitMessage(0);
    return 0;
```

#### 7. Add Forward Declarations (~line 380)

```cpp
// Search functions (Phase 2.9)
LPWSTR ParseEscapeSequences(LPCWSTR pszInput);
LONG FindTextInDocument(LPCWSTR pszSearchText, BOOL bMatchCase, BOOL bWholeWord, 
                       BOOL bSearchDown, LONG nStartPos);
BOOL DoFind(BOOL bSearchDown);
INT_PTR CALLBACK DlgFindProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void LoadFindHistory();
void SaveFindHistory();
void AddToFindHistory(LPCWSTR pszText);
```

### Testing Checklist

- [ ] Ctrl+F opens Find dialog
- [ ] F3 finds next occurrence
- [ ] Shift+F3 finds previous occurrence
- [ ] Match case works correctly
- [ ] Whole word works correctly
- [ ] Escape sequences work: `\n`, `\t`, `\r`, `\xNN`, `\uNNNN`
- [ ] `\r\n` searches for literal CRLF (two separate escapes)
- [ ] Unknown escapes preserved (e.g., `\q` → `\q`)
- [ ] History saved on dialog close
- [ ] History loaded on dialog open
- [ ] Checkbox states (Match case, Whole word, Use escapes) persist across sessions
- [ ] Dialog defaults to most recent history entry when opened with no current term
- [ ] MessageBox shown when not found with search term in message
- [ ] Escape key closes dialog
- [ ] SelectAfterFind=1: text selected after find
- [ ] SelectAfterFind=0: cursor moved to match start
- [ ] Dialog can be opened/closed multiple times
- [ ] Czech localization works
- [ ] No wrapping around document when search reaches end

---

## Phase 2.9.2: Replace

**Estimated:** 300 lines, +6 KB, 3-4 hours  
**Commit Message:** "Add Replace functionality to Find dialog (Phase 2.9.2)"

### Features

- Extends existing Find dialog with Replace controls
- Replace single occurrence
- Replace All with confirmation ("Replaced X occurrences")
- Replace history (20 entries)
- All Find options apply to Replace

### Implementation Strategy

**Extend Existing Dialog:**
- Add "Replace with:" combo below "Find what:"
- Add Replace/Replace All buttons
- Show/hide Replace controls based on which menu item opened dialog (ID_SEARCH_FIND vs ID_SEARCH_REPLACE)
- Simpler than tabbed interface

### New Global Variables

```cpp
// Replace state (add to Phase 2.9 section)
WCHAR g_szReplaceWith[MAX_SEARCH_TEXT] = L"";
WCHAR g_szReplaceHistory[MAX_FIND_HISTORY][MAX_SEARCH_TEXT];
int g_nReplaceHistoryCount = 0;
BOOL g_bReplaceMode = FALSE;  // TRUE when opened via Replace menu
```

### Key Functions to Implement

- `DoReplace()` - Replace current match and find next
- `DoReplaceAll()` - Replace all occurrences with undo support
- `LoadReplaceHistory()`, `SaveReplaceHistory()`, `AddToReplaceHistory()`

### Details Deferred

Full implementation details will be created after Phase 2.9.1 is complete and tested.

---

## Phase 2.9.3: Bookmarks

**Estimated:** 300 lines, +6 KB, 4-6 hours  
**Commit Message:** "Add bookmark system with line tracking (Phase 2.9.3)"

### Features

- Set/clear bookmarks at current line (Ctrl+F2)
- Navigate to next bookmark (F2)
- Navigate to previous bookmark (Shift+F2)
- Clear all bookmarks
- Bookmarks persist through text edits (track by line number + context string)
- Up to 100 bookmarks

### Data Structure

```cpp
#define MAX_BOOKMARKS 100
#define BOOKMARK_CONTEXT_LEN 64

struct Bookmark {
    LONG nLineNumber;                         // Line number (0-based)
    WCHAR szContext[BOOKMARK_CONTEXT_LEN];   // First 64 chars of line (for sync)
    BOOL bActive;                             // TRUE if bookmark is set
};

Bookmark g_Bookmarks[MAX_BOOKMARKS];
int g_nBookmarkCount = 0;
```
* What lines to consider - visual or physical? Hard to tell from the user's point of view what lines they think they're making bookmarks for, so that I would make it respect the text position whether the user has word wrap turned on or off. Please tell what would be necessary to accomplish that.

### Key Functions

- `ToggleBookmark()` - Set/clear at current line
- `NextBookmark()` - Jump to next
- `PreviousBookmark()` - Jump to previous
- `ClearAllBookmarks()` - Remove all
- `UpdateBookmarksAfterEdit()` - Sync after text changes (called from EN_CHANGE)

### Details Deferred

Full implementation will be planned after Phase 2.9.2 is complete.

---

## Phase 2.9.4: Go to Line

**Estimated:** 150 lines, +3 KB, 2-3 hours  
**Commit Message:** "Add Go to Line dialog (Phase 2.9.4)"

### Features

- Simple modal dialog with line number edit box
- Shows current line number in label
- Validates input (must be positive integer ≤ line count)
- Ctrl+G shortcut
- Move cursor to start of line, scroll into view

### Dialog (IDD_GOTO)

```rc
IDD_GOTO DIALOGEX 0, 0, 200, 80
STYLE DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Go to Line"
FONT 8, "MS Shell Dlg", 400, 0
BEGIN
    LTEXT           "&Line number (1-%d):", IDC_GOTO_CURRENT_LINE, 10, 12, 180, 8
    EDITTEXT        IDC_GOTO_LINE, 10, 25, 180, 14, ES_AUTOHSCROLL | ES_NUMBER
    
    DEFPUSHBUTTON   "Go", IDOK, 55, 50, 40, 14
    PUSHBUTTON      "Cancel", IDCANCEL, 105, 50, 40, 14
END
```

### Implementation

```cpp
INT_PTR CALLBACK DlgGotoProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_INITDIALOG:
        {
            // Get current line number
            LONG nCurrentLine = SendMessage(g_hWndEdit, EM_LINEFROMCHAR, -1, 0);
            LONG nTotalLines = SendMessage(g_hWndEdit, EM_GETLINECOUNT, 0, 0);
            
            // Update label with range
            WCHAR szLabel[128];
            swprintf(szLabel, 128, L"&Line number (1-%ld):", nTotalLines);
* probably again no swprintf here
            SetDlgItemText(hDlg, IDC_GOTO_CURRENT_LINE, szLabel);
            
            // Set default to current line + 1 (1-based)
            SetDlgItemInt(hDlg, IDC_GOTO_LINE, nCurrentLine + 1, FALSE);
            
            // Select all text in edit box
            SendDlgItemMessage(hDlg, IDC_GOTO_LINE, EM_SETSEL, 0, -1);
            
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                // Get line number (1-based)
                BOOL bTranslated = FALSE;
                UINT nLine = GetDlgItemInt(hDlg, IDC_GOTO_LINE, &bTranslated, FALSE);
                
                if (!bTranslated || nLine < 1) {
                    // Invalid input
                    WCHAR szMsg[128], szTitle[64];
                    LoadStringResource(IDS_GOTO_INVALID_LINE, szMsg, 128);
                    LoadStringResource(IDS_GOTO_TITLE, szTitle, 64);
                    MessageBox(hDlg, szMsg, szTitle, MB_ICONEXCLAMATION);
                    return TRUE;
                }
                
                // Convert to 0-based
                nLine--;
                
                // Get total lines
                LONG nTotalLines = SendMessage(g_hWndEdit, EM_GETLINECOUNT, 0, 0);
                if (nLine >= (UINT)nTotalLines) {
                    // Line out of range
                    WCHAR szMsg[128], szTitle[64];
                    LoadStringResource(IDS_GOTO_INVALID_LINE, szMsg, 128);
                    LoadStringResource(IDS_GOTO_TITLE, szTitle, 64);
                    MessageBox(hDlg, szMsg, szTitle, MB_ICONEXCLAMATION);
                    return TRUE;
                }
                
                // Get character position of line start
                LONG nCharPos = SendMessage(g_hWndEdit, EM_LINEINDEX, nLine, 0);
                
                // Set cursor position
                CHARRANGE cr;
                cr.cpMin = nCharPos;
                cr.cpMax = nCharPos;
                SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
                SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
                
                // Focus back to edit control
                SetFocus(g_hWndEdit);
                
                EndDialog(hDlg, IDOK);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            break;
    }
    
    return FALSE;
}
```

### Details Deferred

Full implementation will be finalized after Phase 2.9.3 is complete.

---

## Phase 2.9.5: Translation Tables

**Estimated:** 300 lines, +6 KB, 4-5 hours  
**Commit Message:** "Add translation table system for bulk find/replace (Phase 2.9.5)"

### Features

- Load TSV file with find/replace pairs
- Apply to selection or entire document
- Use cases: HTML entities → Unicode, QWERTY → QWERTZ keyboard layout corrections
- Recent translation files menu (MRU-style, 5 entries)
- Progress feedback for large operations

### File Format (TSV)

```
# Lines starting with # are comments
# Format: FIND<tab>REPLACE
# Empty lines ignored
&nbsp;	 
&lt;	<
&gt;	>
&amp;	&
&quot;	"
&#39;	'
```

### Data Structure

```cpp
#define MAX_TRANSLATION_PAIRS 1000
#define MAX_TRANSLATION_MRU 5

struct TranslationPair {
    WCHAR szFind[MAX_SEARCH_TEXT];
    WCHAR szReplace[MAX_SEARCH_TEXT];
};

TranslationPair g_TranslationTable[MAX_TRANSLATION_PAIRS];
int g_nTranslationPairCount = 0;

WCHAR g_szTranslationMRU[MAX_TRANSLATION_MRU][MAX_PATH];
int g_nTranslationMRUCount = 0;
```

### Key Functions

- `LoadTranslationTable()` - Parse TSV file
- `ApplyTranslationTable()` - Bulk replace (optimized algorithm)
- `LoadTranslationMRU()`, `SaveTranslationMRU()`, `AddToTranslationMRU()`

### Menu Structure

```rc
POPUP "Translation Tables"
BEGIN
    MENUITEM "Apply from &File...", ID_SEARCH_TRANSLATION_APPLY
    MENUITEM SEPARATOR
    // Dynamic MRU entries added at runtime
END
```

### Details Deferred

Full implementation will be planned after Phase 2.9.4 is complete.

---

## Size Budget Analysis

| Phase | Feature | Est. Lines | Est. Size (MSVC) | Running Total |
|-------|---------|------------|------------------|---------------|
| Current | v2.8 | 7,557 | 253 KB | 253 KB |
| 2.9.1 | Find    | 730        | +14 KB           | 267 KB        |
| 2.9.2 | Replace | 300        | +6 KB            | 273 KB        |
| 2.9.3 | Bookmarks | 300      | +6 KB            | 279 KB        |
| 2.9.4 | Go to Line | 150     | +3 KB            | 282 KB        |
| 2.9.5 | Translation | 300    | +6 KB            | 288 KB        |
| **Total** | **All features** | **~9,337** | **+35 KB** | **~288 KB** |

**Final Target:** ~288 KB (MSVC) - **Still under 290 KB goal!** ✅

**Note:** MinGW builds will be larger (~340 KB estimated) due to static linking overhead, but MSVC remains the primary build.

---

## Success Criteria

### Functionality
- [ ] All search features work correctly
- [ ] No crashes or memory leaks
- [ ] Fast performance (no lag on large files >1 MB)
- [ ] Undo/Redo works for all replace operations
- [ ] Escape sequences parsed correctly (including edge cases)

### Quality
- [ ] Full Czech localization for all UI elements
- [ ] Screen reader accessible (menu descriptions work)
- [ ] Consistent keyboard shortcuts (no conflicts)
- [ ] Clear error messages (user-friendly, localized)
- [ ] History persistence works across sessions

### Documentation
- [ ] README.md updated with search features section
- [ ] AGENTS.md updated with Phase 2.9 implementation notes
- [ ] INI file settings documented with examples
- [ ] Escape sequence syntax documented

### Binary Size
- [ ] MSVC build stays under 290 KB
- [ ] Each phase increases size by <10 KB (monitored per commit)
- [ ] No unnecessary string duplication

---

## Design Decisions Summary

### User Confirmations (from planning session)

1. ✅ **Line ending handling:** Literal support - `\r\n` searches for CRLF, `\n` searches for LF
   - Document in README what RichEdit expects
   - No automatic conversion

2. ✅ **History save timing:** On dialog close
   - Good balance between reliability and performance
   - User has confirmed what they wanted when closing dialog

3. ✅ **Translation Tables:** Confirmed for v2.9.5
   - Potential for unexpected use cases
   - Related to Replace engine optimization

4. ✅ **Dialog position:** Always center on main window
   - Consistent with current app behavior (main window doesn't remember position)
   - Simpler implementation

5. ✅ **Search menu:** New dedicated menu between Edit and Tools
   - Room for future features (incremental search, regex, etc.)
   - Logical organization

### Technical Decisions

1. **Modeless Find Dialog:**
   - Allows searching while dialog is open
   - Standard pattern (Notepad, VS Code, etc.)
   - Requires IsDialogMessage() in message loop

2. **EM_FINDTEXTEX API:**
   - RichEdit built-in search (fast, reliable)
   - Supports case-sensitive, whole word, direction
   - No regex needed (user confirmed)

3. **History in ComboBox:**
   - Standard Windows pattern (Run dialog, Command Prompt)
   - 20 entries (balance between usefulness and clutter)
   - Most recent first

4. **Escape Sequence Syntax:**
   - C-style escapes (familiar to programmers)
   - `\n`, `\t`, `\r`, `\\`, `\xNN`, `\uNNNN`
   - Unknown escapes preserved (safe fallback)

5. **Replace All Undo:**
   - Use EM_BEGINUNDOACTION / EM_ENDUNDOACTION
   - Single undo operation for entire Replace All
   - Standard RichEdit pattern

---

## Common Patterns to Follow

### String Resources
```cpp
LoadStringResource(IDS_FIND_TITLE, szBuffer, 256);
```

### INI File Access
```cpp
ReadINIValue(szIniPath, L"Section", L"Key", szValue, 256, L"Default");
WriteINIValue(szIniPath, L"Section", L"Key", L"Value");
```

### RichEdit Selection
```cpp
CHARRANGE cr;
SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
```

### Modeless Dialog
```cpp
// Create once
g_hDlgFind = CreateDialog(hInst, MAKEINTRESOURCE(IDD_FIND), hWnd, DlgProc);
ShowWindow(g_hDlgFind, SW_SHOW);

// Message loop
if (g_hDlgFind && IsDialogMessage(g_hDlgFind, &msg)) continue;

// Cleanup
DestroyWindow(g_hDlgFind);
g_hDlgFind = NULL;
```

### Memory Management
```cpp
// Allocate
LPWSTR pszResult = (LPWSTR)malloc(size * sizeof(WCHAR));
if (!pszResult) return NULL;

// Use
// ...

// Free
free(pszResult);
```

---

## Critical Implementation Notes

### Avoid These Mistakes

1. **Use swprintf() carefully - know when it's safe**
   - ✅ **OK for numeric/simple formatting:** `swprintf(buf, 32, L"Item%d", num)`
   - ❌ **NOT OK for user Unicode text:** `swprintf(buf, 512, L"Find \"%s\"", userText)`
   - **Reason:** MinGW's swprintf has UTF-16 issues with user-provided text
   - **Safe alternative:** Use `wcscpy`/`wcscat` for user text concatenation

2. **Always free allocated memory**
   - `ParseEscapeSequences()` returns malloc'd memory
   - Caller must call `free()`

3. **Check for NULL before dereferencing**
   - All pointer parameters
   - All malloc() returns

4. **Update accelerator table count**
   - `BUILTIN_COUNT` must match actual count
   - Test all shortcuts after changes

5. **Test with Czech locale**
   - Ensure diacritics work in all dialogs
   - UTF-8 BOM pragma must be present in RC file

6. **Save dialog state properly**
   - History saved on close, not on every search
   - Handle WM_CLOSE and IDCANCEL consistently

7. **Handle edge cases in ParseEscapeSequences**
   - Incomplete escape at end of string (`\u12`)
   - Multiple backslashes (`\\\\` → `\\`)
   - Invalid hex digits (`\xGG` → literal `\xGG`)

### RichEdit API Notes

**EM_FINDTEXTEX:**
- Returns -1 if not found
- `ft.chrgText` contains match range on success
- Flags: `FR_MATCHCASE`, `FR_WHOLEWORD`, `FR_DOWN`
- Backward search: set `chrg.cpMin > chrg.cpMax`

**Line Numbers:**
- `EM_LINEFROMCHAR` with -1 = current line
- `EM_LINEINDEX` converts line → char position
- `EM_GETLINECOUNT` = total lines
- **All are 0-based!** (User sees 1-based in UI)

**Undo/Redo:**
- `EM_BEGINUNDOACTION` / `EM_ENDUNDOACTION` for grouping
- Must be balanced (every BEGIN needs END)
- Works with `EM_REPLACESEL`

---

## Testing Strategy

### Phase 2.9.1 Testing

**Basic Functionality:**
1. Open dialog (Ctrl+F)
2. Enter search term
3. Click "Find Next →" (should find and select/move cursor)
4. Click "Find Previous ←" (should search backward)
5. Press F3 (should find next)
6. Press Shift+F3 (should find previous)
7. Press Escape (should close dialog)

**Options:**
1. Match case ON: "Hello" ≠ "hello"
2. Match case OFF: "Hello" = "hello"
3. Whole word ON: "cat" doesn't match "concatenate"
4. Whole word OFF: "cat" matches "concatenate"

**Escape Sequences:**
1. `\n` finds line breaks (if enabled)
2. `\t` finds tabs
3. `\r\n` finds CRLF (two escapes)
4. `\x41` finds 'A'
5. `\u00E9` finds 'é'
6. `\\` finds backslash
7. `\q` finds literal `\q` (unknown escape)
8. Disabled: `\n` finds literal backslash+n

**History:**
1. Search for "foo"
2. Close dialog
3. Reopen dialog
4. "foo" should appear in dropdown
5. Search for "bar"
6. "bar" should be added to history
7. Restart app
8. History should persist

**Edge Cases:**
1. Search with no document text
2. Search for text not in document (MessageBox shown)
3. Search forward at end of document (not found)
4. Search backward at start of document (not found)
5. Very long search term (>256 chars - should truncate)
6. Unicode search term (emoji, Chinese, etc.)

**Localization:**
1. Switch to Czech locale
2. Dialog title should be "Najít"
3. Button text in Czech
4. MessageBox in Czech

---

## Open Questions / Future Enhancements

### For This Phase

- [ ] Should search wrap around (beginning→end, end→beginning)?
  - **Current:** No wrapping (shows "not found")
  - **Future:** Could add "Wrap around" checkbox

- [ ] Should F3 work without opening dialog first?
  - **Current:** Opens Find dialog if no search term
  - **Alternative:** Use last search term from history

### For Future Phases

- [ ] Incremental search (search-as-you-type)?
- [ ] Multi-line search support?
- [ ] Preserve case in replace (e.g., "foo"→"bar", "Foo"→"Bar")?
- [ ] Regular expressions (user said no, but might reconsider)?
- [ ] Search in selection only?
- [ ] Visual bookmark indicators (gutter icons)?

---

## Next Steps

1. **✅ Review this plan** - User approval
2. **→ Implement Phase 2.9.1** - Find dialog (this is next!)
   - Add global variables
   - Add resource IDs
   - Create dialog templates
   - Implement functions
   - Integrate into main.cpp
   - Test thoroughly
3. **→ Commit Phase 2.9.1** - Clean commit message
4. **→ Test binary size** - Verify <270 KB target
5. **→ Move to Phase 2.9.2** - Replace functionality

---

## Implementation Checklist for Phase 2.9.1

### Code Changes

- [ ] Add global variables to main.cpp (~line 70)
- [ ] Add resource IDs to resource.h
- [ ] Add forward declarations to main.cpp (~line 380)
- [ ] Add Search menu to resource.rc (English + Czech)
- [ ] Add IDD_FIND dialog to resource.rc (English + Czech)
- [ ] Add string resources to resource.rc (English + Czech)
- [ ] Implement `ParseEscapeSequences()`
- [ ] Implement `FindTextInDocument()`
- [ ] Implement `DoFind()`
- [ ] Implement `DlgFindProc()`
- [ ] Implement `LoadFindHistory()`
- [ ] Implement `SaveFindHistory()`
- [ ] Implement `AddToFindHistory()`
- [ ] Update `BuildAcceleratorTable()` (add 3 shortcuts)
- [ ] Update `g_ReservedShortcuts[]` (add 3 entries)
- [ ] Update `WndProc` WM_COMMAND (add 3 cases)
- [ ] Update `WinMain` message loop (add IsDialogMessage)
- [ ] Update `LoadSettings()` (add SelectAfterFind + LoadFindHistory)
- [ ] Update `WM_DESTROY` (add dialog cleanup)

### Testing

- [ ] Build with MSVC (check size)
- [ ] Build with MinGW (verify it still works)
- [ ] Test all functionality (see Testing Strategy above)
- [ ] Test Czech localization
- [ ] Test on Windows 7, 10, 11 (if available)
- [ ] Memory leak check (close/reopen dialog multiple times)

### Documentation

- [ ] Update README.md (add Find feature section)
- [ ] Update AGENTS.md (add Phase 2.9.1 notes)
- [ ] Update version number in resource.rc (2.9.1)
- [ ] Update About dialog version

### Commit

- [ ] Stage all changes
- [ ] Write clear commit message
- [ ] Push to repository

---

**Plan Version:** 1.0  
**Created:** 2026-01-19  
**Status:** ✅ Ready for Implementation  
**Approved By:** User  
**Next Action:** Begin Phase 2.9.1 implementation
