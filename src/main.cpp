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
const UINT_PTR IDT_AUTOSAVE = 1;             // Timer ID for autosave
const UINT_PTR IDT_FILTER_STATUSBAR = 2;     // Timer ID for filter status bar display

// Accessibility settings
BOOL g_bShowMenuDescriptions = TRUE;         // Show filter descriptions in menus (for accessibility)

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
    FILTER_ACTION_NONE = 3
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
};

FilterInfo g_Filters[MAX_FILTERS];
int g_nFilterCount = 0;
int g_nCurrentFilter = -1;  // -1 = no filter selected, 0-99 = filter index

// Status bar filter display
WCHAR g_szFilterStatusBarText[512] = L"";
BOOL g_bFilterStatusBarActive = FALSE;

// MRU list
WCHAR g_MRU[MAX_MRU][EXTENDED_PATH_MAX];
int g_nMRUCount = 0;

//============================================================================
// Function Declarations
//============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /* lParam */);
BOOL InitRichEditLibrary();
HWND CreateRichEditControl(HWND hwndParent);
HWND CreateStatusBar(HWND hwndParent);
void UpdateStatusBar();
void UpdateTitle(HWND hwnd = NULL);
void UpdateMenuUndoRedo(HMENU hMenu);
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
void UpdateFilterDisplay();
void BuildFilterMenu(HWND hwnd);
void DoAutosave();
void StartAutosaveTimer(HWND hwnd);
void LoadMRU();
void SaveMRU();
void AddToMRU(LPCWSTR pszFilePath);
void UpdateMRUMenu(HWND hwnd);
void GetSystemLanguageCode(LPWSTR pszLangCode, int cchLangCode);

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
    
    // If a filename was passed as argument, store it for later
    if (argc > 1 && argv[1][0] != L'\0') {
        wcscpy_s(szCommandLineFile, EXTENDED_PATH_MAX, argv[1]);
    }
    
    if (argv) {
        LocalFree(argv);
    }
    
    // Load RichEdit library
    if (!InitRichEditLibrary()) {
        WCHAR szError[256], szTitle[64];
        LoadStringResource(IDS_RICHEDIT_LOAD_FAILED, szError, 256);
        LoadStringResource(IDS_ERROR, szTitle, 64);
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
    
    // Load file from command line if provided
    if (szCommandLineFile[0] != L'\0') {
        LoadTextFile(szCommandLineFile);
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
            if (g_bAutosaveEnabled && g_bAutosaveOnFocusLoss) {
                DoAutosave();
            }
            return 0;
            
        case WM_TIMER:
            // Handle autosave timer
            if (wParam == IDT_AUTOSAVE) {
                OutputDebugString(L"WM_TIMER: Autosave timer fired\n");
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
                    UpdateTitle();
                }
                // No need to track typing - RichEdit reports this via EM_GETUNDONAME
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
                    ExecuteFilter();
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
                        // Handle filter selection from Tools menu
                        else if (wmId >= ID_TOOLS_FILTER_BASE && wmId < ID_TOOLS_FILTER_BASE + 100) {
                            int filterIdx = wmId - ID_TOOLS_FILTER_BASE;
                            if (filterIdx >= 0 && filterIdx < g_nFilterCount) {
                                g_nCurrentFilter = filterIdx;
                                SaveCurrentFilter();
                                BuildFilterMenu(hwnd);
                                UpdateFilterDisplay();
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
            
        case WM_CLOSE:
            // Check for unsaved changes
            if (!PromptSaveChanges()) {
                return 0; // Cancel close
            }
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            // Kill timers
            KillTimer(hwnd, IDT_AUTOSAVE);
            KillTimer(hwnd, IDT_FILTER_STATUSBAR);
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
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
        // Set plain text mode
        SendMessage(hwndEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        
        // Set undo limit
        SendMessage(hwndEdit, EM_SETUNDOLIMIT, 100, 0);
        
        // Set event mask for notifications
        SendMessage(hwndEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);
        
        // Set large text limit (2GB)
        SendMessage(hwndEdit, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
        
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
        visualCol = cr.cpMin - currentLineStart + 1;
        
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
                    
                    // Calculate column: how many buffer positions from line start to end
                    physicalCol = bufferPos - physicalLineStart + 1;
                }
                
                free(buffer);
            }
        }
        
    } else {
        // When word wrap is OFF, visual = physical (no soft wraps)
        visualLine = (int)SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin) + 1;
        int lineStart = (int)SendMessage(g_hWndEdit, EM_LINEINDEX, visualLine - 1, 0);
        visualCol = cr.cpMin - lineStart + 1;
        
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
    WCHAR szTitle[MAX_PATH + 50];
    WCHAR szUntitled[64];
    
    // Use provided hwnd or fall back to g_hWndMain
    HWND targetWnd = hwnd ? hwnd : g_hWndMain;
    if (!targetWnd) return;
    
    LoadStringResource(IDS_UNTITLED, szUntitled, 64);
    
    if (g_szFileTitle[0]) {
        _snwprintf(szTitle, MAX_PATH + 50, L"%s%s - RichEditor",
                   g_bModified ? L"*" : L"", g_szFileTitle);
    } else {
        _snwprintf(szTitle, MAX_PATH + 50, L"%s%s - RichEditor",
                   g_bModified ? L"*" : L"", szUntitled);
    }
    
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
    
    // Don't prompt if document is empty (no text)
    int len = GetWindowTextLength(g_hWndEdit);
    if (len == 0) {
        return TRUE;
    }
    
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
    SendMessage(g_hWndEdit, WM_PASTE, 0, 0);
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
// PHASE 2: Filter System Stubs
//============================================================================

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
        "; Filter System\r\n"
        "; Filters transform text using external commands\r\n"
        "; Action types: insert, display, clipboard, none\r\n"
        "; Insert modes: replace, below, append\r\n"
        "; Display modes: statusbar, messagebox\r\n"
        "; Clipboard modes: copy, append\r\n"
        "; ContextMenu: 1=show in right-click menu, 0=Tools menu only\r\n"
        "; ContextMenuOrder: Sort order in context menu (lower numbers first)\r\n"
        "\r\n"
        "[Filters]\r\n"
        "Count=8\r\n"
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
    
    // Load last selected filter from settings (by name, defaulting to None)
    WCHAR szLastFilter[MAX_FILTER_NAME] = L"";
    ReadINIValue(szIniPath, L"Settings", L"CurrentFilter", szLastFilter, MAX_FILTER_NAME, L"");
    
    // Try to find filter by name
    g_nCurrentFilter = -1; // Default to None
    if (szLastFilter[0] != L'\0') {
        for (int i = 0; i < g_nFilterCount; i++) {
            if (wcscmp(g_Filters[i].szName, szLastFilter) == 0) {
                g_nCurrentFilter = i;
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
    
    // Determine filter name to save
    WCHAR szFilterName[MAX_FILTER_NAME] = L"";
    if (g_nCurrentFilter >= 0 && g_nCurrentFilter < g_nFilterCount) {
        wcscpy_s(szFilterName, MAX_FILTER_NAME, g_Filters[g_nCurrentFilter].szName);
    }
    // If g_nCurrentFilter is -1 or invalid, szFilterName stays empty (means "None")
    
    // Use our custom WriteINIValue to save (works with UNC paths)
    WriteINIValue(szIniPath, L"Settings", L"CurrentFilter", szFilterName);
}

//============================================================================
// UpdateFilterDisplay - Update status bar with current filter info
//============================================================================
void UpdateFilterDisplay()
{
    if (!g_hWndStatus) return;
    
    WCHAR szFilter[128];
    
    if (g_nCurrentFilter >= 0 && g_nCurrentFilter < g_nFilterCount) {
        _snwprintf(szFilter, 128, L"[Filter: %s]", g_Filters[g_nCurrentFilter].szLocalizedName);
    } else {
        wcscpy(szFilter, L"[Filter: None]");
    }
    
    SendMessage(g_hWndStatus, SB_SETTEXT, (WPARAM)1, (LPARAM)szFilter);
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
                if (filterIndex == g_nCurrentFilter) {
                    flags |= MF_CHECKED;
                }
                
                // Build accessible menu text: "Name: Description" (using localized strings)
                WCHAR szMenuText[MAX_FILTER_NAME + MAX_FILTER_DESC + 4];
                if (g_bShowMenuDescriptions && g_Filters[filterIndex].szLocalizedDescription[0] != L'\0') {
                    // Build menu text: "LocalizedName: LocalizedDescription"
                    wcscpy(szMenuText, g_Filters[filterIndex].szLocalizedName);
                    wcscat(szMenuText, L": ");
                    wcscat(szMenuText, g_Filters[filterIndex].szLocalizedDescription);
                } else {
                    // Just show the localized name
                    wcscpy(szMenuText, g_Filters[filterIndex].szLocalizedName);
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
        UINT_PTR result = SetTimer(hwnd, IDT_AUTOSAVE, interval, NULL);
        
        WCHAR debug[256];
        _snwprintf(debug, 256, L"StartAutosaveTimer: hwnd=%p, SetTimer called with interval=%d ms, result=%p\n", 
                   hwnd, interval, (void*)result);
        OutputDebugString(debug);
    } else {
        OutputDebugString(L"StartAutosaveTimer: Timer not started (disabled or interval=0)\n");
    }
}

//============================================================================
// DoAutosave - Perform autosave if file has been modified and has a name
//============================================================================
void DoAutosave()
{
    OutputDebugString(L"DoAutosave: Called\n");
    
    // Only autosave if:
    // 1. Autosave is enabled
    // 2. Document has been modified
    // 3. Document has a filename (not "Untitled")
    if (!g_bAutosaveEnabled || !g_bModified || g_szFileName[0] == L'\0') {
        WCHAR debug[256];
        _snwprintf(debug, 256, L"DoAutosave: Skipped (enabled=%d, modified=%d, hasFilename=%d)\n",
                   g_bAutosaveEnabled, g_bModified, g_szFileName[0] != L'\0');
        OutputDebugString(debug);
        return;
    }
    
    OutputDebugString(L"DoAutosave: Saving...\n");
    
    // Save the file silently
    if (SaveTextFile(g_szFileName)) {
        // Update status briefly to show autosave happened
        WCHAR szOldStatus[512];
        SendMessage(g_hWndStatus, SB_GETTEXT, 0, (LPARAM)szOldStatus);
        SendMessage(g_hWndStatus, SB_SETTEXT, 0, (LPARAM)L"[Autosaved]");
        
        // Restore original status after 1 second
        Sleep(100);  // Brief flash
        SendMessage(g_hWndStatus, SB_SETTEXT, 0, (LPARAM)szOldStatus);
    }
}
