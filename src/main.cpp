//============================================================================
// RichEditor - A lightweight text editor using RichEdit control
// Phase 1: Basic text editing with UTF-8 support
//============================================================================

#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdio.h>
#include <string>
#include "resource.h"

//============================================================================
// Global Variables
//============================================================================
// Extended path length for UNC and long paths (Windows maximum is 32767)
#define EXTENDED_PATH_MAX 32767

// Custom messages for REPL (Phase 2.5)
#define WM_REPL_OUTPUT  (WM_USER + 100)  // wParam: unused, lParam: LPWSTR (output text to insert)
#define WM_REPL_EXITED  (WM_USER + 101)  // wParam: unused, lParam: unused (filter process exited)

HWND g_hWndMain = NULL;           // Main window handle
HWND g_hWndEdit = NULL;           // RichEdit control handle (to be added)
HWND g_hWndStatus = NULL;         // Status bar handle (to be added)
WCHAR g_szFileName[EXTENDED_PATH_MAX];     // Current file path
WCHAR g_szFileTitle[MAX_PATH];    // Current file name only
BOOL g_bModified = FALSE;         // Document modified flag
BOOL g_bSettingText = FALSE;      // Flag to prevent EN_CHANGE during SetWindowText
BOOL g_bWordWrap = TRUE;          // Word wrap enabled by default
HMODULE g_hRichEditLib = NULL;    // RichEdit DLL handle

// Autosave settings
BOOL g_bAutosaveEnabled = TRUE;              // Enable/disable autosave
UINT g_nAutosaveIntervalMinutes = 1;         // Autosave interval in minutes (0 = disabled)
BOOL g_bAutosaveOnFocusLoss = TRUE;          // Autosave when window loses focus
BOOL g_bPromptingForSave = FALSE;            // TRUE when showing save prompt (prevents autosave on focus loss)
const UINT_PTR IDT_AUTOSAVE = 1;             // Timer ID for autosave
const UINT_PTR IDT_FILTER_STATUSBAR = 2;     // Timer ID for filter status bar display

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

//============================================================================
// Filter System (Phase 2+)
//============================================================================
#define MAX_FILTERS 100
#define MAX_FILTER_NAME 64
#define MAX_FILTER_COMMAND 512
#define MAX_FILTER_DESC 256
#define MAX_FILTER_CATEGORY 32

#define MAX_MRU 10                 // Maximum number of MRU items
#define ID_FILE_MRU_BASE 5000      // Base ID for MRU menu items (5000-5009)
#define ID_CONTEXT_FILTER_BASE 6000  // Base ID for context menu filter items (6000-6099)

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

// MRU list
WCHAR g_MRU[MAX_MRU][EXTENDED_PATH_MAX];
int g_nMRUCount = 0;
BOOL g_bNoMRU = FALSE;              // TRUE when /nomru command-line option is specified

//============================================================================
// Function Declarations
//============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /* lParam */);
BOOL InitRichEditLibrary();
HWND CreateRichEditControl(HWND hwndParent);
HWND CreateStatusBar(HWND hwndParent);
void UpdateStatusBar();
void UpdateTitle(HWND hwnd = NULL);
void UpdateMenuUndoRedo(HMENU hMenu);
int CalculateTabAwareColumn(LPCWSTR pszLineText, int charPosition);
BOOL GetURLAtCursor(HWND hWndEdit, LPWSTR pszURL, int cchMax, CHARRANGE* pRange);
void OpenURL(HWND hwnd, LPCWSTR pszURL);
void CopyURLToClipboard(HWND hwnd, LPCWSTR pszURL);
void LoadStringResource(UINT uID, LPWSTR lpBuffer, int cchBufferMax);
LPWSTR UTF8ToUTF16(LPCSTR pszUTF8);
LPSTR UTF16ToUTF8(LPCWSTR pszUTF16);
BOOL LoadTextFile(LPCWSTR pszFileName);
BOOL SaveTextFile(LPCWSTR pszFileName);
void GetDocumentsPath(LPWSTR pszPath, DWORD cchPath);
void ShowError(UINT uMessageID, LPCWSTR pszEnglishMessage, DWORD dwError);
void FileNew();
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

// INI file functions
BOOL ReadINIValue(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, LPWSTR pszValue, DWORD dwSize, LPCWSTR pszDefault);
int ReadINIInt(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, int nDefault);
BOOL WriteINIValue(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, LPCWSTR pszValue);

