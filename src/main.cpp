//============================================================================
// RichEditor - A lightweight text editor using RichEdit control
// Phase 1: Basic text editing with UTF-8 support
//============================================================================

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
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
HMODULE g_hRichEditLib = NULL;    // RichEdit DLL handle

//============================================================================
// Function Declarations
//============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL InitRichEditLibrary();
HWND CreateRichEditControl(HWND hwndParent);
HWND CreateStatusBar(HWND hwndParent);
void UpdateStatusBar();
void UpdateTitle();
LPWSTR UTF8ToUTF16(LPCSTR pszUTF8);
LPSTR UTF16ToUTF8(LPCWSTR pszUTF16);
BOOL LoadTextFile(LPCWSTR pszFileName);
BOOL SaveTextFile(LPCWSTR pszFileName);
void GetDocumentsPath(LPWSTR pszPath, DWORD cchPath);
void ShowError(LPCWSTR pszMessage, DWORD dwError);

//============================================================================
// WinMain - Entry Point
//============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow)
{
    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
    
    // Load RichEdit library
    if (!InitRichEditLibrary()) {
        MessageBox(NULL, L"Failed to load RichEdit library (Msftedit.dll)", L"Error", MB_ICONERROR);
        return 1;
    }
    
    // Register window class
    WNDCLASSEX wc = {0};
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
        MessageBox(NULL, L"Window registration failed", L"Error", MB_ICONERROR);
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
        MessageBox(NULL, L"Window creation failed", L"Error", MB_ICONERROR);
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
            // Initialize state
            g_szFileName[0] = L'\0';
            g_szFileTitle[0] = L'\0';
            g_bModified = FALSE;
            
            // Create RichEdit control
            g_hWndEdit = CreateRichEditControl(hwnd);
            if (!g_hWndEdit) {
                MessageBox(hwnd, L"Failed to create RichEdit control", L"Error", MB_ICONERROR);
                return -1;
            }
            
            // Create status bar
            g_hWndStatus = CreateStatusBar(hwnd);
            
            // Update status bar
            UpdateStatusBar();
            
            // TODO: Load filters
            
            return 0;
            
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
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                // File menu
                case ID_FILE_NEW:
                    MessageBox(hwnd, L"New - Not implemented yet", L"Info", MB_OK);
                    break;
                case ID_FILE_OPEN:
                    MessageBox(hwnd, L"Open - Not implemented yet", L"Info", MB_OK);
                    break;
                case ID_FILE_SAVE:
                    MessageBox(hwnd, L"Save - Not implemented yet", L"Info", MB_OK);
                    break;
                case ID_FILE_SAVEAS:
                    MessageBox(hwnd, L"Save As - Not implemented yet", L"Info", MB_OK);
                    break;
                case ID_FILE_EXIT:
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                    break;
                    
                // Edit menu
                case ID_EDIT_UNDO:
                case ID_EDIT_REDO:
                case ID_EDIT_CUT:
                case ID_EDIT_COPY:
                case ID_EDIT_PASTE:
                case ID_EDIT_SELECTALL:
                    MessageBox(hwnd, L"Edit operation - Not implemented yet", L"Info", MB_OK);
                    break;
                
                // Tools menu
                case ID_TOOLS_EXECUTEFILTER:
                    MessageBox(hwnd, L"Filter execution - Not implemented yet", L"Info", MB_OK);
                    break;
                    
                // Help menu
                case ID_HELP_ABOUT:
                    MessageBox(hwnd, L"About dialog - Not implemented yet", L"Info", MB_OK);
                    break;
            }
            return 0;
            
        case WM_NOTIFY:
            // Handle RichEdit notifications
            if (((LPNMHDR)lParam)->hwndFrom == g_hWndEdit) {
                if (((LPNMHDR)lParam)->code == EN_SELCHANGE) {
                    UpdateStatusBar();
                }
            }
            return 0;
            
        case WM_CLOSE:
            // TODO: Check for unsaved changes
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
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
    HWND hwndEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
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
    
    // Get line and column
    int line = (int)SendMessage(g_hWndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin) + 1;
    int lineStart = (int)SendMessage(g_hWndEdit, EM_LINEINDEX, line - 1, 0);
    int col = cr.cpMin - lineStart + 1;
    
    // Format status text
    WCHAR szStatus[512];
    if (g_szFileName[0]) {
        swprintf(szStatus, 512, L"%s    Line %d, Col %d    [Filter: None]",
                 g_szFileTitle, line, col);
    } else {
        swprintf(szStatus, 512, L"Untitled    Line %d, Col %d    [Filter: None]",
                 line, col);
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
        swprintf(szTitle, MAX_PATH + 50, L"%s%s - RichEditor",
                 g_bModified ? L"*" : L"", g_szFileTitle);
    } else {
        swprintf(szTitle, MAX_PATH + 50, L"%sUntitled - RichEditor",
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
    
    // Set text in RichEdit control
    SetWindowText(g_hWndEdit, pszUTF16);
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
        
        swprintf(szError, 512, L"%s\n\nError: %s", pszMessage, szErrorMsg);
        
        // Also output to debugger
        OutputDebugString(L"RichEditor Error: ");
        OutputDebugString(pszMessage);
        OutputDebugString(L" - ");
        OutputDebugString(szErrorMsg);
        OutputDebugString(L"\n");
    } else {
        wcscpy_s(szError, 512, pszMessage);
    }
    
    MessageBox(g_hWndMain, szError, L"Error", MB_OK | MB_ICONERROR);
}
