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
HWND g_hWndMain = NULL;           // Main window handle
HWND g_hWndEdit = NULL;           // RichEdit control handle (to be added)
HWND g_hWndStatus = NULL;         // Status bar handle (to be added)
WCHAR g_szFileName[MAX_PATH];     // Current file path
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

//============================================================================
// Filter System (Phase 2)
//============================================================================
#define MAX_FILTERS 100
#define MAX_FILTER_NAME 64
#define MAX_FILTER_COMMAND 512
#define MAX_FILTER_DESC 256
#define MAX_FILTER_MODE 16
#define MAX_FILTER_CATEGORY 32

enum FilterOutputMode {
    FILTER_MODE_BELOW = 0,    // Insert output below input (default)
    FILTER_MODE_REPLACE = 1,  // Replace input with output
    FILTER_MODE_APPEND = 2    // Append output after input on same line
};

struct FilterInfo {
    WCHAR szName[MAX_FILTER_NAME];
    WCHAR szCommand[MAX_FILTER_COMMAND];
    WCHAR szDescription[MAX_FILTER_DESC];
    WCHAR szCategory[MAX_FILTER_CATEGORY];
    FilterOutputMode mode;
};

FilterInfo g_Filters[MAX_FILTERS];
int g_nFilterCount = 0;
int g_nCurrentFilter = -1;  // -1 = no filter selected, 0-99 = filter index

