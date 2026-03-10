//============================================================================
// RichEditor - A lightweight text editor using RichEdit control
// Phase 1: Basic text editing with UTF-8 support
//============================================================================

#include <windows.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>    // Path functions (PathIsRelative, PathCombine, etc.)
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include "resource.h"

//============================================================================
// Windows API Constants (Phase 2.10 - Define if missing from older SDKs)
//============================================================================
#ifndef DATE_MONTHDAY
#define DATE_MONTHDAY 0x00000080
#endif

//============================================================================
// Global Variables
//============================================================================
// Extended path length for UNC and long paths (Windows maximum is 32767)
#define EXTENDED_PATH_MAX 32767

// Custom messages for REPL (Phase 2.5)
#define WM_REPL_OUTPUT  (WM_USER + 100)  // wParam: unused, lParam: LPWSTR (output text to insert)
#define WM_REPL_EXITED  (WM_USER + 101)  // wParam: unused, lParam: unused (filter process exited)

// Deferred file-load message: allows the menu to fully close before the blocking
// LoadTextFile call starts, preventing accessibility tree confusion on slow RichEdit.
// lParam: malloc'd LPWSTR file path; handler must call LoadTextFile then free it.
#define WM_APP_LOAD_FILE  (WM_APP + 1)

HWND g_hWndMain = NULL;           // Main window handle
HWND g_hWndEdit = NULL;           // RichEdit control handle (to be added)
HWND g_hWndStatus = NULL;         // Status bar handle (to be added)
WCHAR g_szFileName[EXTENDED_PATH_MAX];     // Current file path
WCHAR g_szFileTitle[MAX_PATH];    // Current file name only
BOOL g_bModified = FALSE;         // Document modified flag
BOOL g_bSettingText = FALSE;      // Flag to prevent EN_CHANGE during SetWindowText
BOOL g_bAutoURLEnabled = FALSE;   // TRUE if AURL_ENABLEURL was enabled at startup (from DetectURLs INI setting)
BOOL g_bWordWrap = TRUE;          // Word wrap enabled by default
BOOL g_bReadOnly = FALSE;         // Read-only mode (can be set via /readonly or File menu)
int  g_nZoomPercent = 100;        // Current zoom level in percent (100 = default)
BOOL g_bSaveInProgress = FALSE;   // Prevent concurrent saves/autosave reentrancy

//============================================================================
// Bookmarks (Phase 2.9.3)
//============================================================================
#define MAX_BOOKMARKS 100
#define BOOKMARK_CONTEXT_LEN 64

struct Bookmark {
    LONG charPos;                          // Char position (0-based)
    LONG lineIndex;                        // Line index (wrap-aware)
    WCHAR context[BOOKMARK_CONTEXT_LEN];   // Line context snippet
    BOOL active;
};

Bookmark g_Bookmarks[MAX_BOOKMARKS];
int g_nBookmarkCount = 0;
BOOL g_bBookmarksDirty = FALSE;
int g_nLastTextLen = 0;
// Line-starts index for O(log N) physical line queries (avoids slow RichEdit APIs on large files)
static std::vector<LONG> g_lineStarts;   // g_lineStarts[i] = char offset of line (i+1)
static bool g_bLineIndexDirty = true;    // rebuild needed; set on any content change
WCHAR g_szBookmarkSectionKey[64] = L"";

// INI cache (single in-memory copy)
struct IniCache {
    std::wstring data;
    BOOL loaded;
    BOOL dirty;
};

IniCache g_IniCache = {L"", FALSE, FALSE};

// RichEdit library management (Phase 2.8)
HMODULE g_hRichEditLib = NULL;                      // RichEdit DLL handle
float g_fRichEditVersion = 0.0f;                    // Detected version (e.g., 7.5, 8.0)
ITextDocument* g_pTextDoc = NULL;                   // TOM interface for O(1) physical line queries
// IID_ITextDocument GUID — not in any MinGW static import lib; define it here.
// {8CC497C0-A1DF-11CE-8098-00AA0047BE5D}
static const GUID IID_ITextDocument_ =
    {0x8CC497C0,0xA1DF,0x11CE,{0x80,0x98,0x00,0xAA,0x00,0x47,0xBE,0x5D}};
WCHAR g_szRichEditLibPath[MAX_PATH] = L"";          // Full path to loaded DLL
WCHAR g_szRichEditLibPathINI[MAX_PATH] = L"";       // User preference from INI
WCHAR g_szRichEditClassName[64] = L"RICHEDIT50W";  // Window class to use
WCHAR g_szRichEditClassNameINI[64] = L"";           // User override from INI (Phase 2.8.5)

// Autosave settings
BOOL g_bAutosaveEnabled = TRUE;              // Enable/disable autosave
UINT g_nAutosaveIntervalMinutes = 1;         // Autosave interval in minutes (0 = disabled)
BOOL g_bAutosaveOnFocusLoss = TRUE;          // Autosave when window loses focus
const UINT_PTR IDT_AUTOSAVE = 1;             // Timer ID for autosave
const UINT_PTR IDT_FILTER_STATUSBAR = 2;     // Timer ID for filter status bar display
const UINT_PTR IDT_AUTOSAVE_FLASH = 3;       // Timer ID for "[Autosaved]" status bar flash
const UINT_PTR IDT_FOCUS_RESTORE = 4;        // Timer ID for deferred focus restore after MRU load

// Accessibility settings
BOOL g_bShowMenuDescriptions = TRUE;         // Show filter descriptions in menus (for accessibility)

// Editor behavior settings
BOOL g_bSelectAfterPaste = FALSE;            // Select pasted text after paste operation (default: off)

// Resume file feature (Phase 2.6)
WCHAR g_szResumeFilePath[EXTENDED_PATH_MAX] = L"";    // Current resume temp file path
WCHAR g_szOriginalFilePath[EXTENDED_PATH_MAX] = L""; // Original file path (for resumed files)
BOOL g_bIsResumedFile = FALSE;                        // TRUE if current file was opened from resume
BOOL g_bAutoSaveUntitledOnClose = FALSE;              // Auto-save untitled files on close (no prompt)

// Tab settings
UINT g_nTabSize = 8;                          // Tab size in spaces (default 8)

// URL context menu
WCHAR g_szContextMenuURL[2048] = L"";         // URL from context menu (for WM_COMMAND handler)
CHARRANGE g_lastURLRange = {-1, -1};           // Last URL range from EN_LINK (for performance)

// RichEdit subclassing
WNDPROC g_pfnOriginalEditProc = NULL;         // Original RichEdit window procedure

//============================================================================
// Undo/Redo Type Tracking
//============================================================================
// We only need to track filter operations manually.
// All standard operations (typing, delete, cut, paste, drag-drop) are
// reported by RichEdit via EM_GETUNDONAME / EM_GETREDONAME messages.
BOOL g_bLastOperationWasFilter = FALSE;  // TRUE if last operation was a filter
BOOL g_bLastOperationWasReplace = FALSE; // TRUE if last operation was Replace All (Phase 2.9.2)

//============================================================================
// Filter System (Phase 2+)
//============================================================================
#define MAX_FILTERS 100
#define MAX_FILTER_NAME 64
#define MAX_FILTER_COMMAND 512
#define MAX_FILTER_DESC 256
#define MAX_FILTER_CATEGORY 32

#define MAX_MRU 10                 // Maximum number of MRU items
// ID_FILE_MRU_BASE is now defined in resource.h (6000-6009)
#define ID_CONTEXT_FILTER_BASE 9000  // Base ID for context menu filter items (9000-9099)

//============================================================================
// Template System
//============================================================================
#define MAX_TEMPLATES 100
#define MAX_TEMPLATE_NAME 64
#define MAX_TEMPLATE_VALUE 4096     // Template text with variables (increased for complex templates)
#define MAX_TEMPLATE_DESC 256
#define MAX_TEMPLATE_CATEGORY 32
#define MAX_TEMPLATE_FILEEXT 16

#define ID_TOOLS_TEMPLATE_BASE 7000      // Base ID for template menu items (7000-7099)
#define ID_FILE_NEW_TEMPLATE_BASE 8000   // Base ID for File→New submenu (8000-8031)

#define ID_TOOLS_TEMPLATE_BASE 7000      // Base ID for template menu items (7000-7099)
#define ID_FILE_NEW_TEMPLATE_BASE 8000   // Base ID for File→New submenu (8000-8031)

//============================================================================
// Keyboard Shortcut Support for Templates
//============================================================================

struct KeyMapping {
    LPCWSTR szName;
    WORD wVirtualKey;
};

// Map key names to VK codes for shortcut parsing
const KeyMapping g_KeyMap[] = {
    // Function keys
    { L"F1", VK_F1 }, { L"F2", VK_F2 }, { L"F3", VK_F3 }, { L"F4", VK_F4 },
    { L"F5", VK_F5 }, { L"F6", VK_F6 }, { L"F7", VK_F7 }, { L"F8", VK_F8 },
    { L"F9", VK_F9 }, { L"F10", VK_F10 }, { L"F11", VK_F11 }, { L"F12", VK_F12 },
    
    // Number keys
    { L"0", '0' }, { L"1", '1' }, { L"2", '2' }, { L"3", '3' }, { L"4", '4' },
    { L"5", '5' }, { L"6", '6' }, { L"7", '7' }, { L"8", '8' }, { L"9", '9' },
    
    // Letter keys
    { L"A", 'A' }, { L"B", 'B' }, { L"C", 'C' }, { L"D", 'D' }, { L"E", 'E' },
    { L"F", 'F' }, { L"G", 'G' }, { L"H", 'H' }, { L"I", 'I' }, { L"J", 'J' },
    { L"K", 'K' }, { L"L", 'L' }, { L"M", 'M' }, { L"N", 'N' }, { L"O", 'O' },
    { L"P", 'P' }, { L"Q", 'Q' }, { L"R", 'R' }, { L"S", 'S' }, { L"T", 'T' },
    { L"U", 'U' }, { L"V", 'V' }, { L"W", 'W' }, { L"X", 'X' }, { L"Y", 'Y' },
    { L"Z", 'Z' },
    
    // Special keys
    { L"Enter", VK_RETURN },
    { L"Return", VK_RETURN },
    { L"Space", VK_SPACE },
    { L"Tab", VK_TAB },
    { L"Backspace", VK_BACK },
    { L"Delete", VK_DELETE },
    { L"Insert", VK_INSERT },
    { L"Home", VK_HOME },
    { L"End", VK_END },
    { L"PageUp", VK_PRIOR },
    { L"PageDown", VK_NEXT },
    { L"Up", VK_UP },
    { L"Down", VK_DOWN },
    { L"Left", VK_LEFT },
    { L"Right", VK_RIGHT },
    { L"Escape", VK_ESCAPE },
    
    // Punctuation
    { L"Plus", VK_OEM_PLUS },
    { L"Minus", VK_OEM_MINUS },
    { L"Comma", VK_OEM_COMMA },
    { L"Period", VK_OEM_PERIOD },
    
    { NULL, 0 }  // Sentinel
};

const int g_nKeyMapCount = (sizeof(g_KeyMap) / sizeof(KeyMapping)) - 1;

// Built-in shortcuts that cannot be overridden by templates
struct ReservedShortcut {
    WORD wVirtualKey;
    BYTE fModifiers;
    LPCWSTR szDescription;
};

const ReservedShortcut g_ReservedShortcuts[] = {
    { 'N', FCONTROL | FVIRTKEY, L"Ctrl+N (New File)" },
    { 'O', FCONTROL | FVIRTKEY, L"Ctrl+O (Open)" },
    { 'S', FCONTROL | FVIRTKEY, L"Ctrl+S (Save)" },
    { 'Z', FCONTROL | FVIRTKEY, L"Ctrl+Z (Undo)" },
    { 'Y', FCONTROL | FVIRTKEY, L"Ctrl+Y (Redo)" },
    { 'X', FCONTROL | FVIRTKEY, L"Ctrl+X (Cut)" },
    { 'C', FCONTROL | FVIRTKEY, L"Ctrl+C (Copy)" },
    { 'V', FCONTROL | FVIRTKEY, L"Ctrl+V (Paste)" },
    { 'A', FCONTROL | FVIRTKEY, L"Ctrl+A (Select All)" },
    { 'W', FCONTROL | FVIRTKEY, L"Ctrl+W (Word Wrap)" },
    { VK_F5, FVIRTKEY, L"F5 (Time/Date)" },
    { VK_RETURN, FCONTROL | FVIRTKEY, L"Ctrl+Enter (Execute Filter)" },
    { 'I', FCONTROL | FSHIFT | FVIRTKEY, L"Ctrl+Shift+I (Start Interactive)" },
    { 'Q', FCONTROL | FSHIFT | FVIRTKEY, L"Ctrl+Shift+Q (Exit Interactive)" },
    { 'T', FCONTROL | FSHIFT | FVIRTKEY, L"Ctrl+Shift+T (Insert Template)" },
    { 'F', FCONTROL | FVIRTKEY, L"Ctrl+F (Find)" },
    { 'G', FCONTROL | FVIRTKEY, L"Ctrl+G (Go to Line)" },
    { VK_F2, FVIRTKEY, L"F2 (Next Bookmark)" },
    { VK_F2, FSHIFT | FVIRTKEY, L"Shift+F2 (Previous Bookmark)" },
    { VK_F2, FCONTROL | FVIRTKEY, L"Ctrl+F2 (Toggle Bookmark)" },
    { VK_F3, FVIRTKEY, L"F3 (Find Next)" },
    { VK_F3, FSHIFT | FVIRTKEY, L"Shift+F3 (Find Previous)" },
    { 0, 0, NULL }  // Sentinel
};

enum FilterAction {
    FILTER_ACTION_INSERT = 0,
    FILTER_ACTION_DISPLAY = 1,
    FILTER_ACTION_CLIPBOARD = 2,
    FILTER_ACTION_NONE = 3,
    FILTER_ACTION_REPL = 4        // Interactive REPL filter (Phase 2.5)
};

enum FilterInsertMode {
    FILTER_INSERT_REPLACE = 0,
    FILTER_INSERT_BELOW = 1,
    FILTER_INSERT_APPEND = 2
};

enum FilterDisplayMode {
    FILTER_DISPLAY_STATUSBAR = 0,
    FILTER_DISPLAY_MESSAGEBOX = 1
};

enum FilterClipboardMode {
    FILTER_CLIPBOARD_COPY = 0,
    FILTER_CLIPBOARD_APPEND = 1
};

enum REPLEOLMode {
    REPL_EOL_AUTO = 0,      // Auto-detect from first output
    REPL_EOL_CRLF = 1,      // Windows (\r\n)
    REPL_EOL_LF = 2,        // Unix (\n)
    REPL_EOL_CR = 3         // Old Mac (\r)
};

struct FilterInfo {
    WCHAR szName[MAX_FILTER_NAME];
    WCHAR szCommand[MAX_FILTER_COMMAND];
    WCHAR szDescription[MAX_FILTER_DESC];
    WCHAR szCategory[MAX_FILTER_CATEGORY];
    
    // Localized display strings (for UI only, not for identification)
    WCHAR szLocalizedName[MAX_FILTER_NAME];
    WCHAR szLocalizedDescription[MAX_FILTER_DESC];
    
    FilterAction action;
    FilterInsertMode insertMode;
    FilterDisplayMode displayMode;
    FilterClipboardMode clipboardMode;
    
    BOOL bContextMenu;
    int nContextMenuOrder;
    
    // REPL filter settings (Phase 2.5)
    WCHAR szPromptEnd[16];          // Prompt ending characters (e.g., "> ", "$ ")
    REPLEOLMode replEOLMode;        // EOL mode for REPL input/output
    BOOL bExitNotification;         // Show notification when REPL exits
};

FilterInfo g_Filters[MAX_FILTERS];
int g_nFilterCount = 0;
int g_nCurrentFilter = -1;  // -1 = no filter selected, 0-99 = filter index (classic filters for Execute)
int g_nSelectedREPLFilter = -1;  // Index of selected REPL filter (for Start Interactive Mode)

//============================================================================
// Template System
//============================================================================

struct TemplateInfo {
    WCHAR szName[MAX_TEMPLATE_NAME];
    WCHAR szDescription[MAX_TEMPLATE_DESC];
    WCHAR szCategory[MAX_TEMPLATE_CATEGORY];
    WCHAR szFileExtension[MAX_TEMPLATE_FILEEXT];  // Empty = always available
    WCHAR szTemplate[MAX_TEMPLATE_VALUE];          // Raw template with variables
    
    // Localized display strings (for UI only)
    WCHAR szLocalizedName[MAX_TEMPLATE_NAME];
    WCHAR szLocalizedDescription[MAX_TEMPLATE_DESC];
    
    // Keyboard shortcuts
    WORD wVirtualKey;      // 0 = no shortcut, VK_F1-VK_F12, '0'-'9', 'A'-'Z', etc.
    BYTE fModifiers;       // FCONTROL, FSHIFT, FALT (bitwise OR)
};

TemplateInfo g_Templates[MAX_TEMPLATES];
int g_nTemplateCount = 0;

// Current file extension for template filtering
WCHAR g_szCurrentFileExtension[MAX_TEMPLATE_FILEEXT] = L"txt";

// File type tracking for File→New submenu
struct FileTypeInfo {
    WCHAR szCategory[MAX_TEMPLATE_CATEGORY];
    WCHAR szExtension[MAX_TEMPLATE_FILEEXT];
};

FileTypeInfo g_FileTypes[32];  // Max 32 file types in New submenu
int g_nFileTypeCount = 0;

// Accelerator table management
HACCEL g_hAccel = NULL;  // Dynamic accelerator table (includes built-in + template shortcuts)

// REPL mode state (Phase 2.5)
BOOL g_bREPLMode = FALSE;              // TRUE when in Interactive Mode
HANDLE g_hREPLProcess = NULL;          // REPL process handle
HANDLE g_hREPLStdin = NULL;            // Pipe to filter stdin
HANDLE g_hREPLStdout = NULL;           // Pipe from filter stdout  
HANDLE g_hREPLStderr = NULL;           // Pipe from filter stderr
int g_nCurrentREPLFilter = -1;         // Index of active REPL filter (when g_bREPLMode is TRUE)
REPLEOLMode g_REPLEOLMode = REPL_EOL_AUTO;  // Detected EOL mode
WCHAR g_szREPLPromptEnd[16] = L"";     // Prompt ending characters
HANDLE g_hREPLStdoutThread = NULL;     // Background thread for reading stdout
DWORD g_dwREPLStdoutThreadId = 0;      // Stdout thread ID
HANDLE g_hREPLStderrThread = NULL;     // Background thread for reading stderr
DWORD g_dwREPLStderrThreadId = 0;      // Stderr thread ID
BOOL g_bREPLIntentionalExit = FALSE;   // TRUE when user intentionally exits REPL (suppress notification)

// Status bar filter display
WCHAR g_szFilterStatusBarText[512] = L"";
BOOL g_bFilterStatusBarActive = FALSE;
WCHAR g_szAutosaveFlashPrevStatus[512] = L"";  // Status text saved before "[Autosaved]" flash

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

// Replace System (Phase 2.9.2)
WCHAR g_szReplaceWith[MAX_SEARCH_TEXT] = L"";    // Current replacement text
WCHAR g_szReplaceHistory[MAX_FIND_HISTORY][MAX_SEARCH_TEXT]; // Replace history
int g_nReplaceHistoryCount = 0;                  // Replace history count
BOOL g_bReplaceMode = FALSE;                     // Dialog mode (FALSE=Find, TRUE=Replace)

//============================================================================
// Date/Time Configuration (Phase 2.10, ToDo #3)
//============================================================================
WCHAR g_szDateTimeTemplate[256] = L"%date% %time%";            // F5/menu template
WCHAR g_szDateFormat[128] = L"%shortdate%";                    // %date% format
WCHAR g_szTimeFormat[128] = L"HH:mm";                          // %time% format

// MRU list
WCHAR g_MRU[MAX_MRU][EXTENDED_PATH_MAX];
int g_nMRUCount = 0;
BOOL g_bNoMRU = FALSE;              // TRUE when /nomru command-line option is specified

// SaveTextFile failure tracking (ToDo #2)
enum SaveTextFailure {
    SAVE_TEXT_FAILURE_NONE = 0,
    SAVE_TEXT_FAILURE_OUT_OF_MEMORY,
    SAVE_TEXT_FAILURE_CONVERT,
    SAVE_TEXT_FAILURE_CREATE,
    SAVE_TEXT_FAILURE_WRITE
};

//============================================================================
// Function Declarations
//============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /* lParam */);
HWND CreateRichEditControl(HWND hwndParent);
HWND CreateStatusBar(HWND hwndParent);
void UpdateStatusBar();
void UpdateTitle(HWND hwnd = NULL);
void UpdateMenuUndoRedo(HMENU hMenu);
int CalculateTabAwareColumn(LPCWSTR pszLineText, int charPosition);
BOOL GetURLAtCursor(HWND hWndEdit, LPWSTR pszURL, int cchMax, CHARRANGE* pRange);
void OpenURL(HWND hwnd, LPCWSTR pszURL);
INT_PTR CALLBACK DlgGotoProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void CopyURLToClipboard(HWND hwnd, LPCWSTR pszURL);
void LoadStringResource(UINT uID, LPWSTR lpBuffer, int cchBufferMax);
void LoadBookmarksForCurrentFile();
void SaveBookmarksForCurrentFile();
void ClearBookmarks();
void ToggleBookmark();
void NextBookmark(BOOL bForward);
void ClearAllBookmarks();
void UpdateBookmarksAfterEdit(LONG nEditPos, int nDelta);
static void RefreshBookmarkLineIndices();
LPWSTR UTF8ToUTF16(LPCSTR pszUTF8);
LPSTR UTF16ToUTF8(LPCWSTR pszUTF16);
BOOL LoadTextFile(LPCWSTR pszFileName, BOOL bClearResumeState = TRUE);
BOOL SaveTextFile(LPCWSTR pszFileName, BOOL bClearResumeState = TRUE);
BOOL SaveTextFileInternal(LPCWSTR pszFileName, BOOL bClearResumeState, DWORD* pLastError, SaveTextFailure* pFailure, BOOL bUpdateState, BOOL bShowErrors);
BOOL SaveTextFileSilently(LPCWSTR pszFileName, BOOL bClearResumeState, DWORD* pLastError, SaveTextFailure* pFailure);
void GetDocumentsPath(LPWSTR pszPath, DWORD cchPath);
void ShowError(UINT uMessageID, LPCWSTR pszEnglishMessage, DWORD dwError);
void ShowSaveTextFailure(SaveTextFailure failure, DWORD dwError);
static void RestoreForegroundAfterElevation();
void FileNew();
void FileNewFromTemplate(int nTemplateIndex);
void BuildFileDialogFilter(LPWSTR pszFilter, DWORD cchFilter, int* pnFilterCount, int* pnTxtFilterIndex);
void FileOpen();
BOOL FileSave();
BOOL FileSaveAs();
BOOL PromptSaveChanges();
void EditUndo();
void EditRedo();
void EditCut();
void EditCopy();
void EditPaste();
void EditSelectAll();
void EditInsertTimeDate();
void ViewWordWrap();
void SetRichEditWordWrap(HWND hEdit, LONG widthTwips);
LONG GetTwipsForPixels(HWND hWnd, int widthPx);
void ApplyWordWrap(HWND hEdit);
void ExecuteFilter();
void CreateDefaultINI();
void LoadSettings();
void LoadFilters();
BOOL ValidateFilter(const FilterInfo* filter, int filterIndex, WCHAR* errorMsg, int errorMsgSize);
void SaveCurrentFilter();
void SaveCurrentREPLFilter();
void UpdateFilterDisplay();
void UpdateMenuStates(HWND hwnd);
void BuildFilterMenu(HWND hwnd);
void DoAutosave();
void StartAutosaveTimer(HWND hwnd);
void LoadMRU();
void SaveMRU();
void AddToMRU(LPCWSTR pszFilePath);
void UpdateMRUMenu(HWND hwnd);
void GetSystemLanguageCode(LPWSTR pszLangCode, int cchLangCode);
void GetINIFilePath(LPWSTR pszPath, DWORD dwSize);

// RichEdit library management functions (Phase 2.8)
float GetRichEditVersion(HMODULE hModule, LPWSTR pszPath, DWORD cchPath);
LPCWSTR GetRichEditClassName(float fVersion);
BOOL LoadRichEditLibrary();

// Template system functions
BOOL ParseShortcut(LPCWSTR pszShortcut, WORD* pVirtualKey, BYTE* pModifiers);
BOOL IsShortcutReserved(WORD wVirtualKey, BYTE fModifiers);
void LoadTemplates();
HACCEL BuildAcceleratorTable();
void ViewZoomReset();
void ExtractFileExtension(LPCWSTR pszFilePath, LPWSTR pszExt, DWORD dwExtSize);
void UpdateFileExtension(LPCWSTR pszFilePath);
LPWSTR ExpandTemplateVariables(LPCWSTR pszTemplate, LONG* pCursorOffset);
void InsertTemplate(int nTemplateIndex);
BOOL PopulateTemplateMenu(HMENU hMenu, BOOL bForToolsMenu);  // Helper: populate any menu with templates
void ShowTemplatePickerMenu(HWND hwnd);
void BuildTemplateMenu(HWND hwnd);
void BuildFileNewMenu(HWND hwnd);

// Date/Time formatting functions (Phase 2.10, ToDo #3)
void FormatDateByFlag(SYSTEMTIME* pst, DWORD dwFlags, WCHAR* pszOutput, size_t cchMax);
void FormatTimeByFlag(SYSTEMTIME* pst, DWORD dwFlags, WCHAR* pszOutput, size_t cchMax);
void FormatDateByString(SYSTEMTIME* pst, LPCWSTR pszFormat, WCHAR* pszOutput, size_t cchMax);
void FormatTimeByString(SYSTEMTIME* pst, LPCWSTR pszFormat, WCHAR* pszOutput, size_t cchMax);

// Search functions (Phase 2.9)
LPWSTR ParseEscapeSequences(LPCWSTR pszInput);
LONG FindTextInDocument(LPCWSTR pszSearchText, BOOL bMatchCase, BOOL bWholeWord, BOOL bSearchDown, LONG nStartPos);
BOOL DoFind(BOOL bSearchDown, BOOL bSilent = FALSE);
INT_PTR CALLBACK DlgFindProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void LoadFindHistory();
void SaveFindHistory();
void SaveFindOptions();
void AddToFindHistory(LPCWSTR pszText);
int LoadHistoryList(LPCWSTR pszSection, WCHAR history[][MAX_SEARCH_TEXT], int maxCount);

// Replace functions (Phase 2.9.2)
void UpdateDialogMode(HWND hDlg, BOOL bReplaceMode);
LPWSTR ExpandReplacePlaceholder(LPCWSTR pszReplace, LPCWSTR pszMatched);
void DoReplace();
void DoReplaceAll();
void LoadReplaceHistory();
void SaveReplaceHistory();
void AddToReplaceHistory(LPCWSTR pszText);

// INI file functions
BOOL ReadINIValue(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, LPWSTR pszValue, DWORD dwSize, LPCWSTR pszDefault);
int ReadINIInt(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, int nDefault);
BOOL WriteINIValue(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, LPCWSTR pszValue);
BOOL EnsureIniCacheLoaded();
BOOL FlushIniCache();
BOOL ReplaceINISection(LPCWSTR pszIniPath, LPCWSTR pszSection, const std::wstring& sectionContent);
void AppendKeyValueLine(std::wstring& section, LPCWSTR pszKey, LPCWSTR pszValue);
void AppendIndexedLine(std::wstring& section, LPCWSTR pszKeyPrefix, int index, LPCWSTR pszValue);
void BuildHistorySection(std::wstring& section, LPCWSTR pszSectionName, const WCHAR history[][MAX_SEARCH_TEXT], int count);

// Resume file functions (Phase 2.6)
enum ResumeFileSaveMode {
    RESUME_SAVE_WITH_INI = 0,      // Normal save, register in INI (default)
    RESUME_SAVE_WITHOUT_INI = 1    // Shutdown save, don't register yet (for two-phase commit)
};

BOOL GetRichEditorTempDir(WCHAR* pszPath, DWORD dwSize);
BOOL EnsureRichEditorTempDirExists();
BOOL GenerateResumeFileName(const WCHAR* pszOriginalPath, WCHAR* pszResumeFile, DWORD dwSize);
void WriteResumeToINI(const WCHAR* pszResumeFile, const WCHAR* pszOriginalPath);
BOOL ReadResumeFromINI(WCHAR* pszResumeFile, DWORD dwResumeSize, WCHAR* pszOriginalPath, DWORD dwOriginalSize);
void ClearResumeFromINI();
BOOL DeleteResumeFile(const WCHAR* pszResumeFile);
BOOL SaveToResumeFile(ResumeFileSaveMode mode = RESUME_SAVE_WITH_INI);
BOOL CreateElevatedSaveStagingFile(WCHAR* pszStagingPath, DWORD cchPath);
BOOL RunElevatedSave(LPCWSTR pszStagingPath, LPCWSTR pszTargetPath, DWORD* pLastError);
void FinalizeSuccessfulSave(LPCWSTR pszFileName, BOOL bClearResumeState);
BOOL PerformElevatedSave(LPCWSTR pszTargetPath);
BOOL ElevatedSaveWorker(LPCWSTR pszStagingPath, LPCWSTR pszTargetPath);

// REPL filter functions (Phase 2.5)
void StartREPLFilter(int filterIndex);
void ExitREPLMode();
void SendLineToREPL();
void InsertREPLOutput(LPCWSTR pszOutput);
DWORD WINAPI REPLStdoutThread(LPVOID lpParam);
DWORD WINAPI REPLStderrThread(LPVOID lpParam);
REPLEOLMode DetectEOL(LPCSTR pszOutput, size_t len);
BOOL DetectPrompt(LPCWSTR pszLine, LPCWSTR pszPromptEnd, int* pInputStart);
void StripANSIEscapes(LPWSTR pszText);

//============================================================================
// WinMain - Entry Point
//============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
                    LPWSTR /* lpCmdLine */, int nCmdShow)
{
    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // Parse command line arguments early (needed for elevated save mode)
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    WCHAR szCommandLineFile[EXTENDED_PATH_MAX] = L"";
    WCHAR szElevatedStaging[EXTENDED_PATH_MAX] = L"";
    WCHAR szElevatedTarget[EXTENDED_PATH_MAX] = L"";
    BOOL bElevatedSaveMode = FALSE;
    BOOL bCmdNoMRU = FALSE;
    BOOL bCmdReadOnly = FALSE;
    
    // Parse arguments: look for /nomru option and filename
    // /nomru can appear before or after the filename
    // Examples: RichEditor.exe file.json /nomru
    //           RichEditor.exe /nomru file.json
    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"/nomru") == 0) {
            bCmdNoMRU = TRUE;
        } else if (_wcsicmp(argv[i], L"/readonly") == 0) {
            bCmdReadOnly = TRUE;
        } else if (_wcsicmp(argv[i], L"/elevated-save") == 0) {
            if (i + 2 < argc) {
                wcscpy_s(szElevatedStaging, EXTENDED_PATH_MAX, argv[i + 1]);
                wcscpy_s(szElevatedTarget, EXTENDED_PATH_MAX, argv[i + 2]);
                bElevatedSaveMode = TRUE;
                i += 2;
            }
        } else if (argv[i][0] != L'\0' && szCommandLineFile[0] == L'\0') {
            // First non-option argument is the filename
            wcscpy_s(szCommandLineFile, EXTENDED_PATH_MAX, argv[i]);
        }
    }

    if (argv) {
        LocalFree(argv);
    }

    if (bElevatedSaveMode) {
        BOOL bResult = ElevatedSaveWorker(szElevatedStaging, szElevatedTarget);
        return bResult ? 0 : (GetLastError() ? GetLastError() : 1);
    }

    // Create default INI and load settings (must be before LoadRichEditLibrary)
    OleInitialize(NULL);    // Required for TOM (ITextDocument) and RichEdit OLE support
    CreateDefaultINI();
    EnsureIniCacheLoaded();
    LoadSettings();
    
    // Command-line options override settings
    if (bCmdNoMRU) {
        g_bNoMRU = TRUE;
    }
    if (bCmdReadOnly) {
        g_bReadOnly = TRUE;
    }
    
    // Load RichEdit library (uses custom path from INI if specified)
    if (!LoadRichEditLibrary()) {
        WCHAR szError[256], szTitle[64];
        LoadString(GetModuleHandle(NULL), IDS_RICHEDIT_LOAD_FAILED, szError, 256);
        LoadString(GetModuleHandle(NULL), IDS_ERROR, szTitle, 64);
        MessageBox(NULL, szError, szTitle, MB_ICONERROR);
        return 1;
    }

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU_MAIN);
    wc.lpszClassName = L"RichEditorClass";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wc)) {
        WCHAR szError[256], szTitle[64];
        LoadStringResource(IDS_WINDOW_REG_FAILED, szError, 256);
        LoadStringResource(IDS_ERROR, szTitle, 64);
        MessageBox(NULL, szError, szTitle, MB_ICONERROR);
        return 1;
    }
    
    // Create main window (80% of work area, centered, with reasonable bounds)
    // Use work area instead of screen to respect taskbar and other system UI
    RECT rcWork;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);
    int workWidth = rcWork.right - rcWork.left;
    int workHeight = rcWork.bottom - rcWork.top;
    
    // Default to 80% of work area
    int windowWidth = (workWidth * 80) / 100;
    int windowHeight = (workHeight * 80) / 100;
    
    // Enforce reasonable bounds: minimum 640x480, maximum work area size
    if (windowWidth < 640) windowWidth = (workWidth < 640) ? workWidth : 640;
    if (windowHeight < 480) windowHeight = (workHeight < 480) ? workHeight : 480;
    if (windowWidth > workWidth) windowWidth = workWidth;
    if (windowHeight > workHeight) windowHeight = workHeight;
    
    // Center in work area (not screen, so it appears correctly with taskbar)
    int x = rcWork.left + (workWidth - windowWidth) / 2;
    int y = rcWork.top + (workHeight - windowHeight) / 2;
    
    g_hWndMain = CreateWindowEx(
        0,
        L"RichEditorClass",
        L"RichEditor",  // Temporary title, will be updated in WM_CREATE
        WS_OVERLAPPEDWINDOW,
        x, y, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_hWndMain) {
        WCHAR szError[256], szTitle[64];
        LoadStringResource(IDS_WINDOW_CREATE_FAILED, szError, 256);
        LoadStringResource(IDS_ERROR, szTitle, 64);
        MessageBox(NULL, szError, szTitle, MB_ICONERROR);
        return 1;
    }
    
    // Load accelerators - use dynamic table with template shortcuts
    g_hAccel = BuildAcceleratorTable();
    
    // Show window
    ShowWindow(g_hWndMain, nCmdShow);
    UpdateWindow(g_hWndMain);
    
    // Command-line arguments have precedence over resume files
    // This allows RichEditor to be used as a file viewer while preserving
    // unsaved work for the next launch without arguments
    if (szCommandLineFile[0] != L'\0') {
        // Command-line file specified - load it and ignore resume file
        // Pass FALSE to not delete resume file (user might have multiple instances)
        LoadTextFile(szCommandLineFile, FALSE);
        
        // Add to MRU (respects g_bNoMRU flag set by /nomru command-line option)
        // LoadTextFile with bClearResumeState=FALSE doesn't add to MRU, so we do it here
        AddToMRU(szCommandLineFile);
        
        // DON'T clear resume from INI - defer recovery to next launch without args
        // User's unsaved work is preserved for later
    } else {
        // No command-line file - check for resume file from previous session
        WCHAR szResumeFile[EXTENDED_PATH_MAX];
        WCHAR szOriginalPath[EXTENDED_PATH_MAX];
        
        if (ReadResumeFromINI(szResumeFile, EXTENDED_PATH_MAX, 
                              szOriginalPath, EXTENDED_PATH_MAX)) {
            // Check if resume file actually exists
            DWORD dwAttrib = GetFileAttributes(szResumeFile);
            if (dwAttrib != INVALID_FILE_ATTRIBUTES) {
                // Temporarily set g_bNoMRU to prevent adding temp file to MRU
                BOOL bPrevNoMRU = g_bNoMRU;
                g_bNoMRU = TRUE;
                
                // Use the working LoadTextFile() function to load content
                // This ensures consistent UTF-8 decoding without bugs
                // Pass FALSE to prevent deleting the resume file we're loading
                if (LoadTextFile(szResumeFile, FALSE)) {
                    // LoadTextFile() set g_szFileName to the resume file path
                    // Override with original file path (if available)
                    if (szOriginalPath[0] != L'\0') {
                        wcscpy(g_szFileName, szOriginalPath);
                        const WCHAR* pszFileTitle = wcsrchr(szOriginalPath, L'\\');
                        if (pszFileTitle) {
                            wcscpy(g_szFileTitle, pszFileTitle + 1);
                        } else {
                            wcscpy(g_szFileTitle, szOriginalPath);
                        }
                    } else {
                        // Untitled file - clear filename
                        g_szFileName[0] = L'\0';
                        g_szFileTitle[0] = L'\0';
                    }
                    
                    // Set up resumed file state
                    wcscpy(g_szResumeFilePath, szResumeFile);
                    wcscpy(g_szOriginalFilePath, szOriginalPath);
                    g_bIsResumedFile = TRUE;
                    g_bModified = TRUE;  // Mark as modified
                    
                    // Update title bar to show [Resumed] indicator
                    UpdateTitle(g_hWndMain);
                    
                    // Resume file temp path is NOT added to MRU (g_bNoMRU was TRUE)
                    // Resume file will be kept as backup until explicit save
                }

                // Restore g_bNoMRU in any case
                g_bNoMRU = bPrevNoMRU;
            }
            
            // Clear INI entry (one-time recovery)
            ClearResumeFromINI();
        }
    }
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Handle modeless Find dialog (Phase 2.9)
        if (g_hDlgFind && IsDialogMessage(g_hDlgFind, &msg)) {
            continue;
        }
        
        if (!TranslateAccelerator(g_hWndMain, g_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // Cleanup
    if (g_hAccel) {
        DestroyAcceleratorTable(g_hAccel);
    }
    if (g_hRichEditLib) {
        FreeLibrary(g_hRichEditLib);
    }
    OleUninitialize();
    
    return (int)msg.wParam;
}

//============================================================================
// ParseShortcut - Parse shortcut string like "Ctrl+Shift+F1" into VK code + modifiers
// Returns: TRUE if valid, FALSE if invalid
//============================================================================
BOOL ParseShortcut(LPCWSTR pszShortcut, WORD* pVirtualKey, BYTE* pModifiers)
{
    if (!pszShortcut || !pVirtualKey || !pModifiers) return FALSE;
    
    *pVirtualKey = 0;
    *pModifiers = FVIRTKEY;  // Always virtual key
    
    // Copy shortcut string for tokenization (max 64 chars)
    WCHAR szShortcut[64];
    wcsncpy(szShortcut, pszShortcut, 63);
    szShortcut[63] = L'\0';
    
    // Trim whitespace
    LPWSTR psz = szShortcut;
    while (*psz == L' ' || *psz == L'\t') psz++;
    
    if (*psz == L'\0') return FALSE;  // Empty string
    
    // Split by '+' and process tokens
    WCHAR* pToken = wcstok(psz, L"+");
    WCHAR szKeyName[32] = L"";
    
    while (pToken) {
        // Trim whitespace from token
        while (*pToken == L' ' || *pToken == L'\t') pToken++;
        LPWSTR pEnd = pToken + wcslen(pToken) - 1;
        while (pEnd > pToken && (*pEnd == L' ' || *pEnd == L'\t')) {
            *pEnd = L'\0';
            pEnd--;
        }
        
        // Check if it's a modifier (case-insensitive)
        if (_wcsicmp(pToken, L"Ctrl") == 0 || _wcsicmp(pToken, L"Control") == 0) {
            *pModifiers |= FCONTROL;
        } else if (_wcsicmp(pToken, L"Shift") == 0) {
            *pModifiers |= FSHIFT;
        } else if (_wcsicmp(pToken, L"Alt") == 0) {
            *pModifiers |= FALT;
        } else {
            // This is the key name (last token)
            wcscpy(szKeyName, pToken);
        }
        
        pToken = wcstok(NULL, L"+");
    }
    
    // Must have a key name
    if (szKeyName[0] == L'\0') return FALSE;
    
    // Look up key name in mapping table (case-insensitive)
    for (int i = 0; i < g_nKeyMapCount; i++) {
        if (_wcsicmp(szKeyName, g_KeyMap[i].szName) == 0) {
            *pVirtualKey = g_KeyMap[i].wVirtualKey;
            return TRUE;
        }
    }
    
    // Key name not found
    return FALSE;
}

//============================================================================
// IsShortcutReserved - Check if shortcut conflicts with built-in shortcuts
// Returns: TRUE if reserved (cannot be used), FALSE if available
//============================================================================
BOOL IsShortcutReserved(WORD wVirtualKey, BYTE fModifiers)
{
    for (int i = 0; g_ReservedShortcuts[i].szDescription != NULL; i++) {
        if (g_ReservedShortcuts[i].wVirtualKey == wVirtualKey &&
            g_ReservedShortcuts[i].fModifiers == fModifiers) {
            return TRUE;
        }
    }
    return FALSE;
}

//============================================================================
// UnescapeTemplateString - Convert escape sequences like \n, \t, \\ to actual characters
//============================================================================
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
// Unknown escapes are preserved as literals (e.g., \q stays as \q)
//============================================================================
LPWSTR ParseEscapeSequences(LPCWSTR pszInput)
{
    if (!pszInput) return NULL;
    
    // Allocate output buffer (same size as input is always enough)
    size_t len = wcslen(pszInput);
    LPWSTR pszResult = (LPWSTR)malloc((len + 1) * sizeof(WCHAR));
    if (!pszResult) return NULL;
    
    const WCHAR* pSrc = pszInput;
    WCHAR* pDest = pszResult;
    
    while (*pSrc) {
        if (*pSrc == L'\\' && *(pSrc + 1)) {
            pSrc++;  // Skip backslash
            
            switch (*pSrc) {
                case L'n':
                    *pDest++ = L'\n';
                    pSrc++;
                    break;
                    
                case L'r':
                    *pDest++ = L'\r';
                    pSrc++;
                    break;
                    
                case L't':
                    *pDest++ = L'\t';
                    pSrc++;
                    break;
                    
                case L'\\':
                    *pDest++ = L'\\';
                    pSrc++;
                    break;
                    
                case L'x':  // \xNN - hex byte
                {
                    pSrc++;  // Skip 'x'
                    
                    // Check if we have 2 hex digits
                    if (iswxdigit(pSrc[0]) && iswxdigit(pSrc[1])) {
                        WCHAR szHex[3] = {pSrc[0], pSrc[1], L'\0'};
                        WCHAR* pEnd;
                        long val = wcstol(szHex, &pEnd, 16);
                        *pDest++ = (WCHAR)val;
                        pSrc += 2;
                    } else {
                        // Invalid hex sequence - keep literal \x
                        *pDest++ = L'\\';
                        *pDest++ = L'x';
                    }
                    break;
                }
                
                case L'u':  // \uNNNN - Unicode codepoint
                {
                    pSrc++;  // Skip 'u'
                    
                    // Check if we have 4 hex digits
                    if (iswxdigit(pSrc[0]) && iswxdigit(pSrc[1]) &&
                        iswxdigit(pSrc[2]) && iswxdigit(pSrc[3])) {
                        WCHAR szHex[5] = {pSrc[0], pSrc[1], pSrc[2], pSrc[3], L'\0'};
                        WCHAR* pEnd;
                        long val = wcstol(szHex, &pEnd, 16);
                        *pDest++ = (WCHAR)val;
                        pSrc += 4;
                    } else {
                        // Invalid Unicode sequence - keep literal \u
                        *pDest++ = L'\\';
                        *pDest++ = L'u';
                    }
                    break;
                }
                
                default:
                    // Unknown escape - preserve backslash and character
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

//============================================================================
// LoadTemplates - Load templates from INI file
//============================================================================
void LoadTemplates()
{
    // Get path to INI file
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);

    // Read template count
    g_nTemplateCount = ReadINIInt(szIniPath, L"Templates", L"Count", 0);
    if (g_nTemplateCount > MAX_TEMPLATES) {
        g_nTemplateCount = MAX_TEMPLATES;
    }
    
    // Load each template
    for (int i = 0; i < g_nTemplateCount; i++) {
        WCHAR szSection[32];
        swprintf(szSection, 32, L"Template%d", i + 1);
        
        // Read basic fields
        ReadINIValue(szIniPath, szSection, L"Name", 
                     g_Templates[i].szName, MAX_TEMPLATE_NAME, L"");
        ReadINIValue(szIniPath, szSection, L"Description", 
                     g_Templates[i].szDescription, MAX_TEMPLATE_DESC, L"");
        ReadINIValue(szIniPath, szSection, L"Category", 
                     g_Templates[i].szCategory, MAX_TEMPLATE_CATEGORY, L"");
        ReadINIValue(szIniPath, szSection, L"FileExtension", 
                     g_Templates[i].szFileExtension, MAX_TEMPLATE_FILEEXT, L"");
        
        // Read template value (with escape sequences)
        WCHAR szTemplateRaw[MAX_TEMPLATE_VALUE];
        ReadINIValue(szIniPath, szSection, L"Template", 
                     szTemplateRaw, MAX_TEMPLATE_VALUE, L"");
        
        // Parse escape sequences (\n → newline, \t → tab, \xNN, \uNNNN, etc.)
        LPWSTR pszParsed = ParseEscapeSequences(szTemplateRaw);
        if (pszParsed) {
            wcsncpy(g_Templates[i].szTemplate, pszParsed, MAX_TEMPLATE_VALUE - 1);
            g_Templates[i].szTemplate[MAX_TEMPLATE_VALUE - 1] = L'\0';
            free(pszParsed);
        } else {
            // Fallback: use raw string if parsing failed
            wcsncpy(g_Templates[i].szTemplate, szTemplateRaw, MAX_TEMPLATE_VALUE - 1);
            g_Templates[i].szTemplate[MAX_TEMPLATE_VALUE - 1] = L'\0';
        }
        
        // Get system language code for localized strings
        WCHAR szLangCode[16];
        GetSystemLanguageCode(szLangCode, 16);
        
        // Try to load localized name
        // First try full locale (e.g., "Name.cs_CZ")
        WCHAR szLocalizedKey[64];
        _snwprintf(szLocalizedKey, 64, L"Name.%s", szLangCode);
        szLocalizedKey[63] = L'\0';
        ReadINIValue(szIniPath, szSection, szLocalizedKey, 
                     g_Templates[i].szLocalizedName, MAX_TEMPLATE_NAME, L"");
        
        // If not found, try just language code (e.g., "Name.cs")
        if (g_Templates[i].szLocalizedName[0] == L'\0') {
            // Extract language code (first 2 chars before underscore)
            WCHAR szLangOnly[8];
            wcscpy(szLangOnly, szLangCode);
            WCHAR* pUnderscore = wcschr(szLangOnly, L'_');
            if (pUnderscore) {
                *pUnderscore = L'\0';
            }
            
            _snwprintf(szLocalizedKey, 64, L"Name.%s", szLangOnly);
            szLocalizedKey[63] = L'\0';
            ReadINIValue(szIniPath, szSection, szLocalizedKey, 
                         g_Templates[i].szLocalizedName, MAX_TEMPLATE_NAME, L"");
        }
        
        // If no localized name, use default name
        if (g_Templates[i].szLocalizedName[0] == L'\0') {
            wcscpy(g_Templates[i].szLocalizedName, g_Templates[i].szName);
        }
        
        // Try to load localized description
        // First try full locale (e.g., "Description.cs_CZ")
        _snwprintf(szLocalizedKey, 64, L"Description.%s", szLangCode);
        szLocalizedKey[63] = L'\0';
        ReadINIValue(szIniPath, szSection, szLocalizedKey, 
                     g_Templates[i].szLocalizedDescription, MAX_TEMPLATE_DESC, L"");
        
        // If not found, try just language code (e.g., "Description.cs")
        if (g_Templates[i].szLocalizedDescription[0] == L'\0') {
            // Extract language code (first 2 chars before underscore)
            WCHAR szLangOnly[8];
            wcscpy(szLangOnly, szLangCode);
            WCHAR* pUnderscore = wcschr(szLangOnly, L'_');
            if (pUnderscore) {
                *pUnderscore = L'\0';
            }
            
            _snwprintf(szLocalizedKey, 64, L"Description.%s", szLangOnly);
            szLocalizedKey[63] = L'\0';
            ReadINIValue(szIniPath, szSection, szLocalizedKey, 
                         g_Templates[i].szLocalizedDescription, MAX_TEMPLATE_DESC, L"");
        }
        
        // If no localized description, use default description
        if (g_Templates[i].szLocalizedDescription[0] == L'\0') {
            wcscpy(g_Templates[i].szLocalizedDescription, g_Templates[i].szDescription);
        }
        
        // Parse Shortcut field (optional)
        WCHAR szShortcut[64];
        ReadINIValue(szIniPath, szSection, L"Shortcut", szShortcut, 64, L"");
        if (szShortcut[0] != L'\0') {
            WORD wVirtualKey;
            BYTE fModifiers;
            
            if (ParseShortcut(szShortcut, &wVirtualKey, &fModifiers)) {
                // Check if reserved
                if (IsShortcutReserved(wVirtualKey, fModifiers)) {
                    // Skip - reserved shortcut
                    g_Templates[i].wVirtualKey = 0;
                    g_Templates[i].fModifiers = 0;
                } else {
                    g_Templates[i].wVirtualKey = wVirtualKey;
                    g_Templates[i].fModifiers = fModifiers;
                }
            } else {
                // Invalid shortcut format - ignore
                g_Templates[i].wVirtualKey = 0;
                g_Templates[i].fModifiers = 0;
            }
        } else {
            // No shortcut defined
            g_Templates[i].wVirtualKey = 0;
            g_Templates[i].fModifiers = 0;
        }
    }
}

//============================================================================
// BuildAcceleratorTable - Build dynamic accelerator table with built-in + template shortcuts
// Returns: HACCEL handle (caller must eventually DestroyAcceleratorTable)
//============================================================================
HACCEL BuildAcceleratorTable()
{
    // Count total accelerators needed
    const int BUILTIN_COUNT = 24;  // Built-in shortcuts (including search shortcuts - Phase 2.9.3)
    int nTemplateShortcuts = 0;
    
    for (int i = 0; i < g_nTemplateCount; i++) {
        if (g_Templates[i].wVirtualKey != 0) {
            nTemplateShortcuts++;
        }
    }
    
    int nTotalAccel = BUILTIN_COUNT + nTemplateShortcuts;
    
    // Allocate ACCEL array
    ACCEL* pAccel = (ACCEL*)malloc(nTotalAccel * sizeof(ACCEL));
    if (!pAccel) return NULL;
    
    int idx = 0;
    
    // Add built-in shortcuts (must match resource.rc order!)
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'N'; pAccel[idx++].cmd = ID_FILE_NEW;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'O'; pAccel[idx++].cmd = ID_FILE_OPEN;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'S'; pAccel[idx++].cmd = ID_FILE_SAVE;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'Z'; pAccel[idx++].cmd = ID_EDIT_UNDO;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'Y'; pAccel[idx++].cmd = ID_EDIT_REDO;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'X'; pAccel[idx++].cmd = ID_EDIT_CUT;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'C'; pAccel[idx++].cmd = ID_EDIT_COPY;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'V'; pAccel[idx++].cmd = ID_EDIT_PASTE;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'A'; pAccel[idx++].cmd = ID_EDIT_SELECTALL;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'W'; pAccel[idx++].cmd = ID_VIEW_WORDWRAP;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = '0'; pAccel[idx++].cmd = ID_VIEW_ZOOM_RESET;
    pAccel[idx].fVirt = FVIRTKEY; pAccel[idx].key = VK_F5; pAccel[idx++].cmd = ID_EDIT_TIMEDATE;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = VK_RETURN; pAccel[idx++].cmd = ID_TOOLS_EXECUTEFILTER;
    pAccel[idx].fVirt = FCONTROL | FSHIFT | FVIRTKEY; pAccel[idx].key = 'I'; pAccel[idx++].cmd = ID_TOOLS_START_INTERACTIVE;
    pAccel[idx].fVirt = FCONTROL | FSHIFT | FVIRTKEY; pAccel[idx].key = 'Q'; pAccel[idx++].cmd = ID_TOOLS_EXIT_INTERACTIVE;
    pAccel[idx].fVirt = FCONTROL | FSHIFT | FVIRTKEY; pAccel[idx].key = 'T'; pAccel[idx++].cmd = ID_TOOLS_INSERT_TEMPLATE;
    
    // Search shortcuts (Phase 2.9)
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'F'; pAccel[idx++].cmd = ID_SEARCH_FIND;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'H'; pAccel[idx++].cmd = ID_SEARCH_REPLACE;
    pAccel[idx].fVirt = FVIRTKEY; pAccel[idx].key = VK_F3; pAccel[idx++].cmd = ID_SEARCH_FIND_NEXT;
    pAccel[idx].fVirt = FSHIFT | FVIRTKEY; pAccel[idx].key = VK_F3; pAccel[idx++].cmd = ID_SEARCH_FIND_PREVIOUS;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = 'G'; pAccel[idx++].cmd = ID_SEARCH_GOTO_LINE;
    pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = VK_F2; pAccel[idx++].cmd = ID_SEARCH_TOGGLE_BOOKMARK;
    pAccel[idx].fVirt = FVIRTKEY; pAccel[idx].key = VK_F2; pAccel[idx++].cmd = ID_SEARCH_NEXT_BOOKMARK;
    pAccel[idx].fVirt = FSHIFT | FVIRTKEY; pAccel[idx].key = VK_F2; pAccel[idx++].cmd = ID_SEARCH_PREV_BOOKMARK;
    
    // Add template shortcuts
    for (int i = 0; i < g_nTemplateCount; i++) {
        if (g_Templates[i].wVirtualKey != 0) {
            pAccel[idx].fVirt = g_Templates[i].fModifiers;
            pAccel[idx].key = g_Templates[i].wVirtualKey;
            pAccel[idx].cmd = ID_TOOLS_TEMPLATE_BASE + i;
            idx++;
        }
    }
    
    // Create accelerator table
    HACCEL hAccel = CreateAcceleratorTable(pAccel, nTotalAccel);
    free(pAccel);
    
    return hAccel;
}

//============================================================================
// ExtractFileExtension - Extract file extension from path (without dot)
// Wrapper around PathFindExtensionW() from shlwapi.dll
// Example: "file.txt" -> "txt", "file.md" -> "md", "file" -> ""
//============================================================================
void ExtractFileExtension(LPCWSTR pszFilePath, LPWSTR pszExt, DWORD dwExtSize)
{
    if (!pszFilePath || !pszExt || dwExtSize == 0) return;
    
    pszExt[0] = L'\0';  // Default: no extension
    
    // Use shlwapi.dll function to find extension
    LPCWSTR pszExtWithDot = PathFindExtensionW(pszFilePath);
    
    // PathFindExtensionW returns pointer to dot (or empty string if no extension)
    if (pszExtWithDot && pszExtWithDot[0] == L'.') {
        // Copy extension without the dot
        wcsncpy(pszExt, pszExtWithDot + 1, dwExtSize - 1);
        pszExt[dwExtSize - 1] = L'\0';
        
        // Convert to lowercase for case-insensitive comparison
        for (WCHAR* p = pszExt; *p; p++) {
            *p = towlower(*p);
        }
    }
}

//============================================================================
// FormatDateByFlag - Format date using Windows API dwFlags (Phase 2.10, ToDo #3)
// dwFlags: DATE_SHORTDATE, DATE_LONGDATE, DATE_YEARMONTH, DATE_MONTHDAY
// Used by internal variables: %shortdate%, %longdate%, %yearmonth%, %monthday%
// Note: Function always succeeds with valid SYSTEMTIME and locale constant.
//       Result is not checked - catastrophic errors (out of memory) are not handled.
//============================================================================
void FormatDateByFlag(SYSTEMTIME* pst, DWORD dwFlags, WCHAR* pszOutput, size_t cchMax)
{
    GetDateFormatEx(
        LOCALE_NAME_USER_DEFAULT,
        dwFlags,
        pst,
        NULL,  // Use default format for locale
        pszOutput,
        (int)cchMax,
        NULL
    );
}

//============================================================================
// FormatTimeByFlag - Format time using Windows API dwFlags (Phase 2.10, ToDo #3)
// dwFlags: 0 (with seconds), TIME_NOSECONDS
// Used by internal variables: %longtime%, %shorttime%
// Note: Function always succeeds with valid SYSTEMTIME and locale constant.
//       Result is not checked - catastrophic errors (out of memory) are not handled.
//============================================================================
void FormatTimeByFlag(SYSTEMTIME* pst, DWORD dwFlags, WCHAR* pszOutput, size_t cchMax)
{
    GetTimeFormatEx(
        LOCALE_NAME_USER_DEFAULT,
        dwFlags,
        pst,
        NULL,  // Use default format for locale
        pszOutput,
        (int)cchMax
    );
}

//============================================================================
// FormatDateByString - Format date using custom format string (Phase 2.10, ToDo #3)
// Format syntax: https://learn.microsoft.com/en-us/windows/win32/intl/day--month--year--and-era-format-pictures
// Supports format specifiers (yyyy, MM, dd, etc.) and literal text (enclosed in 'quotes')
// Examples: yyyy-MM-dd, dd.MM.yyyy, 'Day 'dd' of 'MMMM', 'yyyy
// Note: Invalid format strings are NOT validated - they produce undefined output.
//       Per Microsoft docs: "returns no errors for bad format string, just forms best possible date string"
//       Example: "INVALID_FORMAT" → "INVALID_FOR1AT" (M replaced with month number, rest unchanged)
//============================================================================
void FormatDateByString(SYSTEMTIME* pst, LPCWSTR pszFormat, WCHAR* pszOutput, size_t cchMax)
{
    if (pszFormat == NULL || pszFormat[0] == L'\0') {
        // Empty format - fall back to default %shortdate%
        FormatDateByFlag(pst, DATE_SHORTDATE, pszOutput, cchMax);
        return;
    }
    
    GetDateFormatEx(
        LOCALE_NAME_USER_DEFAULT,
        0,  // No flags when using custom format
        pst,
        pszFormat,
        pszOutput,
        (int)cchMax,
        NULL
    );
}

//============================================================================
// FormatTimeByString - Format time using custom format string (Phase 2.10, ToDo #3)
// Format syntax: https://learn.microsoft.com/en-us/windows/win32/intl/time-format-pictures
// Supports format specifiers (HH, mm, ss, tt, etc.) and literal text (enclosed in 'quotes')
// Examples: HH:mm:ss, h:mm tt, 'at 'h:mm' 'tt
// Note: Invalid format strings are NOT validated - they produce undefined output.
//       Per Microsoft docs: "returns no errors for bad format string, just forms best possible time string"
//============================================================================
void FormatTimeByString(SYSTEMTIME* pst, LPCWSTR pszFormat, WCHAR* pszOutput, size_t cchMax)
{
    if (pszFormat == NULL || pszFormat[0] == L'\0') {
        // Empty format - fall back to default HH:mm via GetTimeFormatEx
        GetTimeFormatEx(
            LOCALE_NAME_USER_DEFAULT,
            TIME_NOSECONDS,
            pst,
            L"HH:mm",  // Explicit fallback format
            pszOutput,
            (int)cchMax
        );
        return;
    }
    
    GetTimeFormatEx(
        LOCALE_NAME_USER_DEFAULT,
        0,  // No flags when using custom format
        pst,
        pszFormat,
        pszOutput,
        (int)cchMax
    );
}

//============================================================================
// UpdateFileExtension - Update g_szCurrentFileExtension based on current filename
// Also rebuilds template menu to show only relevant templates
//============================================================================
void UpdateFileExtension(LPCWSTR pszFilePath)
{
    if (pszFilePath && pszFilePath[0] != L'\0') {
        ExtractFileExtension(pszFilePath, g_szCurrentFileExtension, MAX_TEMPLATE_FILEEXT);
    } else {
        // No file (Untitled) - default to txt
        wcscpy(g_szCurrentFileExtension, L"txt");
    }
    
    // Rebuild template menu to filter by new extension
    if (g_hWndMain) {
        BuildTemplateMenu(g_hWndMain);
    }
}

//============================================================================
// ExpandTemplateVariables - Replace %varname% with actual values
// Returns: Allocated string (caller must free()), or NULL on failure
// pCursorOffset: Receives cursor position offset (-1 if no %cursor%)
//============================================================================
LPWSTR ExpandTemplateVariables(LPCWSTR pszTemplate, LONG* pCursorOffset)
{
    if (!pszTemplate) return NULL;
    
    // Allocate output buffer (max 64 KB for expanded template)
    const DWORD MAX_EXPANDED = 65536;
    LPWSTR pszOutput = (LPWSTR)malloc(MAX_EXPANDED * sizeof(WCHAR));
    if (!pszOutput) return NULL;
    
    *pCursorOffset = -1;  // No cursor marker found yet
    BOOL bCursorFound = FALSE;
    
    DWORD outIdx = 0;
    const WCHAR* p = pszTemplate;
    
    while (*p && outIdx < MAX_EXPANDED - 1) {
        if (*p == L'%') {
            // Potential variable
            const WCHAR* pVarStart = p + 1;
            const WCHAR* pVarEnd = wcschr(pVarStart, L'%');
            
            if (pVarEnd) {
                // Extract variable name
                size_t varLen = pVarEnd - pVarStart;
                WCHAR szVarName[64];
                
                if (varLen < 63) {
                    wcsncpy(szVarName, pVarStart, varLen);
                    szVarName[varLen] = L'\0';
                    
                    // Convert to lowercase for case-insensitive comparison
                    for (WCHAR* pv = szVarName; *pv; pv++) {
                        *pv = towlower(*pv);
                    }
                    
                    // Handle variables
                    if (wcscmp(szVarName, L"cursor") == 0) {
                        // %cursor% - Mark cursor position
                        if (!bCursorFound) {
                            *pCursorOffset = (LONG)outIdx;
                            bCursorFound = TRUE;
                        }
                        // Don't insert anything for cursor
                        p = pVarEnd + 1;
                        continue;
                    } else if (wcscmp(szVarName, L"selection") == 0) {
                        // %selection% - Insert current selection
                        CHARRANGE cr;
                        SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
                        
                        if (cr.cpMin != cr.cpMax) {
                            int selLen = cr.cpMax - cr.cpMin;
                            LPWSTR pszSel = (LPWSTR)malloc((selLen + 1) * sizeof(WCHAR));
                            if (pszSel) {
                                TEXTRANGE tr;
                                tr.chrg = cr;
                                tr.lpstrText = pszSel;
                                SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
                                pszSel[selLen] = L'\0';
                                
                                // Copy selection to output
                                for (int i = 0; i < selLen && outIdx < MAX_EXPANDED - 1; i++) {
                                    pszOutput[outIdx++] = pszSel[i];
                                }
                                
                                free(pszSel);
                            }
                        }
                        p = pVarEnd + 1;
                        continue;
                    } else if (wcscmp(szVarName, L"shortdate") == 0) {
                        // %shortdate% - System short date format (Phase 2.10, ToDo #3)
                        SYSTEMTIME st;
                        GetLocalTime(&st);
                        WCHAR szDate[128];
                        FormatDateByFlag(&st, DATE_SHORTDATE, szDate, 128);
                        
                        for (const WCHAR* pd = szDate; *pd && outIdx < MAX_EXPANDED - 1; pd++) {
                            pszOutput[outIdx++] = *pd;
                        }
                        p = pVarEnd + 1;
                        continue;
                        
                    } else if (wcscmp(szVarName, L"longdate") == 0) {
                        // %longdate% - System long date format (Phase 2.10, ToDo #3)
                        SYSTEMTIME st;
                        GetLocalTime(&st);
                        WCHAR szDate[128];
                        FormatDateByFlag(&st, DATE_LONGDATE, szDate, 128);
                        
                        for (const WCHAR* pd = szDate; *pd && outIdx < MAX_EXPANDED - 1; pd++) {
                            pszOutput[outIdx++] = *pd;
                        }
                        p = pVarEnd + 1;
                        continue;

                    } else if (wcscmp(szVarName, L"yearmonth") == 0) {
                        // %yearmonth% - Year and month (Phase 2.10, ToDo #3)
                        SYSTEMTIME st;
                        GetLocalTime(&st);
                        WCHAR szDate[128];
                        FormatDateByFlag(&st, DATE_YEARMONTH, szDate, 128);
                        
                        for (const WCHAR* pd = szDate; *pd && outIdx < MAX_EXPANDED - 1; pd++) {
                            pszOutput[outIdx++] = *pd;
                        }
                        p = pVarEnd + 1;
                        continue;

                    } else if (wcscmp(szVarName, L"monthday") == 0) {
                        // %monthday% - Month and day (Phase 2.10, ToDo #3)
                        SYSTEMTIME st;
                        GetLocalTime(&st);
                        WCHAR szDate[128];
                        FormatDateByFlag(&st, DATE_MONTHDAY, szDate, 128);
                        
                        for (const WCHAR* pd = szDate; *pd && outIdx < MAX_EXPANDED - 1; pd++) {
                            pszOutput[outIdx++] = *pd;
                        }
                        p = pVarEnd + 1;
                        continue;

                    } else if (wcscmp(szVarName, L"longtime") == 0) {
                        // %longtime% - Time with seconds (Phase 2.10, ToDo #3)
                        SYSTEMTIME st;
                        GetLocalTime(&st);
                        WCHAR szTime[128];
                        FormatTimeByFlag(&st, 0, szTime, 128);  // 0 = include seconds
                        
                        for (const WCHAR* pt = szTime; *pt && outIdx < MAX_EXPANDED - 1; pt++) {
                            pszOutput[outIdx++] = *pt;
                        }
                        p = pVarEnd + 1;
                        continue;

                    } else if (wcscmp(szVarName, L"shorttime") == 0) {
                        // %shorttime% - Time without seconds (Phase 2.10, ToDo #3)
                        SYSTEMTIME st;
                        GetLocalTime(&st);
                        WCHAR szTime[128];
                        FormatTimeByFlag(&st, TIME_NOSECONDS, szTime, 128);
                        
                        for (const WCHAR* pt = szTime; *pt && outIdx < MAX_EXPANDED - 1; pt++) {
                            pszOutput[outIdx++] = *pt;
                        }
                        p = pVarEnd + 1;
                        continue;
                        
                    } else if (wcscmp(szVarName, L"date") == 0) {
                        // %date% - Configurable date format (Phase 2.10, ToDo #3)
                        // Uses DateFormat INI setting (can be internal variable or custom format)
                        SYSTEMTIME st;
                        GetLocalTime(&st);
                        WCHAR szDate[128];
                        
                        // Check if DateFormat is an internal variable (exclusive mode)
                        if (wcscmp(g_szDateFormat, L"%shortdate%") == 0) {
                            FormatDateByFlag(&st, DATE_SHORTDATE, szDate, 128);
                        } else if (wcscmp(g_szDateFormat, L"%longdate%") == 0) {
                            FormatDateByFlag(&st, DATE_LONGDATE, szDate, 128);
                        } else if (wcscmp(g_szDateFormat, L"%yearmonth%") == 0) {
                            FormatDateByFlag(&st, DATE_YEARMONTH, szDate, 128);
                        } else if (wcscmp(g_szDateFormat, L"%monthday%") == 0) {
                            FormatDateByFlag(&st, DATE_MONTHDAY, szDate, 128);
                        } else {
                            // Not an internal variable - treat as custom format string
                            FormatDateByString(&st, g_szDateFormat, szDate, 128);
                        }
                        
                        for (const WCHAR* pd = szDate; *pd && outIdx < MAX_EXPANDED - 1; pd++) {
                            pszOutput[outIdx++] = *pd;
                        }
                        p = pVarEnd + 1;
                        continue;
                        
                    } else if (wcscmp(szVarName, L"time") == 0) {
                        // %time% - Configurable time format (Phase 2.10, ToDo #3)
                        // Uses TimeFormat INI setting (can be internal variable or custom format)
                        SYSTEMTIME st;
                        GetLocalTime(&st);
                        WCHAR szTime[128];
                        
                        // Check if TimeFormat is an internal variable (exclusive mode)
                        if (wcscmp(g_szTimeFormat, L"%longtime%") == 0) {
                            FormatTimeByFlag(&st, 0, szTime, 128);  // With seconds
                        } else if (wcscmp(g_szTimeFormat, L"%shorttime%") == 0) {
                            FormatTimeByFlag(&st, TIME_NOSECONDS, szTime, 128);
                        } else {
                            // Not an internal variable - treat as custom format string
                            FormatTimeByString(&st, g_szTimeFormat, szTime, 128);
                        }
                        
                        for (const WCHAR* pt = szTime; *pt && outIdx < MAX_EXPANDED - 1; pt++) {
                            pszOutput[outIdx++] = *pt;
                        }
                        p = pVarEnd + 1;
                        continue;
                        
                    } else if (wcscmp(szVarName, L"clipboard") == 0) {
                        // %clipboard% - Insert clipboard text
                        if (OpenClipboard(g_hWndMain)) {
                            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                            if (hData) {
                                LPCWSTR pszClip = (LPCWSTR)GlobalLock(hData);
                                if (pszClip) {
                                    for (const WCHAR* pc = pszClip; *pc && outIdx < MAX_EXPANDED - 1; pc++) {
                                        pszOutput[outIdx++] = *pc;
                                    }
                                    GlobalUnlock(hData);
                                }
                            }
                            CloseClipboard();
                        }
                        p = pVarEnd + 1;
                        continue;
                    } else {
                        // Unknown variable - leave as literal
                        pszOutput[outIdx++] = L'%';
                        for (size_t i = 0; i < varLen && outIdx < MAX_EXPANDED - 1; i++) {
                            pszOutput[outIdx++] = pVarStart[i];
                        }
                        if (outIdx < MAX_EXPANDED - 1) {
                            pszOutput[outIdx++] = L'%';
                        }
                        p = pVarEnd + 1;
                        continue;
                    }
                }
            }
        }
        
        // Regular character
        pszOutput[outIdx++] = *p++;
    }
    
    pszOutput[outIdx] = L'\0';
    return pszOutput;
}

//============================================================================
// InsertTemplate - Insert template at cursor position with variable expansion
//============================================================================
void InsertTemplate(int nTemplateIndex)
{
    if (nTemplateIndex < 0 || nTemplateIndex >= g_nTemplateCount) return;
    
    // Block template insertion in read-only mode
    if (g_bReadOnly) return;
    
    TemplateInfo* pTemplate = &g_Templates[nTemplateIndex];
    
    // Check if template is available for current file type
    if (pTemplate->szFileExtension[0] != L'\0') {
        // Template has file extension requirement
        if (_wcsicmp(pTemplate->szFileExtension, g_szCurrentFileExtension) != 0) {
            // File extension doesn't match - don't insert
            // Silent fail (user probably used wrong extension)
            return;
        }
    }
    
    // Expand variables
    LONG nCursorOffset = -1;
    LPWSTR pszExpanded = ExpandTemplateVariables(pTemplate->szTemplate, &nCursorOffset);
    if (!pszExpanded) return;
    
    // Get current selection
    CHARRANGE crOriginal;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&crOriginal);
    
    // Replace selection with expanded template
    SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszExpanded);
    
    // Position cursor if %cursor% was found
    if (nCursorOffset >= 0) {
        CHARRANGE crNew;
        crNew.cpMin = crOriginal.cpMin + nCursorOffset;
        crNew.cpMax = crNew.cpMin;
        SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crNew);
    }
    
    free(pszExpanded);
    
    // Mark document as modified
    g_bModified = TRUE;
    UpdateTitle(g_hWndMain);
}

//============================================================================
// PopulateTemplateMenu - Helper to populate any HMENU with categorized templates
// Filters templates by g_szCurrentFileExtension and organizes them by category
// bForToolsMenu: TRUE = use for Tools→Insert Template (allows "no templates" messages)
//                FALSE = use for Ctrl+Shift+T popup (caller handles empty case)
// Returns: TRUE if any templates were added, FALSE if no matching templates
//============================================================================
BOOL PopulateTemplateMenu(HMENU hMenu, BOOL bForToolsMenu)
{
    if (!hMenu) return FALSE;
    
    // Build category map
    struct TemplateCategoryInfo {
        WCHAR szName[MAX_TEMPLATE_CATEGORY];
        int templateIndices[MAX_TEMPLATES];
        int count;
    };
    TemplateCategoryInfo categories[32];
    int categoryCount = 0;
    int uncategorizedTemplates[MAX_TEMPLATES];
    int uncategorizedCount = 0;
    
    // Group templates by category, filtered by current file extension
    for (int i = 0; i < g_nTemplateCount; i++) {
        // Skip templates not available for current file type
        if (g_Templates[i].szFileExtension[0] != L'\0') {
            if (_wcsicmp(g_Templates[i].szFileExtension, g_szCurrentFileExtension) != 0) {
                continue;  // Skip this template
            }
        }
        
        // Check if template has a category
        if (g_Templates[i].szCategory[0] == L'\0') {
            // No category - add to uncategorized list
            uncategorizedTemplates[uncategorizedCount++] = i;
            continue;
        }
        
        // Find or create category
        int catIndex = -1;
        for (int c = 0; c < categoryCount; c++) {
            if (wcscmp(categories[c].szName, g_Templates[i].szCategory) == 0) {
                catIndex = c;
                break;
            }
        }
        
        if (catIndex == -1) {
            catIndex = categoryCount++;
            wcscpy(categories[catIndex].szName, g_Templates[i].szCategory);
            categories[catIndex].count = 0;
        }
        
        categories[catIndex].templateIndices[categories[catIndex].count++] = i;
    }
    
    // If no templates match current file type
    if (categoryCount == 0 && uncategorizedCount == 0) {
        if (bForToolsMenu) {
            // For Tools menu, show "No templates for file type" message
            WCHAR szNoTemplatesForType[64];
            LoadString(GetModuleHandle(NULL), IDS_NO_TEMPLATES_FOR_FILETYPE, szNoTemplatesForType, 64);
            AppendMenu(hMenu, MF_STRING | MF_GRAYED, ID_TOOLS_TEMPLATE_BASE, szNoTemplatesForType);
        }
        return FALSE;
    }
    
    // Create submenu for each category (Tools menu) OR flatten with headers (popup menu)
    for (int c = 0; c < categoryCount; c++) {
        if (bForToolsMenu) {
            // Tools→Insert Template: Create cascading submenu (traditional menu bar behavior)
            HMENU hCategoryMenu = CreatePopupMenu();
            
            // Add templates in this category
            for (int t = 0; t < categories[c].count; t++) {
                int templateIndex = categories[c].templateIndices[t];
                
                // Build menu text with description if enabled
                WCHAR szMenuText[MAX_TEMPLATE_NAME + MAX_TEMPLATE_DESC + 4];
                wcscpy(szMenuText, g_Templates[templateIndex].szLocalizedName);
                
                if (g_bShowMenuDescriptions && g_Templates[templateIndex].szLocalizedDescription[0] != L'\0') {
                    wcscat(szMenuText, L": ");
                    wcscat(szMenuText, g_Templates[templateIndex].szLocalizedDescription);
                }
                
                AppendMenu(hCategoryMenu, MF_STRING, ID_TOOLS_TEMPLATE_BASE + templateIndex, szMenuText);
            }
            
            // Add category submenu
            AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hCategoryMenu, categories[c].szName);
        } else {
            // Ctrl+Shift+T popup: Flatten menu with category headers for quick access
            
            // Add category name as disabled header (visual grouping)
            AppendMenu(hMenu, MF_STRING | MF_GRAYED, 0, categories[c].szName);
            
            // Add templates in this category directly to root menu
            for (int t = 0; t < categories[c].count; t++) {
                int templateIndex = categories[c].templateIndices[t];
                
                // Build menu text with description if enabled
                WCHAR szMenuText[MAX_TEMPLATE_NAME + MAX_TEMPLATE_DESC + 4];
                wcscpy(szMenuText, g_Templates[templateIndex].szLocalizedName);
                
                if (g_bShowMenuDescriptions && g_Templates[templateIndex].szLocalizedDescription[0] != L'\0') {
                    wcscat(szMenuText, L": ");
                    wcscat(szMenuText, g_Templates[templateIndex].szLocalizedDescription);
                }
                
                AppendMenu(hMenu, MF_STRING, ID_TOOLS_TEMPLATE_BASE + templateIndex, szMenuText);
            }
            
            // Add separator after category (except after last one if no uncategorized templates)
            if (c < categoryCount - 1 || uncategorizedCount > 0) {
                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            }
        }
    }
    
    // For Tools menu only: Add separator if we have both categorized and uncategorized templates
    // (Popup menu already handled separators in the loop above)
    if (bForToolsMenu && categoryCount > 0 && uncategorizedCount > 0) {
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    }
    
    // Add uncategorized templates at root level (below categories)
    for (int i = 0; i < uncategorizedCount; i++) {
        int templateIndex = uncategorizedTemplates[i];
        
        // Build menu text with description if enabled
        WCHAR szMenuText[MAX_TEMPLATE_NAME + MAX_TEMPLATE_DESC + 4];
        wcscpy(szMenuText, g_Templates[templateIndex].szLocalizedName);
        
        if (g_bShowMenuDescriptions && g_Templates[templateIndex].szLocalizedDescription[0] != L'\0') {
            wcscat(szMenuText, L": ");
            wcscat(szMenuText, g_Templates[templateIndex].szLocalizedDescription);
        }
        
        AppendMenu(hMenu, MF_STRING, ID_TOOLS_TEMPLATE_BASE + templateIndex, szMenuText);
    }
    
    return TRUE;
}

//============================================================================
// ShowTemplatePickerMenu - Show template picker popup menu at cursor (Ctrl+Shift+T)
// Creates a popup menu with templates filtered by file type and shows it at cursor position
//============================================================================
void ShowTemplatePickerMenu(HWND hwnd)
{
    // Block in read-only mode
    if (g_bReadOnly) return;
    
    if (g_nTemplateCount == 0) {
        // No templates configured
        WCHAR szNoTemplates[64];
        LoadString(GetModuleHandle(NULL), IDS_NO_TEMPLATES, szNoTemplates, 64);
        MessageBox(hwnd, szNoTemplates, L"RichEditor", MB_ICONINFORMATION);
        return;
    }
    
    // Create popup menu
    HMENU hPopupMenu = CreatePopupMenu();
    if (!hPopupMenu) return;
    
    // Populate menu with templates (FALSE = popup menu, not Tools menu)
    BOOL bHasTemplates = PopulateTemplateMenu(hPopupMenu, FALSE);
    
    // If no templates match current file type, show message
    if (!bHasTemplates) {
        DestroyMenu(hPopupMenu);
        WCHAR szNoTemplatesForType[64];
        LoadString(GetModuleHandle(NULL), IDS_NO_TEMPLATES_FOR_FILETYPE, szNoTemplatesForType, 64);
        MessageBox(hwnd, szNoTemplatesForType, L"RichEditor", MB_ICONINFORMATION);
        return;
    }
    
    // Get cursor position in RichEdit
    CHARRANGE cr;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    POINTL ptlEdit = {0, 0};
    SendMessage(g_hWndEdit, EM_POSFROMCHAR, (WPARAM)&ptlEdit, cr.cpMin);
    
    // Convert to screen coordinates
    POINT ptScreen = {ptlEdit.x, ptlEdit.y};
    ClientToScreen(g_hWndEdit, &ptScreen);
    
    // Show popup menu at cursor position
    TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, 
                   ptScreen.x, ptScreen.y, 0, hwnd, NULL);
    
    // Cleanup
    DestroyMenu(hPopupMenu);
}

//============================================================================
// BuildTemplateMenu - Build dynamic template menu with categories
// Similar to BuildFilterMenu but for templates
//============================================================================
void BuildTemplateMenu(HWND hwnd)
{
    HMENU hMenu = GetMenu(hwnd);
    if (!hMenu) return;
    
    // Find Tools menu
    int toolsMenuPos = -1;
    int menuCount = GetMenuItemCount(hMenu);
    for (int i = 0; i < menuCount; i++) {
        HMENU hSubMenu = GetSubMenu(hMenu, i);
        if (hSubMenu) {
            int subItemCount = GetMenuItemCount(hSubMenu);
            for (int j = 0; j < subItemCount; j++) {
                if (GetMenuItemID(hSubMenu, j) == ID_TOOLS_EXECUTEFILTER) {
                    toolsMenuPos = i;
                    break;
                }
            }
            if (toolsMenuPos != -1) break;
        }
    }
    
    if (toolsMenuPos == -1) return;
    
    HMENU hToolsMenu = GetSubMenu(hMenu, toolsMenuPos);
    if (!hToolsMenu) return;
    
    // Find or create "Insert Template" submenu
    // It should be the second submenu (after Select Filter)
    int insertTemplatePos = -1;
    int foundSubmenus = 0;
    int toolsItemCount = GetMenuItemCount(hToolsMenu);
    
    for (int i = 0; i < toolsItemCount; i++) {
        HMENU hSubMenu = GetSubMenu(hToolsMenu, i);
        if (hSubMenu) {
            foundSubmenus++;
            if (foundSubmenus == 2) {  // Second submenu is Insert Template
                insertTemplatePos = i;
                break;
            }
        }
    }
    
    HMENU hTemplateMenu;
    
    if (insertTemplatePos == -1) {
        // Create new "Insert Template" submenu if not found
        hTemplateMenu = CreatePopupMenu();
        
        // Insert after Select Filter submenu (typically position 1 or 2)
        // Find position after first submenu
        int insertPos = 0;
        for (int i = 0; i < toolsItemCount; i++) {
            if (GetSubMenu(hToolsMenu, i)) {
                insertPos = i + 1;
                break;
            }
        }
        
        WCHAR szInsertTemplate[64];
        LoadString(GetModuleHandle(NULL), IDS_MENU_INSERT_TEMPLATE, szInsertTemplate, 64);
        
        InsertMenu(hToolsMenu, insertPos, MF_BYPOSITION | MF_STRING | MF_POPUP, 
                   (UINT_PTR)hTemplateMenu, szInsertTemplate);
    } else {
        hTemplateMenu = GetSubMenu(hToolsMenu, insertTemplatePos);
        if (!hTemplateMenu) return;
        
        // Clear existing items
        while (GetMenuItemCount(hTemplateMenu) > 0) {
            DeleteMenu(hTemplateMenu, 0, MF_BYPOSITION);
        }
    }
    
    // Add templates or "No templates" message
    if (g_nTemplateCount == 0) {
        WCHAR szNoTemplates[64];
        LoadString(GetModuleHandle(NULL), IDS_NO_TEMPLATES, szNoTemplates, 64);
        AppendMenu(hTemplateMenu, MF_STRING | MF_GRAYED, ID_TOOLS_TEMPLATE_BASE, szNoTemplates);
    } else {
        // Populate menu with templates (TRUE = Tools menu, handles "no templates" message)
        PopulateTemplateMenu(hTemplateMenu, TRUE);
    }
    
    DrawMenuBar(hwnd);
}

//============================================================================
// BuildFileNewMenu - Build dynamic File→New submenu with template file types
//============================================================================
void BuildFileNewMenu(HWND hwnd)
{
    HMENU hMenu = GetMenu(hwnd);
    if (!hMenu) return;
    
    // Find File menu (first menu)
    HMENU hFileMenu = GetSubMenu(hMenu, 0);
    if (!hFileMenu) return;
    
    // Find or convert "New" menu item to submenu
    // Look for ID_FILE_NEW in File menu
    int newItemPos = -1;
    int fileItemCount = GetMenuItemCount(hFileMenu);
    
    for (int i = 0; i < fileItemCount; i++) {
        if (GetMenuItemID(hFileMenu, i) == ID_FILE_NEW) {
            newItemPos = i;
            break;
        }
    }
    
    if (newItemPos == -1) return;
    
    // Check if it's already a submenu
    HMENU hNewMenu = GetSubMenu(hFileMenu, newItemPos);
    
    if (!hNewMenu) {
        // Convert from menu item to submenu
        hNewMenu = CreatePopupMenu();
        
        // Remove old "New" item
        DeleteMenu(hFileMenu, newItemPos, MF_BYPOSITION);
        
        // Insert "New" submenu at same position with localized text
        WCHAR szNew[64];
        LoadString(GetModuleHandle(NULL), IDS_MENU_NEW, szNew, 64);
        InsertMenu(hFileMenu, newItemPos, MF_BYPOSITION | MF_STRING | MF_POPUP,
                   (UINT_PTR)hNewMenu, szNew);
    } else {
        // Clear existing submenu items
        while (GetMenuItemCount(hNewMenu) > 0) {
            DeleteMenu(hNewMenu, 0, MF_BYPOSITION);
        }
    }
    
    // Add "Blank Document" as first item (default behavior)
    WCHAR szBlankDoc[64];
    LoadString(GetModuleHandle(NULL), IDS_BLANK_DOCUMENT, szBlankDoc, 64);
    WCHAR szBlankMenuItem[128];
    wcscpy(szBlankMenuItem, L"&");
    wcscat(szBlankMenuItem, szBlankDoc);
    wcscat(szBlankMenuItem, L"\tCtrl+N");
    AppendMenu(hNewMenu, MF_STRING, ID_FILE_NEW_BLANK, szBlankMenuItem);
    
    // Add separator
    AppendMenu(hNewMenu, MF_SEPARATOR, 0, NULL);
    
    // Group templates by file type (extension)
    if (g_nTemplateCount > 0) {
        // Build file type map
        struct FileTypeTemplateInfo {
            WCHAR szExtension[MAX_TEMPLATE_FILEEXT];
            WCHAR szTypeName[64];  // Display name like "Markdown Document"
            int templateIndices[MAX_TEMPLATES];
            int count;
        };
        FileTypeTemplateInfo fileTypes[32];
        int fileTypeCount = 0;
        
        // Group templates by file extension
        for (int i = 0; i < g_nTemplateCount; i++) {
            // Skip templates without file extension (universal templates)
            if (g_Templates[i].szFileExtension[0] == L'\0') {
                continue;
            }
            
            // Skip unknown extensions (only show known types: md, txt, html, htm)
            BOOL isKnownExtension = FALSE;
            if (_wcsicmp(g_Templates[i].szFileExtension, L"md") == 0 ||
                _wcsicmp(g_Templates[i].szFileExtension, L"txt") == 0 ||
                _wcsicmp(g_Templates[i].szFileExtension, L"html") == 0 ||
                _wcsicmp(g_Templates[i].szFileExtension, L"htm") == 0) {
                isKnownExtension = TRUE;
            }
            
            if (!isKnownExtension) {
                continue;  // Skip this template for File→New menu
            }
            
            // Find or create file type
            int typeIndex = -1;
            for (int t = 0; t < fileTypeCount; t++) {
                if (_wcsicmp(fileTypes[t].szExtension, g_Templates[i].szFileExtension) == 0) {
                    typeIndex = t;
                    break;
                }
            }
            
            if (typeIndex == -1) {
                // Create new file type entry
                typeIndex = fileTypeCount++;
                wcscpy(fileTypes[typeIndex].szExtension, g_Templates[i].szFileExtension);
                
                // Generate display name based on extension (only known types reach here)
                if (_wcsicmp(g_Templates[i].szFileExtension, L"md") == 0) {
                    LoadString(GetModuleHandle(NULL), IDS_MARKDOWN_DOCUMENT, fileTypes[typeIndex].szTypeName, 64);
                } else if (_wcsicmp(g_Templates[i].szFileExtension, L"txt") == 0) {
                    LoadString(GetModuleHandle(NULL), IDS_TEXT_DOCUMENT, fileTypes[typeIndex].szTypeName, 64);
                } else if (_wcsicmp(g_Templates[i].szFileExtension, L"html") == 0 || 
                           _wcsicmp(g_Templates[i].szFileExtension, L"htm") == 0) {
                    LoadString(GetModuleHandle(NULL), IDS_HTML_DOCUMENT, fileTypes[typeIndex].szTypeName, 64);
                }
                // Note: No fallback needed - unknown extensions filtered earlier
                
                fileTypes[typeIndex].count = 0;
            }
            
            fileTypes[typeIndex].templateIndices[fileTypes[typeIndex].count++] = i;
        }
        
        // Add file type menu items
        // For each file type, add ONE menu item that creates file with first template
        for (int t = 0; t < fileTypeCount; t++) {
            // Use first template of this type for File→New
            int templateIndex = fileTypes[t].templateIndices[0];
            
            // Menu text: "Markdown Document"
            AppendMenu(hNewMenu, MF_STRING, 
                      ID_FILE_NEW_TEMPLATE_BASE + templateIndex,
                      fileTypes[t].szTypeName);
        }
    }
    
    DrawMenuBar(hwnd);
}

//============================================================================
// Search Functions (Phase 2.9)
//============================================================================

//============================================================================
// FindTextInDocument - Search for text in RichEdit control
// Returns: Character position of match, or -1 if not found
//============================================================================
LONG FindTextInDocument(LPCWSTR pszSearchText, BOOL bMatchCase, BOOL bWholeWord, 
                       BOOL bSearchDown, LONG nStartPos)
{
    if (!pszSearchText || !pszSearchText[0]) {
        return -1;
    }
    
    // Get document length
    GETTEXTLENGTHEX gtl = {GTL_DEFAULT, 1200};  // UTF-16LE
    int nDocLen = (int)SendMessage(g_hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    
    // Setup search range and flags
    FINDTEXTEXW ft;
    DWORD dwFlags = 0;
    
    if (bMatchCase) dwFlags |= FR_MATCHCASE;
    if (bWholeWord) dwFlags |= FR_WHOLEWORD;
    if (bSearchDown) dwFlags |= FR_DOWN;
    
    // Set search range
    if (bSearchDown) {
        ft.chrg.cpMin = nStartPos;
        ft.chrg.cpMax = nDocLen;
    } else {
        ft.chrg.cpMin = nStartPos;
        ft.chrg.cpMax = 0;
    }
    
    ft.lpstrText = pszSearchText;
    
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
            // Position cursor based on search direction
            if (bSearchDown) {
                // Forward search: position cursor after match (allows F3 to continue forward)
                cr.cpMin = cr.cpMax;
            } else {
                // Backward search: position cursor before match (allows Shift+F3 to continue backward)
                cr.cpMax = cr.cpMin;
            }
            SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        }
        
        // Scroll into view
        SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
    }
    
    return nPos;
}

//============================================================================
// DoFind - Perform find operation with current settings
// Called by Find Next/Previous buttons and F3/Shift+F3 shortcuts
// Returns: TRUE if found, FALSE if not found
//============================================================================
BOOL DoFind(BOOL bSearchDown, BOOL bSilent)
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

    AddToFindHistory(g_szFindWhat);
    SaveFindHistory();
    
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
        // Not found - show message (unless caller requested silent mode)
        if (!bSilent) {
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
        }
        return FALSE;
    }
    
    return TRUE;
}

//============================================================================
// DlgGotoProc - Go to Line dialog procedure (modal)
//============================================================================
INT_PTR CALLBACK DlgGotoProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;  // Unused parameter
    switch (message) {
        case WM_INITDIALOG:
        {
            LONG nCurrentLine = SendMessage(g_hWndEdit, EM_LINEFROMCHAR, -1, 0);
            LONG nTotalLines = SendMessage(g_hWndEdit, EM_GETLINECOUNT, 0, 0);

            WCHAR szLabel[128];
            WCHAR szTemplate[128];
            LoadStringResource(IDS_GOTO_LABEL, szTemplate, 128);

            WCHAR szNumber[16];
            _snwprintf(szNumber, 16, L"%ld", nTotalLines);
            szNumber[15] = L'\0';

            // Build label text without swprintf (MinGW-safe)
            // Replace %d with number manually
            const WCHAR* pTemplate = szTemplate;
            WCHAR* pOut = szLabel;
            size_t remaining = sizeof(szLabel) / sizeof(szLabel[0]);
            while (*pTemplate && remaining > 1) {
                if (pTemplate[0] == L'%' && pTemplate[1] == L'd') {
                    const WCHAR* pNum = szNumber;
                    while (*pNum && remaining > 1) {
                        *pOut++ = *pNum++;
                        remaining--;
                    }
                    pTemplate += 2;
                    continue;
                }
                *pOut++ = *pTemplate++;
                remaining--;
            }
            *pOut = L'\0';

            SetDlgItemText(hDlg, IDC_GOTO_LABEL, szLabel);
            SetDlgItemInt(hDlg, IDC_GOTO_LINE, nCurrentLine + 1, FALSE);
            SendDlgItemMessage(hDlg, IDC_GOTO_LINE, EM_SETSEL, 0, -1);
            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                BOOL bTranslated = FALSE;
                UINT nLine = GetDlgItemInt(hDlg, IDC_GOTO_LINE, &bTranslated, FALSE);
                if (!bTranslated || nLine < 1) {
                    WCHAR szMsg[128], szTitle[64];
                    LoadStringResource(IDS_GOTO_INVALID_LINE, szMsg, 128);
                    LoadStringResource(IDS_GOTO_TITLE, szTitle, 64);
                    MessageBox(hDlg, szMsg, szTitle, MB_ICONEXCLAMATION);
                    return TRUE;
                }

                LONG nTotalLines = SendMessage(g_hWndEdit, EM_GETLINECOUNT, 0, 0);
                if (nLine > (UINT)nTotalLines) {
                    WCHAR szMsg[128], szTitle[64];
                    LoadStringResource(IDS_GOTO_INVALID_LINE, szMsg, 128);
                    LoadStringResource(IDS_GOTO_TITLE, szTitle, 64);
                    MessageBox(hDlg, szMsg, szTitle, MB_ICONEXCLAMATION);
                    return TRUE;
                }

                LONG nCharPos = SendMessage(g_hWndEdit, EM_LINEINDEX, nLine - 1, 0);
                if (nCharPos < 0) {
                    WCHAR szMsg[128], szTitle[64];
                    LoadStringResource(IDS_GOTO_INVALID_LINE, szMsg, 128);
                    LoadStringResource(IDS_GOTO_TITLE, szTitle, 64);
                    MessageBox(hDlg, szMsg, szTitle, MB_ICONEXCLAMATION);
                    return TRUE;
                }

                CHARRANGE cr;
                cr.cpMin = nCharPos;
                cr.cpMax = nCharPos;
                SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
                SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
                SetFocus(g_hWndEdit);

                EndDialog(hDlg, IDOK);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            break;
    }

    return FALSE;
}

//============================================================================
// DlgFindProc - Find dialog procedure (modeless)
//============================================================================
INT_PTR CALLBACK DlgFindProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;  // Unused parameter
    switch (message) {
        case WM_INITDIALOG:
        {
            // Load Find history into combo box
            HWND hFindCombo = GetDlgItem(hDlg, IDC_FIND_WHAT);
            for (int i = 0; i < g_nFindHistoryCount; i++) {
                SendMessage(hFindCombo, CB_ADDSTRING, 0, (LPARAM)g_szFindHistory[i]);
            }
            
            // If no current search term, use most recent from history
            if (g_szFindWhat[0] == L'\0' && g_nFindHistoryCount > 0) {
                wcscpy(g_szFindWhat, g_szFindHistory[0]);
            }
            
            // Set current search term (or most recent from history)
            SetDlgItemText(hDlg, IDC_FIND_WHAT, g_szFindWhat);
            
            // Load Replace history into combo box
            HWND hReplaceCombo = GetDlgItem(hDlg, IDC_REPLACE_WITH);
            for (int i = 0; i < g_nReplaceHistoryCount; i++) {
                SendMessage(hReplaceCombo, CB_ADDSTRING, 0, (LPARAM)g_szReplaceHistory[i]);
            }
            
            // Set current replace text
            if (g_szReplaceWith[0] != L'\0') {
                SetDlgItemText(hDlg, IDC_REPLACE_WITH, g_szReplaceWith);
            }
            
            // Set checkbox states (restored from saved preferences)
            CheckDlgButton(hDlg, IDC_MATCH_CASE, g_bFindMatchCase ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_WHOLE_WORD, g_bFindWholeWord ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_USE_ESCAPES, g_bFindUseEscapes ? BST_CHECKED : BST_UNCHECKED);
            
            // Set dialog mode (Find or Replace)
            UpdateDialogMode(hDlg, g_bReplaceMode);
            
            // Focus on search box and select all text (for easy overtyping)
            SetFocus(hFindCombo);
            SendMessage(hFindCombo, CB_SETEDITSEL, 0, MAKELPARAM(0, -1));  // Select all
            
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
                    SaveFindOptions();
                    
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
                    SaveFindOptions();
                    
                    // Perform search
                    DoFind(FALSE);  // Search up
                    return TRUE;
                }
                
                case IDC_REPLACE_BTN:
                {
                    // Get search and replace terms
                    GetDlgItemText(hDlg, IDC_FIND_WHAT, g_szFindWhat, MAX_SEARCH_TEXT);
                    GetDlgItemText(hDlg, IDC_REPLACE_WITH, g_szReplaceWith, MAX_SEARCH_TEXT);
                    
                    // Get checkbox states
                    g_bFindMatchCase = (IsDlgButtonChecked(hDlg, IDC_MATCH_CASE) == BST_CHECKED);
                    g_bFindWholeWord = (IsDlgButtonChecked(hDlg, IDC_WHOLE_WORD) == BST_CHECKED);
                    g_bFindUseEscapes = (IsDlgButtonChecked(hDlg, IDC_USE_ESCAPES) == BST_CHECKED);
                    SaveFindOptions();
                    
                    // Perform replace
                    DoReplace();
                    return TRUE;
                }
                
                case IDC_REPLACE_ALL_BTN:
                {
                    // Get search and replace terms
                    GetDlgItemText(hDlg, IDC_FIND_WHAT, g_szFindWhat, MAX_SEARCH_TEXT);
                    GetDlgItemText(hDlg, IDC_REPLACE_WITH, g_szReplaceWith, MAX_SEARCH_TEXT);
                    
                    // Get checkbox states
                    g_bFindMatchCase = (IsDlgButtonChecked(hDlg, IDC_MATCH_CASE) == BST_CHECKED);
                    g_bFindWholeWord = (IsDlgButtonChecked(hDlg, IDC_WHOLE_WORD) == BST_CHECKED);
                    g_bFindUseEscapes = (IsDlgButtonChecked(hDlg, IDC_USE_ESCAPES) == BST_CHECKED);
                    SaveFindOptions();
                    
                    // Perform replace all
                    DoReplaceAll();
                    return TRUE;
                }
                
                case IDC_CLOSE_BTN:
                case IDCANCEL:
                    ShowWindow(hDlg, SW_HIDE);  // Hide instead of destroy (modeless)
                    return TRUE;
                
                case IDC_MATCH_CASE:
                case IDC_WHOLE_WORD:
                case IDC_USE_ESCAPES:
                    g_bFindMatchCase = (IsDlgButtonChecked(hDlg, IDC_MATCH_CASE) == BST_CHECKED);
                    g_bFindWholeWord = (IsDlgButtonChecked(hDlg, IDC_WHOLE_WORD) == BST_CHECKED);
                    g_bFindUseEscapes = (IsDlgButtonChecked(hDlg, IDC_USE_ESCAPES) == BST_CHECKED);
                    SaveFindOptions();
                    return TRUE;
            }
            break;
        
        case WM_CLOSE:
            ShowWindow(hDlg, SW_HIDE);  // Hide instead of destroy
            return TRUE;
    }
    
    return FALSE;
}

//============================================================================
// LoadFindHistory - Load find history from INI file
//============================================================================
void LoadFindHistory()
{
    g_nFindHistoryCount = LoadHistoryList(L"FindHistory", g_szFindHistory, MAX_FIND_HISTORY);
}

//============================================================================
// SaveFindHistory - Save find history to INI file
//============================================================================
void SaveFindHistory()
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    std::wstring historySection;
    BuildHistorySection(historySection, L"FindHistory", g_szFindHistory, g_nFindHistoryCount);
    ReplaceINISection(szIniPath, L"FindHistory", historySection);
}

void SaveFindOptions()
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
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

//============================================================================
// Replace Functions (Phase 2.9.2)
//============================================================================

void UpdateDialogMode(HWND hDlg, BOOL bReplaceMode)
{
    // Get control handles
    HWND hReplaceLabel = GetDlgItem(hDlg, IDC_REPLACE_WITH_LABEL);
    HWND hReplaceCombo = GetDlgItem(hDlg, IDC_REPLACE_WITH);
    HWND hReplaceBtn = GetDlgItem(hDlg, IDC_REPLACE_BTN);
    HWND hReplaceAllBtn = GetDlgItem(hDlg, IDC_REPLACE_ALL_BTN);
    
    // Show/hide Replace controls
    int nShow = bReplaceMode ? SW_SHOW : SW_HIDE;
    ShowWindow(hReplaceLabel, nShow);
    ShowWindow(hReplaceCombo, nShow);
    ShowWindow(hReplaceBtn, nShow);
    ShowWindow(hReplaceAllBtn, nShow);
    
    // Update dialog title
    WCHAR szTitle[64];
    if (bReplaceMode) {
        LoadString(GetModuleHandle(NULL), IDS_FIND_REPLACE_TITLE, szTitle, 64);
    } else {
        LoadString(GetModuleHandle(NULL), IDS_FIND_TITLE, szTitle, 64);
    }
    SetWindowText(hDlg, szTitle);
    
    // Disable Replace buttons in read-only mode
    if (bReplaceMode && g_bReadOnly) {
        EnableWindow(hReplaceBtn, FALSE);
        EnableWindow(hReplaceAllBtn, FALSE);
    }
    
    // Update global state
    g_bReplaceMode = bReplaceMode;
}

LPWSTR ExpandReplacePlaceholder(LPCWSTR pszReplace, LPCWSTR pszMatched)
{
    if (!pszReplace) return NULL;
    
    size_t nReplaceLen = wcslen(pszReplace);
    size_t nMatchedLen = pszMatched ? wcslen(pszMatched) : 0;
    
    // Calculate worst-case buffer size
    size_t nMaxLen = nReplaceLen * 2 + nMatchedLen * 10 + 1;
    LPWSTR pszResult = (LPWSTR)malloc(nMaxLen * sizeof(WCHAR));
    if (!pszResult) return NULL;
    
    const WCHAR *pSrc = pszReplace;
    WCHAR *pDst = pszResult;
    
    while (*pSrc) {
        if (*pSrc == L'%') {
            if (*(pSrc + 1) == L'0') {
                // %0 → Insert matched text
                if (pszMatched) {
                    wcscpy(pDst, pszMatched);
                    pDst += nMatchedLen;
                }
                pSrc += 2;
            }
            else if (*(pSrc + 1) == L'%') {
                // %% → Insert single %
                *pDst++ = L'%';
                pSrc += 2;
            }
            else {
                // Unknown placeholder → Copy literally
                *pDst++ = *pSrc++;
            }
        }
        else {
            *pDst++ = *pSrc++;
        }
    }
    
    *pDst = L'\0';
    return pszResult;  // Caller must free!
}

void DoReplace()
{
    // Read-only protection
    if (g_bReadOnly) return;
    
    if (!g_hDlgFind) return;
    
    // Get Find/Replace text from dialog
    GetDlgItemText(g_hDlgFind, IDC_FIND_WHAT, g_szFindWhat, MAX_SEARCH_TEXT);
    GetDlgItemText(g_hDlgFind, IDC_REPLACE_WITH, g_szReplaceWith, MAX_SEARCH_TEXT);
    
    if (g_szFindWhat[0] == L'\0') return;  // Empty find text
    
    // Get current selection
    CHARRANGE cr;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    
    // If no selection, find first occurrence then replace it immediately.
    // Without this, the first press would only locate the match and the
    // second press would replace — requiring two presses for one replace.
    if (cr.cpMax - cr.cpMin <= 0) {
        DoFind(TRUE);  // Search down — selects the match if found
        // If DoFind found and selected a match, replace it right away so
        // that a single button press both replaces and advances to next.
        CHARRANGE crFound;
        SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&crFound);
        if (crFound.cpMax - crFound.cpMin > 0) {
            DoReplace();  // Selection now matches; replaces + finds next
        }
        return;
    }
    
    // Get selected text
    int nSelLen = cr.cpMax - cr.cpMin;
    LPWSTR pszSelection = (LPWSTR)malloc((nSelLen + 1) * sizeof(WCHAR));
    if (!pszSelection) return;
    
    TEXTRANGEW tr;
    tr.chrg = cr;
    tr.lpstrText = pszSelection;
    SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    
    // Parse Find What (apply escape sequences if enabled)
    LPWSTR pszFindParsed = g_bFindUseEscapes 
        ? ParseEscapeSequences(g_szFindWhat)
        : _wcsdup(g_szFindWhat);
    
    if (!pszFindParsed) {
        free(pszSelection);
        return;
    }
    
    // Check if selection matches search text
    BOOL bMatches = FALSE;
    if (g_bFindMatchCase) {
        bMatches = (wcscmp(pszSelection, pszFindParsed) == 0);
    } else {
        bMatches = (_wcsicmp(pszSelection, pszFindParsed) == 0);
    }
    
    if (!bMatches) {
        // Selection doesn't match - find next occurrence
        free(pszSelection);
        free(pszFindParsed);
        DoFind(TRUE);  // Search down
        return;
    }
    
    // Selection matches - perform replacement
    
    // Parse Replace With (apply escape sequences if enabled)
    LPWSTR pszReplaceParsed = g_bFindUseEscapes 
        ? ParseEscapeSequences(g_szReplaceWith)
        : _wcsdup(g_szReplaceWith);
    
    if (!pszReplaceParsed) {
        free(pszSelection);
        free(pszFindParsed);
        return;
    }
    
    // Expand placeholders (%0 = matched text, %% = %)
    LPWSTR pszReplaceExpanded = ExpandReplacePlaceholder(pszReplaceParsed, pszSelection);
    
    if (!pszReplaceExpanded) {
        free(pszSelection);
        free(pszFindParsed);
        free(pszReplaceParsed);
        return;
    }
    
    // Replace selected text
    SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszReplaceExpanded);
    
    // Mark modified
    g_bModified = TRUE;
    UpdateTitle();
    
    // Cleanup
    free(pszSelection);
    free(pszFindParsed);
    free(pszReplaceParsed);
    free(pszReplaceExpanded);
    
    // Add to history
    AddToReplaceHistory(g_szReplaceWith);
    SaveReplaceHistory();
    
    // Find next occurrence; silent if nothing remains — the replacement already
    // succeeded and announcing "Cannot find" in the same press is misleading.
    // The user can press Replace once more to be told there is no next match.
    DoFind(TRUE, TRUE);  // Search down, suppress not-found notification
}

//============================================================================
// Word Boundary Detection
//============================================================================

// Check if character is a word character (alphanumeric or underscore)
// Used for whole-word matching - matches standard regex \b behavior
// Locale-aware via iswalnum() (handles Czech, etc.)
inline BOOL IsWordCharacter(WCHAR ch)
{
    return iswalnum(ch) || ch == L'_';
}

//============================================================================
// Replace All Implementation
//============================================================================

void DoReplaceAll()
{
    // Read-only protection
    if (g_bReadOnly) return;
    
    if (!g_hDlgFind) return;
    
    // Get Find/Replace text from dialog
    GetDlgItemText(g_hDlgFind, IDC_FIND_WHAT, g_szFindWhat, MAX_SEARCH_TEXT);
    GetDlgItemText(g_hDlgFind, IDC_REPLACE_WITH, g_szReplaceWith, MAX_SEARCH_TEXT);
    
    if (g_szFindWhat[0] == L'\0') return;  // Empty find text
    
    // Get checkbox states
    g_bFindMatchCase = (IsDlgButtonChecked(g_hDlgFind, IDC_MATCH_CASE) == BST_CHECKED);
    g_bFindWholeWord = (IsDlgButtonChecked(g_hDlgFind, IDC_WHOLE_WORD) == BST_CHECKED);
    g_bFindUseEscapes = (IsDlgButtonChecked(g_hDlgFind, IDC_USE_ESCAPES) == BST_CHECKED);
    
    // Parse escape sequences (once for efficiency)
    LPWSTR pszFindParsed = g_bFindUseEscapes 
        ? ParseEscapeSequences(g_szFindWhat)
        : _wcsdup(g_szFindWhat);
    
    if (!pszFindParsed) return;
    
    LPWSTR pszReplaceParsed = g_bFindUseEscapes 
        ? ParseEscapeSequences(g_szReplaceWith)
        : _wcsdup(g_szReplaceWith);
    
    if (!pszReplaceParsed) {
        free(pszFindParsed);
        return;
    }
    
    // Expand placeholder using search text (all matches are identical in literal search)
    LPWSTR pszReplaceExpanded = ExpandReplacePlaceholder(pszReplaceParsed, pszFindParsed);
    
    if (!pszReplaceExpanded) {
        free(pszFindParsed);
        free(pszReplaceParsed);
        return;
    }
    
    // Unified fast in-memory replacement with optional whole-word checking
    // Get all text from RichEdit
    GETTEXTLENGTHEX gtl = {GTL_DEFAULT, 1200};  // UTF-16LE
    int nLen = SendMessage(g_hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    
    if (nLen <= 0) {
        free(pszFindParsed);
        free(pszReplaceParsed);
        free(pszReplaceExpanded);
        return;
    }
    
    WCHAR *pszOriginal = (WCHAR*)malloc((nLen + 1) * sizeof(WCHAR));
    if (!pszOriginal) {
        free(pszFindParsed);
        free(pszReplaceParsed);
        free(pszReplaceExpanded);
        return;
    }
    
    GETTEXTEX gt;
    gt.cb = (nLen + 1) * sizeof(WCHAR);
    gt.flags = GTL_DEFAULT;
    gt.codepage = 1200;  // UTF-16LE
    gt.lpDefaultChar = NULL;
    gt.lpUsedDefChar = NULL;
    SendMessage(g_hWndEdit, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)pszOriginal);
    
    // Do in-memory replacement
    size_t nFindLen = wcslen(pszFindParsed);
    size_t nReplaceLen = wcslen(pszReplaceExpanded);
    
    // Safety check: prevent division by zero
    if (nFindLen == 0) {
        free(pszOriginal);
        free(pszFindParsed);
        free(pszReplaceParsed);
        free(pszReplaceExpanded);
        return;
    }
    
    // Allocate result buffer (worst case: all text becomes replacement)
    size_t nMaxResult = nLen * nReplaceLen / nFindLen + nLen + 1000;
    WCHAR *pszResult = (WCHAR*)malloc(nMaxResult * sizeof(WCHAR));
    if (!pszResult) {
        free(pszOriginal);
        free(pszFindParsed);
        free(pszReplaceParsed);
        free(pszReplaceExpanded);
        return;
    }
    
    WCHAR *pSrc = pszOriginal;
    WCHAR *pDst = pszResult;
    int nReplacedCount = 0;
    
    while (*pSrc) {
        BOOL bMatch = FALSE;
        
        // Check for string match
        if (g_bFindMatchCase) {
            bMatch = (wcsncmp(pSrc, pszFindParsed, nFindLen) == 0);
        } else {
            bMatch = (_wcsnicmp(pSrc, pszFindParsed, nFindLen) == 0);
        }
        
        // If matched and whole-word mode is enabled, check word boundaries
        if (bMatch && g_bFindWholeWord) {
            // Check character BEFORE match
            if (pSrc > pszOriginal) {  // Not at document start
                if (IsWordCharacter(*(pSrc - 1))) {
                    bMatch = FALSE;  // Preceded by word character - not a whole word
                }
            }
            
            // Check character AFTER match (only if still matched)
            if (bMatch && *(pSrc + nFindLen) != L'\0') {  // Not at document end
                if (IsWordCharacter(*(pSrc + nFindLen))) {
                    bMatch = FALSE;  // Followed by word character - not a whole word
                }
            }
        }
        
        if (bMatch) {
            // Match found and passed whole-word check (if enabled)
            wcscpy(pDst, pszReplaceExpanded);
            pDst += nReplaceLen;
            pSrc += nFindLen;
            nReplacedCount++;
        } else {
            // No match or failed whole-word check - copy original character
            *pDst++ = *pSrc++;
        }
    }
    *pDst = L'\0';
    
    if (nReplacedCount > 0) {
        // Replace entire text content using EM_SETTEXTEX (single undo operation!)
        SendMessage(g_hWndEdit, WM_SETREDRAW, FALSE, 0);
        
        // Select all text first for proper undo support
        SendMessage(g_hWndEdit, EM_SETSEL, 0, -1);
        
        SETTEXTEX st;
        st.flags = ST_SELECTION | ST_KEEPUNDO;  // Replace selection with undo support
        st.codepage = 1200;  // UTF-16LE
        SendMessage(g_hWndEdit, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)pszResult);
        
        SendMessage(g_hWndEdit, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(g_hWndEdit, NULL, TRUE);
        
        g_bModified = TRUE;
        UpdateTitle();
        AddToFindHistory(g_szFindWhat);
        SaveFindHistory();
        AddToReplaceHistory(g_szReplaceWith);
        SaveReplaceHistory();
        
        // Mark as Replace operation for undo menu display
        g_bLastOperationWasReplace = TRUE;
        
        // Show completion message using localized format string
        WCHAR szMsgFormat[128];
        LoadString(GetModuleHandle(NULL), IDS_REPLACE_COMPLETE_MSG, szMsgFormat, 128);
        
        WCHAR szMsg[256];
        WCHAR szCount[32];
        _itow(nReplacedCount, szCount, 10);
        
        wcscpy(szMsg, L"");
        WCHAR *pFormat = wcsstr(szMsgFormat, L"%d");
        if (pFormat) {
            size_t nBeforeLen = pFormat - szMsgFormat;
            wcsncpy(szMsg, szMsgFormat, nBeforeLen);
            szMsg[nBeforeLen] = L'\0';
            wcscat(szMsg, szCount);
            wcscat(szMsg, pFormat + 2);
        } else {
            wcscpy(szMsg, szMsgFormat);
            wcscat(szMsg, L" ");
            wcscat(szMsg, szCount);
        }
        
        WCHAR szTitle[64];
        LoadString(GetModuleHandle(NULL), IDS_REPLACE_COMPLETE_TITLE, szTitle, 64);
        
        MessageBox(g_hDlgFind, szMsg, szTitle, MB_OK | MB_ICONINFORMATION);
    } else {
        // No matches — show the same "Cannot find" message as plain Find/Replace
        WCHAR szMsg[512], szTitle[64], szPrefix[256];
        LoadStringResource(IDS_FIND_NOTFOUND_PREFIX, szPrefix, 256);
        LoadStringResource(IDS_FIND_NOTFOUND_TITLE, szTitle, 64);
        wcscpy(szMsg, szPrefix);
        wcscat(szMsg, g_szFindWhat);
        wcscat(szMsg, L"\"");
        MessageBox(g_hDlgFind ? g_hDlgFind : g_hWndMain, szMsg, szTitle, MB_ICONINFORMATION);
    }
    
    // Cleanup
    free(pszOriginal);
    free(pszResult);
    free(pszFindParsed);
    free(pszReplaceParsed);
    free(pszReplaceExpanded);
}

void LoadReplaceHistory()
{
    g_nReplaceHistoryCount = LoadHistoryList(L"ReplaceHistory", g_szReplaceHistory, MAX_FIND_HISTORY);
}

void SaveReplaceHistory()
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    std::wstring historySection;
    BuildHistorySection(historySection, L"ReplaceHistory", g_szReplaceHistory, g_nReplaceHistoryCount);
    ReplaceINISection(szIniPath, L"ReplaceHistory", historySection);
}

void AddToReplaceHistory(LPCWSTR pszText)
{
    if (!pszText || !pszText[0]) return;
    
    // Check if already in history
    for (int i = 0; i < g_nReplaceHistoryCount; i++) {
        if (wcscmp(g_szReplaceHistory[i], pszText) == 0) {
            // Already exists - move to top
            WCHAR szTemp[MAX_SEARCH_TEXT];
            wcscpy(szTemp, g_szReplaceHistory[i]);
            
            // Shift items down
            for (int j = i; j > 0; j--) {
                wcscpy(g_szReplaceHistory[j], g_szReplaceHistory[j - 1]);
            }
            
            // Put at top
            wcscpy(g_szReplaceHistory[0], szTemp);
            return;
        }
    }
    
    // Not in history - add to top
    if (g_nReplaceHistoryCount < MAX_FIND_HISTORY) {
        g_nReplaceHistoryCount++;
    }
    
    // Shift items down
    for (int i = g_nReplaceHistoryCount - 1; i > 0; i--) {
        wcscpy(g_szReplaceHistory[i], g_szReplaceHistory[i - 1]);
    }
    
    // Add new item at top
    wcscpy(g_szReplaceHistory[0], pszText);
}

//============================================================================
// WndProc - Main Window Procedure
//============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CREATE:
        {
            // Initialize state
            g_szFileName[0] = L'\0';
            g_szFileTitle[0] = L'\0';
            g_bModified = FALSE;
            wcscpy(g_szCurrentFileExtension, L"txt");  // Default to txt for Untitled files
            
            // Create RichEdit control
            g_hWndEdit = CreateRichEditControl(hwnd);
            if (!g_hWndEdit) {
                // Show detailed error with INI guidance (Phase 2.8.5)
                WCHAR szError[1024], szTemplate[900], szTitle[64];
                LoadStringResource(IDS_RICHEDIT_CREATE_FAILED_DETAIL, szTemplate, 900);
                _snwprintf(szError, 1024, szTemplate, g_szRichEditClassName);
                LoadStringResource(IDS_ERROR, szTitle, 64);
                MessageBox(hwnd, szError, szTitle, MB_ICONERROR);
                return -1;
            }
            
            // Acquire TOM ITextDocument for O(1) physical line queries in UpdateStatusBar
            {
                IRichEditOle* pRichOle = NULL;
                if (SendMessage(g_hWndEdit, EM_GETOLEINTERFACE, 0, (LPARAM)&pRichOle) && pRichOle) {
                    pRichOle->QueryInterface(IID_ITextDocument_, (void**)&g_pTextDoc);
                    pRichOle->Release();
                }
            }
            
            // Create status bar
            g_hWndStatus = CreateStatusBar(hwnd);
            
            // Update status bar
            UpdateStatusBar();
            
            // Load filters and templates (settings already loaded in wWinMain)
            LoadFilters();
            LoadTemplates();  // Load templates with keyboard shortcuts
            BuildFilterMenu(hwnd);
            BuildTemplateMenu(hwnd);  // Build template submenu
            BuildFileNewMenu(hwnd);   // Build File→New submenu
            UpdateFilterDisplay();
            UpdateMenuStates(hwnd);
            
            // Load MRU list
            LoadMRU();
            UpdateMRUMenu(hwnd);

            // Initialize bookmark state
            g_nLastTextLen = GetWindowTextLength(g_hWndEdit);
            
            // Set initial word wrap menu checkmark
            HMENU hMenu = GetMenu(hwnd);
            CheckMenuItem(hMenu, ID_VIEW_WORDWRAP, g_bWordWrap ? MF_CHECKED : MF_UNCHECKED);
            
            // Start autosave timer if enabled
            StartAutosaveTimer(hwnd);
            
            // Set initial window title with localized "Untitled"
            UpdateTitle(hwnd);
            
            return 0;
        }
            
        case WM_SIZE:
            // Resize status bar
            if (g_hWndStatus) {
                SendMessage(g_hWndStatus, WM_SIZE, 0, 0);
                
                // Update status bar parts based on new window size
                RECT rcStatus;
                GetClientRect(g_hWndStatus, &rcStatus);
                int parts[] = {rcStatus.right - 200, -1}; // Part 0 takes all but 200px, part 1 gets 200px
                SendMessage(g_hWndStatus, SB_SETPARTS, 2, (LPARAM)parts);
            }
            
            // Resize RichEdit control to fill client area minus status bar
            if (g_hWndEdit) {
                RECT rcClient, rcStatus;
                GetClientRect(hwnd, &rcClient);
                GetClientRect(g_hWndStatus, &rcStatus);
                
                SetWindowPos(g_hWndEdit, NULL,
                    0, 0,
                    rcClient.right,
                    rcClient.bottom - rcStatus.bottom,
                    SWP_NOZORDER);

                if (g_bWordWrap) {
                    ApplyWordWrap(g_hWndEdit);
                }
            }
            return 0;
            
        case WM_SETFOCUS:
            // Restore focus to edit control when window receives focus
            if (g_hWndEdit) {
                SetFocus(g_hWndEdit);
            }
            return 0;
            
        case WM_ACTIVATEAPP:
        {
            if (!wParam) { // app is being deactivated
                if (g_bAutosaveEnabled && g_bAutosaveOnFocusLoss) {
                    DoAutosave();
                }
            }
            return 0;
        }
            
        case WM_TIMER:
            // Handle autosave timer
            if (wParam == IDT_AUTOSAVE) {
                DoAutosave();
            }
            // Handle "[Autosaved]" status bar flash — restore previous text
            else if (wParam == IDT_AUTOSAVE_FLASH) {
                KillTimer(hwnd, IDT_AUTOSAVE_FLASH);
                if (g_hWndStatus)
                    SendMessage(g_hWndStatus, SB_SETTEXT, 0, (LPARAM)g_szAutosaveFlashPrevStatus);
            }
            // Handle filter status bar timer (30-second display)
            else if (wParam == IDT_FILTER_STATUSBAR) {
                KillTimer(hwnd, IDT_FILTER_STATUSBAR);
                g_bFilterStatusBarActive = FALSE;
                UpdateStatusBar();  // Revert to normal display
            }
            // Deferred focus restore after MRU load: old RichEdit reports
            // STATE_SYSTEM_UNAVAILABLE briefly after a large SetWindowText.
            // Re-firing SetFocus 200 ms later gives the control time to settle
            // so the Braille display / screen reader sees the correct state.
            else if (wParam == IDT_FOCUS_RESTORE) {
                KillTimer(hwnd, IDT_FOCUS_RESTORE);
                if (g_hWndEdit && g_hWndMain) {
                    // SetFocus(g_hWndEdit) is a no-op when focus is already
                    // there — Windows does not fire EVENT_OBJECT_FOCUS and
                    // NVDA has no reason to re-query, so the Braille display
                    // stays stuck on "unavailable".
                    // Fix: briefly hand focus to the frame window. The
                    // WM_SETFOCUS handler for g_hWndMain immediately calls
                    // SetFocus(g_hWndEdit), which IS a focus change and
                    // fires EVENT_OBJECT_FOCUS — causing NVDA to re-query
                    // the edit control (which is now settled and available).
                    SetFocus(g_hWndMain);
                    SendMessage(g_hWndEdit, EM_SETSEL, 0, 0);
                    SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
                }
            }
            return 0;
            
        case WM_COMMAND:
            // Handle edit control notifications
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_hWndEdit) {
                if (!g_bSettingText) {
                    int currentLen = GetWindowTextLength(g_hWndEdit);
                    int delta = currentLen - g_nLastTextLen;
                    CHARRANGE cr;
                    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
                    if (g_nLastTextLen > 0) {
                        LONG editPos = cr.cpMin;
                        if (delta > 0) {
                            editPos = cr.cpMin - delta;
                            if (editPos < 0) editPos = 0;
                        }
                        UpdateBookmarksAfterEdit(editPos, delta);
                    }
                    g_nLastTextLen = currentLen;
                    g_bLineIndexDirty = true;  // text changed; rebuild line index before next query

                    if (!g_bModified) {
                        g_bModified = TRUE;
                        g_lastURLRange.cpMin = -1;  // Invalidate cached URL range
                        UpdateTitle();
                    }
                }
                return 0;
            }
            
            switch (LOWORD(wParam)) {
                // File menu
                case ID_FILE_NEW:
                case ID_FILE_NEW_BLANK:
                    FileNew();
                    break;
                case ID_FILE_OPEN:
                    FileOpen();
                    break;
                case ID_FILE_READONLY:
                    g_bReadOnly = !g_bReadOnly;
                    SendMessage(g_hWndEdit, EM_SETREADONLY, g_bReadOnly, 0);
                    BuildFilterMenu(hwnd);  // Rebuild filter menu to update grayed state
                    UpdateMenuStates(hwnd);  // Update Execute Filter / Start Interactive
                    UpdateTitle();
                    return 0;
                case ID_FILE_SAVE:
                    FileSave();
                    break;
                case ID_FILE_SAVEAS:
                    FileSaveAs();
                    break;
                case ID_FILE_EXIT:
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                    break;
                    
                // Edit menu
                case ID_EDIT_UNDO:
                    EditUndo();
                    break;
                case ID_EDIT_REDO:
                    EditRedo();
                    break;
                case ID_EDIT_CUT:
                    EditCut();
                    break;
                case ID_EDIT_COPY:
                    EditCopy();
                    break;
                case ID_EDIT_PASTE:
                    EditPaste();
                    break;
                case ID_EDIT_SELECTALL:
                    EditSelectAll();
                    break;
                case ID_EDIT_TIMEDATE:
                    EditInsertTimeDate();
                    break;
                
                // View menu
                case ID_VIEW_WORDWRAP:
                    ViewWordWrap();
                    break;
                case ID_VIEW_ZOOM_RESET:
                    ViewZoomReset();
                    break;
                
                // Search menu (Phase 2.9)
                case ID_SEARCH_FIND:
                    // Close existing dialog if open (allows switching from Replace to Find)
                    if (g_hDlgFind) {
                        DestroyWindow(g_hDlgFind);
                        g_hDlgFind = NULL;
                    }
                    
                    // Create modeless Find dialog in Find mode
                    g_bReplaceMode = FALSE;
                    g_hDlgFind = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_FIND), 
                                              hwnd, DlgFindProc);
                    if (g_hDlgFind) {
                        ShowWindow(g_hDlgFind, SW_SHOW);
                    }
                    break;
                
                case ID_SEARCH_REPLACE:
                    if (g_bReadOnly) break;  // Block in read-only mode
                    
                    // Close existing dialog if open (allows switching from Find to Replace)
                    if (g_hDlgFind) {
                        DestroyWindow(g_hDlgFind);
                        g_hDlgFind = NULL;
                    }
                    
                    // Create modeless Find dialog in Replace mode
                    g_bReplaceMode = TRUE;
                    g_hDlgFind = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_FIND), 
                                              hwnd, DlgFindProc);
                    if (g_hDlgFind) {
                        ShowWindow(g_hDlgFind, SW_SHOW);
                    }
                    break;
                
                case ID_SEARCH_FIND_NEXT:
                    DoFind(TRUE);  // Search down
                    break;
                
                case ID_SEARCH_FIND_PREVIOUS:
                    DoFind(FALSE);  // Search up
                    break;

                case ID_SEARCH_GOTO_LINE:
                    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_GOTO), hwnd, DlgGotoProc);
                    break;

                case ID_SEARCH_TOGGLE_BOOKMARK:
                    ToggleBookmark();
                    break;

                case ID_SEARCH_NEXT_BOOKMARK:
                    NextBookmark(TRUE);
                    break;

                case ID_SEARCH_PREV_BOOKMARK:
                    NextBookmark(FALSE);
                    break;

                case ID_SEARCH_CLEAR_BOOKMARKS:
                    ClearAllBookmarks();
                    break;
                
                // Tools menu
                case ID_TOOLS_EXECUTEFILTER:
                    // Menu item is only enabled when a valid classic filter is selected
                    // Execute the filter (works for classic filters, even during REPL mode)
                    if (g_nCurrentFilter >= 0 && g_nCurrentFilter < g_nFilterCount) {
                        ExecuteFilter();
                    }
                    break;
                
                // Tools -> Start Interactive Mode
                case ID_TOOLS_START_INTERACTIVE:
                    // Menu item is only enabled when a REPL filter is selected and not in REPL mode
                    if (g_nSelectedREPLFilter >= 0 && g_nSelectedREPLFilter < g_nFilterCount &&
                        g_Filters[g_nSelectedREPLFilter].action == FILTER_ACTION_REPL && !g_bREPLMode) {
                        StartREPLFilter(g_nSelectedREPLFilter);
                        UpdateMenuStates(hwnd);
                    }
                    break;
                
                // Tools -> Exit Interactive Mode
                case ID_TOOLS_EXIT_INTERACTIVE:
                    if (g_bREPLMode) {
                        g_bREPLIntentionalExit = TRUE;
                        ExitREPLMode();
                        UpdateMenuStates(hwnd);
                    }
                    break;
                
                // Tools -> Insert Template (Ctrl+Shift+T) - Show template picker menu
                case ID_TOOLS_INSERT_TEMPLATE:
                    ShowTemplatePickerMenu(hwnd);
                    break;
                
                // Tools -> Select Filter submenu (dynamic filter selection)
                default:
                    {
                        int wmId = LOWORD(wParam);
                        
                        // Handle MRU file clicks
                        if (wmId >= ID_FILE_MRU_BASE && wmId < ID_FILE_MRU_BASE + MAX_MRU) {
                            int mruIdx = wmId - ID_FILE_MRU_BASE;
                            if (mruIdx >= 0 && mruIdx < g_nMRUCount) {
                                // Check for unsaved changes (synchronous — must happen before
                                // the menu closes so the user's intent is still clear)
                                if (!PromptSaveChanges()) {
                                    break;
                                }
                                
                                // Defer the actual file load via PostMessage so the menu has
                                // time to fully close before the blocking LoadTextFile starts.
                                // This prevents accessibility tree confusion on slow RichEdit.
                                LPWSTR pszDeferred = (LPWSTR)malloc(EXTENDED_PATH_MAX * sizeof(WCHAR));
                                if (pszDeferred) {
                                    wcscpy_s(pszDeferred, EXTENDED_PATH_MAX, g_MRU[mruIdx]);
                                    PostMessage(hwnd, WM_APP_LOAD_FILE, 0, (LPARAM)pszDeferred);
                                }
                            }
                        }
                        // Handle URL actions from context menu
                        else if (wmId == ID_URL_OPEN) {
                            // Open URL from context menu (use stored URL)
                            if (g_szContextMenuURL[0] != L'\0') {
                                OpenURL(hwnd, g_szContextMenuURL);
                                g_szContextMenuURL[0] = L'\0';  // Clear after use
                            }
                            return 0;
                        }
                        else if (wmId == ID_URL_COPY) {
                            // Copy URL to clipboard (use stored URL)
                            if (g_szContextMenuURL[0] != L'\0') {
                                CopyURLToClipboard(hwnd, g_szContextMenuURL);
                                g_szContextMenuURL[0] = L'\0';  // Clear after use
                            }
                            return 0;
                        }
                        // Handle template shortcuts (Ctrl+1, Ctrl+B, etc.)
                        else if (wmId >= ID_TOOLS_TEMPLATE_BASE && wmId < ID_TOOLS_TEMPLATE_BASE + MAX_TEMPLATES) {
                            int templateIdx = wmId - ID_TOOLS_TEMPLATE_BASE;
                            if (templateIdx >= 0 && templateIdx < g_nTemplateCount) {
                                InsertTemplate(templateIdx);
                            }
                        }
                        // Handle File→New template items
                        else if (wmId >= ID_FILE_NEW_TEMPLATE_BASE && wmId < ID_FILE_NEW_TEMPLATE_BASE + 32) {
                            int templateIdx = wmId - ID_FILE_NEW_TEMPLATE_BASE;
                            if (templateIdx >= 0 && templateIdx < g_nTemplateCount) {
                                FileNewFromTemplate(templateIdx);
                            }
                        }
                        // Handle filter selection from Tools menu
                        else if (wmId >= ID_TOOLS_FILTER_BASE && wmId < ID_TOOLS_FILTER_BASE + 100) {
                            int filterIdx = wmId - ID_TOOLS_FILTER_BASE;
                            if (filterIdx >= 0 && filterIdx < g_nFilterCount) {
                                if (g_Filters[filterIdx].action == FILTER_ACTION_REPL) {
                                    // Selecting a REPL filter
                                    // If another REPL is already running, prompt to exit
                                    if (g_bREPLMode && filterIdx != g_nCurrentREPLFilter) {
                                        WCHAR szPrompt[512], szTitle[128];
                                        LoadStringResource(IDS_REPL_SWITCH_PROMPT, szPrompt, 512);
                                        LoadStringResource(IDS_CONFIRM, szTitle, 128);
                                        
                                        int result = MessageBox(hwnd, szPrompt, szTitle, 
                                                               MB_YESNO | MB_ICONQUESTION);
                                        if (result == IDYES) {
                                            ExitREPLMode();
                                            g_nSelectedREPLFilter = filterIdx;
                                            SaveCurrentREPLFilter();
                                            BuildFilterMenu(hwnd);
                                            UpdateFilterDisplay();
                                            UpdateMenuStates(hwnd);
                                        }
                                    } else {
                                        // Set as selected REPL filter
                                        g_nSelectedREPLFilter = filterIdx;
                                        SaveCurrentREPLFilter();
                                        BuildFilterMenu(hwnd);
                                        UpdateFilterDisplay();
                                        UpdateMenuStates(hwnd);
                                    }
                                } else {
                                    // Selecting a classic filter
                                    g_nCurrentFilter = filterIdx;
                                    SaveCurrentFilter();
                                    BuildFilterMenu(hwnd);
                                    UpdateFilterDisplay();
                                    UpdateMenuStates(hwnd);
                                }
                            }
                        }
                        // Handle filter execution from context menu
                        else if (wmId >= ID_CONTEXT_FILTER_BASE && wmId < ID_CONTEXT_FILTER_BASE + 100) {
                            int filterIdx = wmId - ID_CONTEXT_FILTER_BASE;
                            if (filterIdx >= 0 && filterIdx < g_nFilterCount) {
                                // Block insert and REPL filters in read-only mode
                                if (g_bReadOnly && (g_Filters[filterIdx].action == FILTER_ACTION_INSERT || 
                                                     g_Filters[filterIdx].action == FILTER_ACTION_REPL)) {
                                    return 0;  // Silently fail (should not happen - menu item hidden)
                                }
                                g_nCurrentFilter = filterIdx;
                                ExecuteFilter();
                            }
                        }
                    }
                    break;
                    
                // Help menu
                case ID_HELP_ABOUT:
                    DialogBox(GetModuleHandle(NULL),
                             MAKEINTRESOURCE(IDD_ABOUT),
                             hwnd,
                             AboutDlgProc);
                    SetFocus(g_hWndEdit);
                    break;
            }
            return 0;
            
        case WM_NOTIFY:
            // Handle RichEdit notifications
            if (((LPNMHDR)lParam)->hwndFrom == g_hWndEdit) {
                switch (((LPNMHDR)lParam)->code) {
                    case EN_SELCHANGE:
                        UpdateStatusBar();
                        break;
                        
                    case EN_LINK:
                        // Handle URL link interactions
                        {
                            ENLINK* pEnLink = (ENLINK*)lParam;
                            
                            // Always store the URL range for performance optimization
                            g_lastURLRange = pEnLink->chrg;
                            
                            if (pEnLink->msg == WM_LBUTTONUP) {
                                // Mouse click on URL - open it
                                WCHAR szURL[2048];
                                TEXTRANGE tr;
                                tr.chrg = pEnLink->chrg;
                                tr.lpstrText = szURL;
                                
                                if (tr.chrg.cpMax - tr.chrg.cpMin < 2048) {
                                    SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
                                    OpenURL(hwnd, szURL);
                                }
                                
                                return 1; // Prevent default handling
                            }
                            else if (pEnLink->msg == WM_SETCURSOR) {
                                // Change cursor to hand pointer over URLs
                                SetCursor(LoadCursor(NULL, IDC_HAND));
                                return 1; // Prevent default handling
                            }
                        }
                        break;
                        
                    case EN_STOPNOUNDO:
                        // Undo buffer is full - notify user
                        {
                            WCHAR szTitle[128];
                            WCHAR szMessage[256];
                            LoadStringResource(IDS_UNDO_BUFFER_FULL_TITLE, szTitle, 128);
                            LoadStringResource(IDS_UNDO_BUFFER_FULL_MESSAGE, szMessage, 256);
                            
                            int result = MessageBox(hwnd, szMessage, szTitle, 
                                                   MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1);
                            
                            if (result == IDNO) {
                                // User wants to stop editing - close without saving
                                PostMessage(hwnd, WM_CLOSE, 0, 0);
                            }
                            // If IDYES, continue editing (do nothing)
                        }
                        break;
                }
            }
            return 0;
            
        case WM_INITMENUPOPUP:
            // Update File menu when opened
            if (LOWORD(lParam) == 0) {  // File menu is at position 0
                HMENU hMenu = (HMENU)wParam;
                CheckMenuItem(hMenu, ID_FILE_READONLY, 
                             g_bReadOnly ? MF_CHECKED : MF_UNCHECKED);
                EnableMenuItem(hMenu, ID_FILE_SAVE, 
                              g_bReadOnly ? MF_GRAYED : MF_ENABLED);
            }
            // Update Undo/Redo menu items when Edit menu is opened
            if (LOWORD(lParam) == 1) {  // Edit menu is at position 1
                HMENU hMenu = (HMENU)wParam;
                UpdateMenuUndoRedo(hMenu);
                if (g_bReadOnly) {
                    // Disable editing operations in read-only mode
                    EnableMenuItem(hMenu, ID_EDIT_UNDO, MF_GRAYED);
                    EnableMenuItem(hMenu, ID_EDIT_REDO, MF_GRAYED);
                    EnableMenuItem(hMenu, ID_EDIT_CUT, MF_GRAYED);
                    EnableMenuItem(hMenu, ID_EDIT_PASTE, MF_GRAYED);
                    EnableMenuItem(hMenu, ID_EDIT_TIMEDATE, MF_GRAYED);
                } else {
                    // Re-enable editing operations when not in read-only mode
                    // (UpdateMenuUndoRedo already handles Undo/Redo based on availability)
                    BOOL canPaste = SendMessage(g_hWndEdit, EM_CANPASTE, 0, 0);
                    CHARRANGE cr;
                    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
                    BOOL hasSelection = (cr.cpMin != cr.cpMax);
                    
                    EnableMenuItem(hMenu, ID_EDIT_CUT, hasSelection ? MF_ENABLED : MF_GRAYED);
                    EnableMenuItem(hMenu, ID_EDIT_PASTE, canPaste ? MF_ENABLED : MF_GRAYED);
                    EnableMenuItem(hMenu, ID_EDIT_TIMEDATE, MF_ENABLED);
                }
            }
            // Update Tools menu when opened
            if (LOWORD(lParam) == 4) {  // Tools menu is at position 4
                HMENU hMenu = (HMENU)wParam;
                // Disable Insert Template in read-only mode
                EnableMenuItem(hMenu, ID_TOOLS_INSERT_TEMPLATE, 
                              g_bReadOnly ? MF_GRAYED : MF_ENABLED);
            }
            // Update View menu when opened
            if (LOWORD(lParam) == 3) {  // View menu is at position 3
                HMENU hMenu = (HMENU)wParam;
                CheckMenuItem(hMenu, ID_VIEW_WORDWRAP,
                              g_bWordWrap ? MF_CHECKED : MF_UNCHECKED);
                // Gray Reset Zoom when already at 100%
                DWORD nNum = 0, nDen = 0;
                BOOL bZoomed = (BOOL)SendMessage(g_hWndEdit, EM_GETZOOM, (WPARAM)&nNum, (LPARAM)&nDen);
                BOOL bAtDefault = (!bZoomed || nNum == 0 || nDen == 0 || nNum == nDen);
                EnableMenuItem(hMenu, ID_VIEW_ZOOM_RESET,
                               bAtDefault ? MF_GRAYED : MF_ENABLED);
            }
            return 0;
            
        case WM_CONTEXTMENU:
            // Handle context menu on RichEdit control
            if ((HWND)wParam == g_hWndEdit) {
                // Get cursor position (extract x and y from lParam)
                int xPos = (short)LOWORD(lParam);
                int yPos = (short)HIWORD(lParam);
                
                // Create context menu
                HMENU hMenu = CreatePopupMenu();
                if (hMenu) {
                    // Check if cursor is in a URL
                    BOOL isInURL = FALSE;
                    g_szContextMenuURL[0] = L'\0';  // Clear stored URL
                    LONG cursorPos = -1;
                    
                    // Determine cursor position for URL detection
                    if (xPos == -1 && yPos == -1) {
                        // Keyboard context menu (Shift+F10 or context menu key)
                        // Use current cursor position
                        CHARRANGE cr;
                        SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
                        cursorPos = cr.cpMin;
                    } else {
                        // Mouse right-click - convert screen coords to client
                        POINT pt = {xPos, yPos};
                        ScreenToClient(g_hWndEdit, &pt);
                        cursorPos = SendMessage(g_hWndEdit, EM_CHARFROMPOS, 0, (LPARAM)&pt);
                    }
                    
                    // Check if this position is in a URL
                    // GetURLAtCursor does the CFE_LINK check internally
                    if (cursorPos >= 0 && GetURLAtCursor(g_hWndEdit, g_szContextMenuURL, 2048, NULL)) {
                        isInURL = TRUE;
                    }
                    
                    // Add URL menu items first (highest priority) if in URL
                    if (isInURL) {
                        WCHAR szOpenURL[64], szCopyURL[64];
                        LoadStringResource(IDS_CONTEXT_OPEN_URL, szOpenURL, 64);
                        LoadStringResource(IDS_CONTEXT_COPY_URL, szCopyURL, 64);
                        
                        AppendMenu(hMenu, MF_STRING, ID_URL_OPEN, szOpenURL);
                        AppendMenu(hMenu, MF_STRING, ID_URL_COPY, szCopyURL);
                        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                    }
                    
                    // Add filters with ContextMenu=1, sorted by ContextMenuOrder
                    // Build array of filters to show in context menu
                    struct ContextMenuFilter {
                        int filterIdx;
                        int order;
                    };
                    ContextMenuFilter contextFilters[MAX_FILTERS];
                    int contextFilterCount = 0;
                    
                    for (int i = 0; i < g_nFilterCount; i++) {
                        if (g_Filters[i].bContextMenu) {
                            // Skip insert and REPL filters in read-only mode
                            if (g_bReadOnly && (g_Filters[i].action == FILTER_ACTION_INSERT || 
                                                 g_Filters[i].action == FILTER_ACTION_REPL)) {
                                continue;
                            }
                            contextFilters[contextFilterCount].filterIdx = i;
                            contextFilters[contextFilterCount].order = g_Filters[i].nContextMenuOrder;
                            contextFilterCount++;
                        }
                    }
                    
                    // Simple bubble sort by order
                    for (int i = 0; i < contextFilterCount - 1; i++) {
                        for (int j = 0; j < contextFilterCount - i - 1; j++) {
                            if (contextFilters[j].order > contextFilters[j + 1].order) {
                                ContextMenuFilter temp = contextFilters[j];
                                contextFilters[j] = contextFilters[j + 1];
                                contextFilters[j + 1] = temp;
                            }
                        }
                    }
                    
                    // Add sorted filters to menu with descriptions for accessibility (using localized strings)
                    for (int i = 0; i < contextFilterCount; i++) {
                        int filterIdx = contextFilters[i].filterIdx;
                        
                        // Build accessible menu text: "LocalizedName: LocalizedDescription"
                        WCHAR szMenuText[MAX_FILTER_NAME + MAX_FILTER_DESC + 4];
                        if (g_bShowMenuDescriptions && g_Filters[filterIdx].szLocalizedDescription[0] != L'\0') {
                            // Build menu text: "LocalizedName: LocalizedDescription"
                            wcscpy(szMenuText, g_Filters[filterIdx].szLocalizedName);
                            wcscat(szMenuText, L": ");
                            wcscat(szMenuText, g_Filters[filterIdx].szLocalizedDescription);
                        } else {
                            // Just show the localized name
                            wcscpy(szMenuText, g_Filters[filterIdx].szLocalizedName);
                        }
                        
                        AppendMenu(hMenu, MF_STRING, 
                                   ID_CONTEXT_FILTER_BASE + filterIdx, 
                                   szMenuText);
                    }
                    
                    // Add separator if we added any filters
                    if (contextFilterCount > 0) {
                        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                    }
                    
                    // Add standard edit menu items
                    // Check if operations are available
                    BOOL canUndo = SendMessage(g_hWndEdit, EM_CANUNDO, 0, 0);
                    BOOL canPaste = SendMessage(g_hWndEdit, EM_CANPASTE, 0, 0);
                    
                    CHARRANGE cr;
                    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
                    BOOL hasSelection = (cr.cpMin != cr.cpMax);
                    
                    WCHAR szUndo[32], szCut[32], szCopy[32], szPaste[32], szSelectAll[32];
                    LoadStringResource(IDS_CONTEXT_UNDO, szUndo, 32);
                    LoadStringResource(IDS_CONTEXT_CUT, szCut, 32);
                    LoadStringResource(IDS_CONTEXT_COPY, szCopy, 32);
                    LoadStringResource(IDS_CONTEXT_PASTE, szPaste, 32);
                    LoadStringResource(IDS_CONTEXT_SELECT_ALL, szSelectAll, 32);
                    
                    AppendMenu(hMenu, (canUndo && !g_bReadOnly) ? MF_STRING : MF_STRING | MF_GRAYED, 
                               ID_EDIT_UNDO, szUndo);
                    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenu(hMenu, (hasSelection && !g_bReadOnly) ? MF_STRING : MF_STRING | MF_GRAYED, 
                               ID_EDIT_CUT, szCut);
                    AppendMenu(hMenu, hasSelection ? MF_STRING : MF_STRING | MF_GRAYED, 
                               ID_EDIT_COPY, szCopy);
                    AppendMenu(hMenu, (canPaste && !g_bReadOnly) ? MF_STRING : MF_STRING | MF_GRAYED, 
                               ID_EDIT_PASTE, szPaste);
                    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenu(hMenu, MF_STRING, ID_EDIT_SELECTALL, szSelectAll);
                    
                    // Show menu
                    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                                   xPos, yPos, 0, hwnd, NULL);
                    
                    DestroyMenu(hMenu);
                }
                return 0;
            }
            break;
            
        case WM_QUERYENDSESSION:
            // Windows is shutting down - save to temp file but DON'T write to INI yet
            // We'll only write to INI in WM_ENDSESSION if shutdown actually happens
            // This prevents resume file from being registered if another app cancels shutdown
            {
                // Exit REPL silently (user wants to shutdown)
                if (g_bREPLMode) {
                    g_bREPLIntentionalExit = TRUE;
                    ExitREPLMode();
                }
                
                // Handle unsaved changes - save to temp file but don't register in INI yet
                if (g_bModified) {
                    // Use SaveToResumeFile with WITHOUT_INI mode for two-phase commit
                    // This saves the file content but doesn't register in INI yet
                    // WM_ENDSESSION will register in INI if shutdown is confirmed
                    SaveToResumeFile(RESUME_SAVE_WITHOUT_INI);
                }
                
                // Allow Windows to proceed with shutdown
                return TRUE;
            }
            
        case WM_CLOSE:
            // Normal close - behavior depends on AutoSaveUntitledOnClose setting
            {
                // Stop autosave timer to prevent save attempts during shutdown
                KillTimer(hwnd, IDT_AUTOSAVE);
                BOOL bPrevSaveInProgress = g_bSaveInProgress;
                g_bSaveInProgress = FALSE; // allow saves during prompt/close

                // Check if REPL is active and prompt user
                if (g_bREPLMode) {
                    WCHAR szPrompt[512], szTitle[128];
                    LoadStringResource(IDS_REPL_CLOSE_PROMPT, szPrompt, 512);
                    LoadStringResource(IDS_CONFIRM, szTitle, 128);
                    
                    int result = MessageBox(hwnd, szPrompt, szTitle, 
                                           MB_YESNO | MB_ICONQUESTION);
                    if (result != IDYES) {
                        return 0; // Cancel close
                    }
                    g_bREPLIntentionalExit = TRUE;
                    ExitREPLMode();
                }
                
                // Handle unsaved changes
                if (g_bModified) {
                    BOOL isUntitled = (g_szFileName[0] == L'\0');
                    
                    // For resumed files, always save back to resume file (note-taker behavior)
                    // For untitled files with AutoSaveUntitledOnClose, also save to resume
                    if (g_bIsResumedFile || (g_bAutoSaveUntitledOnClose && isUntitled)) {
                        // Auto-save to resume file (no prompt)
                        if (!SaveToResumeFile()) {
                            // Error already shown - ask if user wants to close anyway
                            WCHAR szPrompt[256];
                            LoadStringResource(IDS_ERROR, szPrompt, 256);
                            int result = MessageBox(hwnd,
                                L"Failed to save session. Close without saving?",
                                szPrompt, MB_YESNO | MB_ICONWARNING);
                            if (result != IDYES) {
                                g_bSaveInProgress = FALSE;
                                return 0;
                            }
                        }
                    } else {
                        // Traditional mode - prompt user
                        if (!PromptSaveChanges()) {
                            if (g_bAutosaveEnabled && g_nAutosaveIntervalMinutes > 0) {
                                StartAutosaveTimer(hwnd);
                            }
                            g_bSaveInProgress = bPrevSaveInProgress;
                            return 0; // User cancelled - keep editing
                        }
                    }
                }

                // Close the window
                DestroyWindow(hwnd);
                g_bSaveInProgress = bPrevSaveInProgress;
                return 0;
            }
            
        case WM_ENDSESSION:
            // Windows is actually shutting down now (or shutdown was cancelled)
            if (wParam) {
                // Session is actually ending - NOW write resume file to INI
                if (g_szResumeFilePath[0] != L'\0') {
                    WriteResumeToINI(g_szResumeFilePath, 
                                   g_szFileName[0] ? g_szFileName : L"");
                }
                FlushIniCache();
                DestroyWindow(hwnd);
            } else {
                // Shutdown was cancelled by another application
                // Delete the temp resume file we created in WM_QUERYENDSESSION
                if (g_szResumeFilePath[0] != L'\0') {
                    DeleteResumeFile(g_szResumeFilePath);
                    g_szResumeFilePath[0] = L'\0';
                }
            }
            return 0;
            
        case WM_REPL_OUTPUT:
        {
            // REPL output received from background thread
            LPWSTR pszOutput = (LPWSTR)lParam;
            if (pszOutput) {
                InsertREPLOutput(pszOutput);
                free(pszOutput);  // Free memory allocated by thread
            }
            return 0;
        }
        
        case WM_REPL_EXITED:
        {
            // REPL filter process has exited
            // Cleanup REPL resources
            ExitREPLMode();
            
            // Only show exit notification if it was NOT an intentional exit
            if (!g_bREPLIntentionalExit) {
                WCHAR szMsg[256], szTitle[64];
                LoadStringResource(IDS_REPL_EXITED, szMsg, 256);
                LoadStringResource(IDS_INFORMATION, szTitle, 64);
                MessageBox(hwnd, szMsg, szTitle, MB_ICONINFORMATION);
            }
            
            // Reset flag for next REPL session
            g_bREPLIntentionalExit = FALSE;
            
            return 0;
        }

        case WM_APP_LOAD_FILE:
        {
            // Deferred MRU file load: the menu is now fully closed, so the
            // accessibility tree is clean before the blocking load begins.
            LPWSTR pszPath = (LPWSTR)lParam;
            if (pszPath) {
                BOOL bLoaded = LoadTextFile(pszPath);
                free(pszPath);
                // Only fire the deferred focus/caret restore when the load
                // succeeded.  On failure (e.g. file no longer exists) the
                // document is untouched and the caret must stay in place;
                // firing the timer would reset it to position 0.
                if (bLoaded) {
                    SetTimer(hwnd, IDT_FOCUS_RESTORE, 200, NULL);
                }
            }
            return 0;
        }
        
        case WM_DESTROY:
            // Exit REPL mode if active
            if (g_bREPLMode) {
                ExitREPLMode();
            }

            SaveBookmarksForCurrentFile();
            
            // Destroy Find dialog if open (Phase 2.9)
            if (g_hDlgFind) {
                DestroyWindow(g_hDlgFind);
                g_hDlgFind = NULL;
            }
            
            // Kill timers
            KillTimer(hwnd, IDT_AUTOSAVE);
            KillTimer(hwnd, IDT_FILTER_STATUSBAR);
            KillTimer(hwnd, IDT_AUTOSAVE_FLASH);
            // Release TOM interface
            if (g_pTextDoc) { g_pTextDoc->Release(); g_pTextDoc = NULL; }
            // Save current zoom level before flushing INI
            {
                WCHAR szIniPath[EXTENDED_PATH_MAX];
                GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
                DWORD nNum = 0, nDen = 0;
                BOOL bZoomed = (BOOL)SendMessage(g_hWndEdit, EM_GETZOOM, (WPARAM)&nNum, (LPARAM)&nDen);
                int zoomPct = 100;
                if (bZoomed && nNum > 0 && nDen > 0)
                    zoomPct = MulDiv((int)nNum, 100, (int)nDen);
                WCHAR szZoom[16];
                _snwprintf(szZoom, 16, L"%d", zoomPct);
                WriteINIValue(szIniPath, L"Settings", L"Zoom", szZoom);
            }
            FlushIniCache();
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//============================================================================
// EditSubclassProc - Subclass procedure for RichEdit control
//
// Intercepts WM_KEYDOWN to handle Enter key for REPL mode and URLs
//============================================================================
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN) {
        // Handle Ctrl+Shift+I (Start Interactive Mode)
        if (wParam == 'I' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
            // Send command to main window to start interactive mode
            PostMessage(g_hWndMain, WM_COMMAND, MAKEWPARAM(ID_TOOLS_START_INTERACTIVE, 0), 0);
            return 0; // Prevent default behavior
        }
        
        // Handle Ctrl+Shift+Q (Exit Interactive Mode)
        if (wParam == 'Q' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
            // Send command to main window to exit interactive mode
            PostMessage(g_hWndMain, WM_COMMAND, MAKEWPARAM(ID_TOOLS_EXIT_INTERACTIVE, 0), 0);
            return 0; // Prevent default behavior
        }
        
        if (wParam == VK_RETURN) {
            // Check if Shift is held: Shift+Enter always inserts newline
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                // Allow default behavior (insert newline)
                return CallWindowProc(g_pfnOriginalEditProc, hwnd, msg, wParam, lParam);
            }
            
            // If in REPL mode: Enter sends command if on a line with prompt, or at end of document
            if (g_bREPLMode) {
                // Get current cursor position
                CHARRANGE cr;
                SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
                
                // Get current line number
                LONG lineIndex = SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin);
                
                // Get line start/end
                LONG lineStart = SendMessage(g_hWndEdit, EM_LINEINDEX, lineIndex, 0);
                LONG lineEnd = SendMessage(g_hWndEdit, EM_LINEINDEX, lineIndex + 1, 0);
                if (lineEnd == -1) {
                    // Last line - get document length
                    GETTEXTLENGTHEX gtl;
                    gtl.flags = GTL_DEFAULT;
                    gtl.codepage = 1200; // Unicode
                    lineEnd = SendMessage(g_hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
                }
                
                // Check if we're at the very end of the document (safety: send empty line to recall prompt)
                GETTEXTLENGTHEX gtl;
                gtl.flags = GTL_DEFAULT;
                gtl.codepage = 1200;
                LONG docLength = SendMessage(g_hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
                if (cr.cpMin >= docLength) {
                    // At end of document - send to REPL (empty line or whatever is on current line)
                    SendLineToREPL();
                    return 0;
                }
                
                // Extract line text
                int lineLen = lineEnd - lineStart;
                if (lineLen > 0) {
                    LPWSTR pszLine = (LPWSTR)malloc((lineLen + 1) * sizeof(WCHAR));
                    if (pszLine) {
                        TEXTRANGE tr;
                        tr.chrg.cpMin = lineStart;
                        tr.chrg.cpMax = lineEnd;
                        tr.lpstrText = pszLine;
                        SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
                        
                        // Check if line contains prompt
                        int inputStart = 0;
                        if (DetectPrompt(pszLine, g_szREPLPromptEnd, &inputStart)) {
                            // Found prompt on this line - send to REPL
                            free(pszLine);
                            SendLineToREPL();
                            return 0;
                        }
                        free(pszLine);
                    }
                }
                
                // No prompt found and not at end - allow normal Enter (insert newline, or open URL if at one)
            }
            
            // Check if cursor is in a URL
            WCHAR szURL[2048];
            if (GetURLAtCursor(hwnd, szURL, 2048, NULL)) {
                // Open URL
                OpenURL(g_hWndMain, szURL);
                return 0; // Prevent default Enter behavior (don't insert newline)
            }
            
            // If not in URL and not in REPL mode, allow normal Enter key behavior (insert newline)
        }
    }
    
    // Suppress WM_CHAR for Ctrl+Shift+I and Ctrl+Shift+Q to prevent TAB insertion
    if (msg == WM_CHAR) {
        if ((wParam == '\t' || wParam == 'I' || wParam == 'i') && 
            (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
            return 0; // Block the character from being inserted
        }
        if ((wParam == 'Q' || wParam == 'q') && 
            (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
            return 0; // Block the character from being inserted
        }
        // Guard: drop NUL char (some keyboard layouts emit it for Ctrl+digit combinations)
        if (wParam == 0 && (GetKeyState(VK_CONTROL) & 0x8000)) {
            return 0;
        }
    }

// Ctrl+mouse wheel: RichEdit handles the zoom internally; react by fixing word wrap layout
if (msg == WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL)) {
    LRESULT lResult = CallWindowProc(g_pfnOriginalEditProc, hwnd, msg, wParam, lParam);
    // Keep g_nZoomPercent in sync so file-load can restore zoom correctly.
    // EM_GETZOOM returns FALSE (leaves params 0) when zoom is exactly 100%.
    WPARAM nNum = 0; LPARAM nDen = 0;
    if (SendMessage(hwnd, EM_GETZOOM, (WPARAM)&nNum, (LPARAM)&nDen) && nNum > 0 && nDen > 0)
        g_nZoomPercent = (int)MulDiv((int)nNum, 100, (int)nDen);
    else
        g_nZoomPercent = 100;
    ApplyWordWrap(hwnd);
    UpdateStatusBar();
    return lResult;
}
    
    // Call original window procedure for all other messages
    return CallWindowProc(g_pfnOriginalEditProc, hwnd, msg, wParam, lParam);
}

//============================================================================
// Resume File Management Functions (Phase 2.6)
//============================================================================

//============================================================================
// GetRichEditorTempDir - Get temp directory path for RichEditor
// Returns: TRUE on success, FALSE on failure
//============================================================================
BOOL GetRichEditorTempDir(WCHAR* pszPath, DWORD dwSize)
{
    WCHAR szTempPath[MAX_PATH];
    
    // Get Windows temp directory
    if (GetTempPath(MAX_PATH, szTempPath) == 0) {
        return FALSE;
    }
    
    // Append RichEditor subdirectory
    _snwprintf(pszPath, dwSize, L"%sRichEditor\\", szTempPath);
    pszPath[dwSize - 1] = L'\0';
    
    return TRUE;
}

//============================================================================
// EnsureRichEditorTempDirExists - Create temp directory if needed
// Returns: TRUE on success, FALSE on failure
//============================================================================
BOOL EnsureRichEditorTempDirExists()
{
    WCHAR szTempDir[MAX_PATH];
    
    if (!GetRichEditorTempDir(szTempDir, MAX_PATH)) {
        return FALSE;
    }
    
    // Check if directory exists
    DWORD dwAttrib = GetFileAttributes(szTempDir);
    if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return TRUE;  // Already exists
    }
    
    // Create directory
    if (!CreateDirectory(szTempDir, NULL)) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_ALREADY_EXISTS) {
            return FALSE;
        }
    }
    
    return TRUE;
}

//============================================================================
// GenerateResumeFileName - Create unique resume file name
// pszOriginalPath: Original file path (or empty for untitled)
// pszResumeFile: Buffer to receive resume file path
// dwSize: Buffer size
// Returns: TRUE on success, FALSE on failure
//============================================================================
BOOL GenerateResumeFileName(const WCHAR* pszOriginalPath, WCHAR* pszResumeFile, DWORD dwSize)
{
    WCHAR szTempDir[MAX_PATH];
    WCHAR szBaseName[MAX_PATH];
    WCHAR szExt[MAX_PATH];
    
    // Get temp directory
    if (!GetRichEditorTempDir(szTempDir, MAX_PATH)) {
        return FALSE;
    }
    
    // Ensure directory exists
    if (!EnsureRichEditorTempDirExists()) {
        // Show error message
        WCHAR szError[512];
        LoadStringResource(IDS_ERROR, szError, 512);
        MessageBox(g_hWndMain, 
                   L"Cannot create temporary directory for session recovery.\n"
                   L"Unsaved changes will be lost on shutdown.",
                   szError, MB_OK | MB_ICONERROR);
        return FALSE;
    }
    
    // Determine base name and extension
    if (pszOriginalPath && pszOriginalPath[0] != L'\0') {
        // Saved file - extract basename and extension
        const WCHAR* pszFileName = wcsrchr(pszOriginalPath, L'\\');
        if (!pszFileName) {
            pszFileName = wcsrchr(pszOriginalPath, L'/');
        }
        pszFileName = pszFileName ? (pszFileName + 1) : pszOriginalPath;
        
        // Split into name and extension
        const WCHAR* pszDot = wcsrchr(pszFileName, L'.');
        if (pszDot) {
            size_t nameLen = pszDot - pszFileName;
            if (nameLen >= MAX_PATH) nameLen = MAX_PATH - 1;
            wcsncpy(szBaseName, pszFileName, nameLen);
            szBaseName[nameLen] = L'\0';
            wcscpy(szExt, pszDot);
        } else {
            wcscpy(szBaseName, pszFileName);
            wcscpy(szExt, L".txt");
        }
    } else {
        // Untitled file - use timestamp for uniqueness
        SYSTEMTIME st;
        GetLocalTime(&st);
        _snwprintf(szBaseName, MAX_PATH, L"Untitled_%04d%02d%02d_%02d%02d%02d",
                   st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        wcscpy(szExt, L".txt");
    }
    
    // Check if path would fit in buffer (avoid truncation)
    size_t requiredLen = wcslen(szTempDir) + wcslen(szBaseName) + 
                         wcslen(L"_resume") + wcslen(szExt) + 1;
    if (requiredLen > dwSize) {
        WCHAR szError[512];
        LoadStringResource(IDS_ERROR, szError, 512);
        MessageBox(g_hWndMain,
                   L"Resume filename too long. Cannot save session.",
                   szError, MB_OK | MB_ICONERROR);
        return FALSE;
    }
    
    // Construct full resume file path (safe - we checked the length)
    _snwprintf(pszResumeFile, dwSize, L"%s%s_resume%s", szTempDir, szBaseName, szExt);
    pszResumeFile[dwSize - 1] = L'\0';
    
    return TRUE;
}

//============================================================================
// GetINIFilePath - Get path to RichEditor.ini (same directory as .exe)
//============================================================================
void GetINIFilePath(LPWSTR pszPath, DWORD dwSize)
{
    if (!pszPath || dwSize == 0) return;
    
    // Get executable path
    GetModuleFileName(NULL, pszPath, dwSize);
    
    // Replace .exe extension with .ini
    WCHAR* pszExt = wcsrchr(pszPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
}

//============================================================================
// WriteResumeToINI - Store resume file info in INI
//============================================================================
void WriteResumeToINI(const WCHAR* pszResumeFile, const WCHAR* pszOriginalPath)
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    WriteINIValue(szIniPath, L"Resume", L"ResumeFile", pszResumeFile);
    WriteINIValue(szIniPath, L"Resume", L"OriginalPath", 
                  pszOriginalPath ? pszOriginalPath : L"");
    FlushIniCache();
}

//============================================================================
// ReadResumeFromINI - Read resume file info from INI
// Returns: TRUE if resume file exists, FALSE otherwise
//============================================================================
BOOL ReadResumeFromINI(WCHAR* pszResumeFile, DWORD dwResumeSize,
                       WCHAR* pszOriginalPath, DWORD dwOriginalSize)
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    ReadINIValue(szIniPath, L"Resume", L"ResumeFile", pszResumeFile, dwResumeSize, L"");
    ReadINIValue(szIniPath, L"Resume", L"OriginalPath", pszOriginalPath, dwOriginalSize, L"");
    
    return (pszResumeFile[0] != L'\0');
}

//============================================================================
// ClearResumeFromINI - Remove resume section from INI
//============================================================================
void ClearResumeFromINI()
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    WriteINIValue(szIniPath, L"Resume", L"ResumeFile", L"");
    WriteINIValue(szIniPath, L"Resume", L"OriginalPath", L"");
    FlushIniCache();
}

//============================================================================
// DeleteResumeFile - Delete temp resume file
// Returns: TRUE if file was deleted or didn't exist, FALSE on error
//============================================================================
BOOL DeleteResumeFile(const WCHAR* pszResumeFile)
{
    if (pszResumeFile && pszResumeFile[0] != L'\0') {
        return DeleteFile(pszResumeFile);
    }
    return TRUE;  // Nothing to delete
}

//============================================================================
// WriteResumeFileContent - Helper function to write document content to resume file
// Parameters:
//   pszResumeFile - Full path to resume file
// Returns: TRUE on success, FALSE on failure
// Note: Does NOT modify global state or write to INI - just writes the file
//============================================================================
BOOL WriteResumeFileContent(LPCWSTR pszResumeFile)
{
    // Save current document state (SaveTextFile() modifies these globals)
    WCHAR szSavedFileName[MAX_PATH];
    WCHAR szSavedFileTitle[MAX_PATH];
    BOOL bSavedModified = g_bModified;
    BOOL bSavedIsResumed = g_bIsResumedFile;
    WCHAR szSavedResumeFilePath[EXTENDED_PATH_MAX];
    WCHAR szSavedOriginalFilePath[EXTENDED_PATH_MAX];
    
    wcscpy(szSavedFileName, g_szFileName);
    wcscpy(szSavedFileTitle, g_szFileTitle);
    wcscpy(szSavedResumeFilePath, g_szResumeFilePath);
    wcscpy(szSavedOriginalFilePath, g_szOriginalFilePath);
    
    // Use the working SaveTextFile() function to save content
    // This ensures consistent UTF-8 encoding without bugs
    // Pass FALSE to prevent deleting the resume file we're currently writing!
    BOOL bSuccess = SaveTextFile(pszResumeFile, FALSE);
    
    // Restore document state (we're saving to resume file, not actually saving the document)
    wcscpy(g_szFileName, szSavedFileName);
    wcscpy(g_szFileTitle, szSavedFileTitle);
    g_bModified = bSavedModified;
    g_bIsResumedFile = bSavedIsResumed;
    wcscpy(g_szResumeFilePath, szSavedResumeFilePath);
    wcscpy(g_szOriginalFilePath, szSavedOriginalFilePath);
    
    // Update title bar (SaveTextFile cleared [Resumed], restore it)
    UpdateTitle();
    UpdateStatusBar();
    
    if (!bSuccess) {
        // SaveTextFile already showed error message
        DeleteFile(pszResumeFile);  // Clean up partial file
    }
    
    return bSuccess;
}

//============================================================================
// SaveToResumeFile - Save current document to resume file and register in INI
// Returns: TRUE on success, FALSE on failure
//============================================================================
BOOL SaveToResumeFile(ResumeFileSaveMode mode)
{
    WCHAR szResumeFile[EXTENDED_PATH_MAX];
    
    // If this is already a resumed file, reuse the existing resume file path
    // This prevents creating a new file every time for untitled documents
    if (g_bIsResumedFile && g_szResumeFilePath[0] != L'\0') {
        wcscpy(szResumeFile, g_szResumeFilePath);
    } else {
        // Generate new resume file name
        if (!GenerateResumeFileName(g_szFileName, szResumeFile, EXTENDED_PATH_MAX)) {
            return FALSE;  // Error already shown to user
        }
    }
    
    // Write resume file content using helper function
    if (!WriteResumeFileContent(szResumeFile)) {
        return FALSE;  // Error already shown to user
    }
    
    // Store resume file path and original path in INI (if requested)
    if (mode == RESUME_SAVE_WITH_INI) {
        WriteResumeToINI(szResumeFile, g_szFileName[0] ? g_szFileName : L"");
    }
    
    // Remember resume file path globally
    wcscpy(g_szResumeFilePath, szResumeFile);
    
    return TRUE;
}

//============================================================================
// CreateElevatedSaveStagingFile - Save document to a temp staging file
// Returns: TRUE on success, FALSE on failure
//============================================================================
BOOL CreateElevatedSaveStagingFile(WCHAR* pszStagingPath, DWORD cchPath)
{
    if (!pszStagingPath || cchPath == 0) {
        return FALSE;
    }

    pszStagingPath[0] = L'\0';

    if (!EnsureRichEditorTempDirExists()) {
        ShowError(IDS_ERROR_CREATE_FILE, L"Could not create temporary directory", 0);
        return FALSE;
    }

    WCHAR szTempDir[MAX_PATH];
    if (!GetRichEditorTempDir(szTempDir, MAX_PATH)) {
        ShowError(IDS_ERROR_CREATE_FILE, L"Could not locate temporary directory", 0);
        return FALSE;
    }

    WCHAR szTempFile[MAX_PATH];
    if (GetTempFileName(szTempDir, L"RES", 0, szTempFile) == 0) {
        ShowError(IDS_ERROR_CREATE_FILE, L"Could not create temporary file", GetLastError());
        return FALSE;
    }

    wcscpy_s(pszStagingPath, cchPath, szTempFile);

    DWORD dwError = 0;
    SaveTextFailure failure = SAVE_TEXT_FAILURE_NONE;
    if (!SaveTextFileInternal(pszStagingPath, FALSE, &dwError, &failure, FALSE, FALSE)) {
        ShowSaveTextFailure(failure, dwError);
        DeleteFile(pszStagingPath);
        pszStagingPath[0] = L'\0';
        return FALSE;
    }

    // Preserve timestamps and attributes of target if it already exists so the elevated worker can restore them
    DWORD dwAttrib = GetFileAttributes(pszStagingPath);
    if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributes(pszStagingPath, dwAttrib & ~FILE_ATTRIBUTE_READONLY);
    }

    return TRUE;
}

//============================================================================
// RunElevatedSave - Launch elevated helper to save staged content
//============================================================================
BOOL RunElevatedSave(LPCWSTR pszStagingPath, LPCWSTR pszTargetPath, DWORD* pLastError)
{
    if (pLastError) {
        *pLastError = 0;
    }

    if (!pszStagingPath || !pszTargetPath) {
        return FALSE;
    }

    WCHAR szExePath[MAX_PATH];
    if (GetModuleFileName(NULL, szExePath, MAX_PATH) == 0) {
        return FALSE;
    }

    WCHAR szParams[EXTENDED_PATH_MAX * 2 + 64];
    _snwprintf(szParams, sizeof(szParams) / sizeof(szParams[0]), L"/elevated-save \"%s\" \"%s\"", pszStagingPath, pszTargetPath);
    szParams[(sizeof(szParams) / sizeof(szParams[0])) - 1] = L'\0';

    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = g_hWndMain;
    sei.lpVerb = L"runas";
    sei.lpFile = szExePath;
    sei.lpParameters = szParams;
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteEx(&sei) || !sei.hProcess) {
        if (pLastError) {
            *pLastError = GetLastError();
        }
        return FALSE;
    }

    WaitForSingleObject(sei.hProcess, INFINITE);

    DWORD dwExitCode = 1;
    GetExitCodeProcess(sei.hProcess, &dwExitCode);
    CloseHandle(sei.hProcess);

    if (dwExitCode != 0) {
        if (pLastError) {
            *pLastError = (dwExitCode == STILL_ACTIVE) ? ERROR_GEN_FAILURE : dwExitCode;
        }
        return FALSE;
    }

    if (pLastError) {
        *pLastError = 0;
    }
    return TRUE;
}

//============================================================================
// ElevatedSaveWorker - Perform the elevated copy from staging to target
//============================================================================
BOOL ElevatedSaveWorker(LPCWSTR pszStagingPath, LPCWSTR pszTargetPath)
{
    DWORD dwError = ERROR_SUCCESS;

    if (!pszStagingPath || !pszTargetPath || pszStagingPath[0] == L'\0' || pszTargetPath[0] == L'\0') {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    HANDLE hSource = INVALID_HANDLE_VALUE;
    HANDLE hDest = INVALID_HANDLE_VALUE;
    BYTE* pBuffer = NULL;
    BOOL bSuccess = FALSE;
    const DWORD kBufferSize = 64 * 1024;
    DWORD dwAttributes = INVALID_FILE_ATTRIBUTES;
    DWORD dwOriginalAttributes = INVALID_FILE_ATTRIBUTES;
    BOOL bTargetExists = FALSE;
    FILETIME ftCreate = {};
    FILETIME ftAccess = {};
    FILETIME ftWrite = {};
    DWORD dwCreateAttributes = FILE_ATTRIBUTE_NORMAL;

    hSource = CreateFile(pszStagingPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSource == INVALID_HANDLE_VALUE) {
        dwError = GetLastError();
        goto Cleanup;
    }

    dwAttributes = GetFileAttributes(pszTargetPath);
    dwOriginalAttributes = dwAttributes;
    bTargetExists = (dwAttributes != INVALID_FILE_ATTRIBUTES);

    if (bTargetExists) {
        HANDLE hExisting = CreateFile(pszTargetPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hExisting != INVALID_HANDLE_VALUE) {
            GetFileTime(hExisting, &ftCreate, &ftAccess, &ftWrite);
            CloseHandle(hExisting);
        }
    }

    if (bTargetExists && dwOriginalAttributes != INVALID_FILE_ATTRIBUTES && (dwOriginalAttributes & FILE_ATTRIBUTE_READONLY)) {
        if (!SetFileAttributes(pszTargetPath, dwOriginalAttributes & ~FILE_ATTRIBUTE_READONLY)) {
            dwError = GetLastError();
            goto Cleanup;
        }
    }

    if (bTargetExists && dwOriginalAttributes != INVALID_FILE_ATTRIBUTES) {
        dwCreateAttributes = (dwOriginalAttributes & ~FILE_ATTRIBUTE_READONLY);
    }

    hDest = CreateFile(pszTargetPath, GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, dwCreateAttributes, NULL);
    if (hDest == INVALID_HANDLE_VALUE) {
        dwError = GetLastError();
        goto Cleanup;
    }

    pBuffer = (BYTE*)malloc(kBufferSize);
    if (!pBuffer) {
        dwError = ERROR_OUTOFMEMORY;
        goto Cleanup;
    }

    bSuccess = TRUE;
    SetLastError(ERROR_SUCCESS);
    while (TRUE) {
        DWORD dwReadChunk = 0;
        BOOL bReadOk = ReadFile(hSource, pBuffer, kBufferSize, &dwReadChunk, NULL);
        if (!bReadOk) {
            dwError = GetLastError();
            if (dwError == ERROR_HANDLE_EOF) {
                dwError = ERROR_SUCCESS;
            }
            bSuccess = FALSE;
            break;
        }

        if (dwReadChunk == 0) {
            break; // EOF
        }

        DWORD dwWritten = 0;
        if (!WriteFile(hDest, pBuffer, dwReadChunk, &dwWritten, NULL) || dwWritten != dwReadChunk) {
            dwError = GetLastError();
            bSuccess = FALSE;
            break;
        }
    }

    if (bSuccess) {
        if (!FlushFileBuffers(hDest)) {
            dwError = GetLastError();
            bSuccess = FALSE;
        }
    }

    if (bSuccess && bTargetExists && (ftCreate.dwLowDateTime || ftCreate.dwHighDateTime ||
                                      ftAccess.dwLowDateTime || ftAccess.dwHighDateTime ||
                                      ftWrite.dwLowDateTime || ftWrite.dwHighDateTime)) {
        SetFileTime(hDest, &ftCreate, &ftAccess, &ftWrite);
    }

    if (bTargetExists && dwOriginalAttributes != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributes(pszTargetPath, dwOriginalAttributes);
    }

Cleanup:
    if (pBuffer) free(pBuffer);
    if (hSource != INVALID_HANDLE_VALUE) CloseHandle(hSource);
    if (hDest != INVALID_HANDLE_VALUE) CloseHandle(hDest);

    if (!bSuccess && dwError == ERROR_SUCCESS) {
        dwError = ERROR_GEN_FAILURE;
    }

    SetLastError(dwError);
    return bSuccess;
}

//============================================================================
// RestoreForegroundAfterElevation - Best-effort to regain focus after UAC
//============================================================================
static void RestoreForegroundAfterElevation()
{
    if (!g_hWndMain) return;

    // Bring the main window to the foreground if allowed by the OS focus rules
    ShowWindow(g_hWndMain, SW_SHOWNORMAL);
    SetForegroundWindow(g_hWndMain);
    SetActiveWindow(g_hWndMain);
    if (g_hWndEdit) {
        SetFocus(g_hWndEdit);
    }
}

//============================================================================
// PerformElevatedSave - Try saving with elevation on access denied
//============================================================================
BOOL PerformElevatedSave(LPCWSTR pszTargetPath)
{
    WCHAR szPrompt[256];
    LoadStringResource(IDS_ELEVATE_SAVE_PROMPT, szPrompt, 256);

    WCHAR szTitle[64];
    LoadStringResource(IDS_CONFIRM, szTitle, 64);

    int result = MessageBox(g_hWndMain, szPrompt, szTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (result != IDYES) {
        return FALSE;
    }

    WCHAR szStagingPath[EXTENDED_PATH_MAX];
    if (!CreateElevatedSaveStagingFile(szStagingPath, EXTENDED_PATH_MAX)) {
        return FALSE;
    }

    DWORD dwElevatedError = 0;
    BOOL bSuccess = RunElevatedSave(szStagingPath, pszTargetPath, &dwElevatedError);
    DeleteFile(szStagingPath);

    // Attempt to bring focus back after UAC prompt
    RestoreForegroundAfterElevation();

    if (!bSuccess) {
        if (dwElevatedError == ERROR_CANCELLED) {
            return FALSE;
        }

        // If the elevated worker propagated a Win32 error, map to create/write failure to reuse messaging
        if (dwElevatedError == ERROR_ACCESS_DENIED || dwElevatedError == ERROR_SHARING_VIOLATION) {
            ShowSaveTextFailure(SAVE_TEXT_FAILURE_WRITE, dwElevatedError);
        } else {
            ShowError(IDS_ERROR_ELEVATED_SAVE, L"Could not save with administrator permissions", dwElevatedError);
        }
        return FALSE;
    }

    FinalizeSuccessfulSave(pszTargetPath, TRUE);
    return TRUE;
}

//============================================================================
// URL Detection and Handling Functions
//============================================================================

//============================================================================
// GetURLAtCursor - Extract URL text at cursor position
//
// Parameters:
//   hWndEdit - RichEdit control handle
//   pszURL - Buffer to receive URL text (must be at least cchMax WCHARs)
//   cchMax - Maximum buffer size in WCHARs
//   pRange - Optional, receives character range of the URL
//
// Returns: TRUE if URL found and extracted, FALSE otherwise
//============================================================================
BOOL GetURLAtCursor(HWND hWndEdit, LPWSTR pszURL, int cchMax, CHARRANGE* pRange)
{
    // Get current cursor position
    CHARRANGE savedSel;
    SendMessage(hWndEdit, EM_EXGETSEL, 0, (LPARAM)&savedSel);
    LONG cursorPos = savedSel.cpMin;
    
    // Fast path: Check if cursor is within cached URL range from EN_LINK
    if (g_lastURLRange.cpMin != -1 && 
        cursorPos >= g_lastURLRange.cpMin && 
        cursorPos < g_lastURLRange.cpMax) {
        
        // Extract URL from known range (instant)
        TEXTRANGE tr;
        tr.chrg = g_lastURLRange;
        tr.lpstrText = pszURL;
        
        if (tr.chrg.cpMax - tr.chrg.cpMin < cchMax) {
            SendMessage(hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
            if (pRange) {
                *pRange = g_lastURLRange;
            }
            return TRUE;
        }
    }
    
    // Suppress redraws while we move the selection for character-format probing.
    // Each EM_EXSETSEL call would otherwise repaint the selection highlight,
    // causing a visible ~1 s delay for a typical URL length.
    SendMessage(hWndEdit, WM_SETREDRAW, FALSE, 0);
    
    // Quick check: is cursor in a URL? (single check, minimal cost)
    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(CHARFORMAT2));
    cf.cbSize = sizeof(CHARFORMAT2);
    cf.dwMask = CFM_LINK;
    
    CHARRANGE cr;
    cr.cpMin = cursorPos;
    cr.cpMax = cursorPos + 1;
    SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessage(hWndEdit, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    
    if (!(cf.dwEffects & CFE_LINK)) {
        SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&savedSel);
        SendMessage(hWndEdit, WM_SETREDRAW, TRUE, 0);
        return FALSE;  // Not in URL
    }
    
    // Scan outward from cursorPos using CFE_LINK character formatting to find URL boundaries.
    // EM_FINDWORDBREAK is NOT used: URLs span multiple word-break units (e.g. two adjacent URLs
    // separated only by punctuation have no spaces, so the entire run is one "word"), which
    // causes boundary-bounded scans to include characters from a neighbouring URL.
    // Character-by-character CFE_LINK scanning is authoritative: RichEdit sets CFE_LINK
    // exactly on each URL's characters, and non-URL separator characters (quotes, colons, etc.)
    // are not CFE_LINK even when adjacent to a URL.
    
    // Scan backward from cursorPos to find URL start
    LONG wordLeft = cursorPos;
    for (LONG pos = cursorPos - 1; pos >= 0; pos--) {
        cr.cpMin = pos;
        cr.cpMax = pos + 1;
        SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        SendMessage(hWndEdit, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        if (cf.dwEffects & CFE_LINK) {
            wordLeft = pos;
        } else {
            break;
        }
    }
    
    // Scan forward from cursorPos to find URL end
    GETTEXTLENGTHEX gtl;
    gtl.flags = GTL_DEFAULT;
    gtl.codepage = 1200;
    LONG docLen = SendMessage(hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    LONG wordRight = cursorPos;
    for (LONG pos = cursorPos; pos < docLen; pos++) {
        cr.cpMin = pos;
        cr.cpMax = pos + 1;
        SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        SendMessage(hWndEdit, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        if (cf.dwEffects & CFE_LINK) {
            wordRight = pos + 1;
        } else {
            break;
        }
    }
    
    // Extract URL text
    int urlLen = wordRight - wordLeft;
    if (urlLen <= 0 || urlLen >= cchMax) {
        SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&savedSel);
        SendMessage(hWndEdit, WM_SETREDRAW, TRUE, 0);
        return FALSE;
    }
    
    TEXTRANGE tr;
    tr.chrg.cpMin = wordLeft;
    tr.chrg.cpMax = wordRight;
    tr.lpstrText = pszURL;
    SendMessage(hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    
    if (pRange) {
        pRange->cpMin = wordLeft;
        pRange->cpMax = wordRight;
    }
    
    // Cache for next time
    g_lastURLRange.cpMin = wordLeft;
    g_lastURLRange.cpMax = wordRight;
    
    // Restore selection (single restore at end)
    SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&savedSel);
    SendMessage(hWndEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hWndEdit, NULL, FALSE);
    
    return TRUE;
}





//============================================================================
// OpenURL - Open URL in default browser/handler
//
// Opens the URL using ShellExecute with no validation.
// Shows error MessageBox on failure, writes error code to debug output.
// Silent on success (no status bar message).
//============================================================================
void OpenURL(HWND hwnd, LPCWSTR pszURL)
{
    // Validate URL is not empty
    if (!pszURL || pszURL[0] == L'\0') {
        return;
    }
    
    // Open URL with ShellExecute (no validation, trust the OS)
    HINSTANCE result = ShellExecute(
        hwnd,
        L"open",
        pszURL,
        NULL,
        NULL,
        SW_SHOWNORMAL
    );
    
    // ShellExecute returns value > 32 on success
    INT_PTR errorCode = (INT_PTR)result;
    if (errorCode <= 32) {
        // Write error code to debug output
        WCHAR szDebug[256];
        _snwprintf(szDebug, 256, L"ShellExecute failed with error code: %d\n", (int)errorCode);
        OutputDebugString(szDebug);
        
        // Show basic error message to user
        WCHAR szError[128];
        LoadStringResource(IDS_ERROR_OPEN_URL, szError, 128);
        
        WCHAR szMessage[512];
        _snwprintf(szMessage, 512, L"%s\n\n%s", szError, pszURL);
        
        MessageBox(hwnd, szMessage, L"RichEditor", MB_OK | MB_ICONERROR);
    }
    // Silent on success - no status bar message, no feedback
}

//============================================================================
// CopyURLToClipboard - Copy URL text to clipboard
//
// Silently copies the URL to clipboard with no visual feedback.
//============================================================================
void CopyURLToClipboard(HWND hwnd, LPCWSTR pszURL)
{
    if (!pszURL || pszURL[0] == L'\0') {
        return;
    }
    
    if (!OpenClipboard(hwnd)) {
        return;
    }
    
    EmptyClipboard();
    
    // Calculate size needed (length + null terminator)
    size_t len = wcslen(pszURL);
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(WCHAR));
    
    if (hGlob) {
        LPWSTR pszCopy = (LPWSTR)GlobalLock(hGlob);
        if (pszCopy) {
            wcscpy(pszCopy, pszURL);
            GlobalUnlock(hGlob);
            SetClipboardData(CF_UNICODETEXT, hGlob);
        } else {
            GlobalFree(hGlob);
        }
    }
    
    CloseClipboard();
    // Silent operation - no confirmation message
}

//============================================================================
// CreateRichEditControl - Create and configure RichEdit control
//============================================================================
HWND CreateRichEditControl(HWND hwndParent)
{
    // Create style based on word wrap setting
    DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | 
                  ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL;
    
    if (!g_bWordWrap) {
        // Add horizontal scroll when word wrap is off
        style |= WS_HSCROLL | ES_AUTOHSCROLL;
    }
    
    // Try to create RichEdit control with primary class
    HWND hwndEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        g_szRichEditClassName,  // Use detected class name from LoadRichEditLibrary()
        L"",
        style,
        0, 0, 0, 0,
        hwndParent,
        (HMENU)IDC_RICHEDIT,
        GetModuleHandle(NULL),
        NULL
    );
    
    // Fallback chain for v8.0+ if primary class (RichEditD2DPT) failed
    // This gracefully handles cases where D2DPT isn't available
    if (!hwndEdit && g_fRichEditVersion >= 8.0f) {
        // Try RichEditD2D (Direct2D without Paint Through)
        hwndEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"RichEditD2D", L"", style,
            0, 0, 0, 0, hwndParent, (HMENU)IDC_RICHEDIT,
            GetModuleHandle(NULL), NULL
        );
        if (hwndEdit) {
            wcscpy(g_szRichEditClassName, L"RichEditD2D");
        }
    }
    
    if (!hwndEdit && g_fRichEditVersion >= 6.0f) {
        // Try RichEdit60W (older Office versions)
        hwndEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"RichEdit60W", L"", style,
            0, 0, 0, 0, hwndParent, (HMENU)IDC_RICHEDIT,
            GetModuleHandle(NULL), NULL
        );
        if (hwndEdit) {
            wcscpy(g_szRichEditClassName, L"RichEdit60W");
        }
    }
    
    if (!hwndEdit && g_fRichEditVersion >= 5.0f) {
        // Try RichEdit20W (universal fallback for modern DLLs)
        hwndEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"RichEdit20W", L"", style,
            0, 0, 0, 0, hwndParent, (HMENU)IDC_RICHEDIT,
            GetModuleHandle(NULL), NULL
        );
        if (hwndEdit) {
            wcscpy(g_szRichEditClassName, L"RichEdit20W");
        }
    }
    
    if (!hwndEdit && g_fRichEditVersion >= 4.0f) {
        // Try RICHEDIT50W (MSFTEDIT.DLL)
        hwndEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"", style,
            0, 0, 0, 0, hwndParent, (HMENU)IDC_RICHEDIT,
            GetModuleHandle(NULL), NULL
        );
        if (hwndEdit) {
            wcscpy(g_szRichEditClassName, L"RICHEDIT50W");
        }
    }
    
    if (!hwndEdit) {
        // Final fallback: Try ancient RICHEDIT (v1.0)
        hwndEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"RICHEDIT", L"", style,
            0, 0, 0, 0, hwndParent, (HMENU)IDC_RICHEDIT,
            GetModuleHandle(NULL), NULL
        );
        if (hwndEdit) {
            wcscpy(g_szRichEditClassName, L"RICHEDIT");
        }
    }
    
    if (hwndEdit) {
        // Set undo limit
        SendMessage(hwndEdit, EM_SETUNDOLIMIT, 100, 0);
        
        // Enable automatic URL detection only if the DetectURLs INI setting is on.
        // AURL_ENABLEURL provides native accessibility (screen reader link roles via
        // IAccessible/UIA, context menu "Open URL", EN_LINK click/hover), but does
        // O(cursor-position) internal work on every EN_SELCHANGE — unusably slow on
        // large files. Users with large files should set DetectURLs=0 in the INI.
        // AURL_ENABLEURL must be sent BEFORE TM_PLAINTEXT on some RichEdit versions.
        if (g_bAutoURLEnabled) {
            SendMessage(hwndEdit, EM_AUTOURLDETECT, AURL_ENABLEURL, 0);
        }
        
        // Set plain text mode (must follow AURL_ENABLEURL on some RichEdit versions)
        SendMessage(hwndEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        
        // Set event mask for notifications; ENM_LINK is required for EN_LINK
        // (mouse hover hand cursor, click-to-open) which works with AURL_ENABLEURL.
        SendMessage(hwndEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE | ENM_LINK);
        
        // Set large text limit (2GB)
        SendMessage(hwndEdit, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
        
        // Set read-only mode if specified
        if (g_bReadOnly) {
            SendMessage(hwndEdit, EM_SETREADONLY, TRUE, 0);
        }

        // Apply saved zoom level before word wrap so the first layout is zoom-aware
        if (g_nZoomPercent != 100) {
            SendMessage(hwndEdit, EM_SETZOOM, (WPARAM)g_nZoomPercent, (LPARAM)100);
        }

        ApplyWordWrap(hwndEdit);
        
        // Subclass the RichEdit control to intercept WM_KEYDOWN
        g_pfnOriginalEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
        
        // Set focus to editor
        SetFocus(hwndEdit);
    }
    
    return hwndEdit;
}

//============================================================================
// CreateStatusBar - Create status bar control
//============================================================================
HWND CreateStatusBar(HWND hwndParent)
{
    HWND hwndStatus = CreateWindowEx(
        0,
        STATUSCLASSNAME,
        NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwndParent,
        (HMENU)IDC_STATUSBAR,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (hwndStatus) {
        // Get the actual window size to set parts correctly
        RECT rcParent;
        GetClientRect(hwndParent, &rcParent);
        
        // Set parts: part 0 for main status, part 1 (200px) for filter
        // Parts array contains right edge positions of each part
        int parts[2];
        parts[0] = rcParent.right - 200;  // Part 0 ends 200px from right
        parts[1] = -1;                     // Part 1 extends to right edge
        SendMessage(hwndStatus, SB_SETPARTS, 2, (LPARAM)parts);
    }
    
    return hwndStatus;
}

//============================================================================
// CalculateTabAwareColumn - Calculate visual column position with tab expansion
// 
// Parameters:
//   pszLineText - text from start of line to cursor
//   charPosition - number of characters from line start (buffer position)
//
// Returns: Visual column number (1-based), accounting for tab stops
//
// Tab behavior: Each tab moves to the next tab stop (multiple of g_nTabSize)
// Example with TabSize=8:
//   ""      -> column 1
//   "a"     -> column 2
//   "ahoj"  -> column 5
//   "\t"    -> column 9 (jumps to next tab stop after column 1)
//   "ahoj\t" -> column 9 (from column 5, jumps to next tab stop)
//   "ahoj\ta" -> column 10
//============================================================================
int CalculateTabAwareColumn(LPCWSTR pszLineText, int charPosition)
{
    int visualColumn = 1;  // Start at column 1
    
    for (int i = 0; i < charPosition; i++) {
        if (pszLineText[i] == L'\t') {
            // Move to next tab stop (next multiple of g_nTabSize)
            visualColumn = ((visualColumn - 1) / g_nTabSize + 1) * g_nTabSize + 1;
        } else {
            // Regular character, advance one column
            visualColumn++;
        }
    }
    
    return visualColumn;
}

//============================================================================
// RebuildLineIndex - Scan document text and cache character offset of each line start.
// Called lazily before any physical-line query; O(N) once per content change.
// Cursor-movement queries then use std::upper_bound: O(log N), no RichEdit API call.
//============================================================================
static void RebuildLineIndex()
{
    g_lineStarts.clear();
    g_lineStarts.push_back(0);  // line 1 always starts at character 0

    int totalLen = g_nLastTextLen;
    if (totalLen <= 0) {
        g_bLineIndexDirty = false;
        return;
    }

    // Reserve a rough estimate to avoid repeated reallocations (assume avg ~40 chars/line)
    g_lineStarts.reserve(totalLen / 40 + 2);

    const int CHUNK = 262144;  // 256 K chars per request (~512 KB); few Win32 round-trips
    LPWSTR buf = (LPWSTR)malloc((CHUNK + 1) * sizeof(WCHAR));
    if (!buf) { g_bLineIndexDirty = false; return; }

    bool prevCR = false;  // tracks \r at end of previous chunk for \r\n pair handling
    int pos = 0;
    while (pos < totalLen) {
        int end = (pos + CHUNK < totalLen) ? pos + CHUNK : totalLen;
        TEXTRANGE tr;
        tr.chrg.cpMin = pos;
        tr.chrg.cpMax = end;
        tr.lpstrText = buf;
        int got = (int)SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
        if (got <= 0) break;

        for (int i = 0; i < got; i++) {
            WCHAR c = buf[i];
            if (prevCR && c == L'\n') {
                // \r\n pair: the tentative line start (pushed after \r) must be after the \n
                g_lineStarts.back() = (LONG)(pos + i + 1);
                prevCR = false;
            } else if (c == L'\r') {
                g_lineStarts.push_back((LONG)(pos + i + 1));
                prevCR = true;
            } else if (c == L'\n') {
                g_lineStarts.push_back((LONG)(pos + i + 1));
                prevCR = false;
            } else {
                prevCR = false;
            }
        }
        pos = end;
    }
    free(buf);
    g_bLineIndexDirty = false;
}

//============================================================================
// UpdateStatusBar - Update status bar with current position and info
//============================================================================
void UpdateStatusBar()
{
    if (!g_hWndStatus || !g_hWndEdit) return;
    
    // Get cursor position
    CHARRANGE cr;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    
    int visualLine, visualCol;
    int physicalLine, physicalCol;

    auto getPhysicalLineAndCol = [&](int charPos, int* outLine, int* outCol) {
        // Use the pre-built line-starts index for O(log N) lookup.
        // RebuildLineIndex() is O(N) but runs at most once per content change,
        // not on every cursor movement.
        if (g_bLineIndexDirty) RebuildLineIndex();

        // Binary search: find the last entry whose value is <= charPos.
        auto it = std::upper_bound(g_lineStarts.begin(), g_lineStarts.end(), (LONG)charPos);
        if (it != g_lineStarts.begin()) --it;

        *outLine   = (int)(it - g_lineStarts.begin()) + 1;  // 1-based line number
        int lineStart = (int)*it;

        int charCount = charPos - lineStart;
        if (charCount > 0) {
            LPWSTR lineText = (LPWSTR)malloc((charCount + 1) * sizeof(WCHAR));
            if (lineText) {
                TEXTRANGE tr;
                tr.chrg.cpMin = lineStart;
                tr.chrg.cpMax = charPos;
                tr.lpstrText = lineText;
                int retrieved = (int)SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
                *outCol = (retrieved > 0) ? CalculateTabAwareColumn(lineText, charCount) : 1;
                free(lineText);
            } else {
                *outCol = charCount + 1;
            }
        } else {
            *outCol = 1;
        }
    };
    
    if (g_bWordWrap) {
        // When word wrap is ON:
        // - Visual line/col: counts display lines including soft wraps (from RichEdit)
        // - Physical line/col: counts only hard line breaks by parsing the text
        
        // Get visual (wrapped) line and its start via TOM.
        // GetIndex(tomLine) uses RichEdit's internal line table and does NOT trigger
        // a full D2D layout pass, unlike EM_EXLINEFROMCHAR on RichEdit 8.
        int currentLineStart = 0;
        if (g_pTextDoc) {
            ITextRange* pRange = NULL;
            if (SUCCEEDED(g_pTextDoc->Range(cr.cpMin, cr.cpMin, &pRange)) && pRange) {
                LONG visLine = 1;
                pRange->GetIndex(tomLine, &visLine);
                visualLine = (int)visLine;
                // Collapse to start of visual line to get line start position
                LONG delta = 0;
                pRange->StartOf(tomLine, 0, &delta);
                LONG visLineStart = 0;
                pRange->GetStart(&visLineStart);
                pRange->Release();
                currentLineStart = (int)visLineStart;
            } else {
                if (pRange) { pRange->Release(); pRange = NULL; }
                // TOM Range failed — fall back to message-based API
                int lineIdx = (int)SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin);
                visualLine = lineIdx + 1;
                currentLineStart = (int)SendMessage(g_hWndEdit, EM_LINEINDEX, lineIdx, 0);
            }
        } else {
            // TOM unavailable — fall back to message-based API
            int lineIdx = (int)SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin);
            visualLine = lineIdx + 1;
            currentLineStart = (int)SendMessage(g_hWndEdit, EM_LINEINDEX, lineIdx, 0);
        }
        
        // Calculate tab-aware visual column
        int charCount = cr.cpMin - currentLineStart;
        if (charCount > 0) {
            // Get line text from currentLineStart to cursor
            LPWSTR lineText = (LPWSTR)malloc((charCount + 1) * sizeof(WCHAR));
            if (lineText) {
                TEXTRANGE tr;
                tr.chrg.cpMin = currentLineStart;
                tr.chrg.cpMax = cr.cpMin;
                tr.lpstrText = lineText;
                int retrieved = (int)SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
                if (retrieved > 0) {
                    visualCol = CalculateTabAwareColumn(lineText, charCount);
                } else {
                    visualCol = 1;  // Fallback
                }
                free(lineText);
            } else {
                visualCol = charCount + 1;  // Fallback if malloc fails
            }
        } else {
            visualCol = 1;  // At start of line
        }
        
        getPhysicalLineAndCol(cr.cpMin, &physicalLine, &physicalCol);
        
    } else {
        // When word wrap is OFF:
        if (g_fRichEditVersion >= 8.0f) {
            // RichEdit 8+ may visually segment long lines; show physical (hard-break) lines
            // to avoid line counts drifting from actual newline-based lines.
            getPhysicalLineAndCol(cr.cpMin, &physicalLine, &physicalCol);
            visualLine = physicalLine;
            visualCol = physicalCol;
        } else {
            // Older RichEdit: visual = physical (no soft wraps)
            visualLine = (int)SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin) + 1;
            int lineStart = (int)SendMessage(g_hWndEdit, EM_LINEINDEX, visualLine - 1, 0);

            // Calculate tab-aware column
            int charCount = cr.cpMin - lineStart;
            if (charCount > 0) {
                // Get line text from lineStart to cursor
                LPWSTR lineText = (LPWSTR)malloc((charCount + 1) * sizeof(WCHAR));
                if (lineText) {
                    TEXTRANGE tr;
                    tr.chrg.cpMin = lineStart;
                    tr.chrg.cpMax = cr.cpMin;
                    tr.lpstrText = lineText;
                    int retrieved = (int)SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
                    if (retrieved > 0) {
                        visualCol = CalculateTabAwareColumn(lineText, charCount);
                    } else {
                        visualCol = 1;  // Fallback
                    }
                    free(lineText);
                } else {
                    visualCol = charCount + 1;  // Fallback if malloc fails
                }
            } else {
                visualCol = 1;  // At start of line
            }

            physicalLine = visualLine;
            physicalCol = visualCol;
        }
    }
    
    // Get character at cursor (handle surrogate pairs for characters > U+FFFF)
    WCHAR charInfo[128] = L"";
    int textLen = g_nLastTextLen;  // Use cached length; updated by EN_CHANGE, avoids O(N) call per keystroke
    
    if (cr.cpMin < textLen) {
        // Get up to 2 WCHARs to handle surrogate pairs
        TEXTRANGE tr;
        WCHAR buffer[3] = {0};
        tr.chrg.cpMin = cr.cpMin;
        tr.chrg.cpMax = cr.cpMin + 2;
        tr.lpstrText = buffer;
        int charsRead = (int)SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
        
        WCHAR firstChar = buffer[0];
        
        // Check if this is a high surrogate (U+D800 to U+DBFF)
        if (firstChar >= 0xD800 && firstChar <= 0xDBFF && charsRead >= 2) {
            WCHAR secondChar = buffer[1];
            // Check if followed by low surrogate (U+DC00 to U+DFFF)
            if (secondChar >= 0xDC00 && secondChar <= 0xDFFF) {
                // This is a surrogate pair - calculate the full Unicode code point
                unsigned int codepoint = 0x10000 + 
                    ((firstChar - 0xD800) << 10) + 
                    (secondChar - 0xDC00);
                
                // Format with the surrogate pair displayed as the character
                WCHAR szChar[32], szDec[32];
                LoadStringResource(IDS_STATUS_CHAR, szChar, 32);
                LoadStringResource(IDS_STATUS_DEC, szDec, 32);
                _snwprintf(charInfo, 128, L"%s: '%c%c' (%s: %u, U+%X)",
                           szChar, firstChar, secondChar, szDec, codepoint, codepoint);
            } else {
                // High surrogate without low surrogate (invalid)
                WCHAR szChar[32], szInvalid[64];
                LoadStringResource(IDS_STATUS_CHAR, szChar, 32);
                LoadStringResource(IDS_STATUS_INVALID_SURROGATE, szInvalid, 64);
                _snwprintf(charInfo, 128, L"%s: (%s: 0x%04X)",
                           szChar, szInvalid, (unsigned int)firstChar);
            }
        } else if (firstChar >= 0xDC00 && firstChar <= 0xDFFF) {
            // Low surrogate without high surrogate (invalid)
            WCHAR szChar[32], szInvalid[64];
            LoadStringResource(IDS_STATUS_CHAR, szChar, 32);
            LoadStringResource(IDS_STATUS_INVALID_SURROGATE, szInvalid, 64);
            _snwprintf(charInfo, 128, L"%s: (%s: 0x%04X)",
                       szChar, szInvalid, (unsigned int)firstChar);
        } else {
            // Regular BMP character (U+0000 to U+FFFF, excluding surrogates)
            WCHAR szChar[32], szDec[32];
            LoadStringResource(IDS_STATUS_CHAR, szChar, 32);
            LoadStringResource(IDS_STATUS_DEC, szDec, 32);
            if (firstChar >= 32 && firstChar != 127) {
                // Printable character
                _snwprintf(charInfo, 128, L"%s: '%lc' (%s: %u, U+%04X)",
                           szChar, firstChar, szDec, (unsigned int)firstChar, (unsigned int)firstChar);
            } else {
                // Control character or non-printable
                _snwprintf(charInfo, 128, L"%s: (%s: %u, U+%04X)",
                           szChar, szDec, (unsigned int)firstChar, (unsigned int)firstChar);
            }
        }
    } else {
        // Cursor is at end of file or empty file
        WCHAR szChar[32], szEOF[32];
        LoadStringResource(IDS_STATUS_CHAR, szChar, 32);
        LoadStringResource(IDS_STATUS_EOF, szEOF, 32);
        _snwprintf(charInfo, 128, L"%s: %s", szChar, szEOF);
    }
    
    // Format position string
    WCHAR posInfo[128];
    WCHAR szLn[32], szCol[32];
    LoadStringResource(IDS_STATUS_LINE, szLn, 32);
    LoadStringResource(IDS_STATUS_COLUMN, szCol, 32);
    
    if (g_bWordWrap) {
        // When word wrap is on, always show both visual and physical positions
        // visualLine/visualCol: includes soft wraps (displayed lines)
        // physicalLine/physicalCol: count only hard line breaks
        _snwprintf(posInfo, 128, L"%s %d, %s %d / %d,%d",
                   szLn, visualLine, szCol, visualCol, physicalLine, physicalCol);
    } else {
        // When word wrap is off, show only one position
        _snwprintf(posInfo, 128, L"%s %d, %s %d",
                   szLn, visualLine, szCol, visualCol);
    }
    
    // Check if filter status bar is active
    if (g_bFilterStatusBarActive) {
        // Show filter result instead of normal position/char info
        SendMessage(g_hWndStatus, SB_SETTEXT, 0, (LPARAM)g_szFilterStatusBarText);
    } else {
        // Format status text (without filename - it's already in title bar)
        WCHAR szStatus[512];

        // Build optional zoom suffix
        WCHAR szZoom[16] = L"";
        {
            DWORD nNum = 0, nDen = 0;
            BOOL bZoomed = (BOOL)SendMessage(g_hWndEdit, EM_GETZOOM, (WPARAM)&nNum, (LPARAM)&nDen);
            if (bZoomed && nNum > 0 && nDen > 0 && nNum != nDen) {
                int pct = MulDiv((int)nNum, 100, (int)nDen);
                _snwprintf(szZoom, 16, L"    %d%%", pct);
            }
        }

        if (!g_bAutoURLEnabled) {
            WCHAR szURLOff[32];
            LoadStringResource(IDS_STATUS_AUTOURL_OFF, szURLOff, 32);
            _snwprintf(szStatus, 512, L"%s    %s    %s%s", posInfo, charInfo, szURLOff, szZoom);
        } else {
            _snwprintf(szStatus, 512, L"%s    %s%s", posInfo, charInfo, szZoom);
        }
        
        SendMessage(g_hWndStatus, SB_SETTEXT, 0, (LPARAM)szStatus);
    }
    
    // Update filter display in separate status bar part
    UpdateFilterDisplay();
}

//============================================================================
// UpdateTitle - Update window title with filename and modified state
//============================================================================
void UpdateTitle(HWND hwnd)
{
    WCHAR szTitle[MAX_PATH + 100];  // Increased size for [Interactive Mode] and [Resumed]
    WCHAR szUntitled[64];
    WCHAR szReadOnly[32];
    WCHAR szResumed[32];
    
    // Use provided hwnd or fall back to g_hWndMain
    HWND targetWnd = hwnd ? hwnd : g_hWndMain;
    if (!targetWnd) return;
    
    LoadStringResource(IDS_UNTITLED, szUntitled, 64);
    
    // Build base title
    if (g_szFileTitle[0]) {
        _snwprintf(szTitle, MAX_PATH + 100, L"%s%s",
                   g_bModified ? L"*" : L"", g_szFileTitle);
    } else {
        _snwprintf(szTitle, MAX_PATH + 100, L"%s%s",
                   g_bModified ? L"*" : L"", szUntitled);
    }
    
    // Append [Read-Only] indicator if in read-only mode
    if (g_bReadOnly) {
        LoadStringResource(IDS_READONLY, szReadOnly, 32);
        wcscat(szTitle, L" [");
        wcscat(szTitle, szReadOnly);
        wcscat(szTitle, L"]");
    }
    
    // Append [Resumed] indicator if this is a resumed file
    if (g_bIsResumedFile) {
        LoadStringResource(IDS_RESUMED, szResumed, 32);
        wcscat(szTitle, L" [");
        wcscat(szTitle, szResumed);
        wcscat(szTitle, L"]");
    }
    
    // Append [Interactive Mode] indicator if in REPL mode
    if (g_bREPLMode) {
        WCHAR szInteractiveMode[64];
        LoadStringResource(IDS_INTERACTIVE_MODE_INDICATOR, szInteractiveMode, 64);
        wcscat(szTitle, L" [");
        wcscat(szTitle, szInteractiveMode);
        wcscat(szTitle, L"]");
    }
    
    // Append application name
    wcscat(szTitle, L" - RichEditor");
    
    SetWindowText(targetWnd, szTitle);
}

//============================================================================
// UpdateMenuUndoRedo - Update Undo/Redo menu items with operation type
//============================================================================
void UpdateMenuUndoRedo(HMENU hMenu)
{
    if (!hMenu) return;
    
    BOOL canUndo = SendMessage(g_hWndEdit, EM_CANUNDO, 0, 0);
    BOOL canRedo = SendMessage(g_hWndEdit, EM_CANREDO, 0, 0);
    
    // Update Undo menu item
    WCHAR szUndoText[64];
    if (canUndo) {
        // Get undo type from RichEdit (or use custom operation flags)
        LRESULT undoType = (g_bLastOperationWasFilter || g_bLastOperationWasReplace) ? 0 : SendMessage(g_hWndEdit, EM_GETUNDONAME, 0, 0);
        
        UINT stringID = IDS_UNDO;
        if (g_bLastOperationWasFilter) {
            stringID = IDS_UNDO_FILTER;
        } else if (g_bLastOperationWasReplace) {
            stringID = IDS_UNDO_REPLACE;
        } else {
            switch (undoType) {
                case 1: stringID = IDS_UNDO_TYPING;   break; // UID_TYPING
                case 2: stringID = IDS_UNDO_DELETE;   break; // UID_DELETE
                case 3: stringID = IDS_UNDO_DRAGDROP; break; // UID_DRAGDROP
                case 4: stringID = IDS_UNDO_CUT;      break; // UID_CUT
                case 5: stringID = IDS_UNDO_PASTE;    break; // UID_PASTE
                default: stringID = IDS_UNDO;         break; // UID_UNKNOWN
            }
        }
        LoadStringResource(stringID, szUndoText, 64);
    } else {
        LoadStringResource(IDS_UNDO, szUndoText, 64);
    }
    
    // Add keyboard shortcut to text
    WCHAR szUndoFinal[80];
    wcscpy(szUndoFinal, szUndoText);
    wcscat(szUndoFinal, L"\tCtrl+Z");
    
    MENUITEMINFO mii = {};
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STRING | MIIM_STATE;
    mii.dwTypeData = szUndoFinal;
    mii.fState = canUndo ? MFS_ENABLED : MFS_GRAYED;
    SetMenuItemInfo(hMenu, ID_EDIT_UNDO, FALSE, &mii);
    
    // Update Redo menu item
    WCHAR szRedoText[64];
    if (canRedo) {
        // Get redo type from RichEdit
        LRESULT redoType = SendMessage(g_hWndEdit, EM_GETREDONAME, 0, 0);
        
        UINT stringID = IDS_REDO;
        switch (redoType) {
            case 1: stringID = IDS_REDO_TYPING;   break; // UID_TYPING
            case 2: stringID = IDS_REDO_DELETE;   break; // UID_DELETE
            case 3: stringID = IDS_REDO_DRAGDROP; break; // UID_DRAGDROP
            case 4: stringID = IDS_REDO_CUT;      break; // UID_CUT
            case 5: stringID = IDS_REDO_PASTE;    break; // UID_PASTE
            // Note: RichEdit doesn't know about filters or replace, so they show as UID_UNKNOWN
            // We can't distinguish between them after undo, so just show generic "Redo"
            default: stringID = IDS_REDO;         break; // UID_UNKNOWN, filter, or replace
        }
        LoadStringResource(stringID, szRedoText, 64);
    } else {
        LoadStringResource(IDS_REDO, szRedoText, 64);
    }
    
    // Add keyboard shortcut to text
    WCHAR szRedoFinal[80];
    wcscpy(szRedoFinal, szRedoText);
    wcscat(szRedoFinal, L"\tCtrl+Y");
    
    mii.dwTypeData = szRedoFinal;
    mii.fState = canRedo ? MFS_ENABLED : MFS_GRAYED;
    SetMenuItemInfo(hMenu, ID_EDIT_REDO, FALSE, &mii);
    
    // Update Cut, Copy, Paste menu items based on current state
    // (Same logic as context menu for consistency)
    BOOL canPaste = SendMessage(g_hWndEdit, EM_CANPASTE, 0, 0);
    
    CHARRANGE cr;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    BOOL hasSelection = (cr.cpMin != cr.cpMax);
    
    // Update Cut - enabled only if there's a selection
    mii.fMask = MIIM_STATE;
    mii.fState = hasSelection ? MFS_ENABLED : MFS_GRAYED;
    SetMenuItemInfo(hMenu, ID_EDIT_CUT, FALSE, &mii);
    
    // Update Copy - enabled only if there's a selection
    mii.fState = hasSelection ? MFS_ENABLED : MFS_GRAYED;
    SetMenuItemInfo(hMenu, ID_EDIT_COPY, FALSE, &mii);
    
    // Update Paste - enabled only if clipboard has pastable content
    mii.fState = canPaste ? MFS_ENABLED : MFS_GRAYED;
    SetMenuItemInfo(hMenu, ID_EDIT_PASTE, FALSE, &mii);
}

//============================================================================
// UTF8ToUTF16 - Convert UTF-8 string to UTF-16 (caller must free result)
//============================================================================
LPWSTR UTF8ToUTF16(LPCSTR pszUTF8)
{
    if (!pszUTF8) return NULL;
    
    // Get required buffer size
    int cchWide = MultiByteToWideChar(CP_UTF8, 0, pszUTF8, -1, NULL, 0);
    if (cchWide == 0) return NULL;
    
    // Allocate buffer
    LPWSTR pszWide = (LPWSTR)malloc(cchWide * sizeof(WCHAR));
    if (!pszWide) return NULL;
    
    // Convert
    if (MultiByteToWideChar(CP_UTF8, 0, pszUTF8, -1, pszWide, cchWide) == 0) {
        free(pszWide);
        return NULL;
    }
    
    return pszWide;
}

//============================================================================
// UTF16ToUTF8 - Convert UTF-16 string to UTF-8 (caller must free result)
//============================================================================
LPSTR UTF16ToUTF8(LPCWSTR pszUTF16)
{
    if (!pszUTF16) return NULL;
    
    // Get required buffer size
    int cbUTF8 = WideCharToMultiByte(CP_UTF8, 0, pszUTF16, -1, NULL, 0, NULL, NULL);
    if (cbUTF8 == 0) return NULL;
    
    // Allocate buffer
    LPSTR pszUTF8 = (LPSTR)malloc(cbUTF8);
    if (!pszUTF8) return NULL;
    
    // Convert
    if (WideCharToMultiByte(CP_UTF8, 0, pszUTF16, -1, pszUTF8, cbUTF8, NULL, NULL) == 0) {
        free(pszUTF8);
        return NULL;
    }
    
    return pszUTF8;
}

//============================================================================
// LoadTextFile - Load a text file into the RichEdit control
// bClearResumeState: TRUE = delete resume file (explicit open), FALSE = keep it
//============================================================================
BOOL LoadTextFile(LPCWSTR pszFileName, BOOL bClearResumeState)
{
    // Open file
    HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        ShowError(IDS_ERROR_OPEN_FILE, L"Could not open file", GetLastError());
        return FALSE;
    }
    
    // Get file size
    DWORD dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        ShowError(IDS_ERROR_GET_FILE_SIZE, L"Could not get file size", GetLastError());
        return FALSE;
    }
    
    // Allocate buffer for UTF-8 data
    LPSTR pszUTF8 = (LPSTR)malloc(dwFileSize + 1);
    if (!pszUTF8) {
        CloseHandle(hFile);
        ShowError(IDS_ERROR_OUT_OF_MEMORY, L"Out of memory", 0);
        return FALSE;
    }
    
    // Read file
    DWORD dwBytesRead;
    if (!ReadFile(hFile, pszUTF8, dwFileSize, &dwBytesRead, NULL)) {
        free(pszUTF8);
        CloseHandle(hFile);
        ShowError(IDS_ERROR_READ_FILE, L"Could not read file", GetLastError());
        return FALSE;
    }
    pszUTF8[dwBytesRead] = '\0';
    CloseHandle(hFile);
    
    // Convert to UTF-16
    LPWSTR pszUTF16 = UTF8ToUTF16(pszUTF8);
    free(pszUTF8);
    
    if (!pszUTF16) {
        ShowError(IDS_ERROR_CONVERT_ENCODING, L"Could not convert file encoding", 0);
        return FALSE;
    }
    
    // Set text in RichEdit control (block EN_CHANGE notifications)
    g_bSettingText = TRUE;
    SetWindowText(g_hWndEdit, pszUTF16);
    g_bSettingText = FALSE;
    // RichEdit resets zoom on WM_SETTEXT; restore the user's zoom level
    if (g_nZoomPercent != 100)
        SendMessage(g_hWndEdit, EM_SETZOOM, (WPARAM)g_nZoomPercent, (LPARAM)100);
    g_bLineIndexDirty = true;  // new file content; EN_CHANGE was suppressed
    free(pszUTF16);
    
    // Apply read-only mode if set
    if (g_bReadOnly) {
        SendMessage(g_hWndEdit, EM_SETREADONLY, TRUE, 0);
    }
    
    // Update state
    wcscpy_s(g_szFileName, MAX_PATH, pszFileName);
    
    // Extract filename from path
    LPCWSTR pszFileNameOnly = wcsrchr(pszFileName, L'\\');
    if (pszFileNameOnly) {
        wcscpy_s(g_szFileTitle, MAX_PATH, pszFileNameOnly + 1);
    } else {
        wcscpy_s(g_szFileTitle, MAX_PATH, pszFileName);
    }
    
    g_bModified = FALSE;
    
    // Update file extension for template filtering
    UpdateFileExtension(pszFileName);

    LoadBookmarksForCurrentFile();
    
    // Clear resume file state when loading a new file (if requested)
    if (bClearResumeState && g_bIsResumedFile) {
        DeleteResumeFile(g_szResumeFilePath);
        g_bIsResumedFile = FALSE;
        g_szResumeFilePath[0] = L'\0';
        g_szOriginalFilePath[0] = L'\0';
    }
    
    UpdateTitle();
    UpdateStatusBar();

    // Add to MRU list (only for explicit opens, not resume loads)
    if (bClearResumeState) {
        AddToMRU(pszFileName);
    }

    // Ensure the editor has keyboard focus and the caret is visible at position 0.
    // This is especially important after a slow load (old RichEdit) where focus may
    // not have been explicitly handed to the edit control by the caller.
    SetFocus(g_hWndEdit);
    SendMessage(g_hWndEdit, EM_SETSEL, 0, 0);
    SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);

    return TRUE;
}

//============================================================================
// SaveTextFileInternal - Save RichEdit control content as UTF-8 text file
// bClearResumeState: TRUE = delete resume file (explicit save), FALSE = keep it (autosave)
// bUpdateState: TRUE = update global state, FALSE = save only
// bShowErrors: TRUE = show UI errors, FALSE = silent
//============================================================================
BOOL SaveTextFileInternal(LPCWSTR pszFileName, BOOL bClearResumeState, DWORD* pLastError, SaveTextFailure* pFailure, BOOL bUpdateState, BOOL bShowErrors)
{
    if (pLastError) {
        *pLastError = 0;
    }
    if (pFailure) {
        *pFailure = SAVE_TEXT_FAILURE_NONE;
    }

    // Get text length
    int cchText = GetWindowTextLength(g_hWndEdit);
    if (cchText < 0) return FALSE;
    
    // Allocate buffer for UTF-16 text
    LPWSTR pszUTF16 = (LPWSTR)malloc((cchText + 1) * sizeof(WCHAR));
    if (!pszUTF16) {
        if (pFailure) {
            *pFailure = SAVE_TEXT_FAILURE_OUT_OF_MEMORY;
        }
        if (bShowErrors) {
            ShowSaveTextFailure(SAVE_TEXT_FAILURE_OUT_OF_MEMORY, 0);
        }
        return FALSE;
    }
    
    // Get text from RichEdit control
    GetWindowText(g_hWndEdit, pszUTF16, cchText + 1);
    
    // Convert to UTF-8
    LPSTR pszUTF8 = UTF16ToUTF8(pszUTF16);
    free(pszUTF16);
    
    if (!pszUTF8) {
        if (pFailure) {
            *pFailure = SAVE_TEXT_FAILURE_CONVERT;
        }
        if (bShowErrors) {
            ShowSaveTextFailure(SAVE_TEXT_FAILURE_CONVERT, 0);
        }
        return FALSE;
    }
    
    // Create file
    HANDLE hFile = CreateFile(pszFileName, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD dwError = GetLastError();
        if (pFailure) {
            *pFailure = SAVE_TEXT_FAILURE_CREATE;
        }
        if (pLastError) {
            *pLastError = dwError;
        }
        free(pszUTF8);
        if (bShowErrors) {
            ShowSaveTextFailure(SAVE_TEXT_FAILURE_CREATE, dwError);
        }
        return FALSE;
    }
    
    // Write UTF-8 data (without BOM)
    DWORD dwBytesToWrite = (DWORD)strlen(pszUTF8);
    DWORD dwBytesWritten;
    if (!WriteFile(hFile, pszUTF8, dwBytesToWrite, &dwBytesWritten, NULL)) {
        DWORD dwError = GetLastError();
        if (pFailure) {
            *pFailure = SAVE_TEXT_FAILURE_WRITE;
        }
        if (pLastError) {
            *pLastError = dwError;
        }
        free(pszUTF8);
        CloseHandle(hFile);
        if (bShowErrors) {
            ShowSaveTextFailure(SAVE_TEXT_FAILURE_WRITE, dwError);
        }
        return FALSE;
    }
    
    free(pszUTF8);
    CloseHandle(hFile);
    
    if (bUpdateState) {
        FinalizeSuccessfulSave(pszFileName, bClearResumeState);
    }
    
    return TRUE;
}

//============================================================================
// FinalizeSuccessfulSave - Update state after a successful save
//============================================================================
void FinalizeSuccessfulSave(LPCWSTR pszFileName, BOOL bClearResumeState)
{
    // Update state
    wcscpy_s(g_szFileName, MAX_PATH, pszFileName);
    
    // Extract filename from path
    LPCWSTR pszFileNameOnly = wcsrchr(pszFileName, L'\\');
    if (pszFileNameOnly) {
        wcscpy_s(g_szFileTitle, MAX_PATH, pszFileNameOnly + 1);
    } else {
        wcscpy_s(g_szFileTitle, MAX_PATH, pszFileName);
    }
    
    // Update file extension for template filtering
    UpdateFileExtension(pszFileName);
    
    // Clear modified flag only for explicit saves, not autosaves
    // (Autosaves shouldn't affect the "unsaved changes" state)
    if (bClearResumeState) {
        g_bModified = FALSE;
    }

    if (bClearResumeState) {
        SaveBookmarksForCurrentFile();
    }
    
    // Clear resume file state after successful save (if requested)
    if (bClearResumeState && g_bIsResumedFile) {
        DeleteResumeFile(g_szResumeFilePath);
        g_bIsResumedFile = FALSE;
        g_szResumeFilePath[0] = L'\0';
        g_szOriginalFilePath[0] = L'\0';
    }
    
    UpdateTitle();
    UpdateStatusBar();
    
    // Add to MRU list (only for explicit saves, not autosaves/resume saves)
    if (bClearResumeState) {
        AddToMRU(pszFileName);
    }
}

//============================================================================
// SaveTextFile - Save RichEdit control content as UTF-8 text file
// bClearResumeState: TRUE = delete resume file (explicit save), FALSE = keep it (autosave)
//============================================================================
BOOL SaveTextFile(LPCWSTR pszFileName, BOOL bClearResumeState)
{
    return SaveTextFileInternal(pszFileName, bClearResumeState, NULL, NULL, TRUE, TRUE);
}

//============================================================================
// SaveTextFileSilently - Save file without showing errors
//============================================================================
BOOL SaveTextFileSilently(LPCWSTR pszFileName, BOOL bClearResumeState, DWORD* pLastError, SaveTextFailure* pFailure)
{
    return SaveTextFileInternal(pszFileName, bClearResumeState, pLastError, pFailure, TRUE, FALSE);
}

//============================================================================
// GetDocumentsPath - Get user's Documents folder path
//============================================================================
void GetDocumentsPath(LPWSTR pszPath, DWORD cchPath)
{
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, 0, pszPath))) {
        return;
    }
    // Fallback to current directory
    GetCurrentDirectory(cchPath, pszPath);
}

//============================================================================
// ShowError - Display error message with Win32 error details
// uMessageID: String resource ID for localized error message
// pszEnglishMessage: English message for debug output
// dwError: Win32 error code (0 if none)
//============================================================================
void ShowError(UINT uMessageID, LPCWSTR pszEnglishMessage, DWORD dwError)
{
    WCHAR szError[512];
    WCHAR szLocalizedMessage[256];
    
    // Load localized error message from resources
    LoadStringResource(uMessageID, szLocalizedMessage, 256);
    
    if (dwError != 0) {
        // Format Win32 error message (this will be localized by Windows)
        WCHAR szErrorMsg[256];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     NULL, dwError, 0, szErrorMsg, 256, NULL);
        
        // Load localized "Error:" prefix
        WCHAR szErrorPrefix[64];
        LoadStringResource(IDS_ERROR_PREFIX, szErrorPrefix, 64);
        
        // Build localized message for UI
        _snwprintf(szError, 512, L"%s\n\n%s: %s", szLocalizedMessage, szErrorPrefix, szErrorMsg);
        
        // Output English message to debugger for troubleshooting
        OutputDebugString(L"RichEditor Error: ");
        OutputDebugString(pszEnglishMessage);
        OutputDebugString(L" - ");
        OutputDebugString(szErrorMsg);
        OutputDebugString(L"\n");
    } else {
        // No Win32 error, just use the message
        wcscpy_s(szError, 512, szLocalizedMessage);
        
        // Output English message to debugger
        OutputDebugString(L"RichEditor Error: ");
        OutputDebugString(pszEnglishMessage);
        OutputDebugString(L"\n");
    }
    
    WCHAR szTitle[64];
    LoadStringResource(IDS_ERROR, szTitle, 64);
    
    MessageBox(g_hWndMain, szError, szTitle, MB_OK | MB_ICONERROR);
}

//============================================================================
// ShowSaveTextFailure - Display save errors based on failure type
//============================================================================
void ShowSaveTextFailure(SaveTextFailure failure, DWORD dwError)
{
    switch (failure) {
        case SAVE_TEXT_FAILURE_OUT_OF_MEMORY:
            ShowError(IDS_ERROR_OUT_OF_MEMORY, L"Out of memory", 0);
            break;
        case SAVE_TEXT_FAILURE_CONVERT:
            ShowError(IDS_ERROR_CONVERT_TEXT_ENCODING, L"Could not convert text encoding", 0);
            break;
        case SAVE_TEXT_FAILURE_CREATE:
            ShowError(IDS_ERROR_CREATE_FILE, L"Could not create file", dwError);
            break;
        case SAVE_TEXT_FAILURE_WRITE:
            ShowError(IDS_ERROR_WRITE_FILE, L"Could not write file", dwError);
            break;
        case SAVE_TEXT_FAILURE_NONE:
        default:
            break;
    }
}

//============================================================================
// Bookmark helpers (Phase 2.9.3)
//============================================================================
static DWORD HashStringFNV1a(LPCWSTR pszText)
{
    DWORD hash = 2166136261u;
    if (!pszText) return hash;

    for (const WCHAR* p = pszText; *p; ++p) {
        WCHAR ch = *p;
        // Normalize to lowercase for stable path hashing
        if (ch >= L'A' && ch <= L'Z') {
            ch = (WCHAR)(ch + (L'a' - L'A'));
        }
        hash ^= (DWORD)ch;
        hash *= 16777619u;
    }

    return hash;
}

static void NormalizePathForBookmarkKey(LPCWSTR pszPath, WCHAR* pszOut, int cchOut)
{
    if (!pszPath || !pszOut || cchOut <= 0) return;
    pszOut[0] = L'\0';

    WCHAR szFull[EXTENDED_PATH_MAX];
    if (GetFullPathName(pszPath, EXTENDED_PATH_MAX, szFull, NULL) == 0) {
        wcsncpy(pszOut, pszPath, cchOut - 1);
        pszOut[cchOut - 1] = L'\0';
        return;
    }

    // Normalize slashes and lowercase
    for (int i = 0; szFull[i] != L'\0' && i < cchOut - 1; i++) {
        WCHAR ch = szFull[i];
        if (ch == L'/') ch = L'\\';
        if (ch >= L'A' && ch <= L'Z') ch = (WCHAR)(ch + (L'a' - L'A'));
        pszOut[i] = ch;
        pszOut[i + 1] = L'\0';
    }
}

static void GetBookmarkSectionKey(LPCWSTR pszPath, WCHAR* pszKey, int cchKey)
{
    if (!pszKey || cchKey <= 0) return;
    pszKey[0] = L'\0';

    WCHAR szNorm[EXTENDED_PATH_MAX];
    NormalizePathForBookmarkKey(pszPath, szNorm, EXTENDED_PATH_MAX);

    DWORD hash = HashStringFNV1a(szNorm);
    _snwprintf(pszKey, cchKey, L"Bookmarks.%08X", hash);
    pszKey[cchKey - 1] = L'\0';
}

static int HexValue(WCHAR ch)
{
    if (ch >= L'0' && ch <= L'9') return ch - L'0';
    if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
    return -1;
}

static void EncodeContextHex(const WCHAR* pszContext, WCHAR* pszOut, int cchOut)
{
    if (!pszContext || !pszOut || cchOut <= 0) return;
    int outPos = 0;
    for (int i = 0; pszContext[i] != L'\0' && outPos + 4 < cchOut; i++) {
        WCHAR ch = pszContext[i];
        _snwprintf(pszOut + outPos, cchOut - outPos, L"%04X", (unsigned int)ch);
        outPos += 4;
    }
    pszOut[outPos] = L'\0';
}

static void DecodeContextHex(const WCHAR* pszHex, WCHAR* pszOut, int cchOut)
{
    if (!pszHex || !pszOut || cchOut <= 0) return;
    int outPos = 0;
    int inPos = 0;
    while (pszHex[inPos] && pszHex[inPos + 1] && pszHex[inPos + 2] && pszHex[inPos + 3] && outPos + 1 < cchOut) {
        int v1 = HexValue(pszHex[inPos]);
        int v2 = HexValue(pszHex[inPos + 1]);
        int v3 = HexValue(pszHex[inPos + 2]);
        int v4 = HexValue(pszHex[inPos + 3]);
        if (v1 < 0 || v2 < 0 || v3 < 0 || v4 < 0) break;
        WCHAR ch = (WCHAR)((v1 << 12) | (v2 << 8) | (v3 << 4) | v4);
        pszOut[outPos++] = ch;
        inPos += 4;
    }
    pszOut[outPos] = L'\0';
}

static int GetLineIndexWrapAwareFromChar(LONG charPos)
{
    if (g_bWordWrap) {
        int visualLine = 1;
        int currentLineStart = 0;
        int lineIndex = 0;

        while (currentLineStart < charPos) {
            int nextLineStart = (int)SendMessage(g_hWndEdit, EM_LINEINDEX, lineIndex + 1, 0);
            if (nextLineStart == -1 || nextLineStart <= currentLineStart) {
                break;
            }
            if (nextLineStart <= charPos) {
                visualLine++;
                currentLineStart = nextLineStart;
                lineIndex++;
            } else {
                break;
            }
        }

        return visualLine - 1;  // 0-based
    }

    return (int)SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, charPos);
}

static int GetCurrentLineIndexWrapAware()
{
    CHARRANGE cr;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    return GetLineIndexWrapAwareFromChar(cr.cpMin);
}

static void GetLineContextFromCharPos(LONG charPos, WCHAR* pszOut, int cchOut)
{
    if (!pszOut || cchOut <= 0) return;
    pszOut[0] = L'\0';

    LONG lineIndex = (LONG)SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, charPos);
    LONG lineStart = (LONG)SendMessage(g_hWndEdit, EM_LINEINDEX, lineIndex, 0);
    LONG lineLength = (LONG)SendMessage(g_hWndEdit, EM_LINELENGTH, lineStart, 0);
    if (lineLength <= 0) return;

    LONG copyLen = lineLength;
    if (copyLen > BOOKMARK_CONTEXT_LEN - 1) {
        copyLen = BOOKMARK_CONTEXT_LEN - 1;
    }

    TEXTRANGE tr;
    tr.chrg.cpMin = lineStart;
    tr.chrg.cpMax = lineStart + copyLen;
    tr.lpstrText = pszOut;
    SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    pszOut[copyLen] = L'\0';
}

static LONG GetBookmarkLineStart(int lineIndex)
{
    if (g_bWordWrap) {
        int currentLineStart = 0;
        int currentIndex = 0;

        while (currentIndex < lineIndex) {
            int nextLineStart = (int)SendMessage(g_hWndEdit, EM_LINEINDEX, currentIndex + 1, 0);
            if (nextLineStart == -1 || nextLineStart <= currentLineStart) {
                return -1;
            }
            currentLineStart = nextLineStart;
            currentIndex++;
        }

        return (LONG)currentLineStart;
    }

    return (LONG)SendMessage(g_hWndEdit, EM_LINEINDEX, lineIndex, 0);
}

void ClearBookmarks()
{
    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        g_Bookmarks[i].active = FALSE;
        g_Bookmarks[i].charPos = 0;
        g_Bookmarks[i].lineIndex = 0;
        g_Bookmarks[i].context[0] = L'\0';
    }
    g_nBookmarkCount = 0;
    g_bBookmarksDirty = FALSE;
}

static void GetBookmarkIniKey(int index, WCHAR* pszKey, int cchKey)
{
    if (!pszKey || cchKey <= 0) return;
    WCHAR szNum[16];
    wcscpy(pszKey, L"Item");
    _itow(index + 1, szNum, 10);
    wcscat(pszKey, szNum);
}

static int FindBookmarkByLineIndex(int lineIndex)
{
    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        if (!g_Bookmarks[i].active) continue;
        if (g_Bookmarks[i].lineIndex == lineIndex) return i;
    }
    return -1;
}

void LoadBookmarksForCurrentFile()
{
    ClearBookmarks();
    g_szBookmarkSectionKey[0] = L'\0';

    if (g_szFileName[0] == L'\0') {
        return;
    }

    WCHAR szSection[64];
    GetBookmarkSectionKey(g_szFileName, szSection, 64);
    wcscpy(g_szBookmarkSectionKey, szSection);

    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);

    int count = ReadINIInt(szIniPath, szSection, L"Count", 0);
    if (count > MAX_BOOKMARKS) count = MAX_BOOKMARKS;

    for (int i = 0; i < count; i++) {
        WCHAR szKey[32];
        WCHAR szValue[512];
        GetBookmarkIniKey(i, szKey, 32);
        ReadINIValue(szIniPath, szSection, szKey, szValue, 512, L"");
        if (szValue[0] == L'\0') continue;

        WCHAR* pPos = wcsstr(szValue, L"pos=");
        WCHAR* pCtx = wcsstr(szValue, L"|ctx=");
        if (!pPos || !pCtx) continue;

        *pCtx = L'\0';
        pCtx += 5;
        LONG pos = _wtol(pPos + 4);

        g_Bookmarks[i].charPos = pos;
        g_Bookmarks[i].lineIndex = 0;
        DecodeContextHex(pCtx, g_Bookmarks[i].context, BOOKMARK_CONTEXT_LEN);
        g_Bookmarks[i].active = TRUE;
        g_nBookmarkCount++;
    }

    g_bBookmarksDirty = TRUE;
    g_nLastTextLen = GetWindowTextLength(g_hWndEdit);
}

void SaveBookmarksForCurrentFile()
{
    if (g_szFileName[0] == L'\0') {
        return;
    }

    WCHAR szSection[64];
    GetBookmarkSectionKey(g_szFileName, szSection, 64);
    wcscpy(g_szBookmarkSectionKey, szSection);

    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);

    int count = 0;
    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        if (!g_Bookmarks[i].active) continue;
        count++;
    }

    if (count == 0) {
        ReplaceINISection(szIniPath, szSection, L"");
        FlushIniCache();
        return;
    }

    std::wstring sectionData;
    sectionData.reserve(4096);

    sectionData += L"[";
    sectionData += szSection;
    sectionData += L"]\r\n";
    sectionData += L"Path=";
    sectionData += g_szFileName;
    sectionData += L"\r\n";

    WCHAR szCount[16];
    _itow(count, szCount, 10);
    sectionData += L"Count=";
    sectionData += szCount;
    sectionData += L"\r\n";

    int idx = 0;
    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        if (!g_Bookmarks[i].active) continue;

        WCHAR szKey[32];
        WCHAR szPos[32];
        WCHAR szCtx[BOOKMARK_CONTEXT_LEN * 4 + 1];

        GetBookmarkIniKey(idx, szKey, 32);
        _snwprintf(szPos, 32, L"%ld", g_Bookmarks[i].charPos);
        szPos[31] = L'\0';
        EncodeContextHex(g_Bookmarks[i].context, szCtx, (int)(sizeof(szCtx) / sizeof(szCtx[0])));

        sectionData += szKey;
        sectionData += L"=pos=";
        sectionData += szPos;
        sectionData += L"|ctx=";
        sectionData += szCtx;
        sectionData += L"\r\n";
        idx++;
    }

    ReplaceINISection(szIniPath, szSection, sectionData);
    FlushIniCache();
}

static BOOL BookmarkContextMatchesAt(LONG charPos, const WCHAR* context)
{
    if (!context || context[0] == L'\0') return FALSE;

    int ctxLen = (int)wcslen(context);
    if (ctxLen <= 0) return FALSE;

    TEXTRANGE tr;
    WCHAR buffer[BOOKMARK_CONTEXT_LEN + 1];
    tr.chrg.cpMin = charPos;
    tr.chrg.cpMax = charPos + ctxLen;
    tr.lpstrText = buffer;
    int retrieved = (int)SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    if (retrieved <= 0) return FALSE;

    buffer[ctxLen] = L'\0';
    return (wcscmp(buffer, context) == 0);
}

static LONG FindContextNearPos(LONG charPos, const WCHAR* context, int windowSize)
{
    if (!context || context[0] == L'\0') return -1;

    int textLen = GetWindowTextLength(g_hWndEdit);
    if (textLen <= 0) return -1;

    LONG start = charPos - windowSize;
    if (start < 0) start = 0;
    LONG end = charPos + windowSize;
    if (end > textLen) end = textLen;

    int ctxLen = (int)wcslen(context);
    if (ctxLen <= 0) return -1;

    LONG rangeLen = end - start;
    if (rangeLen <= 0) return -1;

    WCHAR* buffer = (WCHAR*)malloc((rangeLen + 1) * sizeof(WCHAR));
    if (!buffer) return -1;

    TEXTRANGE tr;
    tr.chrg.cpMin = start;
    tr.chrg.cpMax = end;
    tr.lpstrText = buffer;
    int retrieved = (int)SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    if (retrieved <= 0) {
        free(buffer);
        return -1;
    }

    buffer[retrieved] = L'\0';

    WCHAR* found = wcsstr(buffer, context);
    if (!found) {
        free(buffer);
        return -1;
    }

    LONG offset = (LONG)(found - buffer);
    free(buffer);
    return start + offset;
}

static LONG FindContextInDocument(const WCHAR* context)
{
    if (!context || context[0] == L'\0') return -1;

    int textLen = GetWindowTextLength(g_hWndEdit);
    if (textLen <= 0) return -1;

    WCHAR* buffer = (WCHAR*)malloc((textLen + 1) * sizeof(WCHAR));
    if (!buffer) return -1;

    TEXTRANGE tr;
    tr.chrg.cpMin = 0;
    tr.chrg.cpMax = textLen;
    tr.lpstrText = buffer;
    int retrieved = (int)SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    if (retrieved <= 0) {
        free(buffer);
        return -1;
    }

    buffer[retrieved] = L'\0';
    WCHAR* found = wcsstr(buffer, context);
    if (!found) {
        free(buffer);
        return -1;
    }

    LONG offset = (LONG)(found - buffer);
    free(buffer);
    return offset;
}

static int CompareBookmarksByLine(const void* a, const void* b)
{
    const Bookmark* pa = *(const Bookmark* const*)a;
    const Bookmark* pb = *(const Bookmark* const*)b;
    if (pa->lineIndex < pb->lineIndex) return -1;
    if (pa->lineIndex > pb->lineIndex) return 1;
    return 0;
}

void UpdateBookmarksAfterEdit(LONG nEditPos, int nDelta)
{
    if (nDelta == 0) return;

    LONG deletedCount = 0;
    LONG deletedStart = nEditPos;
    LONG deletedEnd = nEditPos;
    if (nDelta < 0) {
        deletedCount = -nDelta;
        deletedEnd = nEditPos + deletedCount;
        if (deletedEnd < deletedStart) deletedEnd = deletedStart;
    }

    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        if (!g_Bookmarks[i].active) continue;

        LONG pos = g_Bookmarks[i].charPos;

        if (nDelta < 0 && pos >= deletedStart && pos < deletedEnd) {
            LONG newPos = deletedStart;
            int lineIndex = GetLineIndexWrapAwareFromChar(newPos);
            LONG lineStart = GetBookmarkLineStart(lineIndex);
            if (lineStart >= 0) newPos = lineStart;
            g_Bookmarks[i].charPos = newPos;
        } else if (pos >= nEditPos) {
            g_Bookmarks[i].charPos += nDelta;
            if (g_Bookmarks[i].charPos < 0) g_Bookmarks[i].charPos = 0;
        }
    }

    g_bBookmarksDirty = TRUE;
}

void ToggleBookmark()
{
    if (!g_hWndEdit) return;

    if (g_bBookmarksDirty) {
        RefreshBookmarkLineIndices();
    }

    CHARRANGE cr;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);

    int currentLineIndex = GetCurrentLineIndexWrapAware();
    LONG lineStart = GetBookmarkLineStart(currentLineIndex);
    if (lineStart < 0) return;

    // Check if bookmark exists on this line
    int existingIndex = FindBookmarkByLineIndex(currentLineIndex);
    if (existingIndex >= 0) {
        g_Bookmarks[existingIndex].active = FALSE;
        g_nBookmarkCount--;
        if (g_nBookmarkCount < 0) g_nBookmarkCount = 0;
        return;
    }

    // Find empty slot
    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        if (g_Bookmarks[i].active) continue;

        g_Bookmarks[i].active = TRUE;
        g_Bookmarks[i].charPos = lineStart;
        g_Bookmarks[i].lineIndex = currentLineIndex;
        GetLineContextFromCharPos(lineStart, g_Bookmarks[i].context, BOOKMARK_CONTEXT_LEN);
        g_nBookmarkCount++;
        return;
    }
}

static void RefreshBookmarkLineIndices()
{
    CHARRANGE crOriginal;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&crOriginal);

    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        if (!g_Bookmarks[i].active) continue;
        if (!BookmarkContextMatchesAt(g_Bookmarks[i].charPos, g_Bookmarks[i].context)) {
            LONG nearPos = FindContextNearPos(g_Bookmarks[i].charPos, g_Bookmarks[i].context, 8192);
            if (nearPos < 0) {
                nearPos = FindContextInDocument(g_Bookmarks[i].context);
            }
            if (nearPos >= 0) {
                g_Bookmarks[i].charPos = nearPos;
            }
        }
        if (g_bWordWrap) {
            CHARRANGE cr;
            cr.cpMin = g_Bookmarks[i].charPos;
            cr.cpMax = g_Bookmarks[i].charPos;
            SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
            g_Bookmarks[i].lineIndex = GetCurrentLineIndexWrapAware();
        } else {
            g_Bookmarks[i].lineIndex = (int)SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, g_Bookmarks[i].charPos);
        }
    }

    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crOriginal);

    g_bBookmarksDirty = FALSE;
}

void NextBookmark(BOOL bForward)
{
    if (g_nBookmarkCount <= 0) {
        WCHAR szMsg[128], szTitle[64];
        LoadStringResource(IDS_NO_BOOKMARKS, szMsg, 128);
        LoadStringResource(IDS_INFORMATION, szTitle, 64);
        MessageBox(g_hWndMain, szMsg, szTitle, MB_ICONINFORMATION);
        return;
    }

    if (g_bBookmarksDirty) {
        RefreshBookmarkLineIndices();
    }

    CHARRANGE crOriginal;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&crOriginal);

    int currentLine = GetCurrentLineIndexWrapAware();
    Bookmark* sorted[MAX_BOOKMARKS];
    int sortedCount = 0;

    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        if (!g_Bookmarks[i].active) continue;
        sorted[sortedCount++] = &g_Bookmarks[i];
    }

    qsort(sorted, sortedCount, sizeof(Bookmark*), CompareBookmarksByLine);

    int targetIndex = -1;
    if (bForward) {
        for (int i = 0; i < sortedCount; i++) {
            if (sorted[i]->lineIndex > currentLine) {
                targetIndex = i;
                break;
            }
        }
        if (targetIndex < 0) targetIndex = 0;  // Wrap
    } else {
        for (int i = sortedCount - 1; i >= 0; i--) {
            if (sorted[i]->lineIndex < currentLine) {
                targetIndex = i;
                break;
            }
        }
        if (targetIndex < 0) targetIndex = sortedCount - 1;  // Wrap
    }

    if (targetIndex >= 0 && targetIndex < sortedCount) {
        LONG pos = sorted[targetIndex]->charPos;
        CHARRANGE cr;
        cr.cpMin = pos;
        cr.cpMax = pos;
        SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
        SetFocus(g_hWndEdit);
    } else {
        SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crOriginal);
    }
}

void ClearAllBookmarks()
{
    ClearBookmarks();
}

//============================================================================
// LoadStringResource - Load string from resource with language fallback
//============================================================================
void LoadStringResource(UINT uID, LPWSTR lpBuffer, int cchBufferMax)
{
    if (LoadString(GetModuleHandle(NULL), uID, lpBuffer, cchBufferMax) == 0) {
        // Fallback to empty string if resource not found
        lpBuffer[0] = L'\0';
    }
}

//============================================================================
// GetRichEditVersion - Detect RichEdit version from DLL file version
// Based on KeyNote NF logic - maps file version to logical RichEdit version
// Returns: 1.0, 2.0, 3.0, 4.0, 4.1, 5.0, 6.0, 7.5, 8.0, or 0.0 on failure
// pszPath: Receives full path to DLL (buffer must be at least cchPath WCHARs)
//============================================================================
float GetRichEditVersion(HMODULE hModule, LPWSTR pszPath, DWORD cchPath)
{
    if (!hModule || !pszPath || cchPath == 0) return 0.0f;
    
    // Get DLL path
    if (GetModuleFileName(hModule, pszPath, cchPath) == 0) {
        return 0.0f;  // Failed to get path
    }
    
    // Get version info size
    DWORD dwDummy;
    DWORD dwSize = GetFileVersionInfoSize(pszPath, &dwDummy);
    if (dwSize == 0) {
        return 0.0f;  // No version info available
    }
    
    // Allocate buffer and get version info
    BYTE* pBuffer = (BYTE*)malloc(dwSize);
    if (!pBuffer) return 0.0f;
    
    float fResult = 0.0f;
    
    if (GetFileVersionInfo(pszPath, 0, dwSize, pBuffer)) {
        VS_FIXEDFILEINFO* pFileInfo = NULL;
        UINT uLen = 0;
        
        if (VerQueryValue(pBuffer, L"\\", (LPVOID*)&pFileInfo, &uLen) && pFileInfo) {
            // Extract version numbers
            WORD wFileVersionMajor = HIWORD(pFileInfo->dwFileVersionMS);
            WORD wFileVersionMinor = LOWORD(pFileInfo->dwFileVersionMS);
            
            // Extract DLL name (uppercase for comparison)
            WCHAR szDllName[MAX_PATH];
            wcscpy(szDllName, pszPath);
            WCHAR* pFileName = wcsrchr(szDllName, L'\\');
            if (pFileName) {
                wcscpy(szDllName, pFileName + 1);
            }
            _wcsupr(szDllName);
            
            // Map DLL name + version to logical RichEdit version
            // Based on KeyNote NF mapping logic
            
            if (wcscmp(szDllName, L"RICHED32.DLL") == 0) {
                fResult = 1.0f;  // RichEdit 1.0
            }
            else if (wcscmp(szDllName, L"RICHED20.DLL") == 0) {
                // RichEdit 2.0-8.0 (depending on file version)
                switch (wFileVersionMinor) {
                    case 30: fResult = 3.0f; break;  // Windows 98/ME/2000
                    case 31: fResult = 3.1f; break;
                    case 40: fResult = 4.0f; break;  // Windows XP
                    case 50: fResult = 5.0f; break;  // Office 2003
                    case 0:
                        // Minor version 0 - check major version
                        switch (wFileVersionMajor) {
                            case 5:  fResult = 2.0f; break;  // Windows 95/NT 4.0
                            case 12: fResult = 6.0f; break;  // Office 2007
                            case 14: fResult = 6.0f; break;  // Office 2010
                            case 15: fResult = 8.0f; break;  // Office 2013
                            default:
                                if (wFileVersionMajor > 15) {
                                    fResult = 8.0f;  // Office 2016+ (treat as 8.0)
                                } else {
                                    fResult = 3.0f;  // Conservative fallback
                                }
                                break;
                        }
                        break;
                    default:
                        fResult = 3.0f;  // Conservative fallback
                        break;
                }
            }
            else if (wcscmp(szDllName, L"MSFTEDIT.DLL") == 0) {
                // RichEdit 4.1 or 7.5 (depending on version)
                if (wFileVersionMajor == 5 && wFileVersionMinor == 41) {
                    fResult = 4.1f;  // Windows Vista/7
                }
                else if (wFileVersionMajor == 6 && wFileVersionMinor == 2) {
                    fResult = 7.5f;  // Windows 8
                }
                else if (wFileVersionMajor == 10 && wFileVersionMinor == 0) {
                    fResult = 7.5f;  // Windows 10
                }
                else if (wFileVersionMajor > 10) {
                    fResult = 7.5f;  // Windows 11+ (treat as 7.5)
                }
                else {
                    fResult = 4.1f;  // Conservative fallback
                }
            }
        }
    }
    
    free(pBuffer);
    return fResult;
}

//============================================================================
// GetRichEditClassName - Determine window class based on version
// Returns appropriate class name for CreateWindow
//============================================================================
LPCWSTR GetRichEditClassName(float fVersion)
{
    if (fVersion == 1.0f) {
        return L"RICHEDIT";  // Version 1.0 (ANSI)
    }
    else if (fVersion == 5.0f) {
        return L"RichEdit20W";  // Office 2003
    }
    else if (fVersion >= 6.0f && fVersion < 7.0f) {
        return L"RichEdit60W";  // Office 2007/2010 (version 6.0)
    }
    else if (fVersion >= 8.0f) {
        return L"RichEditD2DPT";  // Office 2013+ / Windows 11 (version 8.0+) - Direct2D + UI Automation
    }
    else if (fVersion >= 4.0f) {
        return L"RICHEDIT50W";  // RichEdit 4.x and 7.5 (MSFTEDIT.DLL)
    }
    else {
        return L"RichEdit20W";  // RichEdit 2.x/3.x (fallback)
    }
}

//============================================================================
// LoadRichEditLibrary - Load RichEdit DLL with custom path support
// Returns TRUE on success, FALSE on total failure
// Tries custom path first (if set), then falls back to system cascade
//============================================================================
BOOL LoadRichEditLibrary()
{
    WCHAR szFullPath[MAX_PATH];
    
    // Try user-specified custom path first
    if (g_szRichEditLibPathINI[0] != L'\0') {
        
        // Resolve path (handle both relative and absolute)
        if (PathIsRelative(g_szRichEditLibPathINI)) {
            // Get EXE directory
            WCHAR szExePath[MAX_PATH];
            GetModuleFileName(NULL, szExePath, MAX_PATH);
            PathRemoveFileSpec(szExePath);
            PathCombine(szFullPath, szExePath, g_szRichEditLibPathINI);
        } else {
            wcscpy(szFullPath, g_szRichEditLibPathINI);
        }
        
        // Try to load custom library
        g_hRichEditLib = LoadLibrary(szFullPath);
        
        if (g_hRichEditLib) {
            // Success - detect version
            g_fRichEditVersion = GetRichEditVersion(g_hRichEditLib, g_szRichEditLibPath, MAX_PATH);
            
            // Check for INI class name override (Phase 2.8.5)
            if (g_szRichEditClassNameINI[0] != L'\0') {
                wcscpy(g_szRichEditClassName, g_szRichEditClassNameINI);
            } else {
                wcscpy(g_szRichEditClassName, GetRichEditClassName(g_fRichEditVersion));
            }
            return TRUE;
        } else {
            // Custom load failed - show warning
            // Load localized template from resources and replace {PATH} placeholder
            WCHAR szTemplate[512], szMsg[768], szTitle[64];
            LoadString(GetModuleHandle(NULL), IDS_RICHEDIT_LOAD_FAILED, szTemplate, 512);
            
            // Find {PATH} placeholder and replace with actual path
            WCHAR* pPlaceholder = wcsstr(szTemplate, L"{PATH}");
            if (pPlaceholder) {
                // Copy part before placeholder
                size_t prefixLen = pPlaceholder - szTemplate;
                wcsncpy(szMsg, szTemplate, prefixLen);
                szMsg[prefixLen] = L'\0';
                
                // Append the actual path
                wcscat(szMsg, szFullPath);
                
                // Append part after placeholder
                wcscat(szMsg, pPlaceholder + 6);  // Skip "{PATH}"
            } else {
                // Fallback if placeholder not found (shouldn't happen)
                wcscpy(szMsg, szTemplate);
            }
            
            LoadString(GetModuleHandle(NULL), IDS_ERROR, szTitle, 64);
            MessageBox(NULL, szMsg, szTitle, MB_OK | MB_ICONWARNING);
        }
    }
    
    // Fallback cascade: MSFTEDIT.DLL → RICHED20.DLL → RICHED32.DLL
    
    // Try MSFTEDIT.DLL (RichEdit 4.1+, Windows Vista+)
    g_hRichEditLib = LoadLibrary(L"MSFTEDIT.DLL");
    if (g_hRichEditLib) {
        g_fRichEditVersion = GetRichEditVersion(g_hRichEditLib, g_szRichEditLibPath, MAX_PATH);
        
        // Check for INI class name override (Phase 2.8.5)
        if (g_szRichEditClassNameINI[0] != L'\0') {
            wcscpy(g_szRichEditClassName, g_szRichEditClassNameINI);
        } else {
            wcscpy(g_szRichEditClassName, GetRichEditClassName(g_fRichEditVersion));
        }
        return TRUE;
    }
    
    // Try RICHED20.DLL (RichEdit 2.0-6.0, Windows 2000+)
    g_hRichEditLib = LoadLibrary(L"RICHED20.DLL");
    if (g_hRichEditLib) {
        g_fRichEditVersion = GetRichEditVersion(g_hRichEditLib, g_szRichEditLibPath, MAX_PATH);
        
        // Check for INI class name override (Phase 2.8.5)
        if (g_szRichEditClassNameINI[0] != L'\0') {
            wcscpy(g_szRichEditClassName, g_szRichEditClassNameINI);
        } else {
            wcscpy(g_szRichEditClassName, GetRichEditClassName(g_fRichEditVersion));
        }
        return TRUE;
    }
    
    // Try RICHED32.DLL (RichEdit 1.0, legacy fallback)
    g_hRichEditLib = LoadLibrary(L"RICHED32.DLL");
    if (g_hRichEditLib) {
        g_fRichEditVersion = GetRichEditVersion(g_hRichEditLib, g_szRichEditLibPath, MAX_PATH);
        
        // Check for INI class name override (Phase 2.8.5)
        if (g_szRichEditClassNameINI[0] != L'\0') {
            wcscpy(g_szRichEditClassName, g_szRichEditClassNameINI);
        } else {
            wcscpy(g_szRichEditClassName, GetRichEditClassName(g_fRichEditVersion));
        }
        return TRUE;
    }
    
    // Total failure - no RichEdit library available
    return FALSE;
}

//============================================================================
// GetSystemLanguageCode - Get current system language code (e.g., "cs_CZ")
//============================================================================
void GetSystemLanguageCode(LPWSTR pszLangCode, int cchLangCode)
{
    // Get user's default locale
    LCID lcid = GetUserDefaultLCID();
    
    // Get language code (ISO 639-1, e.g., "cs", "en")
    WCHAR szLang[10];
    if (GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, szLang, 10) == 0) {
        wcscpy_s(pszLangCode, cchLangCode, L"en_US");
        return;
    }
    
    // Get country code (ISO 3166-1, e.g., "CZ", "US")
    WCHAR szCountry[10];
    if (GetLocaleInfo(lcid, LOCALE_SISO3166CTRYNAME, szCountry, 10) == 0) {
        wcscpy_s(pszLangCode, cchLangCode, L"en_US");
        return;
    }
    
    // Combine into "xx_YY" format
    _snwprintf(pszLangCode, cchLangCode, L"%s_%s", szLang, szCountry);
}

//============================================================================
// INI reader backed by cached in-memory data (UNC-safe)
BOOL ReadINIValue(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, LPWSTR pszValue, DWORD cchValue, LPCWSTR pszDefault)
{
    (void)pszIniPath;

    if (!EnsureIniCacheLoaded()) {
        if (pszDefault) {
            wcsncpy(pszValue, pszDefault, cchValue);
            pszValue[cchValue - 1] = L'\0';
        } else {
            pszValue[0] = L'\0';
        }
        return FALSE;
    }
    
    // Parse INI: find [Section]
    WCHAR szSectionHeader[256];
    _snwprintf(szSectionHeader, 256, L"[%s]", pszSection);
    
    const WCHAR* pszData = g_IniCache.data.c_str();
    const WCHAR* pszSectionStart = wcsstr(pszData, szSectionHeader);
    if (!pszSectionStart) {
        if (pszDefault) {
            wcsncpy(pszValue, pszDefault, cchValue);
            pszValue[cchValue - 1] = L'\0';
        } else {
            pszValue[0] = L'\0';
        }
        return FALSE;
    }
    
    // Move past section header to next line
    pszSectionStart = wcschr(pszSectionStart, L'\n');
    if (!pszSectionStart) {
        if (pszDefault) {
            wcsncpy(pszValue, pszDefault, cchValue);
            pszValue[cchValue - 1] = L'\0';
        } else {
            pszValue[0] = L'\0';
        }
        return FALSE;
    }
    pszSectionStart++;
    
    // Find key=value
    const WCHAR* pszLine = pszSectionStart;
    while (pszLine && *pszLine) {
        // Check if we hit another section
        if (*pszLine == L'[') break;
        
        // Skip whitespace
        while (*pszLine == L' ' || *pszLine == L'\t') pszLine++;
        
        // Skip comments and empty lines
        if (*pszLine == L';' || *pszLine == L'#' || *pszLine == L'\r' || *pszLine == L'\n') {
            pszLine = wcschr(pszLine, L'\n');
            if (pszLine) pszLine++;
            continue;
        }
        
        // Check if this line starts with our key
        size_t keyLen = wcslen(pszKey);
        if (wcsncmp(pszLine, pszKey, keyLen) == 0) {
            pszLine += keyLen;
            // Skip whitespace and =
            while (*pszLine == L' ' || *pszLine == L'\t') pszLine++;
            if (*pszLine == L'=') {
                pszLine++;
                while (*pszLine == L' ' || *pszLine == L'\t') pszLine++;
                
                // Copy value until end of line or comment
                DWORD i = 0;
                while (i < cchValue - 1 && *pszLine && *pszLine != L'\r' && *pszLine != L'\n' && *pszLine != L';') {
                    pszValue[i++] = *pszLine++;
                }
                pszValue[i] = L'\0';
                
                // Trim trailing whitespace
                while (i > 0 && (pszValue[i-1] == L' ' || pszValue[i-1] == L'\t')) {
                    pszValue[--i] = L'\0';
                }
                
                return TRUE;
            }
        }
        
        // Move to next line
        pszLine = wcschr(pszLine, L'\n');
        if (pszLine) pszLine++;
    }
    
    if (pszDefault) {
        wcsncpy(pszValue, pszDefault, cchValue);
        pszValue[cchValue - 1] = L'\0';
    } else {
        pszValue[0] = L'\0';
    }
    return FALSE;
}

int ReadINIInt(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, int nDefault)
{
    WCHAR szValue[32];
    if (ReadINIValue(pszIniPath, pszSection, pszKey, szValue, 32, NULL)) {
        return _wtoi(szValue);
    }
    return nDefault;
}

//============================================================================
// WriteINIValue - Write value to INI file (works with UNC paths)
//============================================================================
BOOL WriteINIValue(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, LPCWSTR pszValue)
{
    (void)pszIniPath;

    if (!EnsureIniCacheLoaded()) {
        return FALSE;
    }
    
    std::wstring& wideData = g_IniCache.data;
    
    // Build section header
    WCHAR szSectionHeader[256];
    _snwprintf(szSectionHeader, 256, L"[%s]", pszSection);
    
    // Find or create section
    size_t sectionPos = wideData.find(szSectionHeader);
    std::wstring result;
    
    if (sectionPos == std::wstring::npos) {
        // Section doesn't exist - add it at the end
        if (!wideData.empty() && wideData[wideData.length() - 1] != L'\n') {
            wideData += L"\r\n";
        }
        wideData += szSectionHeader;
        wideData += L"\r\n";
        AppendKeyValueLine(wideData, pszKey, pszValue);
        g_IniCache.dirty = TRUE;
        return TRUE;
    }
    
    // Section exists - find the key or insert it
    size_t lineStart = sectionPos + wcslen(szSectionHeader);
    
    // Skip to next line after section header
    size_t nextLine = wideData.find(L'\n', lineStart);
    if (nextLine != std::wstring::npos) {
        lineStart = nextLine + 1;
    }
    
    // Look for the key in this section
    size_t keyLen = wcslen(pszKey);
    bool keyFound = false;
    size_t searchPos = lineStart;
    
    while (searchPos < wideData.length()) {
        // Check if we hit another section
        if (wideData[searchPos] == L'[') break;
        
        // Check if this line starts with our key
        if (wideData.compare(searchPos, keyLen, pszKey) == 0) {
            // Skip whitespace after key
            size_t afterKey = searchPos + keyLen;
            while (afterKey < wideData.length() && (wideData[afterKey] == L' ' || wideData[afterKey] == L'\t')) {
                afterKey++;
            }
            
            if (afterKey < wideData.length() && wideData[afterKey] == L'=') {
                // Found the key - replace the value
                size_t lineEnd = wideData.find(L'\n', searchPos);
                if (lineEnd == std::wstring::npos) lineEnd = wideData.length();
                
                result = wideData.substr(0, searchPos);
                result += pszKey;
                result += L"=";
                result += pszValue;
                if (lineEnd < wideData.length()) {
                    result += wideData.substr(lineEnd);
                } else {
                    result += L"\r\n";
                }
                keyFound = true;
                break;
            }
        }
        
        // Move to next line
        size_t nextLinePos = wideData.find(L'\n', searchPos);
        if (nextLinePos == std::wstring::npos) break;
        searchPos = nextLinePos + 1;
    }
    
    if (keyFound) {
        wideData = result;
        g_IniCache.dirty = TRUE;
        return TRUE;
    }
    
    // Key not found in section - add it after section header
    result = wideData.substr(0, lineStart);
    AppendKeyValueLine(result, pszKey, pszValue);
    result += wideData.substr(lineStart);
    wideData = result;
    g_IniCache.dirty = TRUE;
    return TRUE;
}

//============================================================================
// EnsureIniCacheLoaded - Load INI file into cache if needed
//============================================================================
BOOL EnsureIniCacheLoaded()
{
    if (g_IniCache.loaded) {
        return TRUE;
    }
    
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    std::string existingData;
    HANDLE hFile = CreateFile(szIniPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwSize = GetFileSize(hFile, NULL);
        if (dwSize > 0 && dwSize != INVALID_FILE_SIZE) {
            char* pszFileData = (char*)malloc(dwSize + 1);
            if (pszFileData) {
                DWORD dwRead;
                if (ReadFile(hFile, pszFileData, dwSize, &dwRead, NULL)) {
                    pszFileData[dwRead] = '\0';
                    existingData.assign(pszFileData, dwRead);
                }
                free(pszFileData);
            }
        }
        CloseHandle(hFile);
    }
    
    g_IniCache.data.clear();
    if (existingData.empty()) {
        g_IniCache.loaded = TRUE;
        g_IniCache.dirty = FALSE;
        return TRUE;
    }
    
    int cchWide = MultiByteToWideChar(CP_UTF8, 0, existingData.c_str(), -1, NULL, 0);
    if (cchWide <= 0) {
        return FALSE;
    }
    
    WCHAR* pszWideData = (WCHAR*)malloc(cchWide * sizeof(WCHAR));
    if (!pszWideData) {
        return FALSE;
    }
    
    int nWideWritten = MultiByteToWideChar(CP_UTF8, 0, existingData.c_str(), -1, pszWideData, cchWide);
    if (nWideWritten <= 0) {
        free(pszWideData);
        return FALSE;
    }
    
    // Strip UTF-8 BOM if present (EF BB BF -> UTF-16 0xFEFF)
    if (pszWideData[0] == 0xFEFF) {
        g_IniCache.data = pszWideData + 1;
    } else {
        g_IniCache.data = pszWideData;
    }
    free(pszWideData);
    
    g_IniCache.loaded = TRUE;
    g_IniCache.dirty = FALSE;
    return TRUE;
}

//============================================================================
// FlushIniCache - Write cached INI to disk if dirty
//============================================================================
BOOL FlushIniCache()
{
    if (!g_IniCache.loaded || !g_IniCache.dirty) {
        return TRUE;
    }
    
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    int cbUTF8 = WideCharToMultiByte(CP_UTF8, 0, g_IniCache.data.c_str(), -1, NULL, 0, NULL, NULL);
    if (cbUTF8 <= 0) {
        return FALSE;
    }
    
    char* pszUTF8 = (char*)malloc(cbUTF8);
    if (!pszUTF8) {
        return FALSE;
    }
    
    int nUTF8Written = WideCharToMultiByte(CP_UTF8, 0, g_IniCache.data.c_str(), -1, pszUTF8, cbUTF8, NULL, NULL);
    if (nUTF8Written <= 0) {
        free(pszUTF8);
        return FALSE;
    }
    
    HANDLE hFile = CreateFile(szIniPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        free(pszUTF8);
        return FALSE;
    }
    
    DWORD dwWritten;
    BOOL success = WriteFile(hFile, pszUTF8, strlen(pszUTF8), &dwWritten, NULL);
    CloseHandle(hFile);
    free(pszUTF8);
    
    if (success) {
        g_IniCache.dirty = FALSE;
    }
    
    return success;
}

//============================================================================
// ReplaceINISection - Replace or append a section with new content
//============================================================================
BOOL ReplaceINISection(LPCWSTR pszIniPath, LPCWSTR pszSection, const std::wstring& sectionContent)
{
    (void)pszIniPath;

    if (!EnsureIniCacheLoaded()) {
        return FALSE;
    }
    
    std::wstring& wideData = g_IniCache.data;
    
    WCHAR szSectionHeader[256];
    _snwprintf(szSectionHeader, 256, L"[%s]", pszSection);
    
    size_t sectionPos = wideData.find(szSectionHeader);
    bool removed = false;
    if (sectionPos != std::wstring::npos) {
        size_t nextSectionPos = wideData.find(L"\n[", sectionPos + wcslen(szSectionHeader));
        if (nextSectionPos != std::wstring::npos) {
            wideData.erase(sectionPos, nextSectionPos - sectionPos + 1);
        } else {
            wideData.erase(sectionPos);
        }
        removed = true;
    }

    if (sectionContent.empty()) {
        if (removed) {
            g_IniCache.dirty = TRUE;
        }
        return TRUE;
    }
    
    if (!wideData.empty() && wideData[wideData.length() - 1] != L'\n') {
        wideData += L"\r\n";
    }
    wideData += sectionContent;
    g_IniCache.dirty = TRUE;
    
    return TRUE;
}

//============================================================================
// AppendKeyValueLine - Append key=value\r\n to section string
//============================================================================
void AppendKeyValueLine(std::wstring& section, LPCWSTR pszKey, LPCWSTR pszValue)
{
    section += pszKey;
    section += L"=";
    section += pszValue;
    section += L"\r\n";
}

//============================================================================
// AppendIndexedLine - Append PrefixN=value\r\n (N is 1-based)
//============================================================================
void AppendIndexedLine(std::wstring& section, LPCWSTR pszKeyPrefix, int index, LPCWSTR pszValue)
{
    WCHAR szKey[64];
    WCHAR szNum[16];
    wcscpy(szKey, pszKeyPrefix);
    _itow(index, szNum, 10);
    wcscat(szKey, szNum);
    AppendKeyValueLine(section, szKey, pszValue);
}

//============================================================================
// BuildHistorySection - Build [Section] with Count and Item1..ItemN
//============================================================================
void BuildHistorySection(std::wstring& section, LPCWSTR pszSectionName, const WCHAR history[][MAX_SEARCH_TEXT], int count)
{
    WCHAR szCount[16];
    _itow(count, szCount, 10);
    
    section = L"[";
    section += pszSectionName;
    section += L"]\r\n";
    AppendKeyValueLine(section, L"Count", szCount);
    
    for (int i = 0; i < count; i++) {
        AppendIndexedLine(section, L"Item", i + 1, history[i]);
    }
}

//============================================================================
// LoadHistoryList - Load Item1..ItemN history list from INI
//============================================================================
int LoadHistoryList(LPCWSTR pszSection, WCHAR history[][MAX_SEARCH_TEXT], int maxCount)
{
    (void)maxCount;

    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    int count = ReadINIInt(szIniPath, pszSection, L"Count", 0);
    if (count > maxCount) {
        count = maxCount;
    }
    
    for (int i = 0; i < count; i++) {
        WCHAR szKey[32];
        WCHAR szNum[16];
        wcscpy(szKey, L"Item");
        _itow(i + 1, szNum, 10);
        wcscat(szKey, szNum);
        ReadINIValue(szIniPath, pszSection, szKey, history[i], MAX_SEARCH_TEXT, L"");
    }
    
    return count;
}

//============================================================================
// FileNew - Create new document
//============================================================================
void FileNew()
{
    // Check for unsaved changes
    if (!PromptSaveChanges()) {
        return;
    }
    
    // Clear editor (block EN_CHANGE notifications)
    g_bSettingText = TRUE;
    SetWindowText(g_hWndEdit, L"");
    g_bSettingText = FALSE;
    // RichEdit resets zoom on WM_SETTEXT; restore the user's zoom level
    if (g_nZoomPercent != 100)
        SendMessage(g_hWndEdit, EM_SETZOOM, (WPARAM)g_nZoomPercent, (LPARAM)100);
    g_bLineIndexDirty = true;  // cleared document; EN_CHANGE was suppressed
    
    // Reset state
    g_szFileName[0] = L'\0';
    g_szFileTitle[0] = L'\0';
    g_bModified = FALSE;
    wcscpy(g_szCurrentFileExtension, L"txt");  // Reset to txt for new file

    ClearBookmarks();
    g_szBookmarkSectionKey[0] = L'\0';
    g_nLastTextLen = 0;
    
    // Clear read-only mode on new file
    g_bReadOnly = FALSE;
    SendMessage(g_hWndEdit, EM_SETREADONLY, FALSE, 0);
    
    // Rebuild template and File→New menus to reflect new file type
    if (g_hWndMain) {
        BuildTemplateMenu(g_hWndMain);
        BuildFileNewMenu(g_hWndMain);
    }
    
    UpdateTitle();
    UpdateStatusBar();
    SetFocus(g_hWndEdit);
}

//============================================================================
// FileNewFromTemplate - Create new file with template content
//============================================================================
void FileNewFromTemplate(int nTemplateIndex)
{
    if (nTemplateIndex < 0 || nTemplateIndex >= g_nTemplateCount) {
        return;
    }
    
    // Check for unsaved changes
    if (!PromptSaveChanges()) {
        return;
    }
    
    TemplateInfo* pTemplate = &g_Templates[nTemplateIndex];
    
    // Expand template variables
    LONG nCursorOffset = -1;
    LPWSTR pszExpanded = ExpandTemplateVariables(pTemplate->szTemplate, &nCursorOffset);
    if (!pszExpanded) {
        // Fall back to blank document if expansion fails
        FileNew();
        return;
    }
    
    // Set expanded template as document content (block EN_CHANGE notifications)
    g_bSettingText = TRUE;
    SetWindowText(g_hWndEdit, pszExpanded);
    g_bSettingText = FALSE;
    // RichEdit resets zoom on WM_SETTEXT; restore the user's zoom level
    if (g_nZoomPercent != 100)
        SendMessage(g_hWndEdit, EM_SETZOOM, (WPARAM)g_nZoomPercent, (LPARAM)100);
    g_bLineIndexDirty = true;  // template content loaded; EN_CHANGE was suppressed
    
    free(pszExpanded);
    
    // Reset state (untitled document with template content)
    g_szFileName[0] = L'\0';
    g_szFileTitle[0] = L'\0';
    g_bModified = TRUE;  // Mark as modified (has unsaved template content)

    ClearBookmarks();
    g_szBookmarkSectionKey[0] = L'\0';
    g_nLastTextLen = GetWindowTextLength(g_hWndEdit);
    
    // Set file extension based on template's file type
    if (pTemplate->szFileExtension[0] != L'\0') {
        wcscpy(g_szCurrentFileExtension, pTemplate->szFileExtension);
    } else {
        wcscpy(g_szCurrentFileExtension, L"txt");
    }
    
    // Rebuild template and File→New menus to reflect new file type
    if (g_hWndMain) {
        BuildTemplateMenu(g_hWndMain);
        BuildFileNewMenu(g_hWndMain);
    }
    
    // Position cursor if %cursor% was found
    if (nCursorOffset >= 0) {
        CHARRANGE cr;
        cr.cpMin = nCursorOffset;
        cr.cpMax = nCursorOffset;
        SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);  // Scroll cursor into view
    } else {
        // No cursor marker - position at start
        SendMessage(g_hWndEdit, EM_SETSEL, 0, 0);
    }
    
    UpdateTitle();
    UpdateStatusBar();
    SetFocus(g_hWndEdit);
}

//============================================================================
// IsExtensionInList - Check if extension exists in semicolon-separated list
//============================================================================
BOOL IsExtensionInList(const WCHAR *szExt, const WCHAR *szList)
{
    if (!szExt || !szList || szExt[0] == L'\0' || szList[0] == L'\0') {
        return FALSE;
    }
    
    WCHAR szListCopy[512];
    wcscpy(szListCopy, szList);
    
    WCHAR *pToken = wcstok(szListCopy, L";");
    while (pToken != NULL) {
        if (_wcsicmp(pToken, szExt) == 0) {
            return TRUE;
        }
        pToken = wcstok(NULL, L";");
    }
    
    return FALSE;
}

//============================================================================
// BuildFileDialogFilter - Build file filter string for Open/Save dialogs
// Builds filter string from template categories with localized labels
// Format: "Markdown Files (*.md)\0*.md\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0"
// pszFilter: Output buffer for filter string (must be at least 1024 WCHARs)
// cchFilter: Size of output buffer in WCHARs
// pnFilterCount: Optional output - receives number of filters added (can be NULL)
// pnTxtFilterIndex: Optional output - receives 1-based index of "Text Files" filter (can be NULL)
//                   Set to -1 if txt is already in a category filter
//============================================================================
void BuildFileDialogFilter(LPWSTR pszFilter, DWORD cchFilter, int* pnFilterCount, int* pnTxtFilterIndex)
{
    if (!pszFilter || cchFilter < 1024) return;
    
    int pos = 0;
    
    // Structure to map category names to their extensions
    struct CategoryFilter {
        WCHAR szCategoryName[MAX_FILTER_NAME];
        WCHAR szExtensions[256];
    };
    CategoryFilter categoryFilters[32];
    int categoryCount = 0;
    
    // Load localized strings
    WCHAR szFiles[64];
    WCHAR szTextFiles[64];
    LoadString(GetModuleHandle(NULL), IDS_FILES, szFiles, 64);
    LoadString(GetModuleHandle(NULL), IDS_TEXT_FILES, szTextFiles, 64);
    
    // Build category → extensions mapping from templates
    for (int i = 0; i < g_nTemplateCount; i++) {
        if (g_Templates[i].szFileExtension[0] == L'\0') continue;
        
        // Determine category name (use Category field or fallback to uppercase extension)
        WCHAR szCategory[MAX_FILTER_NAME];
        if (g_Templates[i].szCategory[0] != L'\0') {
            wcscpy(szCategory, g_Templates[i].szCategory);
        } else {
            // Fallback: use uppercase extension as category name
            wcscpy(szCategory, g_Templates[i].szFileExtension);
            _wcsupr(szCategory);
        }
        
        // Find existing category or create new one
        int catIndex = -1;
        for (int j = 0; j < categoryCount; j++) {
            if (wcscmp(categoryFilters[j].szCategoryName, szCategory) == 0) {
                catIndex = j;
                break;
            }
        }
        
        if (catIndex == -1) {
            // New category - create it
            if (categoryCount < 32) {
                wcscpy(categoryFilters[categoryCount].szCategoryName, szCategory);
                wcscpy(categoryFilters[categoryCount].szExtensions, g_Templates[i].szFileExtension);
                categoryCount++;
            }
        } else {
            // Existing category - append extension if not already present
            if (!IsExtensionInList(g_Templates[i].szFileExtension, categoryFilters[catIndex].szExtensions)) {
                wcscat(categoryFilters[catIndex].szExtensions, L";");
                wcscat(categoryFilters[catIndex].szExtensions, g_Templates[i].szFileExtension);
            }
        }
    }
    
    // Build filter string from categories
    for (int i = 0; i < categoryCount; i++) {
        // Build label: "Markdown Files (*.md)" or "HTML Files (*.htm;*.html)"
        WCHAR szFilterLabel[256];
        wcscpy(szFilterLabel, categoryFilters[i].szCategoryName);
        wcscat(szFilterLabel, L" ");
        wcscat(szFilterLabel, szFiles);  // Localized "Files"
        wcscat(szFilterLabel, L" (*.");
        
        // Replace semicolons with ";*." for display
        WCHAR szDisplayExt[256];
        wcscpy(szDisplayExt, categoryFilters[i].szExtensions);
        WCHAR *pSemi = wcschr(szDisplayExt, L';');
        while (pSemi != NULL) {
            // Move string and insert ";*."
            size_t remaining = wcslen(pSemi);
            wmemmove(pSemi + 3, pSemi + 1, remaining);
            pSemi[0] = L';';
            pSemi[1] = L'*';
            pSemi[2] = L'.';
            pSemi = wcschr(pSemi + 3, L';');
        }
        
        wcscat(szFilterLabel, szDisplayExt);
        wcscat(szFilterLabel, L")");
        
        // Build pattern: "*.md" or "*.htm;*.html"
        WCHAR szPattern[256];
        wcscpy(szPattern, L"*.");
        
        // Replace semicolons with ";*." for pattern
        WCHAR szPatternExt[256];
        wcscpy(szPatternExt, categoryFilters[i].szExtensions);
        pSemi = wcschr(szPatternExt, L';');
        while (pSemi != NULL) {
            size_t remaining = wcslen(pSemi);
            wmemmove(pSemi + 3, pSemi + 1, remaining);
            pSemi[0] = L';';
            pSemi[1] = L'*';
            pSemi[2] = L'.';
            pSemi = wcschr(pSemi + 3, L';');
        }
        
        wcscat(szPattern, szPatternExt);
        
        // Add to filter string
        wcscpy(pszFilter + pos, szFilterLabel);
        pos += wcslen(szFilterLabel) + 1;
        wcscpy(pszFilter + pos, szPattern);
        pos += wcslen(szPattern) + 1;
    }
    
    // Add "Text Files (*.txt)" as built-in filter (if not already from templates)
    BOOL txtAlreadyAdded = FALSE;
    int txtFilterIndex = -1;  // Track txt filter index for FileSaveAs
    
    for (int i = 0; i < categoryCount; i++) {
        if (IsExtensionInList(L"txt", categoryFilters[i].szExtensions)) {
            txtAlreadyAdded = TRUE;
            break;
        }
    }
    
    if (!txtAlreadyAdded) {
        txtFilterIndex = categoryCount + 1;  // 1-based index
        
        WCHAR szTxtLabel[128];
        wcscpy(szTxtLabel, szTextFiles);  // Localized "Text Files"
        wcscat(szTxtLabel, L" (*.txt)");
        
        wcscpy(pszFilter + pos, szTxtLabel);
        pos += wcslen(szTxtLabel) + 1;
        wcscpy(pszFilter + pos, L"*.txt");
        pos += wcslen(L"*.txt") + 1;
    }
    
    // Return txt filter index if requested
    if (pnTxtFilterIndex) {
        *pnTxtFilterIndex = txtFilterIndex;
    }
    
    // Always add "All Files (*.*)" as last option
    WCHAR szFilterAll[64];
    LoadStringResource(IDS_FILE_FILTER_ALL, szFilterAll, 64);
    wcscpy(pszFilter + pos, szFilterAll);
    pos += wcslen(szFilterAll) + 1;
    wcscpy(pszFilter + pos, L"*.*");
    pos += wcslen(L"*.*") + 1;
    pszFilter[pos] = L'\0';  // Double null terminator
    
    // Return filter count if requested
    if (pnFilterCount) {
        *pnFilterCount = categoryCount + (txtAlreadyAdded ? 0 : 1) + 1;  // categories + txt + all files
    }
}

//============================================================================
// FileOpen - Show Open File dialog and load selected file
//============================================================================
void FileOpen()
{
    // Check for unsaved changes
    if (!PromptSaveChanges()) {
        return;
    }
    
    // Setup file dialog
    OPENFILENAME ofn = {};
    WCHAR szFile[EXTENDED_PATH_MAX] = L"";
    WCHAR szInitialDir[EXTENDED_PATH_MAX];
    
    GetDocumentsPath(szInitialDir, EXTENDED_PATH_MAX);
    
    // Build dynamic filter string based on template categories
    WCHAR szFilter[1024];
    BuildFileDialogFilter(szFilter, 1024, NULL, NULL);
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = g_hWndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = EXTENDED_PATH_MAX;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = szInitialDir;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    // Show dialog
    if (GetOpenFileName(&ofn)) {
        LoadTextFile(szFile);
        SetFocus(g_hWndEdit);
    }
}

//============================================================================
// FileSave - Save current file (or show Save As if no filename)
//============================================================================
BOOL FileSave()
{
    if (g_bSaveInProgress) {
        return FALSE; // avoid reentrant save attempts
    }
    g_bSaveInProgress = TRUE;

    // If this is a resumed untitled file, force "Save As" dialog
    if (g_bIsResumedFile && g_szOriginalFilePath[0] == L'\0') {
        g_bSaveInProgress = FALSE;  // FileSaveAs owns the guard for its own execution
        return FileSaveAs();
    }
    
    // If this is a resumed saved file, ask user where to save
    if (g_bIsResumedFile && g_szOriginalFilePath[0] != L'\0') {
        // Load localized template and replace {PATH} placeholder
        WCHAR szTemplate[512], szPrompt[768];
        LoadString(GetModuleHandle(NULL), IDS_RESUME_SAVE_PROMPT, szTemplate, 512);
        
        // Find {PATH} placeholder and replace with actual path
        WCHAR* pPlaceholder = wcsstr(szTemplate, L"{PATH}");
        if (pPlaceholder) {
            // Copy part before placeholder
            size_t prefixLen = pPlaceholder - szTemplate;
            wcsncpy(szPrompt, szTemplate, prefixLen);
            szPrompt[prefixLen] = L'\0';
            
            // Append the actual path
            wcscat(szPrompt, g_szOriginalFilePath);
            
            // Append part after placeholder
            wcscat(szPrompt, pPlaceholder + 6);  // Skip "{PATH}"
        } else {
            // Fallback if placeholder not found
            wcscpy(szPrompt, szTemplate);
        }
        
        int result = MessageBox(g_hWndMain, szPrompt, L"RichEditor",
                               MB_YESNOCANCEL | MB_ICONQUESTION);
        
        if (result == IDCANCEL) {
            g_bSaveInProgress = FALSE;
            return FALSE;
        } else if (result == IDNO) {
            g_bSaveInProgress = FALSE;
            return FileSaveAs();  // User wants to choose new location
        } else {
            // IDYES - save to original location
            wcscpy(g_szFileName, g_szOriginalFilePath);
        }
    }
    
    if (g_szFileName[0] == L'\0') {
        g_bSaveInProgress = FALSE;  // FileSaveAs owns the guard for its own execution
        return FileSaveAs();
    }
    
    DWORD dwLastError = 0;
    SaveTextFailure failure = SAVE_TEXT_FAILURE_NONE;
    if (SaveTextFileSilently(g_szFileName, TRUE, &dwLastError, &failure)) {
        g_bSaveInProgress = FALSE;
        return TRUE;
    }

    if ((failure == SAVE_TEXT_FAILURE_CREATE || failure == SAVE_TEXT_FAILURE_WRITE) &&
        dwLastError == ERROR_ACCESS_DENIED) {
        // Suppress the initial access-denied message; we'll prompt to elevate instead
        BOOL bResult = PerformElevatedSave(g_szFileName);
        g_bSaveInProgress = FALSE;
        return bResult;
    }

    ShowSaveTextFailure(failure, dwLastError);
    g_bSaveInProgress = FALSE;
    return FALSE;
}

//============================================================================
// FileSaveAs - Show Save As dialog and save file
//============================================================================
BOOL FileSaveAs()
{
    if (g_bSaveInProgress) {
        return FALSE; // avoid reentrant save attempts
    }
    g_bSaveInProgress = TRUE;

    // Setup file dialog
    OPENFILENAME ofn = {};
    WCHAR szFile[EXTENDED_PATH_MAX] = L"";
    WCHAR szInitialDir[EXTENDED_PATH_MAX];
    
    // Copy current filename if exists
    if (g_szFileName[0]) {
        wcscpy_s(szFile, EXTENDED_PATH_MAX, g_szFileName);
    }
    
    GetDocumentsPath(szInitialDir, EXTENDED_PATH_MAX);
    
    // Build dynamic filter string based on template categories
    WCHAR szFilter[1024];
    int txtFilterIndex = -1;
    BuildFileDialogFilter(szFilter, 1024, NULL, &txtFilterIndex);
    
    // Determine default filter index and extension
    int nFilterIndex = 1;  // Default to first filter
    WCHAR szDefExt[MAX_TEMPLATE_FILEEXT] = L"txt";
    
    if (g_szFileName[0] == L'\0') {
        // Untitled file - use current file extension from g_szCurrentFileExtension
        // (This is set when creating new file from template, e.g., Markdown document)
        wcscpy(szDefExt, g_szCurrentFileExtension);
    } else {
        // Existing file - extract extension from filename
        ExtractFileExtension(g_szFileName, szDefExt, MAX_TEMPLATE_FILEEXT);
    }
    
    // Now find the filter index for szDefExt
    if (szDefExt[0] != L'\0') {
        // Try to find matching filter by checking if it's txt first
        if (_wcsicmp(szDefExt, L"txt") == 0 && txtFilterIndex > 0) {
            nFilterIndex = txtFilterIndex;
        } else {
            // For other extensions, search through templates to find which category they belong to
            // Count unique categories until we find one matching our extension
            WCHAR seenCategories[32][MAX_FILTER_NAME];
            int seenCount = 0;
            BOOL found = FALSE;
            
            for (int i = 0; i < g_nTemplateCount && !found; i++) {
                if (g_Templates[i].szFileExtension[0] == L'\0') continue;
                
                // Get this template's category
                WCHAR szCategory[MAX_FILTER_NAME];
                if (g_Templates[i].szCategory[0] != L'\0') {
                    wcscpy(szCategory, g_Templates[i].szCategory);
                } else {
                    wcscpy(szCategory, g_Templates[i].szFileExtension);
                    _wcsupr(szCategory);
                }
                
                // Have we seen this category before?
                BOOL isSeen = FALSE;
                for (int j = 0; j < seenCount; j++) {
                    if (wcscmp(seenCategories[j], szCategory) == 0) {
                        isSeen = TRUE;
                        break;
                    }
                }
                
                if (!isSeen) {
                    // New category - does it contain our extension?
                    BOOL categoryMatches = FALSE;
                    for (int k = 0; k < g_nTemplateCount; k++) {
                        if (g_Templates[k].szFileExtension[0] == L'\0') continue;
                        
                        WCHAR szCat2[MAX_FILTER_NAME];
                        if (g_Templates[k].szCategory[0] != L'\0') {
                            wcscpy(szCat2, g_Templates[k].szCategory);
                        } else {
                            wcscpy(szCat2, g_Templates[k].szFileExtension);
                            _wcsupr(szCat2);
                        }
                        
                        if (wcscmp(szCat2, szCategory) == 0 && 
                            _wcsicmp(g_Templates[k].szFileExtension, szDefExt) == 0) {
                            categoryMatches = TRUE;
                            break;
                        }
                    }
                    
                    if (categoryMatches) {
                        nFilterIndex = seenCount + 1;  // 1-based
                        found = TRUE;
                    } else {
                        // Record this category and continue
                        wcscpy(seenCategories[seenCount], szCategory);
                        seenCount++;
                    }
                }
            }
        }
    } else {
        // No extension - default to txt
        wcscpy(szDefExt, L"txt");
        nFilterIndex = (txtFilterIndex > 0) ? txtFilterIndex : 1;
    }
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = g_hWndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = EXTENDED_PATH_MAX;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = nFilterIndex;  // Dynamic filter index based on file type
    ofn.lpstrInitialDir = szInitialDir;
    ofn.lpstrDefExt = szDefExt;  // Dynamic default extension based on current file
    // Avoid built-in test creates in protected folders; we handle overwrite/elevation ourselves.
    // Use NOTESTFILECREATE so the common dialog doesn't probe the path and pop
    // the OS "save to Documents" prompt in protected folders; we handle
    // overwrite and elevation ourselves after selection.
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOTESTFILECREATE;
    
    // Show dialog
    if (GetSaveFileName(&ofn)) {
        // Manual overwrite confirmation to avoid common dialog's protected-path prompt
        DWORD dwAttrib = GetFileAttributes(szFile);
        if (dwAttrib != INVALID_FILE_ATTRIBUTES) {
            WCHAR szPrompt[512];
            WCHAR szTitle[64];
            LoadStringResource(IDS_CONFIRM, szTitle, 64);
            _snwprintf(szPrompt, 512, L"%s\n\n%s", szFile, L"File already exists. Replace it?");
            int overwrite = MessageBox(g_hWndMain, szPrompt, szTitle, MB_YESNO | MB_ICONQUESTION);
            if (overwrite != IDYES) {
                g_bSaveInProgress = FALSE;
                return FALSE;
            }
        }

        DWORD dwLastError = 0;
        SaveTextFailure failure = SAVE_TEXT_FAILURE_NONE;
        if (SaveTextFileSilently(szFile, TRUE, &dwLastError, &failure)) {
            SetFocus(g_hWndEdit);
            g_bSaveInProgress = FALSE;
            return TRUE;
        }

        if ((failure == SAVE_TEXT_FAILURE_CREATE || failure == SAVE_TEXT_FAILURE_WRITE) &&
            dwLastError == ERROR_ACCESS_DENIED) {
            // Suppress the initial access-denied message; we'll prompt to elevate instead
            if (PerformElevatedSave(szFile)) {
                SetFocus(g_hWndEdit);
                g_bSaveInProgress = FALSE;
                return TRUE;
            }
            g_bSaveInProgress = FALSE;
            return FALSE;
        }

        ShowSaveTextFailure(failure, dwLastError);
    }

    g_bSaveInProgress = FALSE;

    return FALSE;
}

//============================================================================
// PromptSaveChanges - Ask user to save changes if modified
//============================================================================
BOOL PromptSaveChanges()
{
    // Don't prompt if document is not modified
    if (!g_bModified) {
        return TRUE;
    }
    
    // ALWAYS prompt if document has been modified, even if it's now empty.
    // The user may have cut/deleted all text, and we should ask if they
    // want to save the now-empty document (which would delete the original content).
    
    WCHAR szPrompt[MAX_PATH + 100];
    WCHAR szTemplate[256];
    WCHAR szUntitled[64];
    
    LoadStringResource(IDS_SAVE_CHANGES_PROMPT, szTemplate, 256);
    LoadStringResource(IDS_UNTITLED, szUntitled, 64);
    
    if (g_szFileTitle[0]) {
        _snwprintf(szPrompt, MAX_PATH + 100, szTemplate, g_szFileTitle);
    } else {
        _snwprintf(szPrompt, MAX_PATH + 100, szTemplate, szUntitled);
    }
    
    int result = MessageBox(g_hWndMain, szPrompt, L"RichEditor",
                           MB_YESNOCANCEL | MB_ICONQUESTION);
    
    switch (result) {
        case IDYES:
            return FileSave(); // Save and continue if successful
        case IDNO:
            // User chose to discard changes
            // If this is a resumed file, clean up resume state now
            if (g_bIsResumedFile) {
                DeleteResumeFile(g_szResumeFilePath);
                g_bIsResumedFile = FALSE;
                g_szResumeFilePath[0] = L'\0';
                g_szOriginalFilePath[0] = L'\0';
            }
            // Clear modified flag so the caller does not re-prompt.
            // Do NOT set g_bSaveInProgress here: PromptSaveChanges is called
            // from FileNew/FileOpen/MRU as well as WM_CLOSE, and setting the
            // flag here permanently blocks saves in any session that survives
            // past this call (e.g. user presses No then cancels the Open dialog).
            // WM_CLOSE kills the autosave timer before reaching here, so no
            // stray autosave can fire in the shutdown path.
            g_bModified = FALSE;
            return TRUE; // Don't save, but continue
        case IDCANCEL:
        default:
            return FALSE; // Cancel operation
    }
}

//============================================================================
// EditUndo - Undo last operation
//============================================================================
void EditUndo()
{
    // Clear operation flags when undoing
    g_bLastOperationWasFilter = FALSE;
    g_bLastOperationWasReplace = FALSE;
    SendMessage(g_hWndEdit, EM_UNDO, 0, 0);
    
    // After undo, document may differ from saved file
    // (If autosave ran after an operation, then undo reverts but file still has the change)
    // Mark as modified to ensure save prompt appears
    g_bModified = TRUE;
    UpdateTitle();
}

//============================================================================
// EditRedo - Redo last undone operation
//============================================================================
void EditRedo()
{
    SendMessage(g_hWndEdit, EM_REDO, 0, 0);
    
    // After redo, document state changes
    // Mark as modified to ensure save prompt appears
    g_bModified = TRUE;
    UpdateTitle();
}

//============================================================================
// EditCut - Cut selected text to clipboard
//============================================================================
void EditCut()
{
    // No need to track - RichEdit reports this via EM_GETUNDONAME
    SendMessage(g_hWndEdit, WM_CUT, 0, 0);
}

//============================================================================
// EditCopy - Copy selected text to clipboard
//============================================================================
void EditCopy()
{
    SendMessage(g_hWndEdit, WM_COPY, 0, 0);
}

//============================================================================
// EditPaste - Paste text from clipboard
//============================================================================
void EditPaste()
{
    // No need to track - RichEdit reports this via EM_GETUNDONAME
    
    // Get selection before paste
    CHARRANGE crBefore;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&crBefore);
    
    // Perform paste
    SendMessage(g_hWndEdit, WM_PASTE, 0, 0);
    
    // If SelectAfterPaste is enabled, select the pasted text
    if (g_bSelectAfterPaste) {
        // Get selection after paste (cursor will be at end of pasted text)
        CHARRANGE crAfter;
        SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&crAfter);
        
        // Select from start of paste to end
        CHARRANGE crSelect;
        crSelect.cpMin = crBefore.cpMin;
        crSelect.cpMax = crAfter.cpMax;
        SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crSelect);
    }
}

//============================================================================
// EditSelectAll - Select all text in editor
//============================================================================
void EditSelectAll()
{
    CHARRANGE cr;
    cr.cpMin = 0;
    cr.cpMax = -1; // -1 means end of text
    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
}

//============================================================================
// EditInsertTimeDate - Insert date/time at cursor (F5 key / menu)
// Now uses configurable DateTimeTemplate setting (Phase 2.10, ToDo #3)
//============================================================================
void EditInsertTimeDate()
{
    // Expand configured template (uses internal variables, e.g., "%shortdate% %shorttime%")
    LONG nCursorOffset = -1;
    LPWSTR pszExpanded = ExpandTemplateVariables(g_szDateTimeTemplate, &nCursorOffset);
    
    if (pszExpanded) {
        // Insert at current cursor position (replaces selection if any)
        SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszExpanded);
        free(pszExpanded);
    }
}

//============================================================================
// SetRichEditWordWrap - Set RichEdit wrap width in twips
//============================================================================
void SetRichEditWordWrap(HWND hEdit, LONG widthTwips)
{
    if (!hEdit) return;

    if (g_fRichEditVersion >= 8.0f) {
        // RichEdit 8+ behaves more predictably with NULL HDC (Notepad-like behavior)
        SendMessage(hEdit, EM_SETTARGETDEVICE, 0, (LPARAM)widthTwips);
        return;
    }

    HDC hdc = GetDC(hEdit);
    if (!hdc) return;
    SendMessage(hEdit, EM_SETTARGETDEVICE, (WPARAM)hdc, (LPARAM)widthTwips);
    ReleaseDC(hEdit, hdc);
}

//============================================================================
// GetTwipsForPixels - Convert pixel width to twips (1/1440 inch)
//============================================================================
LONG GetTwipsForPixels(HWND hWnd, int widthPx)
{
    HDC hdc = GetDC(hWnd);
    if (!hdc) return 0;
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hWnd, hdc);
    return MulDiv(widthPx, 1440, dpiX);
}

//============================================================================
// ApplyWordWrap - Apply current word wrap setting to an edit control
//============================================================================
void ApplyWordWrap(HWND hEdit)
{
    if (!hEdit) return;
    
    LONG_PTR style = GetWindowLongPtr(hEdit, GWL_STYLE);
    
    if (g_bWordWrap) {
        style &= ~(WS_HSCROLL | ES_AUTOHSCROLL);
        SetWindowLongPtr(hEdit, GWL_STYLE, style);
        SetWindowPos(hEdit, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        if (g_fRichEditVersion >= 8.0f) {
            // RichEdit 8+ uses a Notepad-like internal wrap; widthTwips=0 uses window width
            SetRichEditWordWrap(hEdit, 0);
        } else {
            RECT rcClient;
            GetClientRect(hEdit, &rcClient);
            LONG widthTwips = GetTwipsForPixels(hEdit, rcClient.right - rcClient.left);
            // Adjust for zoom: EM_GETZOOM returns FALSE (and leaves params 0) at 100%.
            // When zoomed, the same twip count occupies more screen pixels, so we divide.
            DWORD nNum = 0, nDen = 0;
            BOOL bZoomed = (BOOL)SendMessage(hEdit, EM_GETZOOM, (WPARAM)&nNum, (LPARAM)&nDen);
            if (bZoomed && nNum > 0 && nDen > 0 && nNum != nDen) {
                widthTwips = MulDiv(widthTwips, nDen, nNum);
            }
            SetRichEditWordWrap(hEdit, widthTwips);
        }
    } else {
        style |= (WS_HSCROLL | ES_AUTOHSCROLL);
        SetWindowLongPtr(hEdit, GWL_STYLE, style);
        SetWindowPos(hEdit, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        // RichEdit 8+ may still visually segment long lines (~1000 chars) without hard breaks
        const LONG kNoWrapTwips = 0x7FFFFFFF;
        SetRichEditWordWrap(hEdit, kNoWrapTwips);
    }
}

//============================================================================
// ViewWordWrap - Toggle word wrap on/off
//============================================================================
void ViewWordWrap()
{
    // Toggle word wrap state
    g_bWordWrap = !g_bWordWrap;
    
    ApplyWordWrap(g_hWndEdit);
    
    // Update menu checkmark
    HMENU hMenu = GetMenu(g_hWndMain);
    CheckMenuItem(hMenu, ID_VIEW_WORDWRAP, g_bWordWrap ? MF_CHECKED : MF_UNCHECKED);
    
    // Set focus back to edit control
    SetFocus(g_hWndEdit);

    g_bBookmarksDirty = TRUE;
}

//============================================================================
// ViewZoomReset - Reset zoom to 100% (Ctrl+0 / View → Reset Zoom)
//============================================================================
void ViewZoomReset()
{
    // EM_SETZOOM(0,0) resets to default (100%)
    SendMessage(g_hWndEdit, EM_SETZOOM, 0, 0);
    g_nZoomPercent = 100;
    ApplyWordWrap(g_hWndEdit);
    UpdateStatusBar();
    SetFocus(g_hWndEdit);
}

//============================================================================
// AboutDlgProc - About dialog procedure
//============================================================================
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /* lParam */)
{
    switch (msg) {
        case WM_INITDIALOG:
            {
                // Display RichEdit version information (Phase 2.8.5)
                // Line 1: Version + Class name
                // Line 2: Full DLL path
                
                // Extract just the filename from full path (e.g., "C:\...\RICHED20.DLL" → "RICHED20.DLL")
                const WCHAR* pszFileName = wcsrchr(g_szRichEditLibPath, L'\\');
                if (pszFileName) {
                    pszFileName++;  // Skip backslash
                } else {
                    pszFileName = g_szRichEditLibPath;  // No path separator, use as-is
                }
                
                // Build Line 1: "RichEdit X.X (FILENAME.DLL, ClassName)"
                // Using wcscpy/wcscat instead of swprintf (safer with MinGW, per AGENTS.md)
                WCHAR szVersionText[256];
                wcscpy(szVersionText, L"RichEdit ");
                
                // Append version number
                WCHAR szVersion[16];
                _snwprintf(szVersion, 16, L"%.1f", g_fRichEditVersion);
                wcscat(szVersionText, szVersion);
                
                wcscat(szVersionText, L" (");
                wcscat(szVersionText, pszFileName);
                wcscat(szVersionText, L", ");
                wcscat(szVersionText, g_szRichEditClassName);
                wcscat(szVersionText, L")");
                
                // Set Line 1 in the IDC_RICHEDIT_VERSION control
                SetDlgItemText(hwnd, IDC_RICHEDIT_VERSION, szVersionText);
                
                // Set Line 2: Full DLL path in IDC_RICHEDIT_VERSION_PATH control
                SetDlgItemText(hwnd, IDC_RICHEDIT_VERSION_PATH, g_szRichEditLibPath);
            }
            return TRUE;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, LOWORD(wParam));
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwnd, 0);
            return TRUE;
    }
    
    return FALSE;
}

//============================================================================
// RunFilterCommand - Execute filter command and capture output
// Returns: true on success, false on failure
//============================================================================
bool RunFilterCommand(const WCHAR* pszCommand, const char* pszInputUTF8, 
                      std::string& outputData, std::string& errorData, DWORD& dwExitCode)
{
    // Create pipes for stdin, stdout, stderr
    HANDLE hStdinRead, hStdinWrite;
    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0) ||
        !CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0) ||
        !CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        WCHAR szError[256], szTitle[64];
        LoadStringResource(IDS_PIPE_CREATE_FAILED, szError, 256);
        LoadStringResource(IDS_ERROR, szTitle, 64);
        MessageBox(g_hWndMain, szError, szTitle, MB_ICONERROR);
        return false;
    }
    
    // Ensure write handles aren't inherited
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
    
    // Setup process startup info
    STARTUPINFO si = {};
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    
    PROCESS_INFORMATION pi = {};
    
    // Create process
    WCHAR szCommandCopy[MAX_FILTER_COMMAND + 100];
    wcscpy(szCommandCopy, pszCommand);
    
    if (!CreateProcess(NULL, szCommandCopy, NULL, NULL, TRUE, 
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WCHAR szError[512], szTitle[64], szTemplate[256];
        LoadStringResource(IDS_FILTER_EXEC_FAILED, szTemplate, 256);
        _snwprintf(szError, 512, szTemplate, GetLastError());
        LoadStringResource(IDS_FILTER_EXEC_ERROR, szTitle, 64);
        MessageBox(g_hWndMain, szError, szTitle, MB_ICONERROR);
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrRead);
        CloseHandle(hStderrWrite);
        return false;
    }
    
    // Close unused pipe ends
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
    
    // Write input to process stdin
    DWORD dwWritten;
    WriteFile(hStdinWrite, pszInputUTF8, strlen(pszInputUTF8), &dwWritten, NULL);
    CloseHandle(hStdinWrite);
    
    // Read stdout
    char bufferOut[4096];
    DWORD dwRead;
    outputData.clear();
    while (ReadFile(hStdoutRead, bufferOut, sizeof(bufferOut) - 1, &dwRead, NULL) && dwRead > 0) {
        bufferOut[dwRead] = '\0';
        outputData.append(bufferOut, dwRead);
    }
    CloseHandle(hStdoutRead);
    
    // Read stderr
    char bufferErr[4096];
    errorData.clear();
    while (ReadFile(hStderrRead, bufferErr, sizeof(bufferErr) - 1, &dwRead, NULL) && dwRead > 0) {
        bufferErr[dwRead] = '\0';
        errorData.append(bufferErr, dwRead);
    }
    CloseHandle(hStderrRead);
    
    // Wait for process to complete (with timeout)
    WaitForSingleObject(pi.hProcess, 30000);
    
    // Get exit code
    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return true;
}

//============================================================================
// ExecuteFilterInsert - Handle insert action (replace, below, append)
//============================================================================
void ExecuteFilterInsert(const std::string& outputData, CHARRANGE crSel)
{
    if (outputData.empty()) return;
    
    // Track filter operation for undo
    g_bLastOperationWasFilter = TRUE;
    
    LPWSTR pszOutput = UTF8ToUTF16(outputData.c_str());
    if (!pszOutput) return;
    
    // Strip trailing newline from filter output
    size_t len = wcslen(pszOutput);
    if (len > 0 && pszOutput[len - 1] == L'\n') {
        pszOutput[len - 1] = L'\0';
        if (len > 1 && pszOutput[len - 2] == L'\r') {
            pszOutput[len - 2] = L'\0';
        }
    }
    
    FilterInsertMode insertMode = g_Filters[g_nCurrentFilter].insertMode;
    
    if (insertMode == FILTER_INSERT_REPLACE) {
        // Replace: Replace the selection with output
        SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszOutput);
        
    } else if (insertMode == FILTER_INSERT_APPEND) {
        // Append: Move to end of selection and append
        crSel.cpMin = crSel.cpMax;
        SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crSel);
        SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszOutput);
        
    } else {  // FILTER_INSERT_BELOW (default)
        // Below: Position cursor at end of selection, add newline, then output
        crSel.cpMin = crSel.cpMax;
        SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crSel);
        
        // Insert newline
        WCHAR szNewline[] = L"\r\n";
        SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)szNewline);
        
        // Insert output
        SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszOutput);
    }
    
    free(pszOutput);
}

//============================================================================
// ExecuteFilterDisplay - Handle display action (statusbar, messagebox)
//============================================================================
void ExecuteFilterDisplay(const std::string& outputData)
{
    if (outputData.empty()) return;
    
    LPWSTR pszOutput = UTF8ToUTF16(outputData.c_str());
    if (!pszOutput) return;
    
    // Strip trailing newline
    size_t len = wcslen(pszOutput);
    if (len > 0 && pszOutput[len - 1] == L'\n') {
        pszOutput[len - 1] = L'\0';
        if (len > 1 && pszOutput[len - 2] == L'\r') {
            pszOutput[len - 2] = L'\0';
        }
    }
    
    FilterDisplayMode displayMode = g_Filters[g_nCurrentFilter].displayMode;
    
    if (displayMode == FILTER_DISPLAY_STATUSBAR) {
        // Show in status bar for 30 seconds
        wcscpy(g_szFilterStatusBarText, L"[");
        wcscat(g_szFilterStatusBarText, g_Filters[g_nCurrentFilter].szLocalizedName);
        wcscat(g_szFilterStatusBarText, L"]: ");
        wcscat(g_szFilterStatusBarText, pszOutput);
        g_szFilterStatusBarText[511] = L'\0';  // Ensure null termination
        
        g_bFilterStatusBarActive = TRUE;
        UpdateStatusBar();
        
        // Start 30-second timer
        SetTimer(g_hWndMain, IDT_FILTER_STATUSBAR, 30000, NULL);
        
    } else {  // FILTER_DISPLAY_MESSAGEBOX
        // Show in message box
        WCHAR szTitle[MAX_FILTER_NAME + 64];
        WCHAR szResult[32];
        LoadStringResource(IDS_FILTER_RESULT_TITLE, szResult, 32);
        
        wcscpy(szTitle, g_Filters[g_nCurrentFilter].szLocalizedName);
        wcscat(szTitle, L" ");
        wcscat(szTitle, szResult);
        
        MessageBox(g_hWndMain, pszOutput, szTitle, MB_ICONINFORMATION);
    }
    
    free(pszOutput);
}

//============================================================================
// ExecuteFilterClipboard - Handle clipboard action (copy, append)
//============================================================================
void ExecuteFilterClipboard(const std::string& outputData)
{
    if (outputData.empty()) return;
    
    LPWSTR pszOutput = UTF8ToUTF16(outputData.c_str());
    if (!pszOutput) return;
    
    // Strip trailing newline
    size_t len = wcslen(pszOutput);
    if (len > 0 && pszOutput[len - 1] == L'\n') {
        pszOutput[len - 1] = L'\0';
        if (len > 1 && pszOutput[len - 2] == L'\r') {
            pszOutput[len - 2] = L'\0';
        }
    }
    
    FilterClipboardMode clipboardMode = g_Filters[g_nCurrentFilter].clipboardMode;
    
    if (!OpenClipboard(g_hWndMain)) {
        free(pszOutput);
        return;
    }
    
    if (clipboardMode == FILTER_CLIPBOARD_APPEND) {
        // Append mode: Get existing clipboard text and append
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            LPWSTR pszExisting = (LPWSTR)GlobalLock(hData);
            if (pszExisting) {
                size_t existingLen = wcslen(pszExisting);
                size_t newLen = wcslen(pszOutput);
                size_t totalLen = existingLen + newLen + 1;
                
                LPWSTR pszCombined = (LPWSTR)malloc(totalLen * sizeof(WCHAR));
                if (pszCombined) {
                    wcscpy(pszCombined, pszExisting);
                    wcscat(pszCombined, pszOutput);
                    
                    GlobalUnlock(hData);
                    
                    // Set combined text to clipboard
                    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, totalLen * sizeof(WCHAR));
                    if (hGlobal) {
                        LPWSTR pszGlobal = (LPWSTR)GlobalLock(hGlobal);
                        if (pszGlobal) {
                            wcscpy(pszGlobal, pszCombined);
                            GlobalUnlock(hGlobal);
                            
                            EmptyClipboard();
                            SetClipboardData(CF_UNICODETEXT, hGlobal);
                        }
                    }
                    
                    free(pszCombined);
                } else {
                    GlobalUnlock(hData);
                }
            }
        }
    } else {  // FILTER_CLIPBOARD_COPY
        // Copy mode: Replace clipboard contents
        size_t textLen = wcslen(pszOutput) + 1;
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, textLen * sizeof(WCHAR));
        if (hGlobal) {
            LPWSTR pszGlobal = (LPWSTR)GlobalLock(hGlobal);
            if (pszGlobal) {
                wcscpy(pszGlobal, pszOutput);
                GlobalUnlock(hGlobal);
                
                EmptyClipboard();
                SetClipboardData(CF_UNICODETEXT, hGlobal);
            }
        }
    }
    
    CloseClipboard();
    free(pszOutput);
}

//============================================================================
// ExecuteFilter - Execute current filter on selected text or current line
//============================================================================
void ExecuteFilter()
{
    // Check if a filter is selected
    if (g_nCurrentFilter < 0 || g_nCurrentFilter >= g_nFilterCount) {
        WCHAR szMessage[256], szTitle[64];
        LoadStringResource(IDS_NO_FILTER_SELECTED_MSG, szMessage, 256);
        LoadStringResource(IDS_NO_FILTER_SELECTED, szTitle, 64);
        MessageBox(g_hWndMain, szMessage, szTitle, MB_ICONEXCLAMATION);
        return;
    }
    
    // Block insert filters in read-only mode
    if (g_bReadOnly && g_Filters[g_nCurrentFilter].action == FILTER_ACTION_INSERT) {
        return;  // Silently fail (menu item should be disabled anyway)
    }
    
    // Get selected text range
    CHARRANGE crSel;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&crSel);
    
    // If no selection, select current line (excluding newline)
    if (crSel.cpMin == crSel.cpMax) {
        // Get line number
        LONG lineNum = SendMessage(g_hWndEdit, EM_LINEFROMCHAR, crSel.cpMin, 0);
        // Get line start
                LONG lineStart = SendMessage(g_hWndEdit, EM_LINEINDEX, lineNum, 0);
                // Get line length (excluding newline characters)
                LONG lineLength = SendMessage(g_hWndEdit, EM_LINELENGTH, lineStart, 0);
        
        crSel.cpMin = lineStart;
        crSel.cpMax = lineStart + lineLength;
        // Apply the selection to the editor
        SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crSel);
    }
    
    // Get selected text
    int textLen = crSel.cpMax - crSel.cpMin;
    if (textLen <= 0) {
        WCHAR szError[256], szTitle[64];
        LoadStringResource(IDS_NO_TEXT_TO_PROCESS, szError, 256);
        LoadStringResource(IDS_FILTER_EXECUTION, szTitle, 64);
        MessageBox(g_hWndMain, szError, szTitle, MB_ICONEXCLAMATION);
        return;
    }
    
    LPWSTR pszInput = (LPWSTR)malloc((textLen + 1) * sizeof(WCHAR));
    if (!pszInput) return;
    
    TEXTRANGE tr;
    tr.chrg = crSel;
    tr.lpstrText = pszInput;
    SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    pszInput[textLen] = L'\0';
    
    // Convert to UTF-8 for pipe
    LPSTR pszInputUTF8 = UTF16ToUTF8(pszInput);
    free(pszInput);
    if (!pszInputUTF8) return;
    
    // Run the filter command
    std::string outputData, errorData;
    DWORD dwExitCode;
    
    if (!RunFilterCommand(g_Filters[g_nCurrentFilter].szCommand, pszInputUTF8, 
                          outputData, errorData, dwExitCode)) {
        free(pszInputUTF8);
        return;
    }
    
    free(pszInputUTF8);
    
    // Show errors if any
    if (!errorData.empty()) {
        LPWSTR pszError = UTF8ToUTF16(errorData.c_str());
        if (pszError) {
            WCHAR szMsg[2048], szTitle[64], szTemplate[256];
            LoadStringResource(IDS_FILTER_STDERR_OUTPUT, szTemplate, 256);
            _snwprintf(szMsg, 2048, szTemplate, pszError);
            LoadStringResource(IDS_FILTER_ERROR, szTitle, 64);
            MessageBox(g_hWndMain, szMsg, szTitle, MB_ICONWARNING);
            free(pszError);
        }
    }
    
    // Dispatch based on action type
    FilterAction action = g_Filters[g_nCurrentFilter].action;
    
    switch (action) {
        case FILTER_ACTION_INSERT:
            ExecuteFilterInsert(outputData, crSel);
            break;
            
        case FILTER_ACTION_DISPLAY:
            ExecuteFilterDisplay(outputData);
            break;
            
        case FILTER_ACTION_CLIPBOARD:
            ExecuteFilterClipboard(outputData);
            break;
            
        case FILTER_ACTION_NONE:
            // Do nothing with output - command was run for side effects
            break;
            
        case FILTER_ACTION_REPL:
            // REPL filters don't use ExecuteFilter - they use StartREPLFilter instead
            // This case should never be reached
            break;
    }
    
    // Handle case where filter produced no output and exited with error
    if (outputData.empty() && dwExitCode != 0 && action == FILTER_ACTION_INSERT) {
        WCHAR szMsg[256], szTitle[64], szTemplate[256];
        LoadStringResource(IDS_FILTER_EXIT_CODE, szTemplate, 256);
        _snwprintf(szMsg, 256, szTemplate, dwExitCode);
        LoadStringResource(IDS_FILTER_RESULT, szTitle, 64);
        MessageBox(g_hWndMain, szMsg, szTitle, MB_ICONINFORMATION);
    }
}

//============================================================================
// CreateDefaultINI - Create default INI file with example filters
//============================================================================
void CreateDefaultINI()
{
    // Get path to INI file (in same directory as executable)
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    // Check if file already exists
    DWORD dwAttrib = GetFileAttributes(szIniPath);
    if (dwAttrib != INVALID_FILE_ATTRIBUTES) {
        return;  // File exists, don't overwrite
    }
    
    // Create default INI file with UTF-8 encoding
    HANDLE hFile = CreateFile(szIniPath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;  // Failed to create file
    }
    
    // Write UTF-8 BOM (optional, but helps some text editors)
    const char utf8bom[] = "\xEF\xBB\xBF";
    DWORD dwWritten;
    WriteFile(hFile, utf8bom, 3, &dwWritten, NULL);
    
    // Write default configuration
    const char* szDefaultINI = 
        "[Settings]\r\n"
        "; Editor settings\r\n"
        "WordWrap=1                    ; 1=enabled, 0=disabled (default: 1)\r\n"
        "\r\n"
        "; RichEdit library configuration (Phase 2.8)\r\n"
        "; RichEditLibraryPath=         ; Path to custom RichEdit DLL (optional, leave empty for default)\r\n"
        ";                              ; Examples:\r\n"
        ";                              ;   Relative: RICHED20.DLL  or  libs\\RICHED20.DLL\r\n"
        ";                              ;   Absolute: C:\\Program Files\\Microsoft Office\\root\\Office16\\RICHED20.DLL\r\n"
        ";                              ; Default cascade: MSFTEDIT.DLL (v4.1+) → RICHED20.DLL (v2.0+) → RICHED32.DLL (v1.0)\r\n"
        ";                              ; Office 365/2019+: C:\\Program Files\\Microsoft Office\\root\\Office16\\RICHED20.DLL (v6.0+)\r\n"
        ";                              ; Office 2016:      C:\\Program Files (x86)\\Microsoft Office\\Office16\\RICHED20.DLL (v6.0)\r\n"
        ";                              ; Office 2013:      C:\\Program Files (x86)\\Microsoft Office\\Office15\\RICHED20.DLL (v8.0)\r\n"
        ";                              ; Why? Office versions use UI Automation (faster with NVDA) instead of legacy MSAA\r\n"
        "; RichEditClassName=           ; Window class override (optional, leave empty for auto-detection)\r\n"
        ";                              ; Available classes:\r\n"
        ";                              ;   RichEditD2DPT - Direct2D + UI Automation (v8.0+, Windows 11/Office 365)\r\n"
        ";                              ;   RichEditD2D   - Direct2D without Paint Through (v8.0+)\r\n"
        ";                              ;   RichEdit60W   - Office 2007+ (v6.0+)\r\n"
        ";                              ;   RichEdit20W   - Office 2003+ (v5.0+)\r\n"
        ";                              ;   RICHEDIT50W   - MSFTEDIT.DLL (v4.0+)\r\n"
        ";                              ;   RICHEDIT      - Legacy ANSI (v1.0)\r\n"
        ";                              ; Default: Auto-detects best class (prefers RichEditD2DPT for v8.0+)\r\n"
        ";                              ; Why override? Some DLLs report v8.0 but don't support RichEditD2DPT\r\n"
        ";                              ; Example: RichEditClassName=RichEdit60W\r\n"
        "\r\n"
        "; Autosave settings\r\n"
        "AutosaveEnabled=1             ; 1=enabled, 0=disabled (default: 1)\r\n"
        "AutosaveIntervalMinutes=1     ; Autosave interval in minutes, 0=disabled (default: 1)\r\n"
        "AutosaveOnFocusLoss=1         ; 1=save when window loses focus, 0=don't (default: 1)\r\n"
        "\r\n"
        "; Accessibility settings\r\n"
        "ShowMenuDescriptions=1        ; 1=show descriptions in menus (accessible), 0=names only (default: 1)\r\n"
        "\r\n"
        "; Editor behavior settings\r\n"
        "SelectAfterPaste=0            ; 1=select pasted text, 0=cursor after paste (default: 0)\r\n"
        "AutoSaveUntitledOnClose=0     ; 1=auto-save untitled files on close (no prompt), 0=prompt as usual (default: 0)\r\n"
        "\r\n"
        "; URL detection (accessibility: screen reader link roles, context menu 'Open URL', click-to-open)\r\n"
        "; Set DetectURLs=0 if cursor movement is slow on very large files (RichEdit scans per keystroke when enabled)\r\n"
        "DetectURLs=1                  ; 1=detect URLs (default), 0=disable for large-file performance\r\n"
        "\r\n"
        "; Display settings\r\n"
        "TabSize=8                     ; Tab size in spaces for column calculation (default: 8)\r\n"
        "Zoom=100                      ; Zoom percentage (100 = default, range 1-6400)\r\n"
        "\r\n"
        "; Date/Time formatting (Phase 2.10, ToDo #3)\r\n"
        "; DateTimeTemplate: Format for F5 key and Edit→Insert Time/Date menu (default: %date% %time%)\r\n"
        ";                   Uses %date% and %time% variables, which respect DateFormat/TimeFormat settings below\r\n"
        "; DateFormat: Format for %date% variable in templates (default: %shortdate%)\r\n"
        "; TimeFormat: Format for %time% variable in templates (default: HH:mm)\r\n"
        ";\r\n"
        "; Internal variables (use dwFlags, locale-aware):\r\n"
        ";   %shortdate%  - Short date (e.g., 1/20/2026)\r\n"
        ";   %longdate%   - Long date (e.g., Monday, January 20, 2026)\r\n"
        ";   %yearmonth%  - Year and month (e.g., January 2026)\r\n"
        ";   %monthday%   - Month and day (e.g., January 20)\r\n"
        ";   %shorttime%  - Short time without seconds (e.g., 10:30 PM)\r\n"
        ";   %longtime%   - Long time with seconds (e.g., 10:30:45 PM)\r\n"
        ";\r\n"
        "; Custom format strings (see https://learn.microsoft.com/en-us/windows/win32/intl/day-month-year-and-era-format-pictures):\r\n"
        ";   Date: d dd ddd dddd M MM MMM MMMM y yy yyyy g gg (e.g., yyyy-MM-dd, dd.MM.yyyy, MMMM d, yyyy)\r\n"
        ";   Time: h hh H HH m mm s ss t tt (e.g., HH:mm, h:mm tt, HH:mm:ss)\r\n"
        ";   Literals: Use single quotes (e.g., 'Day 'dd' of 'MMMM → Day 20 of January)\r\n"
        ";\r\n"
        "; Examples:\r\n"
        ";   DateTimeTemplate=%date% 'at' %time%              → Uses your custom DateFormat and TimeFormat\r\n"
        ";   DateTimeTemplate=%longdate% 'at' %shorttime%     → Monday, January 20, 2026 at 10:30 PM\r\n"
        ";   DateFormat=yyyy-MM-dd                            → 2026-01-20 (ISO format)\r\n"
        ";   TimeFormat=HH:mm:ss                              → 22:30:45 (24-hour with seconds)\r\n"
        ";\r\n"
        "; See README.md for comprehensive documentation and more examples\r\n"
        "DateTimeTemplate=%date% %time%\r\n"
        "DateFormat=%shortdate%\r\n"
        "TimeFormat=HH:mm\r\n"
        "\r\n"
        "; Filter System\r\n"
        "; Filters transform text using external commands\r\n"
        "; Action types: insert, display, clipboard, none, repl\r\n"
        "; Insert modes: replace, below, append\r\n"
        "; Display modes: statusbar, messagebox\r\n"
        "; Clipboard modes: copy, append\r\n"
        "; Lines starting with ';' or '#' are comments\r\n"
        "; REPL settings: PromptEnd, EOLDetection (auto/crlf/lf/cr), ExitNotification\r\n"
        ";   EOLDetection: auto=detect from output (defaults to LF), lf=Unix/Linux, crlf=Windows, cr=old Mac\r\n"
        ";   Use 'lf' for WSL/bash/python/node, 'auto' for PowerShell\r\n"
        ";   NOTE: REPL filters with PTY (script command) will echo input - this is normal terminal behavior\r\n"
        ";   You'll see: your typed command, then shell echo + output, then next prompt\r\n"
        "; ContextMenu: 1=show in right-click menu, 0=Tools menu only\r\n"
        "; ContextMenuOrder: Sort order in context menu (lower numbers first)\r\n"
        "\r\n"
        "[Filters]\r\n"
        "Count=10\r\n"
        "\r\n"
        "; === INSERT ACTION EXAMPLES ===\r\n"
        "; Filters that modify the document\r\n"
        "\r\n"
        "[Filter1]\r\n"
        "Name=Uppercase\r\n"
        "Name.cs=Velká písmena\r\n"
        "Command=powershell -NoProfile -Command \"$input | ForEach-Object { $_.ToUpper() }\"\r\n"
        "Description=Converts selected text to UPPERCASE letters\r\n"
        "Description.cs=Převede vybraný text na VELKÁ PÍSMENA\r\n"
        "Category=Transform\r\n"
        "Action=insert\r\n"
        "Insert=replace\r\n"
        "ContextMenu=1\r\n"
        "ContextMenuOrder=1\r\n"
        "\r\n"
        "[Filter2]\r\n"
        "Name=Lowercase\r\n"
        "Name.cs=Malá písmena\r\n"
        "Command=powershell -NoProfile -Command \"$input | ForEach-Object { $_.ToLower() }\"\r\n"
        "Description=Converts selected text to lowercase letters\r\n"
        "Description.cs=Převede vybraný text na malá písmena\r\n"
        "Category=Transform\r\n"
        "Action=insert\r\n"
        "Insert=replace\r\n"
        "ContextMenu=1\r\n"
        "ContextMenuOrder=2\r\n"
        "\r\n"
        "[Filter3]\r\n"
        "Name=Sort Lines\r\n"
        "Name.cs=Seřadit řádky\r\n"
        "Command=powershell -NoProfile -Command \"$input -split '\\r?\\n' | Sort-Object | Out-String\"\r\n"
        "Description=Sorts selected lines alphabetically\r\n"
        "Description.cs=Seřadí vybrané řádky abecedně\r\n"
        "Category=Transform\r\n"
        "Action=insert\r\n"
        "Insert=replace\r\n"
        "ContextMenu=1\r\n"
        "ContextMenuOrder=3\r\n"
        "\r\n"
        "[Filter4]\r\n"
        "Name=Add Line Numbers\r\n"
        "Name.cs=Přidat čísla řádků\r\n"
        "Command=powershell -NoProfile -Command \"$input -split '\\r?\\n' | ForEach-Object -Begin { $i=0 } -Process { \\\"{0,4}: {1}\\\" -f (++$i), $_ } | Out-String\"\r\n"
        "Description=Inserts line numbers before each line\r\n"
        "Description.cs=Vloží čísla řádků před každý řádek\r\n"
        "Category=Transform\r\n"
        "Action=insert\r\n"
        "Insert=below\r\n"
        "ContextMenu=1\r\n"
        "ContextMenuOrder=4\r\n"
        "\r\n"
        "; === DISPLAY ACTION EXAMPLES ===\r\n"
        "; Filters that show information without modifying the document\r\n"
        "\r\n"
        "[Filter5]\r\n"
        "Name=Line Count\r\n"
        "Name.cs=Počet řádků\r\n"
        "Command=powershell -NoProfile -Command \"($input | Measure-Object -Line).Lines\"\r\n"
        "Description=Displays the number of lines in selected text\r\n"
        "Description.cs=Zobrazí počet řádků ve vybraném textu\r\n"
        "Category=Statistics\r\n"
        "Action=display\r\n"
        "Display=messagebox\r\n"
        "ContextMenu=0\r\n"
        "ContextMenuOrder=999\r\n"
        "\r\n"
        "[Filter6]\r\n"
        "Name=Word Count\r\n"
        "Name.cs=Počet slov\r\n"
        "Command=powershell -NoProfile -Command \"($input -split '\\s+' | Where-Object {$_ -ne ''} | Measure-Object).Count\"\r\n"
        "Description=Shows word count in status bar for 30 seconds\r\n"
        "Description.cs=Zobrazí počet slov ve stavovém řádku na 30 sekund\r\n"
        "Category=Statistics\r\n"
        "Action=display\r\n"
        "Display=statusbar\r\n"
        "ContextMenu=0\r\n"
        "ContextMenuOrder=999\r\n"
        "\r\n"
        "; === CLIPBOARD ACTION EXAMPLES ===\r\n"
        "; Filters that copy results to clipboard silently\r\n"
        "\r\n"
        "[Filter7]\r\n"
        "Name=Copy Reversed\r\n"
        "Name.cs=Kopírovat obrácený text\r\n"
        "Command=powershell -NoProfile -Command \"-join (($input -join [Environment]::NewLine).ToCharArray() | Sort-Object {Get-Random})\"\r\n"
        "Description=Reverses text and copies result to clipboard\r\n"
        "Description.cs=Obrátí text a zkopíruje výsledek do schránky\r\n"
        "Category=Clipboard\r\n"
        "Action=clipboard\r\n"
        "Clipboard=copy\r\n"
        "ContextMenu=0\r\n"
        "ContextMenuOrder=999\r\n"
        "\r\n"
        "[Filter8]\r\n"
        "Name=Auto Indent\r\n"
        "Name.cs=Automatické odsazení\r\n"
        "Command=powershell.exe -NoProfile -Command \"([Console]::InputEncoding=[Text.Encoding]::UTF8),([Console]::OutputEncoding=[Text.Encoding]::UTF8),([Environment]::NewLine + [regex]::Match([Console]::In.ReadToEnd(), '\\A[ \\t]*').Value) | Select-Object -Last 1\"\r\n"
        "Description=Appends a newline with the same leading whitespace as the selection start\r\n"
        "Description.cs=Přidá nový řádek se stejným odsazením jako začátek výběru\r\n"
        "Category=Utility\r\n"
        "Action=insert\r\n"
        "Insert=replace\r\n"
        "ContextMenu=1\r\n"
        "ContextMenuOrder=5\r\n"
        "\r\n"
        "; === REPL ACTION EXAMPLE ===\r\n"
        "; Interactive filters that stay running for continuous input/output\r\n"
        "\r\n"
        "[Filter9]\r\n"
        "Name=PowerShell\r\n"
        "Name.cs=PowerShell\r\n"
        "Command=powershell -NoLogo -NoExit\r\n"
        "Description=PowerShell console (Enter to execute, Shift+Enter for newline)\r\n"
        "Description.cs=Konzole PowerShell (Enter pro spuštění, Shift+Enter pro nový řádek)\r\n"
        "Category=Interactive\r\n"
        "Category.cs=Interaktivní\r\n"
        "Action=repl\r\n"
        "PromptEnd=> \r\n"
        "EOLDetection=auto\r\n"
        "ExitNotification=1\r\n"
        "ContextMenu=0\r\n"
        "ContextMenuOrder=999\r\n"
        "\r\n"
        "[Filter10]\r\n"
        "Name=WSL Bash\r\n"
        "Name.cs=WSL Bash\r\n"
        "Command=wsl.exe script -qfc bash /dev/null\r\n"
        "Description=WSL Bash shell (script creates pseudo-TTY for prompts)\r\n"
        "Description.cs=WSL Bash shell (script vytvoří pseudo-TTY pro výzvy)\r\n"
        "Category=Interactive\r\n"
        "Category.cs=Interaktivní\r\n"
        "Action=repl\r\n"
        "PromptEnd=$ \r\n"
        "EOLDetection=lf\r\n"
        "ExitNotification=1\r\n"
        "ContextMenu=0\r\n"
        "ContextMenuOrder=999\r\n"
        "\r\n"
        "; Template System\r\n"
        "; Templates insert pre-defined text snippets with variable expansion\r\n"
        "; Variables: %cursor%, %selection%, %date%, %time%, %datetime%, %clipboard%\r\n"
        "; FileExtension: Empty=always available, or specify extension like 'md', 'txt'\r\n"
        "; Shortcut: Optional keyboard shortcut (e.g., Ctrl+1, Ctrl+Shift+C)\r\n"
        "; Escape sequences: \\n (newline), \\t (tab), \\r (carriage return), \\\\ (backslash)\r\n"
        "\r\n"
        "[Templates]\r\n"
        "Count=15\r\n"
        "\r\n"
        "; === MARKDOWN TEMPLATES ===\r\n"
        "; Templates specific to Markdown files\r\n"
        "\r\n"
        "[Template1]\r\n"
        "Name=Heading 1\r\n"
        "Name.cs=Nadpis 1\r\n"
        "Description=Insert a level 1 heading\r\n"
        "Description.cs=Vložit nadpis úrovně 1\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=# %cursor%\r\n"
        "Shortcut=Ctrl+1\r\n"
        "\r\n"
        "[Template2]\r\n"
        "Name=Heading 2\r\n"
        "Name.cs=Nadpis 2\r\n"
        "Description=Insert a level 2 heading\r\n"
        "Description.cs=Vložit nadpis úrovně 2\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=## %cursor%\r\n"
        "Shortcut=Ctrl+2\r\n"
        "\r\n"
        "[Template3]\r\n"
        "Name=Heading 3\r\n"
        "Name.cs=Nadpis 3\r\n"
        "Description=Insert a level 3 heading\r\n"
        "Description.cs=Vložit nadpis úrovně 3\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=### %cursor%\r\n"
        "Shortcut=Ctrl+3\r\n"
        "\r\n"
        "[Template4]\r\n"
        "Name=Bold Text\r\n"
        "Name.cs=Tučný text\r\n"
        "Description=Make text bold (wraps selection or inserts template)\r\n"
        "Description.cs=Udělat text tučným (obalí výběr nebo vloží šablonu)\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=**%selection%%cursor%**\r\n"
        "Shortcut=Ctrl+B\r\n"
        "\r\n"
        "[Template5]\r\n"
        "Name=Italic Text\r\n"
        "Name.cs=Kurzíva\r\n"
        "Description=Make text italic (wraps selection or inserts template)\r\n"
        "Description.cs=Udělat text kurzívou (obalí výběr nebo vloží šablonu)\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=*%selection%%cursor%*\r\n"
        "Shortcut=Ctrl+I\r\n"
        "\r\n"
        "[Template6]\r\n"
        "Name=Bold Italic\r\n"
        "Name.cs=Tučná kurzíva\r\n"
        "Description=Make text bold and italic\r\n"
        "Description.cs=Udělat text tučným a kurzívou\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=***%selection%%cursor%***\r\n"
        "\r\n"
        "[Template7]\r\n"
        "Name=Strikethrough\r\n"
        "Name.cs=Přeškrtnutí\r\n"
        "Description=Strikethrough text\r\n"
        "Description.cs=Přeškrtnout text\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=~~%selection%%cursor%~~\r\n"
        "\r\n"
        "[Template8]\r\n"
        "Name=Inline Code\r\n"
        "Name.cs=Vložený kód\r\n"
        "Description=Insert inline code\r\n"
        "Description.cs=Vložit vložený kód\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=`%selection%%cursor%`\r\n"
        "\r\n"
        "[Template9]\r\n"
        "Name=Code Block\r\n"
        "Name.cs=Blok kódu\r\n"
        "Description=Insert a code block\r\n"
        "Description.cs=Vložit blok kódu\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=```\\n%cursor%\\n```\r\n"
        "Shortcut=Ctrl+Shift+C\r\n"
        "\r\n"
        "[Template10]\r\n"
        "Name=Unordered List\r\n"
        "Name.cs=Nečíslovaný seznam\r\n"
        "Description=Insert unordered list item\r\n"
        "Description.cs=Vložit položku nečíslovaného seznamu\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=- %cursor%\r\n"
        "\r\n"
        "[Template11]\r\n"
        "Name=Ordered List\r\n"
        "Name.cs=Číslovaný seznam\r\n"
        "Description=Insert ordered list item\r\n"
        "Description.cs=Vložit položku číslovaného seznamu\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=1. %cursor%\r\n"
        "\r\n"
        "[Template12]\r\n"
        "Name=Task List\r\n"
        "Name.cs=Seznam úkolů\r\n"
        "Description=Insert task list item (checkbox)\r\n"
        "Description.cs=Vložit položku seznamu úkolů (zaškrtávací pole)\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=- [ ] %cursor%\r\n"
        "\r\n"
        "[Template13]\r\n"
        "Name=Link\r\n"
        "Name.cs=Odkaz\r\n"
        "Description=Insert a hyperlink\r\n"
        "Description.cs=Vložit hypertextový odkaz\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=[%selection%%cursor%](url)\r\n"
        "\r\n"
        "[Template14]\r\n"
        "Name=Blockquote\r\n"
        "Name.cs=Citace\r\n"
        "Description=Insert a blockquote\r\n"
        "Description.cs=Vložit citaci\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=> %cursor%\r\n"
        "\r\n"
        "[Template15]\r\n"
        "Name=Front Matter\r\n"
        "Name.cs=Záhlaví\r\n"
        "Description=Insert YAML front matter with current date\r\n"
        "Description.cs=Vložit YAML záhlaví s aktuálním datem\r\n"
        "Category=Markdown\r\n"
        "FileExtension=md\r\n"
        "Template=---\\ntitle: %cursor%\\ndate: %date%\\nauthor: \\n---\\n\\n\r\n";
    
    
    WriteFile(hFile, szDefaultINI, strlen(szDefaultINI), &dwWritten, NULL);
    CloseHandle(hFile);

    // Invalidate cache so it reloads the new file
    g_IniCache.loaded = FALSE;
    g_IniCache.dirty = FALSE;
    g_IniCache.data.clear();
}

//============================================================================
// LoadSettings - Load application settings from INI file
//============================================================================
void LoadSettings()
{
    // Get path to INI file (in same directory as executable)
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    // Load settings from [Settings] section using direct file reading
    // Also ensure each setting exists in the INI file with its default value
    
    // Check if settings exist; if not, write defaults
    WCHAR szValue[256];
    
    // WordWrap
    ReadINIValue(szIniPath, L"Settings", L"WordWrap", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"WordWrap", L"1");
        g_bWordWrap = TRUE;
    } else {
        g_bWordWrap = ReadINIInt(szIniPath, L"Settings", L"WordWrap", 1);
    }
    
    // RichEditLibraryPath (Phase 2.8) - optional custom RichEdit DLL path
    WCHAR szRichEditPath[MAX_PATH];
    ReadINIValue(szIniPath, L"Settings", L"RichEditLibraryPath", szRichEditPath, MAX_PATH, L"");
    // Store in global (even if empty) - LoadRichEditLibrary() checks if empty
    wcscpy(g_szRichEditLibPathINI, szRichEditPath);
    // Don't auto-write this setting - it's optional and well-documented in CreateDefaultINI comments
    
    // RichEditClassName (Phase 2.8.5) - optional window class override
    WCHAR szClassName[64];
    ReadINIValue(szIniPath, L"Settings", L"RichEditClassName", szClassName, 64, L"");
    // Store in global (even if empty) - LoadRichEditLibrary() checks if empty
    wcscpy(g_szRichEditClassNameINI, szClassName);
    // Don't auto-write this setting - it's optional and well-documented in CreateDefaultINI comments
    
    // AutosaveEnabled
    ReadINIValue(szIniPath, L"Settings", L"AutosaveEnabled", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"AutosaveEnabled", L"1");
        g_bAutosaveEnabled = TRUE;
    } else {
        g_bAutosaveEnabled = ReadINIInt(szIniPath, L"Settings", L"AutosaveEnabled", 1);
    }
    
    // AutosaveIntervalMinutes
    ReadINIValue(szIniPath, L"Settings", L"AutosaveIntervalMinutes", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"AutosaveIntervalMinutes", L"1");
        g_nAutosaveIntervalMinutes = 1;
    } else {
        g_nAutosaveIntervalMinutes = ReadINIInt(szIniPath, L"Settings", L"AutosaveIntervalMinutes", 1);
    }
    
    // AutosaveOnFocusLoss
    ReadINIValue(szIniPath, L"Settings", L"AutosaveOnFocusLoss", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"AutosaveOnFocusLoss", L"1");
        g_bAutosaveOnFocusLoss = TRUE;
    } else {
        g_bAutosaveOnFocusLoss = ReadINIInt(szIniPath, L"Settings", L"AutosaveOnFocusLoss", 1);
    }
    
    // ShowMenuDescriptions
    ReadINIValue(szIniPath, L"Settings", L"ShowMenuDescriptions", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"ShowMenuDescriptions", L"1");
        g_bShowMenuDescriptions = TRUE;
    } else {
        g_bShowMenuDescriptions = ReadINIInt(szIniPath, L"Settings", L"ShowMenuDescriptions", 1);
    }
    
    // SelectAfterPaste
    ReadINIValue(szIniPath, L"Settings", L"SelectAfterPaste", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"SelectAfterPaste", L"0");
        g_bSelectAfterPaste = FALSE;
    } else {
        g_bSelectAfterPaste = ReadINIInt(szIniPath, L"Settings", L"SelectAfterPaste", 0);
    }
    
    // AutoSaveUntitledOnClose
    ReadINIValue(szIniPath, L"Settings", L"AutoSaveUntitledOnClose", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"AutoSaveUntitledOnClose", L"0");
        g_bAutoSaveUntitledOnClose = FALSE;
    } else {
        g_bAutoSaveUntitledOnClose = ReadINIInt(szIniPath, L"Settings", L"AutoSaveUntitledOnClose", 0);
    }

    // DetectURLs - enable AURL_ENABLEURL for URL highlighting and accessibility.
    // Set to 0 for large-file sessions where cursor-movement lag is a problem.
    ReadINIValue(szIniPath, L"Settings", L"DetectURLs", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"DetectURLs", L"1");
        g_bAutoURLEnabled = TRUE;
    } else {
        g_bAutoURLEnabled = ReadINIInt(szIniPath, L"Settings", L"DetectURLs", 1);
    }
    
    // TabSize
    ReadINIValue(szIniPath, L"Settings", L"TabSize", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"TabSize", L"8");
        g_nTabSize = 8;
    } else {
        int tabSize = ReadINIInt(szIniPath, L"Settings", L"TabSize", 8);
        // Validate tab size (1-32 is reasonable range)
        if (tabSize < 1 || tabSize > 32) {
            tabSize = 8;
            WriteINIValue(szIniPath, L"Settings", L"TabSize", L"8");
        }
        g_nTabSize = tabSize;
    }
    
    // Zoom (UR-001) - persist zoom level as integer percentage
    ReadINIValue(szIniPath, L"Settings", L"Zoom", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"Zoom", L"100");
        g_nZoomPercent = 100;
    } else {
        int z = ReadINIInt(szIniPath, L"Settings", L"Zoom", 100);
        // Clamp to RichEdit supported range (1%-6400%)
        if (z < 1)    z = 1;
        if (z > 6400) z = 6400;
        g_nZoomPercent = z;
    }

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
    
    // DateTimeTemplate (Phase 2.10, ToDo #3) - F5 key / Edit→Time/Date insertion format
    ReadINIValue(szIniPath, L"Settings", L"DateTimeTemplate", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"DateTimeTemplate", L"%shortdate% %shorttime%");
        wcscpy(g_szDateTimeTemplate, L"%shortdate% %shorttime%");
    } else {
        wcscpy(g_szDateTimeTemplate, szValue);
    }
    
    // DateFormat (Phase 2.10, ToDo #3) - Custom format for %date% variable
    ReadINIValue(szIniPath, L"Settings", L"DateFormat", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"DateFormat", L"%shortdate%");
        wcscpy(g_szDateFormat, L"%shortdate%");
    } else {
        wcscpy(g_szDateFormat, szValue);
    }
    
    // TimeFormat (Phase 2.10, ToDo #3) - Custom format for %time% variable
    ReadINIValue(szIniPath, L"Settings", L"TimeFormat", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        WriteINIValue(szIniPath, L"Settings", L"TimeFormat", L"HH:mm");
        wcscpy(g_szTimeFormat, L"HH:mm");
    } else {
        wcscpy(g_szTimeFormat, szValue);
    }
    
    // Load find history
    LoadFindHistory();
    
    // Load replace history
    LoadReplaceHistory();
}

//============================================================================
// LoadFilters - Load filter configurations from INI file
//============================================================================
void LoadFilters()
{
    // Get path to INI file (in same directory as executable)
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    // Read filter count using direct file reading
    g_nFilterCount = ReadINIInt(szIniPath, L"Filters", L"Count", 0);
    if (g_nFilterCount > MAX_FILTERS) {
        g_nFilterCount = MAX_FILTERS;
    }
    
    // Load each filter
    for (int i = 0; i < g_nFilterCount; i++) {
        WCHAR szSection[32];
        swprintf(szSection, 32, L"Filter%d", i + 1);
        
        ReadINIValue(szIniPath, szSection, L"Name", 
                     g_Filters[i].szName, MAX_FILTER_NAME, L"");
        ReadINIValue(szIniPath, szSection, L"Command", 
                     g_Filters[i].szCommand, MAX_FILTER_COMMAND, L"");
        ReadINIValue(szIniPath, szSection, L"Description", 
                     g_Filters[i].szDescription, MAX_FILTER_DESC, L"");
        ReadINIValue(szIniPath, szSection, L"Category", 
                     g_Filters[i].szCategory, MAX_FILTER_CATEGORY, L"");
        
        // Get system language code for localized strings
        WCHAR szLangCode[16];
        GetSystemLanguageCode(szLangCode, 16);
        
        // Try to load localized name (e.g., "Name.cs_CZ")
        WCHAR szLocalizedKey[64];
        _snwprintf(szLocalizedKey, 64, L"Name.%s", szLangCode);
        ReadINIValue(szIniPath, szSection, szLocalizedKey,
                     g_Filters[i].szLocalizedName, MAX_FILTER_NAME, L"");
        
        // If not found, try just language code without country (e.g., "Name.cs")
        if (g_Filters[i].szLocalizedName[0] == L'\0') {
            WCHAR szLangOnly[4] = L"";
            wcsncpy(szLangOnly, szLangCode, 2);
            szLangOnly[2] = L'\0';
            _snwprintf(szLocalizedKey, 64, L"Name.%s", szLangOnly);
            ReadINIValue(szIniPath, szSection, szLocalizedKey,
                         g_Filters[i].szLocalizedName, MAX_FILTER_NAME, L"");
        }
        
        // If still not found, use English name as fallback
        if (g_Filters[i].szLocalizedName[0] == L'\0') {
            wcscpy(g_Filters[i].szLocalizedName, g_Filters[i].szName);
        }
        
        // Try to load localized description (e.g., "Description.cs_CZ")
        _snwprintf(szLocalizedKey, 64, L"Description.%s", szLangCode);
        ReadINIValue(szIniPath, szSection, szLocalizedKey,
                     g_Filters[i].szLocalizedDescription, MAX_FILTER_DESC, L"");
        
        // If not found, try just language code without country
        if (g_Filters[i].szLocalizedDescription[0] == L'\0') {
            WCHAR szLangOnly[4] = L"";
            wcsncpy(szLangOnly, szLangCode, 2);
            szLangOnly[2] = L'\0';
            _snwprintf(szLocalizedKey, 64, L"Description.%s", szLangOnly);
            ReadINIValue(szIniPath, szSection, szLocalizedKey,
                         g_Filters[i].szLocalizedDescription, MAX_FILTER_DESC, L"");
        }
        
        // If still not found, use English description as fallback
        if (g_Filters[i].szLocalizedDescription[0] == L'\0') {
            wcscpy(g_Filters[i].szLocalizedDescription, g_Filters[i].szDescription);
        }
        
        // Read action type (insert, display, clipboard, none)
        WCHAR szAction[32];
        ReadINIValue(szIniPath, szSection, L"Action", 
                     szAction, 32, L"insert");
        
        // Parse action and read action-specific parameters
        if (_wcsicmp(szAction, L"display") == 0) {
            g_Filters[i].action = FILTER_ACTION_DISPLAY;
            
            // Read Display parameter (statusbar or messagebox)
            WCHAR szDisplay[32];
            ReadINIValue(szIniPath, szSection, L"Display", 
                         szDisplay, 32, L"messagebox");
            
            if (_wcsicmp(szDisplay, L"statusbar") == 0) {
                g_Filters[i].displayMode = FILTER_DISPLAY_STATUSBAR;
            } else if (_wcsicmp(szDisplay, L"messagebox") == 0) {
                g_Filters[i].displayMode = FILTER_DISPLAY_MESSAGEBOX;
            } else {
                // Invalid value - show warning and use default
                WCHAR szWarning[256];
                swprintf(szWarning, 256, 
                         L"Filter %d: Invalid Display value '%s', using 'messagebox'. Valid values: statusbar, messagebox", 
                         i + 1, szDisplay);
                OutputDebugString(szWarning);
                OutputDebugString(L"\n");
                g_Filters[i].displayMode = FILTER_DISPLAY_MESSAGEBOX;
            }
            
        } else if (_wcsicmp(szAction, L"clipboard") == 0) {
            g_Filters[i].action = FILTER_ACTION_CLIPBOARD;
            
            // Read Clipboard parameter (copy or append)
            WCHAR szClipboard[32];
            ReadINIValue(szIniPath, szSection, L"Clipboard", 
                         szClipboard, 32, L"copy");
            
            if (_wcsicmp(szClipboard, L"append") == 0) {
                g_Filters[i].clipboardMode = FILTER_CLIPBOARD_APPEND;
            } else if (_wcsicmp(szClipboard, L"copy") == 0) {
                g_Filters[i].clipboardMode = FILTER_CLIPBOARD_COPY;
            } else {
                // Invalid value - show warning and use default
                WCHAR szWarning[256];
                swprintf(szWarning, 256, 
                         L"Filter %d: Invalid Clipboard value '%s', using 'copy'. Valid values: copy, append", 
                         i + 1, szClipboard);
                OutputDebugString(szWarning);
                OutputDebugString(L"\n");
                g_Filters[i].clipboardMode = FILTER_CLIPBOARD_COPY;
            }
            
        } else if (_wcsicmp(szAction, L"none") == 0) {
            g_Filters[i].action = FILTER_ACTION_NONE;
            
        } else if (_wcsicmp(szAction, L"repl") == 0) {
            // REPL (interactive) action (Phase 2.5)
            g_Filters[i].action = FILTER_ACTION_REPL;
            
            // Read PromptEnd parameter (e.g., "> ", "$ ", "# ")
            ReadINIValue(szIniPath, szSection, L"PromptEnd",
                         g_Filters[i].szPromptEnd, 16, L"> ");
            
            // Read EOLDetection parameter (auto, crlf, lf, cr)
            WCHAR szEOL[32];
            ReadINIValue(szIniPath, szSection, L"EOLDetection",
                         szEOL, 32, L"auto");
            
            if (_wcsicmp(szEOL, L"crlf") == 0) {
                g_Filters[i].replEOLMode = REPL_EOL_CRLF;
            } else if (_wcsicmp(szEOL, L"lf") == 0) {
                g_Filters[i].replEOLMode = REPL_EOL_LF;
            } else if (_wcsicmp(szEOL, L"cr") == 0) {
                g_Filters[i].replEOLMode = REPL_EOL_CR;
            } else {
                // Default or invalid: use auto-detection
                g_Filters[i].replEOLMode = REPL_EOL_AUTO;
            }
            
            // Read ExitNotification parameter (1=show dialog when filter exits, 0=silent)
            g_Filters[i].bExitNotification = (ReadINIInt(szIniPath, szSection, L"ExitNotification", 1) != 0);
            
        } else {
            // Default: insert action
            g_Filters[i].action = FILTER_ACTION_INSERT;
            
            // Read Insert parameter (replace, below, or append)
            WCHAR szInsert[32];
            ReadINIValue(szIniPath, szSection, L"Insert", 
                         szInsert, 32, L"below");
            
            if (_wcsicmp(szInsert, L"replace") == 0) {
                g_Filters[i].insertMode = FILTER_INSERT_REPLACE;
            } else if (_wcsicmp(szInsert, L"append") == 0) {
                g_Filters[i].insertMode = FILTER_INSERT_APPEND;
            } else if (_wcsicmp(szInsert, L"below") == 0) {
                g_Filters[i].insertMode = FILTER_INSERT_BELOW;
            } else {
                // Invalid value - show warning and use default
                WCHAR szWarning[256];
                swprintf(szWarning, 256, 
                         L"Filter %d: Invalid Insert value '%s', using 'below'. Valid values: replace, below, append", 
                         i + 1, szInsert);
                OutputDebugString(szWarning);
                OutputDebugString(L"\n");
                g_Filters[i].insertMode = FILTER_INSERT_BELOW;
            }
        }
        
        // Read context menu settings
        g_Filters[i].bContextMenu = (ReadINIInt(szIniPath, szSection, L"ContextMenu", 0) != 0);
        g_Filters[i].nContextMenuOrder = ReadINIInt(szIniPath, szSection, L"ContextMenuOrder", 999);
        
        // Validate filter configuration
        WCHAR szValidationError[512];
        if (!ValidateFilter(&g_Filters[i], i, szValidationError, 512)) {
            // Show error message for invalid filters
            WCHAR szTitle[64];
            LoadStringResource(IDS_ERROR, szTitle, 64);
            MessageBox(g_hWndMain, szValidationError, szTitle, MB_ICONWARNING | MB_OK);
            
            // Skip this filter (mark as invalid by clearing name)
            g_Filters[i].szName[0] = L'\0';
        } else if (wcslen(szValidationError) > 0) {
            // Show warning message (e.g., missing description)
            // We'll log this to debug output but not show a dialog for warnings
            OutputDebugString(L"Filter warning: ");
            OutputDebugString(szValidationError);
            OutputDebugString(L"\n");
        }
    }
    
    // Load last selected classic filter from settings (by name, defaulting to None)
    WCHAR szLastFilter[MAX_FILTER_NAME] = L"";
    ReadINIValue(szIniPath, L"Settings", L"CurrentFilter", szLastFilter, MAX_FILTER_NAME, L"");
    
    // Try to find classic filter by name
    g_nCurrentFilter = -1; // Default to None
    if (szLastFilter[0] != L'\0') {
        for (int i = 0; i < g_nFilterCount; i++) {
            if (wcscmp(g_Filters[i].szName, szLastFilter) == 0) {
                g_nCurrentFilter = i;
                break;
            }
        }
    }
    
    // Load last selected REPL filter from settings (by name, defaulting to None)
    WCHAR szLastREPLFilter[MAX_FILTER_NAME] = L"";
    ReadINIValue(szIniPath, L"Settings", L"CurrentREPLFilter", szLastREPLFilter, MAX_FILTER_NAME, L"");
    
    // Try to find REPL filter by name
    g_nSelectedREPLFilter = -1; // Default to None
    if (szLastREPLFilter[0] != L'\0') {
        for (int i = 0; i < g_nFilterCount; i++) {
            if (wcscmp(g_Filters[i].szName, szLastREPLFilter) == 0) {
                g_nSelectedREPLFilter = i;
                break;
            }
        }
    }
}

//============================================================================
// ValidateFilter - Validates a filter configuration and returns error message
//============================================================================
BOOL ValidateFilter(const FilterInfo* filter, int filterIndex, WCHAR* errorMsg, int errorMsgSize)
{
    // Check if filter has a name
    if (filter->szName[0] == L'\0') {
        swprintf(errorMsg, errorMsgSize, 
                 L"Filter %d: Missing required 'Name' parameter", filterIndex + 1);
        return FALSE;
    }
    
    // Check if filter has a command
    if (filter->szCommand[0] == L'\0') {
        swprintf(errorMsg, errorMsgSize, 
                 L"Filter %d (%s): Missing required 'Command' parameter", 
                 filterIndex + 1, filter->szName);
        return FALSE;
    }
    
    // Validate action-specific parameters
    switch (filter->action) {
        case FILTER_ACTION_INSERT:
            // Insert mode should be valid (already validated during parsing)
            break;
            
        case FILTER_ACTION_DISPLAY:
            // Display mode should be valid (already validated during parsing)
            break;
            
        case FILTER_ACTION_CLIPBOARD:
            // Clipboard mode should be valid (already validated during parsing)
            break;
            
        case FILTER_ACTION_NONE:
            // No additional validation needed
            break;
            
        case FILTER_ACTION_REPL:
            // REPL filters should have a PromptEnd setting
            // (already loaded with default "> " if not specified)
            break;
            
        default:
            swprintf(errorMsg, errorMsgSize, 
                     L"Filter %d (%s): Invalid action type", 
                     filterIndex + 1, filter->szName);
            return FALSE;
    }
    
    // Warn if description is missing (not an error, but recommended for accessibility)
    if (filter->szDescription[0] == L'\0') {
        swprintf(errorMsg, errorMsgSize, 
                 L"Filter %d (%s): Warning - Missing 'Description' parameter (recommended for accessibility)", 
                 filterIndex + 1, filter->szName);
        // Return TRUE anyway - this is just a warning
    }
    
    return TRUE;
}

//============================================================================
// SaveCurrentFilter - Save currently selected filter to INI file
//============================================================================
void SaveCurrentFilter()
{
    // Get path to INI file
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    // Determine classic filter name to save
    WCHAR szFilterName[MAX_FILTER_NAME] = L"";
    if (g_nCurrentFilter >= 0 && g_nCurrentFilter < g_nFilterCount) {
        wcscpy_s(szFilterName, MAX_FILTER_NAME, g_Filters[g_nCurrentFilter].szName);
    }
    // If g_nCurrentFilter is -1 or invalid, szFilterName stays empty (means "None")
    
    // Determine REPL filter name to save
    WCHAR szREPLFilterName[MAX_FILTER_NAME] = L"";
    if (g_nSelectedREPLFilter >= 0 && g_nSelectedREPLFilter < g_nFilterCount) {
        wcscpy_s(szREPLFilterName, MAX_FILTER_NAME, g_Filters[g_nSelectedREPLFilter].szName);
    }
    
    WriteINIValue(szIniPath, L"Settings", L"CurrentFilter", szFilterName);
    WriteINIValue(szIniPath, L"Settings", L"CurrentREPLFilter", szREPLFilterName);
}

//============================================================================
// SaveCurrentREPLFilter - Save selected REPL filter to INI file
//============================================================================
void SaveCurrentREPLFilter()
{
    // Get path to INI file
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    // Determine REPL filter name to save
    WCHAR szREPLFilterName[MAX_FILTER_NAME] = L"";
    if (g_nSelectedREPLFilter >= 0 && g_nSelectedREPLFilter < g_nFilterCount) {
        wcscpy_s(szREPLFilterName, MAX_FILTER_NAME, g_Filters[g_nSelectedREPLFilter].szName);
    }
    
    WriteINIValue(szIniPath, L"Settings", L"CurrentREPLFilter", szREPLFilterName);
}

//============================================================================
// UpdateFilterDisplay - Update status bar with current filter info
//============================================================================
void UpdateFilterDisplay()
{
    if (!g_hWndStatus) return;
    
    WCHAR szFilter[512];
    WCHAR szInteractive[32], szFilterLabel[32], szNone[32];
    
    // Load localized labels
    LoadStringResource(IDS_STATUS_INTERACTIVE, szInteractive, 32);
    LoadStringResource(IDS_STATUS_FILTER, szFilterLabel, 32);
    LoadStringResource(IDS_STATUS_FILTER_NONE, szNone, 32);
    
    BOOL hasREPL = FALSE;
    BOOL hasClassic = FALSE;
    
    if (g_bREPLMode) {
        // Show running REPL filter
        hasREPL = TRUE;
    } else if (g_nSelectedREPLFilter >= 0 && g_nSelectedREPLFilter < g_nFilterCount) {
        // Show selected REPL filter (not yet started)
        hasREPL = TRUE;
    }
    
    if (g_nCurrentFilter >= 0 && 
        g_nCurrentFilter < g_nFilterCount &&
        g_Filters[g_nCurrentFilter].action != FILTER_ACTION_REPL) {
        // Show selected classic filter
        hasClassic = TRUE;
    }
    
    // Build display string
    if (hasREPL && hasClassic) {
        // Both REPL and classic filter
        if (g_bREPLMode) {
            _snwprintf(szFilter, 512, L"[%s: %s] [%s: %s]",
                szInteractive, g_Filters[g_nCurrentREPLFilter].szLocalizedName,
                szFilterLabel, g_Filters[g_nCurrentFilter].szLocalizedName);
        } else {
            _snwprintf(szFilter, 512, L"[%s: %s] [%s: %s]",
                szInteractive, g_Filters[g_nSelectedREPLFilter].szLocalizedName,
                szFilterLabel, g_Filters[g_nCurrentFilter].szLocalizedName);
        }
    } else if (hasREPL) {
        // REPL filter only
        if (g_bREPLMode) {
            _snwprintf(szFilter, 512, L"[%s: %s]",
                szInteractive, g_Filters[g_nCurrentREPLFilter].szLocalizedName);
        } else {
            _snwprintf(szFilter, 512, L"[%s: %s]",
                szInteractive, g_Filters[g_nSelectedREPLFilter].szLocalizedName);
        }
    } else if (hasClassic) {
        // Classic filter only
        _snwprintf(szFilter, 512, L"[%s: %s]", 
            szFilterLabel, g_Filters[g_nCurrentFilter].szLocalizedName);
    } else {
        // No filter selected
        _snwprintf(szFilter, 512, L"[%s: %s]", szFilterLabel, szNone);
    }
    
    SendMessage(g_hWndStatus, SB_SETTEXT, (WPARAM)1, (LPARAM)szFilter);
}

//============================================================================
// UpdateMenuStates - Enable/disable menu items based on current state
//============================================================================
void UpdateMenuStates(HWND hwnd)
{
    HMENU hMenu = GetMenu(hwnd);
    if (!hMenu) return;
    
    BOOL enableExecute = FALSE;
    BOOL enableStartREPL = FALSE;
    BOOL enableExitREPL = FALSE;
    
    if (g_bREPLMode) {
        // In REPL mode
        enableExitREPL = TRUE;
        
        // Allow executing classic filters while in REPL (but not in read-only mode)
        if (!g_bReadOnly &&
            g_nCurrentFilter >= 0 && 
            g_nCurrentFilter < g_nFilterCount &&
            g_Filters[g_nCurrentFilter].action != FILTER_ACTION_REPL) {
            enableExecute = TRUE;
        }
        
        // Can't start another REPL while one is running
        enableStartREPL = FALSE;
    } else {
        // Not in REPL mode
        
        // Enable Execute if a classic filter is selected and not in read-only mode
        // (display/clipboard/none filters work in read-only, insert filters don't)
        if (g_nCurrentFilter >= 0 && 
            g_nCurrentFilter < g_nFilterCount &&
            g_Filters[g_nCurrentFilter].action != FILTER_ACTION_REPL) {
            if (g_bReadOnly) {
                // Only enable non-insert filters in read-only mode
                if (g_Filters[g_nCurrentFilter].action != FILTER_ACTION_INSERT) {
                    enableExecute = TRUE;
                }
            } else {
                enableExecute = TRUE;
            }
        }
        
        // Enable Start Interactive if a REPL filter is selected and not in read-only mode
        if (!g_bReadOnly &&
            g_nSelectedREPLFilter >= 0 && 
            g_nSelectedREPLFilter < g_nFilterCount &&
            g_Filters[g_nSelectedREPLFilter].action == FILTER_ACTION_REPL) {
            enableStartREPL = TRUE;
        }
    }
    
    EnableMenuItem(hMenu, ID_TOOLS_EXECUTEFILTER, 
        enableExecute ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(hMenu, ID_TOOLS_START_INTERACTIVE, 
        enableStartREPL ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(hMenu, ID_TOOLS_EXIT_INTERACTIVE, 
        enableExitREPL ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(hMenu, ID_SEARCH_REPLACE,
        g_bReadOnly ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(hMenu, ID_SEARCH_GOTO_LINE,
        MF_ENABLED);
    EnableMenuItem(hMenu, ID_SEARCH_CLEAR_BOOKMARKS,
        g_nBookmarkCount > 0 ? MF_ENABLED : MF_GRAYED);
    
    DrawMenuBar(hwnd);
}

//============================================================================
// LoadMRU - Load Most Recently Used file list from INI
//============================================================================
void LoadMRU()
{
    // Get path to INI file
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);
    
    // Load up to MAX_MRU files
    g_nMRUCount = 0;
    for (int i = 0; i < MAX_MRU; i++) {
        WCHAR szKey[16];
        _snwprintf(szKey, 16, L"File%d", i + 1);
        
        WCHAR szPath[EXTENDED_PATH_MAX];
        ReadINIValue(szIniPath, L"MRU", szKey, szPath, EXTENDED_PATH_MAX, L"");
        
        if (szPath[0] != L'\0') {
            wcscpy_s(g_MRU[g_nMRUCount], EXTENDED_PATH_MAX, szPath);
            g_nMRUCount++;
        }
    }
}

//============================================================================
// SaveMRU - Save Most Recently Used file list to INI
//============================================================================
void SaveMRU()
{
    // Get path to INI file
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetINIFilePath(szIniPath, EXTENDED_PATH_MAX);

    std::wstring mruSection = L"[MRU]\r\n";
    for (int i = 0; i < g_nMRUCount; i++) {
        AppendIndexedLine(mruSection, L"File", i + 1, g_MRU[i]);
    }
    
    ReplaceINISection(szIniPath, L"MRU", mruSection);
}

//============================================================================
// AddToMRU - Add a file path to the MRU list
//============================================================================
void AddToMRU(LPCWSTR pszFilePath)
{
    if (!pszFilePath || pszFilePath[0] == L'\0') {
        return;
    }
    
    // Don't add to MRU if /nomru command-line option was specified
    if (g_bNoMRU) {
        return;
    }
    
    // Check if file already exists in MRU and remove it
    int existingIndex = -1;
    for (int i = 0; i < g_nMRUCount; i++) {
        if (_wcsicmp(g_MRU[i], pszFilePath) == 0) {
            existingIndex = i;
            break;
        }
    }
    
    // If found, remove it (we'll add it to the top)
    if (existingIndex != -1) {
        for (int i = existingIndex; i < g_nMRUCount - 1; i++) {
            wcscpy_s(g_MRU[i], EXTENDED_PATH_MAX, g_MRU[i + 1]);
        }
        g_nMRUCount--;
    }
    
    // Shift everything down to make room at the top
    if (g_nMRUCount >= MAX_MRU) {
        g_nMRUCount = MAX_MRU - 1;
    }
    
    for (int i = g_nMRUCount; i > 0; i--) {
        wcscpy_s(g_MRU[i], EXTENDED_PATH_MAX, g_MRU[i - 1]);
    }
    
    // Add new file at the top
    wcscpy_s(g_MRU[0], EXTENDED_PATH_MAX, pszFilePath);
    g_nMRUCount++;
    
    // Save to INI
    SaveMRU();
    
    // Update menu
    UpdateMRUMenu(g_hWndMain);
}

//============================================================================
// UpdateMRUMenu - Update the File menu with MRU items
//============================================================================
void UpdateMRUMenu(HWND hwnd)
{
    HMENU hMenu = GetMenu(hwnd);
    if (!hMenu) return;
    
    // Find File menu (first submenu)
    HMENU hFileMenu = GetSubMenu(hMenu, 0);
    if (!hFileMenu) return;
    
    // Remove existing MRU items and separator
    // Work backwards to avoid index shifting issues
    int itemCount = GetMenuItemCount(hFileMenu);
    for (int i = itemCount - 1; i >= 0; i--) {
        UINT itemID = GetMenuItemID(hFileMenu, i);
        if (itemID >= ID_FILE_MRU_BASE && itemID < ID_FILE_MRU_BASE + MAX_MRU) {
            DeleteMenu(hFileMenu, i, MF_BYPOSITION);
        } else if (itemID == (UINT)-1) {
            // Check if this is a separator just before Exit
            if (i < itemCount - 1) {
                UINT nextID = GetMenuItemID(hFileMenu, i + 1);
                if (nextID == ID_FILE_EXIT) {
                    DeleteMenu(hFileMenu, i, MF_BYPOSITION);
                }
            }
        }
    }
    
    // If no MRU items, nothing more to do
    if (g_nMRUCount == 0) {
        DrawMenuBar(hwnd);
        return;
    }
    
    // Find position of Exit menu item
    itemCount = GetMenuItemCount(hFileMenu);
    int exitPos = -1;
    for (int i = 0; i < itemCount; i++) {
        if (GetMenuItemID(hFileMenu, i) == ID_FILE_EXIT) {
            exitPos = i;
            break;
        }
    }
    
    if (exitPos == -1) return; // Exit not found
    
    // Add separator before Exit
    InsertMenu(hFileMenu, exitPos, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    exitPos++; // Adjust for the separator we just added
    
    // Add MRU items (up to 10)
    for (int i = 0; i < g_nMRUCount; i++) {
        // Extract just the filename for display
        WCHAR szDisplay[MAX_PATH];
        LPCWSTR pszFileName = wcsrchr(g_MRU[i], L'\\');
        if (!pszFileName) {
            pszFileName = wcsrchr(g_MRU[i], L'/');
        }
        if (pszFileName) {
            pszFileName++; // Skip the slash
        } else {
            pszFileName = g_MRU[i]; // No path separator found
        }
        
        // Format as "&1 filename.txt" (with accelerator)
        _snwprintf(szDisplay, MAX_PATH, L"&%d %s", i + 1, pszFileName);
        
        // Insert before Exit
        InsertMenu(hFileMenu, exitPos + i, MF_BYPOSITION | MF_STRING, 
                   ID_FILE_MRU_BASE + i, szDisplay);
    }
    
    DrawMenuBar(hwnd);
}

//============================================================================
// BuildFilterMenu - Build dynamic filter submenu
//============================================================================
void BuildFilterMenu(HWND hwnd)
{
    HMENU hMenu = GetMenu(hwnd);
    if (!hMenu) return;
    
    // Find Tools menu by looking for the menu containing ID_TOOLS_EXECUTEFILTER
    int toolsMenuPos = -1;
    int menuCount = GetMenuItemCount(hMenu);
    for (int i = 0; i < menuCount; i++) {
        HMENU hSubMenu = GetSubMenu(hMenu, i);
        if (hSubMenu) {
            // Check if this submenu contains our Tools menu items
            int subItemCount = GetMenuItemCount(hSubMenu);
            for (int j = 0; j < subItemCount; j++) {
                if (GetMenuItemID(hSubMenu, j) == ID_TOOLS_EXECUTEFILTER) {
                    toolsMenuPos = i;
                    break;
                }
            }
            if (toolsMenuPos != -1) break;
        }
    }
    
    if (toolsMenuPos == -1) return;
    
    HMENU hToolsMenu = GetSubMenu(hMenu, toolsMenuPos);
    if (!hToolsMenu) return;
    
    // Find "Select Filter" submenu - it's the first submenu (popup) in Tools menu
    int selectFilterPos = -1;
    int toolsItemCount = GetMenuItemCount(hToolsMenu);
    for (int i = 0; i < toolsItemCount; i++) {
        HMENU hSubMenu = GetSubMenu(hToolsMenu, i);
        if (hSubMenu) {
            // This is the Select Filter submenu (first popup we find)
            selectFilterPos = i;
            break;
        }
    }
    
    if (selectFilterPos == -1) return;
    
    HMENU hFilterMenu = GetSubMenu(hToolsMenu, selectFilterPos);
    if (!hFilterMenu) return;
    
    // Clear existing filter menu items
    while (GetMenuItemCount(hFilterMenu) > 0) {
        DeleteMenu(hFilterMenu, 0, MF_BYPOSITION);
    }
    
    // Add filters or "No filters" message
    if (g_nFilterCount == 0) {
        WCHAR szNoFilters[128];
        LoadStringResource(IDS_NO_FILTERS_CONFIGURED, szNoFilters, 128);
        AppendMenu(hFilterMenu, MF_STRING | MF_GRAYED, ID_TOOLS_FILTER_BASE, szNoFilters);
    } else {
        // Build category map: category name -> list of filter indices
        struct CategoryInfo {
            WCHAR szName[MAX_FILTER_CATEGORY];
            int filterIndices[MAX_FILTERS];
            int count;
        };
        CategoryInfo categories[32];  // Max 32 categories
        int categoryCount = 0;
        int uncategorizedFilters[MAX_FILTERS];  // Filters with no category
        int uncategorizedCount = 0;
        
        // Group filters by category
        for (int i = 0; i < g_nFilterCount; i++) {
            // Check if filter has a category
            if (g_Filters[i].szCategory[0] == L'\0') {
                // No category - add to uncategorized list
                uncategorizedFilters[uncategorizedCount++] = i;
                continue;
            }
            
            // Find existing category or create new one
            int catIndex = -1;
            for (int c = 0; c < categoryCount; c++) {
                if (wcscmp(categories[c].szName, g_Filters[i].szCategory) == 0) {
                    catIndex = c;
                    break;
                }
            }
            
            // Create new category if not found
            if (catIndex == -1) {
                catIndex = categoryCount++;
                wcscpy(categories[catIndex].szName, g_Filters[i].szCategory);
                categories[catIndex].count = 0;
            }
            
            // Add filter index to category
            categories[catIndex].filterIndices[categories[catIndex].count++] = i;
        }
        
        // Create submenu for each category
        for (int c = 0; c < categoryCount; c++) {
            HMENU hCategoryMenu = CreatePopupMenu();
            
            // Add filters in this category
            for (int f = 0; f < categories[c].count; f++) {
                int filterIndex = categories[c].filterIndices[f];
                UINT flags = MF_STRING;
                // Check both classic filter and REPL filter selection
                if (filterIndex == g_nCurrentFilter || filterIndex == g_nSelectedREPLFilter) {
                    flags |= MF_CHECKED;
                }
                
                // Gray out insert filters and REPL filters in read-only mode
                if (g_bReadOnly && (g_Filters[filterIndex].action == FILTER_ACTION_INSERT || 
                                     g_Filters[filterIndex].action == FILTER_ACTION_REPL)) {
                    flags |= MF_GRAYED;
                }
                
                // Build accessible menu text: "Name: Description" (using localized strings)
                // Add "[Interactive] " prefix for REPL filters
                WCHAR szMenuText[MAX_FILTER_NAME + MAX_FILTER_DESC + 32];
                
                // Check if this is a REPL filter and prepend localized indicator
                if (g_Filters[filterIndex].action == FILTER_ACTION_REPL) {
                    wcscpy(szMenuText, L"[");
                    WCHAR szInteractive[32];
                    LoadStringResource(IDS_STATUS_INTERACTIVE, szInteractive, 32);
                    wcscat(szMenuText, szInteractive);
                    wcscat(szMenuText, L"] ");
                } else {
                    szMenuText[0] = L'\0';
                }
                
                // Append localized name
                wcscat(szMenuText, g_Filters[filterIndex].szLocalizedName);
                
                // Append description if enabled
                if (g_bShowMenuDescriptions && g_Filters[filterIndex].szLocalizedDescription[0] != L'\0') {
                    wcscat(szMenuText, L": ");
                    wcscat(szMenuText, g_Filters[filterIndex].szLocalizedDescription);
                }
                
                AppendMenu(hCategoryMenu, flags, ID_TOOLS_FILTER_BASE + filterIndex, 
                           szMenuText);
            }
            
            // Add category submenu to main filter menu
            AppendMenu(hFilterMenu, MF_STRING | MF_POPUP, (UINT_PTR)hCategoryMenu, 
                       categories[c].szName);
        }
        
        // Add separator if we have both categorized and uncategorized filters
        if (categoryCount > 0 && uncategorizedCount > 0) {
            AppendMenu(hFilterMenu, MF_SEPARATOR, 0, NULL);
        }
        
        // Add uncategorized filters at root level (below categories)
        for (int i = 0; i < uncategorizedCount; i++) {
            int filterIndex = uncategorizedFilters[i];
            UINT flags = MF_STRING;
            // Check both classic filter and REPL filter selection
            if (filterIndex == g_nCurrentFilter || filterIndex == g_nSelectedREPLFilter) {
                flags |= MF_CHECKED;
            }
            
            // Gray out insert filters and REPL filters in read-only mode
            if (g_bReadOnly && (g_Filters[filterIndex].action == FILTER_ACTION_INSERT || 
                                 g_Filters[filterIndex].action == FILTER_ACTION_REPL)) {
                flags |= MF_GRAYED;
            }
            
            // Build accessible menu text: "Name: Description" (using localized strings)
            // Add "[Interactive] " prefix for REPL filters
            WCHAR szMenuText[MAX_FILTER_NAME + MAX_FILTER_DESC + 32];
            
            // Check if this is a REPL filter and prepend localized indicator
            if (g_Filters[filterIndex].action == FILTER_ACTION_REPL) {
                wcscpy(szMenuText, L"[");
                WCHAR szInteractive[32];
                LoadStringResource(IDS_STATUS_INTERACTIVE, szInteractive, 32);
                wcscat(szMenuText, szInteractive);
                wcscat(szMenuText, L"] ");
            } else {
                szMenuText[0] = L'\0';
            }
            
            // Append localized name
            wcscat(szMenuText, g_Filters[filterIndex].szLocalizedName);
            
            // Append description if enabled
            if (g_bShowMenuDescriptions && g_Filters[filterIndex].szLocalizedDescription[0] != L'\0') {
                wcscat(szMenuText, L": ");
                wcscat(szMenuText, g_Filters[filterIndex].szLocalizedDescription);
            }
            
            AppendMenu(hFilterMenu, flags, ID_TOOLS_FILTER_BASE + filterIndex, szMenuText);
        }
    }
    
    DrawMenuBar(hwnd);
}

//============================================================================
// StartAutosaveTimer - Start or restart the autosave timer
//============================================================================
void StartAutosaveTimer(HWND hwnd)
{
    // Kill existing timer if any
    KillTimer(hwnd, IDT_AUTOSAVE);
    
    // Start timer if interval is set and autosave is enabled
    if (g_bAutosaveEnabled && g_nAutosaveIntervalMinutes > 0) {
        // Convert minutes to milliseconds
        UINT interval = g_nAutosaveIntervalMinutes * 60 * 1000;
        SetTimer(hwnd, IDT_AUTOSAVE, interval, NULL);
    }
}

//============================================================================
// DoAutosave - Perform autosave if file has been modified and has a name
//============================================================================
void DoAutosave()
{
    // Only autosave if:
    // 1. Autosave is enabled
    // 2. Document has been modified
    // 3. Document has a filename (not "Untitled")
    // 4. Not in read-only mode
    // 5. No dialog from our app is currently in foreground
    
    // Check if a dialog from our app has foreground focus
    BOOL bDialogActive = FALSE;
    HWND hwndForeground = GetForegroundWindow();
    if (hwndForeground != NULL && hwndForeground != g_hWndMain) {
        // Check if foreground window is part of our app hierarchy
        if (GetAncestor(hwndForeground, GA_ROOT) == g_hWndMain) {
            bDialogActive = TRUE;  // One of our dialogs is active
        }
    }
    
    if (!g_bAutosaveEnabled || !g_bModified || g_szFileName[0] == L'\0' || g_bReadOnly || bDialogActive || g_bSaveInProgress) {
        return;
    }

    g_bSaveInProgress = TRUE;

    // Save the file silently (passing FALSE to preserve resume file and MRU behavior)
    if (SaveTextFile(g_szFileName, FALSE)) {
        // Autosave succeeded - clear the modified flag
        g_bModified = FALSE;
        UpdateTitle();  // Remove asterisk from title bar
        
        // Flash "[Autosaved]" in status bar for 1 second without blocking the UI thread
        if (g_hWndStatus) {
            SendMessage(g_hWndStatus, SB_GETTEXT, 0, (LPARAM)g_szAutosaveFlashPrevStatus);
            SendMessage(g_hWndStatus, SB_SETTEXT, 0, (LPARAM)L"[Autosaved]");
            SetTimer(g_hWndMain, IDT_AUTOSAVE_FLASH, 1000, NULL);
        }
    }

    g_bSaveInProgress = FALSE;
}

//============================================================================
// REPL Filter Functions (Phase 2.5)
//============================================================================

//============================================================================
// StartREPLFilter - Start an interactive REPL filter session
//============================================================================
void StartREPLFilter(int filterIndex)
{
    // Check if filter index is valid
    if (filterIndex < 0 || filterIndex >= g_nFilterCount) {
        return;
    }
    
    // Check if filter is a REPL filter
    if (g_Filters[filterIndex].action != FILTER_ACTION_REPL) {
        return;
    }
    
    // Check if already in REPL mode
    if (g_bREPLMode) {
        WCHAR szMsg[256], szTitle[64];
        LoadStringResource(IDS_ERROR, szTitle, 64);
        wcscpy(szMsg, L"Already in Interactive Mode. Exit current session first.");
        MessageBox(g_hWndMain, szMsg, szTitle, MB_ICONWARNING);
        return;
    }
    
    // Create pipes for stdin, stdout, stderr
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE hStdoutRead = NULL, hStdoutWrite = NULL;
    HANDLE hStdinRead = NULL, hStdinWrite = NULL;
    HANDLE hStderrRead = NULL, hStderrWrite = NULL;
    
    // Create stdout pipe
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        WCHAR szError[256];
        WCHAR szErrorTitle[64];
        LoadStringResource(IDS_REPL_FAILED_PIPE_STDOUT, szError, 256);
        LoadStringResource(IDS_ERROR, szErrorTitle, 64);
        MessageBox(g_hWndMain, szError, szErrorTitle, MB_ICONERROR);
        return;
    }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    
    // Create stdin pipe
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        WCHAR szError[256];
        WCHAR szErrorTitle[64];
        LoadStringResource(IDS_REPL_FAILED_PIPE_STDIN, szError, 256);
        LoadStringResource(IDS_ERROR, szErrorTitle, 64);
        MessageBox(g_hWndMain, szError, szErrorTitle, MB_ICONERROR);
        return;
    }
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    
    // Create stderr pipe
    if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        WCHAR szError[256];
        WCHAR szErrorTitle[64];
        LoadStringResource(IDS_REPL_FAILED_PIPE_STDERR, szError, 256);
        LoadStringResource(IDS_ERROR, szErrorTitle, 64);
        MessageBox(g_hWndMain, szError, szErrorTitle, MB_ICONERROR);
        return;
    }
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
    
    // Set up process startup info
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    
    // Create command line (need writable buffer)
    WCHAR szCommand[MAX_FILTER_COMMAND + 1];
    wcscpy(szCommand, g_Filters[filterIndex].szCommand);
    
    // Create the process
    if (!CreateProcess(NULL, szCommand, NULL, NULL, TRUE, 
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        CloseHandle(hStderrRead);
        CloseHandle(hStderrWrite);
        
        WCHAR szMsg[512];
        WCHAR szError[256];
        WCHAR szErrorTitle[64];
        LoadStringResource(IDS_REPL_FAILED_START, szError, 256);
        LoadStringResource(IDS_ERROR, szErrorTitle, 64);
        
        wcscpy(szMsg, szError);
        wcscat(szMsg, L": ");
        wcscat(szMsg, g_Filters[filterIndex].szLocalizedName);
        
        MessageBox(g_hWndMain, szMsg, szErrorTitle, MB_ICONERROR);
        return;
    }
    
    // Close child process handles we don't need
    CloseHandle(pi.hThread);
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
    
    // Store REPL state
    g_hREPLProcess = pi.hProcess;
    g_hREPLStdin = hStdinWrite;
    g_hREPLStdout = hStdoutRead;
    g_hREPLStderr = hStderrRead;
    g_nCurrentREPLFilter = filterIndex;
    g_bREPLMode = TRUE;
    
    // Copy prompt end and EOL mode
    wcscpy(g_szREPLPromptEnd, g_Filters[filterIndex].szPromptEnd);
    g_REPLEOLMode = g_Filters[filterIndex].replEOLMode;
    
    g_hREPLStdoutThread = CreateThread(NULL, 0, REPLStdoutThread, NULL, 0, &g_dwREPLStdoutThreadId);
    if (g_hREPLStdoutThread == NULL) {
        ExitREPLMode();
        WCHAR szError[256];
        WCHAR szErrorTitle[64];
        LoadStringResource(IDS_REPL_FAILED_THREAD_STDOUT, szError, 256);
        LoadStringResource(IDS_ERROR, szErrorTitle, 64);
        MessageBox(g_hWndMain, szError, szErrorTitle, MB_ICONERROR);
        return;
    }
    
    g_hREPLStderrThread = CreateThread(NULL, 0, REPLStderrThread, NULL, 0, &g_dwREPLStderrThreadId);
    if (g_hREPLStderrThread == NULL) {
        ExitREPLMode();
        WCHAR szError[256];
        WCHAR szErrorTitle[64];
        LoadStringResource(IDS_REPL_FAILED_THREAD_STDERR, szError, 256);
        LoadStringResource(IDS_ERROR, szErrorTitle, 64);
        MessageBox(g_hWndMain, szError, szErrorTitle, MB_ICONERROR);
        return;
    }
    
    // Update title bar to show Interactive Mode
    UpdateTitle(g_hWndMain);
    
    // Update status bar
    UpdateStatusBar();
    
    // Update menu states
    UpdateMenuStates(g_hWndMain);
}

//============================================================================
// REPLStdoutThread - Background thread that reads REPL stdout
//============================================================================
DWORD WINAPI REPLStdoutThread(LPVOID /* lpParam */)
{
    char buffer[4096];
    DWORD dwRead;
    
    while (g_bREPLMode) {
        // Try to read from stdout
        BOOL bSuccess = ReadFile(g_hREPLStdout, buffer, sizeof(buffer) - 1, &dwRead, NULL);
        
        if (!bSuccess || dwRead == 0) {
            // Process exited or pipe closed
            PostMessage(g_hWndMain, WM_REPL_EXITED, 0, 0);
            break;
        }
        
        // Null-terminate
        buffer[dwRead] = '\0';
        
        // Auto-detect EOL mode on first output (for informational purposes only)
        // Note: We don't change g_REPLEOLMode from AUTO - it stays AUTO and uses LF
        // This is because output EOL (what we receive) can differ from input EOL (what shell expects)
        // Example: bash with PTY outputs CRLF but expects LF input
        if (g_REPLEOLMode == REPL_EOL_AUTO) {
            // Detect but don't store - AUTO mode always sends LF
            REPLEOLMode detected = DetectEOL(buffer, dwRead);
            (void)detected; // Suppress unused variable warning
        }
        
        // Convert UTF-8 to UTF-16
        LPWSTR pszOutput = UTF8ToUTF16(buffer);
        if (pszOutput) {
            // Strip ANSI escape sequences (colors, cursor positioning, etc.)
            StripANSIEscapes(pszOutput);
            
            // Allocate copy for message (will be freed by message handler)
            LPWSTR pszCopy = (LPWSTR)malloc((wcslen(pszOutput) + 1) * sizeof(WCHAR));
            if (pszCopy) {
                wcscpy(pszCopy, pszOutput);
                PostMessage(g_hWndMain, WM_REPL_OUTPUT, 0, (LPARAM)pszCopy);
            }
            free(pszOutput);
        }
    }
    
    return 0;
}

//============================================================================
// REPLStderrThread - Background thread that reads REPL stderr
//============================================================================
DWORD WINAPI REPLStderrThread(LPVOID /* lpParam */)
{
    char buffer[4096];
    DWORD dwRead;
    
    while (g_bREPLMode) {
        // Try to read from stderr
        BOOL bSuccess = ReadFile(g_hREPLStderr, buffer, sizeof(buffer) - 1, &dwRead, NULL);
        
        if (!bSuccess || dwRead == 0) {
            // Pipe closed - this is normal, stderr might close before stdout
            // Don't post WM_REPL_EXITED here, let stdout thread handle it
            break;
        }
        
        // Null-terminate
        buffer[dwRead] = '\0';
        
        // Convert UTF-8 to UTF-16
        LPWSTR pszOutput = UTF8ToUTF16(buffer);
        if (pszOutput) {
            // Strip ANSI escape sequences (colors, cursor positioning, etc.)
            StripANSIEscapes(pszOutput);
            
            // Allocate copy for message (will be freed by message handler)
            LPWSTR pszCopy = (LPWSTR)malloc((wcslen(pszOutput) + 1) * sizeof(WCHAR));
            if (pszCopy) {
                wcscpy(pszCopy, pszOutput);
                PostMessage(g_hWndMain, WM_REPL_OUTPUT, 0, (LPARAM)pszCopy);
            }
            free(pszOutput);
        }
    }
    
    return 0;
}

//============================================================================
// InsertREPLOutput - Insert REPL output at current cursor position
//============================================================================
void InsertREPLOutput(LPCWSTR pszOutput)
{
    if (!pszOutput || !g_hWndEdit) {
        return;
    }
    
    // Get current selection
    CHARRANGE cr;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    
    // Replace selection with output (cursor moves to end)
    SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszOutput);
    // Scroll to cursor
    SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
    
    // Update status bar
    UpdateStatusBar();
}

//============================================================================
// ExitREPLMode - Exit REPL mode and cleanup resources
//============================================================================
void ExitREPLMode()
{
    if (!g_bREPLMode) {
        return;
    }
    
    // Set flag first to stop thread
    g_bREPLMode = FALSE;
    
    // Terminate process if still running
    if (g_hREPLProcess) {
        TerminateProcess(g_hREPLProcess, 0);
        WaitForSingleObject(g_hREPLProcess, 1000);
        CloseHandle(g_hREPLProcess);
        g_hREPLProcess = NULL;
    }
    
    // Wait for threads to exit
    if (g_hREPLStdoutThread) {
        WaitForSingleObject(g_hREPLStdoutThread, 2000);
        CloseHandle(g_hREPLStdoutThread);
        g_hREPLStdoutThread = NULL;
    }
    if (g_hREPLStderrThread) {
        WaitForSingleObject(g_hREPLStderrThread, 2000);
        CloseHandle(g_hREPLStderrThread);
        g_hREPLStderrThread = NULL;
    }
    
    // Close pipes
    if (g_hREPLStdin) {
        CloseHandle(g_hREPLStdin);
        g_hREPLStdin = NULL;
    }
    if (g_hREPLStdout) {
        CloseHandle(g_hREPLStdout);
        g_hREPLStdout = NULL;
    }
    if (g_hREPLStderr) {
        CloseHandle(g_hREPLStderr);
        g_hREPLStderr = NULL;
    }
    
    // Reset state
    g_nCurrentREPLFilter = -1;
    g_szREPLPromptEnd[0] = L'\0';
    g_REPLEOLMode = REPL_EOL_AUTO;
    g_dwREPLStdoutThreadId = 0;
    g_dwREPLStderrThreadId = 0;
    // Note: g_bREPLIntentionalExit is NOT reset here - it's checked in WM_REPL_EXITED handler
    
    // Update title bar to remove Interactive Mode indicator
    UpdateTitle(g_hWndMain);
    
    // Update status bar
    UpdateStatusBar();
    
    // Update menu states
    UpdateMenuStates(g_hWndMain);
}

//============================================================================
// SendLineToREPL - Send a line of input to the REPL stdin
//============================================================================
void SendLineToREPL()
{
    if (!g_bREPLMode || !g_hREPLStdin) {
        return;
    }
    
    // Get current line text
    CHARRANGE cr;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    
    // Get line number
    LONG lineIndex = SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin);
    
    // Get line start/end
    LONG lineStart = SendMessage(g_hWndEdit, EM_LINEINDEX, lineIndex, 0);
    LONG lineEnd = SendMessage(g_hWndEdit, EM_LINEINDEX, lineIndex + 1, 0);
    if (lineEnd == -1) {
        // Last line - get document length
        GETTEXTLENGTHEX gtl;
        gtl.flags = GTL_DEFAULT;
        gtl.codepage = 1200; // Unicode
        lineEnd = SendMessage(g_hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    }
    
    // Extract line text
    int lineLen = lineEnd - lineStart;
    if (lineLen <= 0) {
        return;
    }
    
    LPWSTR pszLine = (LPWSTR)malloc((lineLen + 1) * sizeof(WCHAR));
    if (!pszLine) {
        return;
    }
    
    TEXTRANGE tr;
    tr.chrg.cpMin = lineStart;
    tr.chrg.cpMax = lineEnd;
    tr.lpstrText = pszLine;
    SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    
    // Try to detect prompt and extract input after it
    int inputStart = 0;
    DetectPrompt(pszLine, g_szREPLPromptEnd, &inputStart);
    
    // Get input portion
    LPCWSTR pszInput = pszLine + inputStart;
    
    // Convert to UTF-8
    LPSTR pszInputUTF8 = UTF16ToUTF8(pszInput);
    if (pszInputUTF8) {
        // Append EOL based on detected mode
        const char* eol;
        if (g_REPLEOLMode == REPL_EOL_CRLF) {
            eol = "\r\n";
        } else if (g_REPLEOLMode == REPL_EOL_CR) {
            eol = "\r";
        } else {
            // Default to LF for AUTO and LF modes
            // LF works for most interactive shells (bash, python, node, etc.)
            // PowerShell also accepts LF even though it outputs CRLF
            eol = "\n";
        }
        
        // Send input + EOL to stdin
        DWORD dwWritten;
        WriteFile(g_hREPLStdin, pszInputUTF8, strlen(pszInputUTF8), &dwWritten, NULL);
        WriteFile(g_hREPLStdin, eol, strlen(eol), &dwWritten, NULL);
        
        // Flush the pipe
        FlushFileBuffers(g_hREPLStdin);
        
        free(pszInputUTF8);
    }
    
    free(pszLine);
    
    // Move cursor to end of current line and insert newline
    // This ensures the shell's output appears right after the command line
    CHARRANGE crEnd;
    crEnd.cpMin = lineEnd;
    crEnd.cpMax = lineEnd;
    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crEnd);
    SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)L"\n");
}

//============================================================================
// DetectEOL - Auto-detect line ending style from output
//============================================================================
REPLEOLMode DetectEOL(LPCSTR pszOutput, size_t len)
{
    // Scan buffer for line endings
    for (size_t i = 0; i < len - 1; i++) {
        if (pszOutput[i] == '\r' && pszOutput[i + 1] == '\n') {
            return REPL_EOL_CRLF;
        }
        if (pszOutput[i] == '\n') {
            return REPL_EOL_LF;
        }
        if (pszOutput[i] == '\r') {
            return REPL_EOL_CR;
        }
    }
    
    // Check last character
    if (len > 0) {
        if (pszOutput[len - 1] == '\n') {
            return REPL_EOL_LF;
        }
        if (pszOutput[len - 1] == '\r') {
            return REPL_EOL_CR;
        }
    }
    
    // Default to CRLF (Windows)
    return REPL_EOL_CRLF;
}

//============================================================================
// StripANSIEscapes - Remove ANSI escape sequences from text (in-place)
//============================================================================
void StripANSIEscapes(LPWSTR pszText)
{
    if (!pszText) {
        return;
    }
    
    LPWSTR src = pszText;
    LPWSTR dst = pszText;
    
    while (*src) {
        if (*src == L'\x1B' || *src == L'\x9B') {
            // Found ESC or CSI - skip escape sequence
            src++;
            
            // Handle OSC (Operating System Command): ESC ] ... BEL or ESC ] ... ST
            // Used for window titles, etc. Example: ESC ] 0 ; title BEL
            if (*src == L']') {
                src++;
                // Skip until BEL (0x07), ST (ESC \), or newline
                while (*src && *src != L'\x07' && *src != L'\n' && *src != L'\r') {
                    if (*src == L'\x1B' && *(src + 1) == L'\\') {
                        // Found ST (String Terminator)
                        src += 2;
                        break;
                    }
                    src++;
                }
                // Skip BEL if present
                if (*src == L'\x07') {
                    src++;
                }
            }
            // Skip CSI sequence: ESC [ ... letter
            // Also handles: ESC ( ... letter, ESC ) ... letter, etc.
            else if (*src == L'[' || *src == L'(' || *src == L')' || *src == L'#' || 
                *src == L'?' || *src == L'>' || *src == L'=' || *src == L'<') {
                src++;
                
                // Skip parameter bytes (0x30-0x3F) and intermediate bytes (0x20-0x2F)
                while (*src && ((*src >= 0x30 && *src <= 0x3F) || (*src >= 0x20 && *src <= 0x2F))) {
                    src++;
                }
                
                // Skip final byte (0x40-0x7E)
                if (*src >= 0x40 && *src <= 0x7E) {
                    src++;
                }
            } else {
                // Other escape sequences (ESC followed by single char)
                if (*src) {
                    src++;
                }
            }
        } else {
            // Normal character - copy it
            *dst++ = *src++;
        }
    }
    
    *dst = L'\0';
}

//============================================================================
// DetectPrompt - Find prompt ending in line and return input start position
//============================================================================
BOOL DetectPrompt(LPCWSTR pszLine, LPCWSTR pszPromptEnd, int* pInputStart)
{
    *pInputStart = 0;
    
    if (!pszLine || !pszPromptEnd || pszPromptEnd[0] == L'\0') {
        return FALSE;
    }
    
    // Search for prompt end string in line
    LPCWSTR pPrompt = wcsstr(pszLine, pszPromptEnd);
    if (pPrompt) {
        // Found prompt - input starts after prompt end
        *pInputStart = (pPrompt - pszLine) + wcslen(pszPromptEnd);
        return TRUE;
    }
    
    return FALSE;
}