// Resume file functions (Phase 2.6)
BOOL GetRichEditorTempDir(WCHAR* pszPath, DWORD dwSize);
BOOL EnsureRichEditorTempDirExists();
BOOL GenerateResumeFileName(const WCHAR* pszOriginalPath, WCHAR* pszResumeFile, DWORD dwSize);
void WriteResumeToINI(const WCHAR* pszResumeFile, const WCHAR* pszOriginalPath);
BOOL ReadResumeFromINI(WCHAR* pszResumeFile, DWORD dwResumeSize, WCHAR* pszOriginalPath, DWORD dwOriginalSize);
void ClearResumeFromINI();
BOOL DeleteResumeFile(const WCHAR* pszResumeFile);
BOOL SaveToResumeFile();

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
    
    // Load RichEdit library
    if (!InitRichEditLibrary()) {
        WCHAR szError[256], szTitle[64];
        LoadStringResource(IDS_RICHEDIT_LOAD_FAILED, szError, 256);
        LoadStringResource(IDS_ERROR, szTitle, 64);
        MessageBox(NULL, szError, szTitle, MB_ICONERROR);
        return 1;
    }
    
    // Parse command line arguments
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    WCHAR szCommandLineFile[EXTENDED_PATH_MAX] = L"";
    
    // Parse arguments: look for /nomru option and filename
    // /nomru can appear before or after the filename
    // Examples: RichEditor.exe file.json /nomru
    //           RichEditor.exe /nomru file.json
    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"/nomru") == 0) {
            g_bNoMRU = TRUE;
        } else if (argv[i][0] != L'\0' && szCommandLineFile[0] == L'\0') {
            // First non-option argument is the filename
            wcscpy_s(szCommandLineFile, EXTENDED_PATH_MAX, argv[i]);
        }
    }
    
    if (argv) {
        LocalFree(argv);
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
    
    // Create main window (800x600, centered)
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 800;
    int windowHeight = 600;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;
    
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
    
    // Load accelerators
    HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));
    
    // Show window
    ShowWindow(g_hWndMain, nCmdShow);
    UpdateWindow(g_hWndMain);
    
    // Command-line arguments have precedence over resume files
    // This allows RichEditor to be used as a file viewer while preserving
    // unsaved work for the next launch without arguments
    if (szCommandLineFile[0] != L'\0') {
        // Command-line file specified - load it and ignore resume file
        LoadTextFile(szCommandLineFile);
        
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
                // Open resume file
                HANDLE hFile = CreateFile(szResumeFile, GENERIC_READ, FILE_SHARE_READ,
                                          NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD dwSize = GetFileSize(hFile, NULL);
                    if (dwSize != INVALID_FILE_SIZE && dwSize > 0) {
                        char* pszUtf8 = (char*)malloc(dwSize + 1);
                        if (pszUtf8) {
                            DWORD dwRead;
                            ReadFile(hFile, pszUtf8, dwSize, &dwRead, NULL);
                            pszUtf8[dwRead] = '\0';
                            
                            // Convert to UTF-16
                            int nWideLen = MultiByteToWideChar(CP_UTF8, 0, pszUtf8, -1, NULL, 0);
                            WCHAR* pszWide = (WCHAR*)malloc(nWideLen * sizeof(WCHAR));
                            if (pszWide) {
                                MultiByteToWideChar(CP_UTF8, 0, pszUtf8, -1, pszWide, nWideLen);
                                SetWindowText(g_hWndEdit, pszWide);
                                free(pszWide);
                            }
                            free(pszUtf8);
                            
                            // Set up resumed file state
                            if (szOriginalPath[0] != L'\0') {
                                wcscpy(g_szFileName, szOriginalPath);
                                const WCHAR* pszFileTitle = wcsrchr(szOriginalPath, L'\\');
                                if (pszFileTitle) {
                                    wcscpy(g_szFileTitle, pszFileTitle + 1);
                                } else {
                                    wcscpy(g_szFileTitle, szOriginalPath);
                                }
                            } else {
                                g_szFileName[0] = L'\0';
                                g_szFileTitle[0] = L'\0';
                            }
                            
                            wcscpy(g_szResumeFilePath, szResumeFile);
                            wcscpy(g_szOriginalFilePath, szOriginalPath);
                            g_bIsResumedFile = TRUE;
                            g_bModified = TRUE;  // Mark as modified
                            
                            // Update title bar to show [Resumed] indicator
                            UpdateTitle(g_hWndMain);
                            
                            // DON'T add to MRU list
                            // DON'T delete resume file yet (keep as backup)
                        }
                    }
                    CloseHandle(hFile);
                }
            }
            
            // Clear INI entry (one-time recovery)
            ClearResumeFromINI();
        }
    }
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(g_hWndMain, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // Cleanup
    if (g_hRichEditLib) {
        FreeLibrary(g_hRichEditLib);
    }
    
    return (int)msg.wParam;
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
            
            // Create RichEdit control
            g_hWndEdit = CreateRichEditControl(hwnd);
            if (!g_hWndEdit) {
                WCHAR szError[256], szTitle[64];
                LoadStringResource(IDS_RICHEDIT_CREATE_FAILED, szError, 256);
                LoadStringResource(IDS_ERROR, szTitle, 64);
                MessageBox(hwnd, szError, szTitle, MB_ICONERROR);
                return -1;
            }
            
            // Create status bar
            g_hWndStatus = CreateStatusBar(hwnd);
            
            // Update status bar
            UpdateStatusBar();
            
            // Load settings and filters (Phase 2)
            CreateDefaultINI();  // Create default INI if it doesn't exist
            LoadSettings();
            LoadFilters();
            BuildFilterMenu(hwnd);
            UpdateFilterDisplay();
            UpdateMenuStates(hwnd);
            
            // Load MRU list
            LoadMRU();
            UpdateMRUMenu(hwnd);
            
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
            }
            return 0;
            
        case WM_SETFOCUS:
            // Restore focus to edit control when window receives focus
            if (g_hWndEdit) {
                SetFocus(g_hWndEdit);
            }
            return 0;
            
        case WM_KILLFOCUS:
            // Autosave on focus loss if enabled
            // BUT: Don't autosave if we're showing the save prompt (prevents saving before user responds)
            if (g_bAutosaveEnabled && g_bAutosaveOnFocusLoss && !g_bPromptingForSave) {
                DoAutosave();
            }
            return 0;
            
        case WM_TIMER:
            // Handle autosave timer
            if (wParam == IDT_AUTOSAVE) {
                DoAutosave();
            }
            // Handle filter status bar timer (30-second display)
            else if (wParam == IDT_FILTER_STATUSBAR) {
                KillTimer(hwnd, IDT_FILTER_STATUSBAR);
                g_bFilterStatusBarActive = FALSE;
                UpdateStatusBar();  // Revert to normal display
            }
            return 0;
            
        case WM_COMMAND:
            // Handle edit control notifications
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_hWndEdit) {
                if (!g_bSettingText && !g_bModified) {
                    g_bModified = TRUE;
                    g_lastURLRange.cpMin = -1;  // Invalidate cached URL range
                    UpdateTitle();
                }
                return 0;
            }
            
            switch (LOWORD(wParam)) {
                // File menu
                case ID_FILE_NEW:
                    FileNew();
                    break;
                case ID_FILE_OPEN:
                    FileOpen();
                    break;
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
                
                // Tools -> Filter Help
                case ID_TOOLS_MANAGEFILTERS:
                    {
                        WCHAR szHelpText[2048], szTitle[64];
                        LoadStringResource(IDS_FILTER_HELP_TEXT, szHelpText, 2048);
                        LoadStringResource(IDS_FILTER_HELP_TITLE, szTitle, 64);
                        MessageBox(hwnd, szHelpText, szTitle, MB_ICONINFORMATION);
                    }
                    break;
                
                // Tools -> Select Filter submenu (dynamic filter selection)
                default:
                    {
                        int wmId = LOWORD(wParam);
                        
                        // Handle MRU file clicks
                        if (wmId >= ID_FILE_MRU_BASE && wmId < ID_FILE_MRU_BASE + MAX_MRU) {
                            int mruIdx = wmId - ID_FILE_MRU_BASE;
                            if (mruIdx >= 0 && mruIdx < g_nMRUCount) {
                                // Check for unsaved changes
                                if (!PromptSaveChanges()) {
                                    break;
                                }
                                
                                // Make a copy of the path before loading
                                // (LoadTextFile->AddToMRU will modify the g_MRU array)
                                WCHAR szMRUPath[EXTENDED_PATH_MAX];
                                wcscpy_s(szMRUPath, EXTENDED_PATH_MAX, g_MRU[mruIdx]);
                                
                                // Load the MRU file (LoadTextFile handles everything)
                                LoadTextFile(szMRUPath);
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
            // Update Undo/Redo menu items when Edit menu is opened
            if (LOWORD(lParam) == 1) {  // Edit menu is at position 1
                UpdateMenuUndoRedo((HMENU)wParam);
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
                    
                    AppendMenu(hMenu, canUndo ? MF_STRING : MF_STRING | MF_GRAYED, 
                               ID_EDIT_UNDO, szUndo);
                    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenu(hMenu, hasSelection ? MF_STRING : MF_STRING | MF_GRAYED, 
                               ID_EDIT_CUT, szCut);
                    AppendMenu(hMenu, hasSelection ? MF_STRING : MF_STRING | MF_GRAYED, 
                               ID_EDIT_COPY, szCopy);
                    AppendMenu(hMenu, canPaste ? MF_STRING : MF_STRING | MF_GRAYED, 
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
                    WCHAR szResumeFile[EXTENDED_PATH_MAX];
                    
                    // Generate resume file name
                    if (GenerateResumeFileName(g_szFileName, szResumeFile, EXTENDED_PATH_MAX)) {
                        // Get text from editor
                        GETTEXTLENGTHEX gtl;
                        gtl.flags = GTL_DEFAULT;
                        gtl.codepage = 1200; // Unicode
                        LONG nTextLen = SendMessage(g_hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
                        
                        if (nTextLen >= 0 && nTextLen < INT_MAX - 1) {
                            WCHAR* pszText = (WCHAR*)malloc((nTextLen + 1) * sizeof(WCHAR));
                            if (pszText) {
                                GetWindowText(g_hWndEdit, pszText, nTextLen + 1);
                                
                                // Save to temp file (UTF-8 without BOM)
                                HANDLE hFile = CreateFile(szResumeFile, GENERIC_WRITE, 0, NULL,
                                                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                                if (hFile != INVALID_HANDLE_VALUE) {
                                    // Convert to UTF-8
                                    int nUtf8Len = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, NULL, 0, NULL, NULL);
                                    char* pszUtf8 = (char*)malloc(nUtf8Len);
                                    if (pszUtf8) {
                                        WideCharToMultiByte(CP_UTF8, 0, pszText, -1, pszUtf8, nUtf8Len, NULL, NULL);
                                        
                                        DWORD dwWritten;
                                        WriteFile(hFile, pszUtf8, nUtf8Len - 1, &dwWritten, NULL);
                                        free(pszUtf8);
                                    }
                                    CloseHandle(hFile);
                                    
                                    // Store resume file path (but don't write to INI yet)
                                    wcscpy(g_szResumeFilePath, szResumeFile);
                                }
                                free(pszText);
                            }
                        }
                    }
                }
                
                // Allow Windows to proceed with shutdown
                return TRUE;
            }
            
        case WM_CLOSE:
            // Normal close - behavior depends on AutoSaveUntitledOnClose setting
            {
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
                    
                    if (g_bAutoSaveUntitledOnClose && isUntitled) {
                        // Modern mode - auto-save untitled files only (no prompt)
                        if (!SaveToResumeFile()) {
                            // Error already shown - ask if user wants to close anyway
                            WCHAR szPrompt[256];
                            LoadStringResource(IDS_ERROR, szPrompt, 256);
                            int result = MessageBox(hwnd,
                                L"Failed to save session. Close without saving?",
                                szPrompt, MB_YESNO | MB_ICONWARNING);
                            if (result != IDYES) {
                                return 0;
                            }
                        }
                    } else {
                        // Traditional mode - prompt user
                        if (!PromptSaveChanges()) {
                            // User cancelled or chose "No" - clean up resume file if exists
                            if (g_bIsResumedFile && g_szResumeFilePath[0] != L'\0') {
                                DeleteResumeFile(g_szResumeFilePath);
                                g_bIsResumedFile = FALSE;
                                g_szResumeFilePath[0] = L'\0';
                                g_szOriginalFilePath[0] = L'\0';
                            }
                            return 0; // User cancelled
                        }
                    }
                }
                
                // Clean up resume file if user closed without saving a resumed file
                if (g_bIsResumedFile && g_szResumeFilePath[0] != L'\0' && !g_bModified) {
                    DeleteResumeFile(g_szResumeFilePath);
                    g_bIsResumedFile = FALSE;
                    g_szResumeFilePath[0] = L'\0';
                    g_szOriginalFilePath[0] = L'\0';
                }
                
                // Close the window
                DestroyWindow(hwnd);
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
        
        case WM_DESTROY:
            // Exit REPL mode if active
            if (g_bREPLMode) {
                ExitREPLMode();
            }
            
            // Kill timers
            KillTimer(hwnd, IDT_AUTOSAVE);
            KillTimer(hwnd, IDT_FILTER_STATUSBAR);
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
// WriteResumeToINI - Store resume file info in INI
//============================================================================
void WriteResumeToINI(const WCHAR* pszResumeFile, const WCHAR* pszOriginalPath)
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    WCHAR* pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
    WriteINIValue(szIniPath, L"Resume", L"ResumeFile", pszResumeFile);
    WriteINIValue(szIniPath, L"Resume", L"OriginalPath", 
                  pszOriginalPath ? pszOriginalPath : L"");
}

//============================================================================
// ReadResumeFromINI - Read resume file info from INI
// Returns: TRUE if resume file exists, FALSE otherwise
//============================================================================
BOOL ReadResumeFromINI(WCHAR* pszResumeFile, DWORD dwResumeSize,
                       WCHAR* pszOriginalPath, DWORD dwOriginalSize)
{
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    WCHAR* pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
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
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    WCHAR* pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
    WriteINIValue(szIniPath, L"Resume", L"ResumeFile", L"");
    WriteINIValue(szIniPath, L"Resume", L"OriginalPath", L"");
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
// SaveToResumeFile - Save current document to resume file
// Returns: TRUE on success, FALSE on failure
//============================================================================
BOOL SaveToResumeFile()
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
    
    // Get text from editor using accurate method
    GETTEXTLENGTHEX gtl;
    gtl.flags = GTL_DEFAULT;
    gtl.codepage = 1200; // Unicode
    LONG nTextLen = SendMessage(g_hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    
    if (nTextLen < 0 || nTextLen > INT_MAX - 1) {
        return FALSE;  // Invalid length
    }
    
    WCHAR* pszText = (WCHAR*)malloc((nTextLen + 1) * sizeof(WCHAR));
    if (!pszText) {
        return FALSE;
    }
    
    GetWindowText(g_hWndEdit, pszText, nTextLen + 1);
    
    // Save to temp file (UTF-8 without BOM)
    HANDLE hFile = CreateFile(szResumeFile, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        free(pszText);
        return FALSE;
    }
    
    // Convert to UTF-8
    int nUtf8Len = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, NULL, 0, NULL, NULL);
    char* pszUtf8 = (char*)malloc(nUtf8Len);
    BOOL bSuccess = FALSE;
    
    if (pszUtf8) {
        WideCharToMultiByte(CP_UTF8, 0, pszText, -1, pszUtf8, nUtf8Len, NULL, NULL);
        
        DWORD dwWritten;
        BOOL bWriteSuccess = WriteFile(hFile, pszUtf8, nUtf8Len - 1, &dwWritten, NULL);
        
        if (bWriteSuccess && dwWritten == (DWORD)(nUtf8Len - 1)) {
            bSuccess = TRUE;
        } else {
            // Write failed - show error
            WCHAR szError[512];
            LoadStringResource(IDS_ERROR, szError, 512);
            MessageBox(g_hWndMain,
                       L"Failed to write resume file. Disk full?",
                       szError, MB_OK | MB_ICONERROR);
        }
        free(pszUtf8);
    }
    
    CloseHandle(hFile);
    free(pszText);
    
    if (!bSuccess) {
        DeleteFile(szResumeFile);  // Clean up partial file
        return FALSE;
    }
    
    // Store resume file path and original path in INI
    WriteResumeToINI(szResumeFile, g_szFileName[0] ? g_szFileName : L"");
    
    // Remember resume file path globally
    wcscpy(g_szResumeFilePath, szResumeFile);
    
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
        return FALSE;  // Not in URL
    }
    
    // Use EM_FINDWORDBREAK to quickly narrow down search area (optimization for large documents)
    LONG wordLeft = SendMessage(hWndEdit, EM_FINDWORDBREAK, WB_LEFT, cursorPos);
    LONG wordRight = SendMessage(hWndEdit, EM_FINDWORDBREAK, WB_RIGHT, cursorPos);
    
    // Find actual URL boundaries within the word break range
    // Word breaks may not align with CFE_LINK boundaries (URLs with = and & are often broken)
    LONG urlStart = wordLeft;
    LONG urlEnd = wordRight;
    
    // Scan forward from wordLeft to find where CFE_LINK actually starts
    BOOL foundStart = FALSE;
    for (LONG pos = wordLeft; pos <= cursorPos; pos++) {
        cr.cpMin = pos;
        cr.cpMax = pos + 1;
        SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        SendMessage(hWndEdit, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        if (cf.dwEffects & CFE_LINK) {
            urlStart = pos;
            foundStart = TRUE;
            break;
        }
    }
    
    // Scan backward from wordRight-1 to find where CFE_LINK actually ends
    BOOL foundEnd = FALSE;
    for (LONG pos = wordRight - 1; pos >= cursorPos; pos--) {
        cr.cpMin = pos;
        cr.cpMax = pos + 1;
        SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        SendMessage(hWndEdit, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        if (cf.dwEffects & CFE_LINK) {
            urlEnd = pos + 1;
            foundEnd = TRUE;
            break;
        }
    }
    
    // If we didn't find boundaries in the word break range, the URL might span beyond word breaks
    // Fall back to scanning from cursor position outward
    if (!foundStart || !foundEnd) {
        // Scan backward from cursor to find start
        urlStart = cursorPos;
        for (LONG pos = cursorPos - 1; pos >= 0; pos--) {
            cr.cpMin = pos;
            cr.cpMax = pos + 1;
            SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessage(hWndEdit, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            if (cf.dwEffects & CFE_LINK) {
                urlStart = pos;
            } else {
                break;
            }
        }
        
        // Scan forward from cursor to find end
        urlEnd = cursorPos;
        GETTEXTLENGTHEX gtl;
        gtl.flags = GTL_DEFAULT;
        gtl.codepage = 1200;
        LONG docLen = SendMessage(hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
        for (LONG pos = cursorPos; pos < docLen; pos++) {
            cr.cpMin = pos;
            cr.cpMax = pos + 1;
            SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessage(hWndEdit, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            if (cf.dwEffects & CFE_LINK) {
                urlEnd = pos + 1;
            } else {
                break;
            }
        }
    }
    
    wordLeft = urlStart;
    wordRight = urlEnd;
    
    // Extract URL text
    int urlLen = wordRight - wordLeft;
    if (urlLen <= 0 || urlLen >= cchMax) {
        SendMessage(hWndEdit, EM_EXSETSEL, 0, (LPARAM)&savedSel);
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
// InitRichEditLibrary - Load RichEdit 4.1 DLL
//============================================================================
BOOL InitRichEditLibrary()
{
    g_hRichEditLib = LoadLibrary(L"Msftedit.dll");
    return (g_hRichEditLib != NULL);
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
    
    HWND hwndEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS,
        L"",
        style,
        0, 0, 0, 0,
        hwndParent,
        (HMENU)IDC_RICHEDIT,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (hwndEdit) {
        // Set undo limit
        SendMessage(hwndEdit, EM_SETUNDOLIMIT, 100, 0);
        
        // Enable automatic URL detection BEFORE setting plain text mode
        // (some RichEdit versions need this order)
        SendMessage(hwndEdit, EM_AUTOURLDETECT, AURL_ENABLEURL, 0);
        
        // Set plain text mode
        SendMessage(hwndEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        
        // Set event mask for notifications
        SendMessage(hwndEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE | ENM_LINK);
        
        // Set large text limit (2GB)
        SendMessage(hwndEdit, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
        
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
    
    if (g_bWordWrap) {
        // When word wrap is ON:
        // - Visual line/col: counts display lines including soft wraps (from RichEdit)
        // - Physical line/col: counts only hard line breaks by parsing the text
        
        // Get visual (wrapped) line by counting display lines from start to cursor
        visualLine = 1;
        int currentLineStart = 0;
        int lineIndex = 0;
        
        while (currentLineStart < cr.cpMin) {
            int nextLineStart = (int)SendMessage(g_hWndEdit, EM_LINEINDEX, lineIndex + 1, 0);
            if (nextLineStart == -1 || nextLineStart <= currentLineStart) {
                break;
            }
            if (nextLineStart <= cr.cpMin) {
                visualLine++;
                currentLineStart = nextLineStart;
                lineIndex++;
            } else {
                break;
            }
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
        
        // Get physical (unwrapped) line and column by manually counting hard newlines
        // Use EM_GETTEXTRANGE to get text up to cursor with exact character positions
        physicalLine = 1;  // Start at line 1
        physicalCol = 1;
        int physicalLineStart = 0;  // Start of file is start of line 1
        
        if (cr.cpMin > 0) {
            // Allocate buffer for text up to cursor
            LPWSTR buffer = (LPWSTR)malloc((cr.cpMin + 1) * sizeof(WCHAR));
            if (buffer) {
                // Use EM_GETTEXTRANGE to get exactly the text from 0 to cursor position
                TEXTRANGE tr;
                tr.chrg.cpMin = 0;
                tr.chrg.cpMax = cr.cpMin;
                tr.lpstrText = buffer;
                
                int retrieved = (int)SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
                
                if (retrieved > 0) {
                    // Count newlines and track positions
                    // Note: retrieved is the number of chars returned, which should equal cr.cpMin
                    int bufferPos = 0;  // Position in buffer
                    
                    while (bufferPos < retrieved) {
                        if (buffer[bufferPos] == L'\r') {
                            // Check if this is CRLF or just CR
                            if (bufferPos + 1 < retrieved && buffer[bufferPos + 1] == L'\n') {
                                // CRLF - treat as one newline
                                bufferPos += 2;
                                physicalLine++;
                                physicalLineStart = bufferPos;
                            } else {
                                // Just CR
                                bufferPos++;
                                physicalLine++;
                                physicalLineStart = bufferPos;
                            }
                        } else if (buffer[bufferPos] == L'\n') {
                            // Just LF
                            bufferPos++;
                            physicalLine++;
                            physicalLineStart = bufferPos;
                        } else {
                            // Regular character
                            bufferPos++;
                        }
                    }
                    
                    // Calculate tab-aware physical column
                    int lineCharCount = bufferPos - physicalLineStart;
                    if (lineCharCount > 0) {
                        // Extract line text from physicalLineStart to bufferPos
                        physicalCol = CalculateTabAwareColumn(buffer + physicalLineStart, lineCharCount);
                    } else {
                        physicalCol = 1;  // At start of line
                    }
                }
                
                free(buffer);
            }
        }
        
    } else {
        // When word wrap is OFF, visual = physical (no soft wraps)
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
    
    // Get character at cursor (handle surrogate pairs for characters > U+FFFF)
    WCHAR charInfo[128] = L"";
    int textLen = GetWindowTextLength(g_hWndEdit);
    
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
        _snwprintf(szStatus, 512, L"%s    %s", posInfo, charInfo);
        
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
    WCHAR szTitle[MAX_PATH + 100];  // Increased size for [Interactive Mode] and [Obnoveno]
    WCHAR szUntitled[64];
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
    
    // Append [Obnoveno] indicator if this is a resumed file
    if (g_bIsResumedFile) {
        LoadStringResource(IDS_RESUMED, szResumed, 32);
        wcscat(szTitle, L" [");
        wcscat(szTitle, szResumed);
        wcscat(szTitle, L"]");
    }
    
    // Append [Interactive Mode] indicator if in REPL mode
    if (g_bREPLMode) {
        wcscat(szTitle, L" [Interactive Mode]");
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
        // Get undo type from RichEdit (or use filter flag)
        LRESULT undoType = g_bLastOperationWasFilter ? 0 : SendMessage(g_hWndEdit, EM_GETUNDONAME, 0, 0);
        
        UINT stringID = IDS_UNDO;
        if (g_bLastOperationWasFilter) {
            stringID = IDS_UNDO_FILTER;
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
            // Note: RichEdit doesn't know about filters, so filter redos will show as UID_UNKNOWN
            default: stringID = IDS_REDO;         break; // UID_UNKNOWN or filter
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
// LoadTextFile - Load UTF-8 text file into RichEdit control
//============================================================================
BOOL LoadTextFile(LPCWSTR pszFileName)
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
    free(pszUTF16);
    
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
    
    // Clear resume file state when loading a new file
    if (g_bIsResumedFile) {
        DeleteResumeFile(g_szResumeFilePath);
        g_bIsResumedFile = FALSE;
        g_szResumeFilePath[0] = L'\0';
        g_szOriginalFilePath[0] = L'\0';
    }
    
    UpdateTitle();
    UpdateStatusBar();
    
    // Add to MRU list
    AddToMRU(pszFileName);
    
    return TRUE;
}

//============================================================================
// SaveTextFile - Save RichEdit control content as UTF-8 text file
//============================================================================
BOOL SaveTextFile(LPCWSTR pszFileName)
{
    // Get text length
    int cchText = GetWindowTextLength(g_hWndEdit);
    if (cchText < 0) return FALSE;
    
    // Allocate buffer for UTF-16 text
    LPWSTR pszUTF16 = (LPWSTR)malloc((cchText + 1) * sizeof(WCHAR));
    if (!pszUTF16) {
        ShowError(IDS_ERROR_OUT_OF_MEMORY, L"Out of memory", 0);
        return FALSE;
    }
    
    // Get text from RichEdit control
    GetWindowText(g_hWndEdit, pszUTF16, cchText + 1);
    
    // Convert to UTF-8
    LPSTR pszUTF8 = UTF16ToUTF8(pszUTF16);
    free(pszUTF16);
    
    if (!pszUTF8) {
        ShowError(IDS_ERROR_CONVERT_TEXT_ENCODING, L"Could not convert text encoding", 0);
        return FALSE;
    }
    
    // Create file
    HANDLE hFile = CreateFile(pszFileName, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        free(pszUTF8);
        ShowError(IDS_ERROR_CREATE_FILE, L"Could not create file", GetLastError());
        return FALSE;
    }
    
    // Write UTF-8 data (without BOM)
    DWORD dwBytesToWrite = (DWORD)strlen(pszUTF8);
    DWORD dwBytesWritten;
    if (!WriteFile(hFile, pszUTF8, dwBytesToWrite, &dwBytesWritten, NULL)) {
        free(pszUTF8);
        CloseHandle(hFile);
        ShowError(IDS_ERROR_WRITE_FILE, L"Could not write file", GetLastError());
        return FALSE;
    }
    
    free(pszUTF8);
    CloseHandle(hFile);
    
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
    
    // Clear resume file state after successful save
    if (g_bIsResumedFile) {
        DeleteResumeFile(g_szResumeFilePath);
        g_bIsResumedFile = FALSE;
        g_szResumeFilePath[0] = L'\0';
        g_szOriginalFilePath[0] = L'\0';
    }
    
    UpdateTitle();
    UpdateStatusBar();
    
    // Add to MRU list
    AddToMRU(pszFileName);
    
    return TRUE;
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
// NormalizePathForINI - Not needed anymore, will read INI files directly
// Keeping this for reference but replacing GetPrivateProfile* with direct file reading
//============================================================================
// Simple INI reader that works with UNC paths
// Reads entire INI file into memory and parses it
BOOL ReadINIValue(LPCWSTR pszIniPath, LPCWSTR pszSection, LPCWSTR pszKey, LPWSTR pszValue, DWORD cchValue, LPCWSTR pszDefault)
{
    // Read entire file
    HANDLE hFile = CreateFile(pszIniPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (pszDefault) {
            wcsncpy(pszValue, pszDefault, cchValue);
            pszValue[cchValue - 1] = L'\0';
        } else {
            pszValue[0] = L'\0';
        }
        return FALSE;
    }
    
    DWORD dwSize = GetFileSize(hFile, NULL);
    if (dwSize == 0 || dwSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        if (pszDefault) {
            wcsncpy(pszValue, pszDefault, cchValue);
            pszValue[cchValue - 1] = L'\0';
        } else {
            pszValue[0] = L'\0';
        }
        return FALSE;
    }
    
    char* pszFileData = (char*)malloc(dwSize + 1);
    if (!pszFileData) {
        CloseHandle(hFile);
        pszValue[0] = L'\0';
        return FALSE;
    }
    
    DWORD dwRead;
    if (!ReadFile(hFile, pszFileData, dwSize, &dwRead, NULL)) {
        free(pszFileData);
        CloseHandle(hFile);
        if (pszDefault) {
            wcsncpy(pszValue, pszDefault, cchValue);
            pszValue[cchValue - 1] = L'\0';
        } else {
            pszValue[0] = L'\0';
        }
        return FALSE;
    }
    pszFileData[dwRead] = '\0';
    CloseHandle(hFile);
    
    // Convert to wide string
    int cchWide = MultiByteToWideChar(CP_UTF8, 0, pszFileData, -1, NULL, 0);
    WCHAR* pszWideData = (WCHAR*)malloc(cchWide * sizeof(WCHAR));
    if (!pszWideData) {
        free(pszFileData);
        pszValue[0] = L'\0';
        return FALSE;
    }
    MultiByteToWideChar(CP_UTF8, 0, pszFileData, -1, pszWideData, cchWide);
    free(pszFileData);
    
    // Parse INI: find [Section]
    WCHAR szSectionHeader[256];
    _snwprintf(szSectionHeader, 256, L"[%s]", pszSection);
    
    WCHAR* pszSectionStart = wcsstr(pszWideData, szSectionHeader);
    if (!pszSectionStart) {
        free(pszWideData);
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
        free(pszWideData);
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
    WCHAR* pszLine = pszSectionStart;
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
                
                free(pszWideData);
                return TRUE;
            }
        }
        
        // Move to next line
        pszLine = wcschr(pszLine, L'\n');
        if (pszLine) pszLine++;
    }
    
    free(pszWideData);
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
    // Read entire file if it exists
    std::string existingData;
    HANDLE hFile = CreateFile(pszIniPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
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
    
    // Convert existing data to wide string for easier manipulation
    std::wstring wideData;
    if (!existingData.empty()) {
        int cchWide = MultiByteToWideChar(CP_UTF8, 0, existingData.c_str(), -1, NULL, 0);
        if (cchWide > 0) {
            WCHAR* pszWideData = (WCHAR*)malloc(cchWide * sizeof(WCHAR));
            if (pszWideData) {
                MultiByteToWideChar(CP_UTF8, 0, existingData.c_str(), -1, pszWideData, cchWide);
                wideData = pszWideData;
                free(pszWideData);
            }
        }
    }
    
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
        wideData += pszKey;
        wideData += L"=";
        wideData += pszValue;
        wideData += L"\r\n";
        result = wideData;
    } else {
        // Section exists - find the key or insert it
        size_t lineStart = sectionPos + wcslen(szSectionHeader);
        
        // Skip to next line after section header
        size_t nextLine = wideData.find(L'\n', lineStart);
        if (nextLine != std::wstring::npos) {
            lineStart = nextLine + 1;
        }
        
        // Look for the key in this section
        WCHAR szKeyPrefix[256];
        _snwprintf(szKeyPrefix, 256, L"%s=", pszKey);
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
        
        if (!keyFound) {
            // Key not found in section - add it after section header
            result = wideData.substr(0, lineStart);
            result += pszKey;
            result += L"=";
            result += pszValue;
            result += L"\r\n";
            result += wideData.substr(lineStart);
        }
    }
    
    // Convert back to UTF-8
    int cbUTF8 = WideCharToMultiByte(CP_UTF8, 0, result.c_str(), -1, NULL, 0, NULL, NULL);
    if (cbUTF8 <= 0) return FALSE;
    
    char* pszUTF8 = (char*)malloc(cbUTF8);
    if (!pszUTF8) return FALSE;
    
    WideCharToMultiByte(CP_UTF8, 0, result.c_str(), -1, pszUTF8, cbUTF8, NULL, NULL);
    
    // Write to file
    hFile = CreateFile(pszIniPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        free(pszUTF8);
        return FALSE;
    }
    
    DWORD dwWritten;
    BOOL success = WriteFile(hFile, pszUTF8, strlen(pszUTF8), &dwWritten, NULL);
    CloseHandle(hFile);
    free(pszUTF8);
    
    return success;
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
    
    // Reset state
    g_szFileName[0] = L'\0';
    g_szFileTitle[0] = L'\0';
    g_bModified = FALSE;
    
    UpdateTitle();
    UpdateStatusBar();
    SetFocus(g_hWndEdit);
}

//============================================================================
// FileOpen - Open file dialog and load file
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
    
    // Build localized filter string: "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0"
    WCHAR szFilterText[64], szFilterAll[64];
    WCHAR szFilter[256];
    LoadStringResource(IDS_FILE_FILTER_TEXT, szFilterText, 64);
    LoadStringResource(IDS_FILE_FILTER_ALL, szFilterAll, 64);
    
    // Manually build the double-null-terminated filter string
    int pos = 0;
    wcscpy(szFilter + pos, szFilterText);
    pos += wcslen(szFilterText) + 1;
    wcscpy(szFilter + pos, L"*.txt");
    pos += wcslen(L"*.txt") + 1;
    wcscpy(szFilter + pos, szFilterAll);
    pos += wcslen(szFilterAll) + 1;
    wcscpy(szFilter + pos, L"*.*");
    pos += wcslen(L"*.*") + 1;
    szFilter[pos] = L'\0';  // Double null terminator
    
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
    // If this is a resumed untitled file, force "Save As" dialog
    if (g_bIsResumedFile && g_szOriginalFilePath[0] == L'\0') {
        return FileSaveAs();
    }
    
    // If this is a resumed saved file, ask user where to save
    if (g_bIsResumedFile && g_szOriginalFilePath[0] != L'\0') {
        WCHAR szPrompt[512];
        _snwprintf(szPrompt, 512,
            L"This file was recovered from a previous session.\n\n"
            L"Original location: %s\n\n"
            L"Save to original location?",
            g_szOriginalFilePath);
        
        int result = MessageBox(g_hWndMain, szPrompt, L"RichEditor",
                               MB_YESNOCANCEL | MB_ICONQUESTION);
        
        if (result == IDCANCEL) {
            return FALSE;
        } else if (result == IDNO) {
            return FileSaveAs();  // User wants to choose new location
        } else {
            // IDYES - save to original location
            wcscpy(g_szFileName, g_szOriginalFilePath);
        }
    }
    
    if (g_szFileName[0] == L'\0') {
        return FileSaveAs();
    }
    
    return SaveTextFile(g_szFileName);
}

//============================================================================
// FileSaveAs - Show Save As dialog and save file
//============================================================================
BOOL FileSaveAs()
{
    // Setup file dialog
    OPENFILENAME ofn = {};
    WCHAR szFile[EXTENDED_PATH_MAX] = L"";
    WCHAR szInitialDir[EXTENDED_PATH_MAX];
    
    // Copy current filename if exists
    if (g_szFileName[0]) {
        wcscpy_s(szFile, EXTENDED_PATH_MAX, g_szFileName);
    }
    
    GetDocumentsPath(szInitialDir, EXTENDED_PATH_MAX);
    
    // Build localized filter string
    WCHAR szFilterText[64], szFilterAll[64];
    WCHAR szFilter[256];
    LoadStringResource(IDS_FILE_FILTER_TEXT, szFilterText, 64);
    LoadStringResource(IDS_FILE_FILTER_ALL, szFilterAll, 64);
    
    // Manually build the double-null-terminated filter string
    int pos = 0;
    wcscpy(szFilter + pos, szFilterText);
    pos += wcslen(szFilterText) + 1;
    wcscpy(szFilter + pos, L"*.txt");
    pos += wcslen(L"*.txt") + 1;
    wcscpy(szFilter + pos, szFilterAll);
    pos += wcslen(szFilterAll) + 1;
    wcscpy(szFilter + pos, L"*.*");
    pos += wcslen(L"*.*") + 1;
    szFilter[pos] = L'\0';  // Double null terminator
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = g_hWndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = EXTENDED_PATH_MAX;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = szInitialDir;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    
    // Show dialog
    if (GetSaveFileName(&ofn)) {
        if (SaveTextFile(szFile)) {
            SetFocus(g_hWndEdit);
            return TRUE;
        }
    }
    
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
    
    // Set flag to prevent autosave-on-focus-loss while showing the prompt
    // (MessageBox steals focus, triggering WM_KILLFOCUS, which would autosave before user responds!)
    g_bPromptingForSave = TRUE;
    
    int result = MessageBox(g_hWndMain, szPrompt, L"RichEditor",
                           MB_YESNOCANCEL | MB_ICONQUESTION);
    
    // Clear flag after user responds
    g_bPromptingForSave = FALSE;
    
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
    // Clear filter flag when undoing
    g_bLastOperationWasFilter = FALSE;
    SendMessage(g_hWndEdit, EM_UNDO, 0, 0);
}

//============================================================================
// EditRedo - Redo last undone operation
//============================================================================
void EditRedo()
{
    SendMessage(g_hWndEdit, EM_REDO, 0, 0);
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
// EditInsertTimeDate - Insert current date and time at cursor position
//============================================================================
void EditInsertTimeDate()
{
    // Get current local time
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Format date and time using locale short format (like Notepad)
    WCHAR szDateTime[256];
    int dateLen = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, szDateTime, 128);
    
    if (dateLen > 0) {
        // Add space separator
        szDateTime[dateLen - 1] = L' ';
        
        // Append time
        GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, szDateTime + dateLen, 128);
        
        // Insert at current cursor position (replaces selection if any)
        SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)szDateTime);
    }
}

//============================================================================
// ViewWordWrap - Toggle word wrap on/off
//============================================================================
void ViewWordWrap()
{
    // Toggle word wrap state
    g_bWordWrap = !g_bWordWrap;
    
    // Get current selection to restore after recreation
    CHARRANGE crSel;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&crSel);
    
    // Get current text to restore
    int textLen = GetWindowTextLength(g_hWndEdit);
    LPWSTR pszText = (LPWSTR)malloc((textLen + 1) * sizeof(WCHAR));
    if (pszText) {
        GetWindowText(g_hWndEdit, pszText, textLen + 1);
    }
    
    // Destroy and recreate the edit control with new style
    DestroyWindow(g_hWndEdit);
    
    // Create new edit control with or without horizontal scroll
    DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | 
                  ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL;
    
    if (!g_bWordWrap) {
        // Add horizontal scroll when word wrap is off
        style |= WS_HSCROLL | ES_AUTOHSCROLL;
    }
    
    g_hWndEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS,
        L"",
        style,
        0, 0, 0, 0,
        g_hWndMain,
        (HMENU)IDC_RICHEDIT,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (g_hWndEdit) {
        // Set plain text mode
        SendMessage(g_hWndEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        
        // Set undo limit
        SendMessage(g_hWndEdit, EM_SETUNDOLIMIT, 100, 0);
        
        // Set event mask for notifications
        SendMessage(g_hWndEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);
        
        // Set large text limit (2GB)
        SendMessage(g_hWndEdit, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
        
        // Restore text
        if (pszText) {
            g_bSettingText = TRUE;
            SetWindowText(g_hWndEdit, pszText);
            g_bSettingText = FALSE;
            free(pszText);
        }
        
        // Restore selection
        SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crSel);
        
        // Trigger resize to position control correctly
        RECT rcClient;
        GetClientRect(g_hWndMain, &rcClient);
        SendMessage(g_hWndMain, WM_SIZE, 0, MAKELPARAM(rcClient.right, rcClient.bottom));
        
        // Update menu checkmark
        HMENU hMenu = GetMenu(g_hWndMain);
        CheckMenuItem(hMenu, ID_VIEW_WORDWRAP, g_bWordWrap ? MF_CHECKED : MF_UNCHECKED);
        
        // Set focus back to edit control
        SetFocus(g_hWndEdit);
    }
}

//============================================================================
// AboutDlgProc - About dialog procedure
//============================================================================
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /* lParam */)
{
    switch (msg) {
        case WM_INITDIALOG:
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
        _snwprintf(g_szFilterStatusBarText, 512, L"[%s]: %s", 
                   g_Filters[g_nCurrentFilter].szName, pszOutput);
        g_szFilterStatusBarText[511] = L'\0';  // Ensure null termination
        
        g_bFilterStatusBarActive = TRUE;
        UpdateStatusBar();
        
        // Start 30-second timer
        SetTimer(g_hWndMain, IDT_FILTER_STATUSBAR, 30000, NULL);
        
    } else {  // FILTER_DISPLAY_MESSAGEBOX
        // Show in message box
        WCHAR szTitle[128];
        _snwprintf(szTitle, 128, L"%s Result", g_Filters[g_nCurrentFilter].szName);
        szTitle[127] = L'\0';
        
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
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    
    // Replace .exe with .ini
    LPWSTR pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
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
        "; Display settings\r\n"
        "TabSize=8                     ; Tab size in spaces for column calculation (default: 8)\r\n"
        "\r\n"
        "; Filter System\r\n"
        "; Filters transform text using external commands\r\n"
        "; Action types: insert, display, clipboard, none, repl\r\n"
        "; Insert modes: replace, below, append\r\n"
        "; Display modes: statusbar, messagebox\r\n"
        "; Clipboard modes: copy, append\r\n"
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
        "Command=powershell -NoProfile -Command \"$i=1; $input -split '\\r?\\n' | ForEach-Object { \\\"{0,4}: {1}\\\" -f $i++,$_ } | Out-String\"\r\n"
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
        "Command=powershell -NoProfile -Command \"$text=$input; -join ($text.ToCharArray() | Sort-Object {Get-Random})\"\r\n"
        "Description=Reverses text and copies result to clipboard\r\n"
        "Description.cs=Obrátí text a zkopíruje výsledek do schránky\r\n"
        "Category=Clipboard\r\n"
        "Action=clipboard\r\n"
        "Clipboard=copy\r\n"
        "ContextMenu=0\r\n"
        "ContextMenuOrder=999\r\n"
        "\r\n"
        "; === NONE ACTION EXAMPLE ===\r\n"
        "; Filters that execute commands for side effects (no output used)\r\n"
        "\r\n"
        "[Filter8]\r\n"
        "Name=Speak Text\r\n"
        "Name.cs=Přečíst text\r\n"
        "Command=powershell -NoProfile -Command \"Add-Type -AssemblyName System.Speech; $synth=New-Object System.Speech.Synthesis.SpeechSynthesizer; $synth.Speak($input); $synth.Dispose()\"\r\n"
        "Description=Uses text-to-speech to read selected text aloud\r\n"
        "Description.cs=Použije převod textu na řeč pro přečtení vybraného textu nahlas\r\n"
        "Category=Utility\r\n"
        "Action=none\r\n"
        "ContextMenu=0\r\n"
        "ContextMenuOrder=999\r\n"
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
        "ContextMenuOrder=999\r\n";
    
    
    WriteFile(hFile, szDefaultINI, strlen(szDefaultINI), &dwWritten, NULL);
    CloseHandle(hFile);
}

//============================================================================
// LoadSettings - Load application settings from INI file
//============================================================================
void LoadSettings()
{
    // Get path to INI file (in same directory as executable)
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    
    // Replace .exe with .ini
    LPWSTR pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
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
}

//============================================================================
// LoadFilters - Load filter configurations from INI file
//============================================================================
void LoadFilters()
{
    // Get path to INI file (in same directory as executable)
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    
    // Replace .exe with .ini
    LPWSTR pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
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
                     g_Filters[i].szCategory, MAX_FILTER_CATEGORY, L"General");
        
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
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    
    LPWSTR pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
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
    
    // Use our custom WriteINIValue to save (works with UNC paths)
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
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    
    LPWSTR pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
    // Determine REPL filter name to save
    WCHAR szREPLFilterName[MAX_FILTER_NAME] = L"";
    if (g_nSelectedREPLFilter >= 0 && g_nSelectedREPLFilter < g_nFilterCount) {
        wcscpy_s(szREPLFilterName, MAX_FILTER_NAME, g_Filters[g_nSelectedREPLFilter].szName);
    }
    
    // Use our custom WriteINIValue to save (works with UNC paths)
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
        
        // Allow executing classic filters while in REPL
        if (g_nCurrentFilter >= 0 && 
            g_nCurrentFilter < g_nFilterCount &&
            g_Filters[g_nCurrentFilter].action != FILTER_ACTION_REPL) {
            enableExecute = TRUE;
        }
        
        // Can't start another REPL while one is running
        enableStartREPL = FALSE;
    } else {
        // Not in REPL mode
        
        // Enable Execute if a classic filter is selected
        if (g_nCurrentFilter >= 0 && 
            g_nCurrentFilter < g_nFilterCount &&
            g_Filters[g_nCurrentFilter].action != FILTER_ACTION_REPL) {
            enableExecute = TRUE;
        }
        
        // Enable Start Interactive if a REPL filter is selected
        if (g_nSelectedREPLFilter >= 0 && 
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
    
    DrawMenuBar(hwnd);
}

//============================================================================
// LoadMRU - Load Most Recently Used file list from INI
//============================================================================
void LoadMRU()
{
    // Get path to INI file
    WCHAR szIniPath[EXTENDED_PATH_MAX];
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    
    LPWSTR pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
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
    GetModuleFileName(NULL, szIniPath, EXTENDED_PATH_MAX);
    
    LPWSTR pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
    // Read entire INI file
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
    
    // Convert to wide string
    std::wstring wideData;
    if (!existingData.empty()) {
        int cchWide = MultiByteToWideChar(CP_UTF8, 0, existingData.c_str(), -1, NULL, 0);
        if (cchWide > 0) {
            WCHAR* pszWideData = (WCHAR*)malloc(cchWide * sizeof(WCHAR));
            if (pszWideData) {
                MultiByteToWideChar(CP_UTF8, 0, existingData.c_str(), -1, pszWideData, cchWide);
                wideData = pszWideData;
                free(pszWideData);
            }
        }
    }
    
    // Find and remove existing [MRU] section
    size_t mruSectionPos = wideData.find(L"[MRU]");
    if (mruSectionPos != std::wstring::npos) {
        // Find the start of the next section or end of file
        size_t nextSectionPos = wideData.find(L"\n[", mruSectionPos + 5);
        if (nextSectionPos != std::wstring::npos) {
            // Remove from [MRU] to start of next section
            wideData.erase(mruSectionPos, nextSectionPos - mruSectionPos + 1);
        } else {
            // Remove from [MRU] to end of file
            wideData.erase(mruSectionPos);
        }
    }
    
    // Build new [MRU] section
    std::wstring mruSection = L"[MRU]\r\n";
    for (int i = 0; i < g_nMRUCount; i++) {
        WCHAR szLine[EXTENDED_PATH_MAX + 32];
        _snwprintf(szLine, EXTENDED_PATH_MAX + 32, L"File%d=%s\r\n", i + 1, g_MRU[i]);
        mruSection += szLine;
    }
    
    // Append new [MRU] section at the end
    if (!wideData.empty() && wideData[wideData.length() - 1] != L'\n') {
        wideData += L"\r\n";
    }
    wideData += mruSection;
    
    // Convert back to UTF-8
    int cbUTF8 = WideCharToMultiByte(CP_UTF8, 0, wideData.c_str(), -1, NULL, 0, NULL, NULL);
    if (cbUTF8 <= 0) return;
    
    char* pszUTF8 = (char*)malloc(cbUTF8);
    if (!pszUTF8) return;
    
    WideCharToMultiByte(CP_UTF8, 0, wideData.c_str(), -1, pszUTF8, cbUTF8, NULL, NULL);
    
    // Write to file
    hFile = CreateFile(szIniPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        free(pszUTF8);
        return;
    }
    
    DWORD dwWritten;
    WriteFile(hFile, pszUTF8, strlen(pszUTF8), &dwWritten, NULL);
    CloseHandle(hFile);
    free(pszUTF8);
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
        
        // Group filters by category
        for (int i = 0; i < g_nFilterCount; i++) {
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
    if (!g_bAutosaveEnabled || !g_bModified || g_szFileName[0] == L'\0') {
        return;
    }
    
    // Save the file silently
    if (SaveTextFile(g_szFileName)) {
        // Update status briefly to show autosave happened
        WCHAR szOldStatus[512];
        SendMessage(g_hWndStatus, SB_GETTEXT, 0, (LPARAM)szOldStatus);
        SendMessage(g_hWndStatus, SB_SETTEXT, 0, (LPARAM)L"[Autosaved]");
        
        // Restore original status after 1 second
        Sleep(1000);  // Brief flash
        SendMessage(g_hWndStatus, SB_SETTEXT, 0, (LPARAM)szOldStatus);
    }
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
        MessageBox(g_hWndMain, L"Failed to create stdout pipe", L"Error", MB_ICONERROR);
        return;
    }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    
    // Create stdin pipe
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        MessageBox(g_hWndMain, L"Failed to create stdin pipe", L"Error", MB_ICONERROR);
        return;
    }
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    
    // Create stderr pipe
    if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        MessageBox(g_hWndMain, L"Failed to create stderr pipe", L"Error", MB_ICONERROR);
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
        
        WCHAR szMsg[256];
        swprintf(szMsg, 256, L"Failed to start interactive filter: %s", g_Filters[filterIndex].szName);
        MessageBox(g_hWndMain, szMsg, L"Error", MB_ICONERROR);
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
        MessageBox(g_hWndMain, L"Failed to create stdout reader thread", L"Error", MB_ICONERROR);
        return;
    }
    
    g_hREPLStderrThread = CreateThread(NULL, 0, REPLStderrThread, NULL, 0, &g_dwREPLStderrThreadId);
    if (g_hREPLStderrThread == NULL) {
        ExitREPLMode();
        MessageBox(g_hWndMain, L"Failed to create stderr reader thread", L"Error", MB_ICONERROR);
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