//============================================================================
// Function Declarations
//============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /* lParam */);
BOOL InitRichEditLibrary();
HWND CreateRichEditControl(HWND hwndParent);
HWND CreateStatusBar(HWND hwndParent);
void UpdateStatusBar();
void UpdateTitle();
void LoadStringResource(UINT uID, LPWSTR lpBuffer, int cchBufferMax);
LPWSTR UTF8ToUTF16(LPCSTR pszUTF8);
LPSTR UTF16ToUTF8(LPCWSTR pszUTF16);
BOOL LoadTextFile(LPCWSTR pszFileName);
BOOL SaveTextFile(LPCWSTR pszFileName);
void GetDocumentsPath(LPWSTR pszPath, DWORD cchPath);
void ShowError(LPCWSTR pszMessage, DWORD dwError);
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
void UpdateFilterDisplay();
void BuildFilterMenu(HWND hwnd);
void DoAutosave();
void StartAutosaveTimer(HWND hwnd);

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
        L"Untitled - RichEditor",
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
            
            // Set initial word wrap menu checkmark
            HMENU hMenu = GetMenu(hwnd);
            CheckMenuItem(hMenu, ID_VIEW_WORDWRAP, g_bWordWrap ? MF_CHECKED : MF_UNCHECKED);
            
            // Start autosave timer if enabled
            StartAutosaveTimer(hwnd);
            
            return 0;
        }
            
        case WM_SIZE:
            // Resize status bar
            if (g_hWndStatus) {
                SendMessage(g_hWndStatus, WM_SIZE, 0, 0);
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
            return 0;
            
        case WM_COMMAND:
            // Handle edit control notifications
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_hWndEdit) {
                if (!g_bSettingText && !g_bModified) {
                    g_bModified = TRUE;
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
                        if (wmId >= ID_TOOLS_FILTER_BASE && wmId < ID_TOOLS_FILTER_BASE + 100) {
                            int filterIdx = wmId - ID_TOOLS_FILTER_BASE;
                            if (filterIdx >= 0 && filterIdx < g_nFilterCount) {
                                g_nCurrentFilter = filterIdx;
                                BuildFilterMenu(hwnd);
                                UpdateFilterDisplay();
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
            
        case WM_CLOSE:
            // Check for unsaved changes
            if (!PromptSaveChanges()) {
                return 0; // Cancel close
            }
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            // Kill autosave timer
            KillTimer(hwnd, IDT_AUTOSAVE);
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
        // Set parts (will be updated dynamically in UpdateStatusBar)
        int parts[] = {-1}; // Single part initially
        SendMessage(hwndStatus, SB_SETPARTS, 1, (LPARAM)parts);
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
    
    // Get character at cursor
    WCHAR charAtCursor = 0;
    WCHAR charInfo[64] = L"";
    int textLen = GetWindowTextLength(g_hWndEdit);
    
    if (cr.cpMin < textLen) {
        // Get the character at cursor position
        TEXTRANGE tr;
        WCHAR buffer[2] = {0};
        tr.chrg.cpMin = cr.cpMin;
        tr.chrg.cpMax = cr.cpMin + 1;
        tr.lpstrText = buffer;
        SendMessage(g_hWndEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
        charAtCursor = buffer[0];
        
        // Format character info (dec and hex)
        if (charAtCursor >= 32 && charAtCursor != 127) {
            // Printable character
            _snwprintf(charInfo, 64, L"Char: '%lc' (Dec: %d, Hex: 0x%04X)",
                       charAtCursor, (int)charAtCursor, (unsigned int)charAtCursor);
        } else {
            // Control character or non-printable
            _snwprintf(charInfo, 64, L"Char: (Dec: %d, Hex: 0x%04X)",
                       (int)charAtCursor, (unsigned int)charAtCursor);
        }
    } else {
        // Cursor is at end of file or empty file
        wcscpy(charInfo, L"Char: EOF");
    }
    
    // Format position string
    WCHAR posInfo[128];
    if (g_bWordWrap) {
        // When word wrap is on, always show both visual and physical positions
        // visualLine/visualCol: includes soft wraps (displayed lines)
        // physicalLine/physicalCol: count only hard line breaks
        _snwprintf(posInfo, 128, L"Ln %d, Col %d / %d,%d",
                   visualLine, visualCol, physicalLine, physicalCol);
    } else {
        // When word wrap is off, show only one position
        _snwprintf(posInfo, 128, L"Ln %d, Col %d",
                   visualLine, visualCol);
    }
    
    // Format status text
    WCHAR szStatus[512];
    if (g_szFileName[0]) {
        _snwprintf(szStatus, 512, L"%s    %s    %s    [Filter: None]",
                   g_szFileTitle, posInfo, charInfo);
    } else {
        _snwprintf(szStatus, 512, L"Untitled    %s    %s    [Filter: None]",
                   posInfo, charInfo);
    }
    
    SendMessage(g_hWndStatus, SB_SETTEXT, 0, (LPARAM)szStatus);
}

//============================================================================
// UpdateTitle - Update window title with filename and modified state
//============================================================================
void UpdateTitle()
{
    WCHAR szTitle[MAX_PATH + 50];
    
    if (g_szFileTitle[0]) {
        _snwprintf(szTitle, MAX_PATH + 50, L"%s%s - RichEditor",
                   g_bModified ? L"*" : L"", g_szFileTitle);
    } else {
        _snwprintf(szTitle, MAX_PATH + 50, L"%sUntitled - RichEditor",
                   g_bModified ? L"*" : L"");
    }
    
    SetWindowText(g_hWndMain, szTitle);
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
        ShowError(L"Could not open file", GetLastError());
        return FALSE;
    }
    
    // Get file size
    DWORD dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        ShowError(L"Could not get file size", GetLastError());
        return FALSE;
    }
    
    // Allocate buffer for UTF-8 data
    LPSTR pszUTF8 = (LPSTR)malloc(dwFileSize + 1);
    if (!pszUTF8) {
        CloseHandle(hFile);
        ShowError(L"Out of memory", 0);
        return FALSE;
    }
    
    // Read file
    DWORD dwBytesRead;
    if (!ReadFile(hFile, pszUTF8, dwFileSize, &dwBytesRead, NULL)) {
        free(pszUTF8);
        CloseHandle(hFile);
        ShowError(L"Could not read file", GetLastError());
        return FALSE;
    }
    pszUTF8[dwBytesRead] = '\0';
    CloseHandle(hFile);
    
    // Convert to UTF-16
    LPWSTR pszUTF16 = UTF8ToUTF16(pszUTF8);
    free(pszUTF8);
    
    if (!pszUTF16) {
        ShowError(L"Could not convert file encoding", 0);
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
        ShowError(L"Out of memory", 0);
        return FALSE;
    }
    
    // Get text from RichEdit control
    GetWindowText(g_hWndEdit, pszUTF16, cchText + 1);
    
    // Convert to UTF-8
    LPSTR pszUTF8 = UTF16ToUTF8(pszUTF16);
    free(pszUTF16);
    
    if (!pszUTF8) {
        ShowError(L"Could not convert text encoding", 0);
        return FALSE;
    }
    
    // Create file
    HANDLE hFile = CreateFile(pszFileName, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        free(pszUTF8);
        ShowError(L"Could not create file", GetLastError());
        return FALSE;
    }
    
    // Write UTF-8 data (without BOM)
    DWORD dwBytesToWrite = (DWORD)strlen(pszUTF8);
    DWORD dwBytesWritten;
    if (!WriteFile(hFile, pszUTF8, dwBytesToWrite, &dwBytesWritten, NULL)) {
        free(pszUTF8);
        CloseHandle(hFile);
        ShowError(L"Could not write file", GetLastError());
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
// ShowError - Display error message with optional Win32 error code
//============================================================================
void ShowError(LPCWSTR pszMessage, DWORD dwError)
{
    WCHAR szError[512];
    
    if (dwError != 0) {
        // Format Win32 error message
        WCHAR szErrorMsg[256];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     NULL, dwError, 0, szErrorMsg, 256, NULL);
        
        _snwprintf(szError, 512, L"%s\n\nError: %s", pszMessage, szErrorMsg);
        
        // Also output to debugger
        OutputDebugString(L"RichEditor Error: ");
        OutputDebugString(pszMessage);
        OutputDebugString(L" - ");
        OutputDebugString(szErrorMsg);
        OutputDebugString(L"\n");
    } else {
        wcscpy_s(szError, 512, pszMessage);
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
    WCHAR szFile[MAX_PATH] = L"";
    WCHAR szInitialDir[MAX_PATH];
    
    GetDocumentsPath(szInitialDir, MAX_PATH);
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = g_hWndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
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
    WCHAR szFile[MAX_PATH] = L"";
    WCHAR szInitialDir[MAX_PATH];
    
    // Copy current filename if exists
    if (g_szFileName[0]) {
        wcscpy_s(szFile, MAX_PATH, g_szFileName);
    }
    
    GetDocumentsPath(szInitialDir, MAX_PATH);
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = g_hWndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
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
    WCHAR szPrompt[MAX_PATH + 100];
    WCHAR szTemplate[256];
    
    LoadStringResource(IDS_SAVE_CHANGES_PROMPT, szTemplate, 256);
    
    if (g_szFileTitle[0]) {
        _snwprintf(szPrompt, MAX_PATH + 100, szTemplate, g_szFileTitle);
    } else {
        _snwprintf(szPrompt, MAX_PATH + 100, szTemplate, L"Untitled");
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
// ExecuteFilter - Execute current filter on selected text or current line
//============================================================================
void ExecuteFilter()
{
    // Check if a filter is selected
    if (g_nCurrentFilter < 0 || g_nCurrentFilter >= g_nFilterCount) {
        MessageBox(g_hWndMain,
                   L"No filter selected.\n\nUse Tools → Select Filter to choose a filter.",
                   L"Filter Execution",
                   MB_ICONEXCLAMATION);
        return;
    }
    
    // Get selected text range
    CHARRANGE crSel;
    SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&crSel);
    
    // If no selection, select current line
    if (crSel.cpMin == crSel.cpMax) {
        // Get line number
        LONG lineNum = SendMessage(g_hWndEdit, EM_LINEFROMCHAR, crSel.cpMin, 0);
        // Get line start and end
        LONG lineStart = SendMessage(g_hWndEdit, EM_LINEINDEX, lineNum, 0);
        LONG lineEnd = SendMessage(g_hWndEdit, EM_LINEINDEX, lineNum + 1, 0);
        if (lineEnd == -1) {
            // Last line - get text length
            lineEnd = GetWindowTextLength(g_hWndEdit);
        }
        crSel.cpMin = lineStart;
        crSel.cpMax = lineEnd;
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
        free(pszInputUTF8);
        return;
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
    WCHAR szCommand[MAX_FILTER_COMMAND + 100];
    wcscpy(szCommand, g_Filters[g_nCurrentFilter].szCommand);
    
    if (!CreateProcess(NULL, szCommand, NULL, NULL, TRUE, 
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WCHAR szError[512], szTitle[64];
        swprintf(szError, 512, L"Failed to execute filter:\n%s\n\nError code: %d",
                 g_Filters[g_nCurrentFilter].szName, GetLastError());
        LoadStringResource(IDS_FILTER_EXEC_ERROR, szTitle, 64);
        MessageBox(g_hWndMain, szError, szTitle, MB_ICONERROR);
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrRead);
        CloseHandle(hStderrWrite);
        free(pszInputUTF8);
        return;
    }
    
    // Close unused pipe ends
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
    
    // Write input to process stdin
    DWORD dwWritten;
    WriteFile(hStdinWrite, pszInputUTF8, strlen(pszInputUTF8), &dwWritten, NULL);
    CloseHandle(hStdinWrite);
    free(pszInputUTF8);
    
    // Read stdout
    char bufferOut[4096];
    DWORD dwRead;
    std::string outputData;
    while (ReadFile(hStdoutRead, bufferOut, sizeof(bufferOut) - 1, &dwRead, NULL) && dwRead > 0) {
        bufferOut[dwRead] = '\0';
        outputData.append(bufferOut, dwRead);
    }
    CloseHandle(hStdoutRead);
    
    // Read stderr
    char bufferErr[4096];
    std::string errorData;
    while (ReadFile(hStderrRead, bufferErr, sizeof(bufferErr) - 1, &dwRead, NULL) && dwRead > 0) {
        bufferErr[dwRead] = '\0';
        errorData.append(bufferErr, dwRead);
    }
    CloseHandle(hStderrRead);
    
    // Wait for process to complete (with timeout)
    WaitForSingleObject(pi.hProcess, 30000);
    
    // Get exit code
    DWORD dwExitCode;
    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Show errors if any
    if (!errorData.empty()) {
        LPWSTR pszError = UTF8ToUTF16(errorData.c_str());
        if (pszError) {
            WCHAR szMsg[2048], szTitle[64];
            swprintf(szMsg, 2048, L"Filter stderr output:\n\n%s", pszError);
            LoadStringResource(IDS_FILTER_ERROR, szTitle, 64);
            MessageBox(g_hWndMain, szMsg, szTitle, MB_ICONWARNING);
            free(pszError);
        }
    }
    
    // Insert output after selection if we have output
    if (!outputData.empty()) {
        LPWSTR pszOutput = UTF8ToUTF16(outputData.c_str());
        if (pszOutput) {
            // Get the output mode for current filter
            FilterOutputMode mode = g_Filters[g_nCurrentFilter].mode;
            
            if (mode == FILTER_MODE_REPLACE) {
                // Replace: Just replace the selection with output
                SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszOutput);
                
            } else if (mode == FILTER_MODE_APPEND) {
                // Append: Move to end of selection and append (no newline)
                crSel.cpMin = crSel.cpMax;
                SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&crSel);
                SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszOutput);
                
            } else {  // FILTER_MODE_BELOW (default)
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
    } else if (dwExitCode != 0) {
        WCHAR szMsg[256], szTitle[64];
        swprintf(szMsg, 256, L"Filter completed with exit code: %d\nNo output produced.", dwExitCode);
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
    WCHAR szIniPath[MAX_PATH];
    GetModuleFileName(NULL, szIniPath, MAX_PATH);
    
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
        "[Filters]\r\n"
        "Count=3\r\n"
        "\r\n"
        "[Filter1]\r\n"
        "Name=Uppercase\r\n"
        "Command=powershell -NoProfile -Command \"$input | ForEach-Object { $_.ToUpper() }\"\r\n"
        "Description=Converts text to UPPERCASE\r\n"
        "Category=Transform\r\n"
        "Mode=Replace\r\n"
        "\r\n"
        "[Filter2]\r\n"
        "Name=Lowercase\r\n"
        "Command=powershell -NoProfile -Command \"$input | ForEach-Object { $_.ToLower() }\"\r\n"
        "Description=Converts text to lowercase\r\n"
        "Category=Transform\r\n"
        "Mode=Replace\r\n"
        "\r\n"
        "[Filter3]\r\n"
        "Name=Line Count\r\n"
        "Command=powershell -NoProfile -Command \"($input | Measure-Object -Line).Lines\"\r\n"
        "Description=Counts the number of lines\r\n"
        "Category=Statistics\r\n"
        "Mode=Append\r\n";
    
    WriteFile(hFile, szDefaultINI, strlen(szDefaultINI), &dwWritten, NULL);
    CloseHandle(hFile);
}

//============================================================================
// LoadSettings - Load application settings from INI file
//============================================================================
void LoadSettings()
{
    // Get path to INI file (in same directory as executable)
    WCHAR szIniPath[MAX_PATH];
    GetModuleFileName(NULL, szIniPath, MAX_PATH);
    
    // Replace .exe with .ini
    LPWSTR pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
    // Load settings from [Settings] section
    g_bWordWrap = GetPrivateProfileInt(L"Settings", L"WordWrap", 1, szIniPath);
    g_bAutosaveEnabled = GetPrivateProfileInt(L"Settings", L"AutosaveEnabled", 1, szIniPath);
    g_nAutosaveIntervalMinutes = GetPrivateProfileInt(L"Settings", L"AutosaveIntervalMinutes", 1, szIniPath);
    g_bAutosaveOnFocusLoss = GetPrivateProfileInt(L"Settings", L"AutosaveOnFocusLoss", 1, szIniPath);
}

//============================================================================
// LoadFilters - Load filter configurations from INI file
//============================================================================
void LoadFilters()
{
    // Get path to INI file (in same directory as executable)
    WCHAR szIniPath[MAX_PATH];
    GetModuleFileName(NULL, szIniPath, MAX_PATH);
    
    // Replace .exe with .ini
    LPWSTR pszExt = wcsrchr(szIniPath, L'.');
    if (pszExt) {
        wcscpy(pszExt, L".ini");
    }
    
    // Read filter count
    g_nFilterCount = GetPrivateProfileInt(L"Filters", L"Count", 0, szIniPath);
    if (g_nFilterCount > MAX_FILTERS) {
        g_nFilterCount = MAX_FILTERS;
    }
    
    // Load each filter
    for (int i = 0; i < g_nFilterCount; i++) {
        WCHAR szSection[32];
        swprintf(szSection, 32, L"Filter%d", i + 1);
        
        GetPrivateProfileString(szSection, L"Name", L"", 
                                g_Filters[i].szName, MAX_FILTER_NAME, szIniPath);
        GetPrivateProfileString(szSection, L"Command", L"", 
                                g_Filters[i].szCommand, MAX_FILTER_COMMAND, szIniPath);
        GetPrivateProfileString(szSection, L"Description", L"", 
                                g_Filters[i].szDescription, MAX_FILTER_DESC, szIniPath);
        GetPrivateProfileString(szSection, L"Category", L"General", 
                                g_Filters[i].szCategory, MAX_FILTER_CATEGORY, szIniPath);
        
        // Read output mode (Below, Replace, Append)
        WCHAR szMode[MAX_FILTER_MODE];
        GetPrivateProfileString(szSection, L"Mode", L"Below", 
                                szMode, MAX_FILTER_MODE, szIniPath);
        
        // Parse mode string
        if (_wcsicmp(szMode, L"Replace") == 0) {
            g_Filters[i].mode = FILTER_MODE_REPLACE;
        } else if (_wcsicmp(szMode, L"Append") == 0) {
            g_Filters[i].mode = FILTER_MODE_APPEND;
        } else {
            g_Filters[i].mode = FILTER_MODE_BELOW;  // Default
        }
    }
    
    // Set current filter to first one if available
    if (g_nFilterCount > 0) {
        g_nCurrentFilter = 0;
    }
}

//============================================================================
// UpdateFilterDisplay - Update status bar with current filter info
//============================================================================
void UpdateFilterDisplay()
{
    WCHAR szFilter[128];
    
    if (g_nCurrentFilter >= 0 && g_nCurrentFilter < g_nFilterCount) {
        swprintf(szFilter, 128, L"[Filter: %s]", g_Filters[g_nCurrentFilter].szName);
    } else {
        wcscpy(szFilter, L"[Filter: None]");
    }
    
    SendMessage(g_hWndStatus, SB_SETTEXT, 3, (LPARAM)szFilter);
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
        AppendMenu(hFilterMenu, MF_STRING | MF_GRAYED, ID_TOOLS_FILTER_BASE, L"(No filters configured)");
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
                AppendMenu(hCategoryMenu, flags, ID_TOOLS_FILTER_BASE + filterIndex, 
                           g_Filters[filterIndex].szName);
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
