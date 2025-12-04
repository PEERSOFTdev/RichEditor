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
void UpdateTitle();

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
            
            // TODO: Create status bar
            // TODO: Load filters
            
            return 0;
            
        case WM_SIZE:
            // Resize RichEdit control to fill client area
            if (g_hWndEdit) {
                RECT rcClient;
                GetClientRect(hwnd, &rcClient);
                SetWindowPos(g_hWndEdit, NULL,
                    0, 0,
                    rcClient.right, rcClient.bottom,
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
