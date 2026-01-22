# AGENTS.md - AI Agent Instructions for RichEditor

This document provides context and guidelines for AI agents working on the RichEditor project.

## Project Overview

**RichEditor** is a lightweight, accessible Win32 text editor built with the RichEdit 4.1 control and MinGW-w64. It features a powerful external filter system that allows text transformation through command-line tools.

**Key Design Principles:**
- Accessibility first (screen reader support is mandatory)
- Single-file architecture (main.cpp is ~4,500 lines)
- Universal binary (English + Czech in one executable)
- UTF-8 without BOM for all text files
- UNC path support throughout
- Static linking (no external DLLs except Windows system libraries)

## Architecture

### File Structure

```
RichEditor/
├── src/
│   ├── main.cpp          # Main application (~4,500 lines)
│   ├── resource.h        # Resource IDs
│   └── resource.rc       # Windows resources (menus, dialogs, strings)
├── Makefile              # Build configuration
├── .gitignore           # Git ignore patterns
├── README.md            # User and developer documentation
└── AGENTS.md            # This file
```

**Note:** `RichEditor.ini` is auto-generated on first run and should NOT be in the repository.

### Key Components

**Main Window (WndProc):**
- Handles all Windows messages
- Located in `main.cpp` around line 267
- Contains message handlers for menus, keyboard, context menu, timers

**RichEdit Control:**
- Windows RichEdit 4.1 (Msftedit.dll)
- Plain text mode (no RTF formatting)
- Provides built-in shortcuts (Alt+X, Ctrl+Up/Down, etc.)
- Handle stored in `g_hWndEdit`

**Status Bar:**
- Shows cursor position (line, column)
- Character at cursor with full Unicode support (including surrogate pairs)
- Current filter name
- Filter results (display action, 30-second timeout)
- Handle stored in `g_hWndStatus`

**Filter System:**
- External command execution via stdin/stdout pipes
- Action-based architecture: insert, display, clipboard, none
- Category-based menu organization
- Context menu integration
- Up to 100 filters supported

### Global Variables

Important globals in `main.cpp`:

```cpp
HWND g_hWndMain         // Main window handle
HWND g_hWndEdit         // RichEdit control handle
HWND g_hWndStatus       // Status bar handle
WCHAR g_szFileName[]    // Current file path (EXTENDED_PATH_MAX = 32767)
BOOL g_bModified        // Document modified flag
BOOL g_bWordWrap        // Word wrap state
BOOL g_bShowMenuDescriptions  // Show filter descriptions in menus
BOOL g_bNoMRU           // TRUE when /nomru command-line option specified
BOOL g_bPromptingForSave // TRUE when showing save prompt (prevents autosave race)

// Session Resume (Phase 2.6)
WCHAR g_szResumeFilePath[]      // Current resume temp file path
WCHAR g_szOriginalFilePath[]    // Original file path (for resumed files)
BOOL g_bIsResumedFile           // TRUE if current file was opened from resume
BOOL g_bAutoSaveUntitledOnClose // Auto-save untitled files on close (no prompt)

// Filter system
FilterInfo g_Filters[MAX_FILTERS]  // Array of filter configurations
int g_nFilterCount                 // Number of loaded filters
int g_nCurrentFilter               // Currently selected filter (-1 = none)
```

### String Handling - CRITICAL

**Always use wide string functions for Win32 APIs:**

```cpp
// CORRECT:
wcscpy(dest, source);
wcscat(dest, L" - ");
wcscat(dest, description);

// WRONG (causes UTF-16 interpretation issues):
swprintf(dest, size, L"%s - %s", name, desc);  // May fail with MinGW
```

**Why:** MinGW's `swprintf` can have issues with UTF-16 string handling. Prefer `wcscpy`, `wcscat`, `wcscmp`, `wcslen` for string operations.

## INI Configuration System

### File Location

INI file is always in the same directory as `RichEditor.exe`:
- Path built using `GetModuleFileName()` and replacing `.exe` with `.ini`
- Full path stored in `WCHAR szIniPath[EXTENDED_PATH_MAX]`

### Settings Auto-Generation

**All settings MUST be written to INI with defaults if missing.** This is implemented in `LoadSettings()`:

```cpp
void LoadSettings() {
    // For each setting:
    ReadINIValue(szIniPath, L"Settings", L"KeyName", szValue, 256, L"");
    if (szValue[0] == L'\0') {
        // Setting doesn't exist - write default
        WriteINIValue(szIniPath, L"Settings", L"KeyName", L"DefaultValue");
        g_variable = DEFAULT_VALUE;
    } else {
        // Setting exists - read it
        g_variable = ReadINIInt(szIniPath, L"Settings", L"KeyName", DEFAULT_VALUE);
    }
}
```

### Current Settings

```ini
[Settings]
WordWrap=1                    ; 1=enabled, 0=disabled
ShowMenuDescriptions=1        ; 1=show descriptions (accessible), 0=names only
AutosaveEnabled=1             ; 1=enabled, 0=disabled
AutosaveIntervalMinutes=1     ; Interval in minutes, 0=disabled
AutosaveOnFocusLoss=1         ; 1=save when losing focus, 0=don't
AutoSaveUntitledOnClose=0     ; 1=auto-save untitled on close, 0=prompt (Phase 2.6)
CurrentFilter=FilterName      ; Last selected filter (auto-saved)
```

### Filter Configuration

Filters use an action-based architecture:

```ini
[Filters]
Count=8

[Filter1]
Name=Uppercase
Command=powershell -NoProfile -Command "$input | ForEach-Object { $_.ToUpper() }"
Description=Converts selected text to UPPERCASE letters
Category=Transform
Action=insert
Insert=replace
ContextMenu=1
ContextMenuOrder=1
```

**Action Types:**
- `Action=insert` - Modifies document (Insert=replace|below|append)
- `Action=display` - Shows output (Display=messagebox|statusbar)
- `Action=clipboard` - Copies to clipboard (Clipboard=copy|append)
- `Action=none` - Executes for side effects only
* action types are written in lowercase, whereas action type keys (like Insert=) are in titlecase

**Context Menu:**
- `ContextMenu=1` - Show in right-click menu
- `ContextMenuOrder=N` - Sort order (lower = first)

### UNC Path Support

**Critical:** Do NOT use Windows' `GetPrivateProfileString` APIs - they don't support UNC paths reliably.

**Use custom INI functions:**
- `ReadINIValue()` - Direct file reading with UTF-8 support
- `WriteINIValue()` - Direct file writing with proper section/key management
- `ReadINIInt()` - Read integer values
- Located around lines 1200-1500 in main.cpp

## Accessibility Requirements

**Screen Reader Support is MANDATORY.**

### Menu Descriptions

Filter descriptions are included in menu text for accessibility:

```cpp
// When ShowMenuDescriptions=1 (default):
"Uppercase: Converts selected text to UPPERCASE letters"

// When ShowMenuDescriptions=0:
"Uppercase"
```

**Implementation in BuildFilterMenu() and context menu:**

```cpp
WCHAR szMenuText[MAX_FILTER_NAME + MAX_FILTER_DESC + 4];
if (g_bShowMenuDescriptions && g_Filters[filterIdx].szDescription[0] != L'\0') {
    wcscpy(szMenuText, g_Filters[filterIdx].szName);
    wcscat(szMenuText, L": ");
    wcscat(szMenuText, g_Filters[filterIdx].szDescription);
} else {
    wcscpy(szMenuText, g_Filters[filterIdx].szName);
}
AppendMenu(hMenu, MF_STRING, ID, szMenuText);
```

**Why this approach:**
- Screen readers (NVDA, JAWS, Narrator) automatically read menu item text
- No manual status bar querying required by users
- Standard Windows pattern (used by Office, Visual Studio, etc.)
- Configurable for users who prefer clean menus

**Never:**
- Don't use tooltips (not supported on standard Win32 menus)
- Don't rely on status bar alone (requires manual user action)
- Don't use owner-drawn menus (complex, unreliable screen reader support)

### Testing Accessibility

When making UI changes:
1. Test with `ShowMenuDescriptions=1` (default)
2. Navigate menus with arrow keys
3. Verify menu items are announced correctly
4. Test with `ShowMenuDescriptions=0` for visual users

## Build System

### Compiler

MinGW-w64 cross-compiler (MXE):
```bash
make CROSS=x86_64-w64-mingw32.static-
```

**Build flags:**
- `-O2` - Optimization
- `-std=c++11` - C++11 standard
- `-static -static-libgcc -static-libstdc++` - Static linking
- `-DUNICODE -D_UNICODE -municode` - Unicode support

### Target Binary

- **Size:** ~1.15 MB (static linking)
- **Format:** PE32+ (x86-64)
- **Resources:** English + Czech (universal binary)
- **Dependencies:** Only Windows system DLLs

### Localization

**Critical:** RC file MUST be UTF-8 with BOM:

```rc
#pragma code_page(65001)  // Essential for MinGW windres

LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US
// ... English resources ...

LANGUAGE LANG_CZECH, SUBLANG_DEFAULT
// ... Czech resources with diacritics ...
```

**Without this pragma:** Czech diacritics (ř, č, š, ž) will be garbled.

## Code Style

### Naming Conventions

```cpp
// Global variables: g_ prefix
HWND g_hWndMain;
BOOL g_bModified;
int g_nFilterCount;

// Constants: UPPERCASE with underscores
#define MAX_FILTERS 100
#define EXTENDED_PATH_MAX 32767

// Windows handles: h prefix
HWND hWnd;
HMENU hMenu;
HDC hDC;

// Strings: sz prefix (zero-terminated)
WCHAR szFileName[MAX_PATH];
LPWSTR pszBuffer;

// Booleans: b prefix
BOOL bSuccess;
BOOL bWordWrap;

// Counts/numbers: n prefix
int nCount;
int nIndex;
```

### Comments

```cpp
// Use C++ style for single-line comments

/* Multi-line comments for larger blocks
   or when disabling code sections */

//============================================================================
// Section Headers - Use this style for major sections
//============================================================================
```

### Function Organization

Functions in `main.cpp` are organized by category:
- Forward declarations (top)
- Utility functions (string helpers, path handling)
- INI file operations
- WinMain entry point
- Window procedure (WndProc)
- Dialog procedures
- File operations
- Filter system functions
- UI update functions

## Recent Features (v2.1)

### Command-Line Options

**`/nomru` Option:**
- Prevents file from being added to MRU list
- Useful for temporary files, logs, JSON viewers
- Can appear before or after filename
- Examples:
  - `RichEditor.exe file.json /nomru`
  - `RichEditor.exe /nomru file.json`
- Implementation:
  - Global flag: `g_bNoMRU` (line ~126)
  - Parsed in `WinMain` command-line loop (line ~205)
  - Checked in `AddToMRU()` before adding (line ~3580)

### Undo Buffer Overflow Notification

**EN_STOPNOUNDO Handler:**
- RichEdit sends this notification when undo buffer is full
- Shows warning MessageBox (localized English/Czech)
- Asks user: "Continue editing?" Yes/No
- "Yes" → Continue editing (undo stops working)
- "No" → Close application via `PostMessage(WM_CLOSE)`
- Implementation:
  - String resources: IDS_UNDO_BUFFER_FULL_TITLE (2060), IDS_UNDO_BUFFER_FULL_MESSAGE (2061)
  - Handler in `WndProc` WM_NOTIFY section (line ~575)

### Save Prompt Protection

**Autosave Race Condition Fix:**
- Problem: MessageBox loses focus → triggers WM_KILLFOCUS → autosave before user responds
- Solution: `g_bPromptingForSave` guard flag (line ~35)
- Set TRUE before MessageBox, FALSE after
- `WM_KILLFOCUS` checks flag and skips autosave if TRUE (line ~377)
- Ensures user's "Yes/No/Cancel" choice is respected

### SelectAfterPaste Feature

**Optional text selection after paste:**
- When enabled, pasted text is automatically selected after paste operation
- Allows quick navigation: Up/Down keys jump to start/end of pasted block
- Default: OFF (not common in editors, can surprise users)
- Configuration: `SelectAfterPaste=0` in INI (0=off, 1=on)
- Implementation:
  - Global flag: `g_bSelectAfterPaste` (line ~42)
  - Modified `EditPaste()` to capture selection before/after paste (line ~2350)
  - Selects from original position to new cursor position if enabled
- **Important caveat**: Typing any character replaces the selected text (standard Windows behavior)
  - Users may accidentally overwrite pasted content if unaware of selection
  - Document this clearly when describing the feature

## Phase 2.6: Session Resume Implementation (Important Patterns)

### Overview

Phase 2.6 implements Windows 11 Notepad-style automatic session recovery. The system saves unsaved work during Windows shutdowns and recovers it on next startup with a `[Resumed]` title indicator.

**Key Design Goals:**
- Zero user interaction required for recovery
- One-time recovery semantics (no stale resume file entries)
- Resume file reuse (prevent %TEMP% pollution)
- Multi-instance safe (no locks needed)
- Handle shutdown cancellations gracefully

### Resume File System Architecture

**Storage Location:**
- Resume files: `%TEMP%\RichEditor\` directory
- Created on first use via `EnsureRichEditorTempDirExists()`
- Tracked in INI: `[Resume]` section

**File Naming Convention:**
```
Untitled files:    Untitled_YYYYMMDD_HHMMSS_resume.txt
Named files:       originalname_resume.ext
```

**File Format:**
- UTF-8 without BOM (consistent with RichEditor standard)
- Plain text (no metadata, no RTF)

### Critical Pattern: WM_QUERYENDSESSION/WM_ENDSESSION Split

**Problem Solved:** If another application cancels Windows shutdown after we've already registered the resume file in INI, we'll have a stale entry that causes incorrect recovery on next startup.

**Solution:** Two-phase shutdown handling:

```cpp
case WM_QUERYENDSESSION:
    // Phase 1: Windows asks "Can I shut down?"
    if (g_bModified) {
        // Save content to temp file but DON'T write to INI yet
        SaveToResumeFile(g_szResumeFilePath, ...);
        // Resume file path is stored in g_szResumeFilePath
    }
    return TRUE;  // Allow shutdown to proceed

case WM_ENDSESSION:
    // Phase 2: Windows tells us "Shutdown is happening" or "Shutdown cancelled"
    if (wParam) {
        // Shutdown actually happening - NOW safe to write to INI
        WriteResumeToINI(g_szResumeFilePath, g_szFileName, g_bIsResumedFile, ...);
    } else {
        // Shutdown was cancelled by another app - cleanup temp file
        DeleteResumeFile(g_szResumeFilePath);
        g_szResumeFilePath[0] = L'\0';
    }
    return 0;
```

**Why This Pattern:**
- WM_QUERYENDSESSION is a **query** - shutdown may still be cancelled
- WM_ENDSESSION with wParam=TRUE means shutdown **confirmed**
- WM_ENDSESSION with wParam=FALSE means shutdown **cancelled**
- Only write to INI when confirmed to prevent stale entries

**Implementation Location:** WndProc around lines 620-680

### Critical Pattern: Resume File Reuse

**Problem Solved:** Creating new timestamped files every close would orphan old resume files in %TEMP%, eventually filling the directory with hundreds of `*_resume.txt` files.

**Solution:** Reuse the same resume file across sessions:

```cpp
BOOL SaveToResumeFile(WCHAR *szResumeFile, size_t cchResumeFile) {
    if (g_bIsResumedFile && g_szResumeFilePath[0] != L'\0') {
        // REUSE existing resume file path
        wcscpy(szResumeFile, g_szResumeFilePath);
    } else {
        // Generate new filename only if this isn't already a resumed file
        GenerateResumeFileName(szResumeFile, cchResumeFile, g_szFileName);
    }
    
    // Write content to the file (overwrites if exists)
    // ...
}
```

**Lifecycle:**
1. First shutdown: Create `filename_resume.ext`
2. Startup: Load from `filename_resume.ext`, set `g_bIsResumedFile = TRUE`, store path in `g_szResumeFilePath`
3. Second shutdown: Reuse same `filename_resume.ext` (overwrite content)
4. Repeat until user explicitly saves via File → Save
5. On explicit save: Call `DeleteResumeFile()` to cleanup

**Result:** One resume file per document, automatically cleaned up on explicit save.

**Implementation Location:** SaveToResumeFile() around line 1443

### Critical Pattern: Clear-on-Read for Multi-Instance

**Problem Solved:** Multiple RichEditor instances might start simultaneously during Windows login. How do we ensure only one instance gets the resume file without complex locking?

**Solution:** Clear INI entry immediately after reading:

```cpp
// In WinMain startup sequence:
if (ReadResumeFromINI(szIniPath, szResumeFile, szOriginalFile, &bWasResumed)) {
    // Got resume info - load the file
    LoadResumeFile(szResumeFile, ...);
    
    // IMMEDIATELY clear the INI entry (atomic operation)
    ClearResumeFromINI(szIniPath);
    
    // Set global state
    g_bIsResumedFile = TRUE;
    // ...
}
```

**Why This Works:**
- First instance to call `ReadResumeFromINI()` gets the data
- That instance immediately clears the INI entry
- Second instance finds empty `[Resume]` section and starts blank
- No file locks, no synchronization primitives needed
- Elegant "first-come-first-served" semantics

**Bonus Behavior:** This was an "emergent behavior" - not originally planned but works perfectly. User loved this pattern.

**Implementation Location:** WinMain around line 225

### Critical Pattern: Cleanup on Explicit Save

**When to Delete Resume File:**
- User clicks File → Save or File → Save As
- AutoSave timer triggers (for non-untitled files)
- Focus loss autosave (for non-untitled files)

**When NOT to Delete:**
- WM_CLOSE shutdown saves (keep for recovery)
- AutoSave for untitled files (user hasn't explicitly saved yet)

**Implementation:**
```cpp
// In SaveFile() function:
if (bSuccess) {
    // ... update g_szFileName, g_bModified, etc ...
    
    // Delete resume file after successful explicit save
    if (g_bIsResumedFile) {
        DeleteResumeFile(g_szResumeFilePath);
        g_bIsResumedFile = FALSE;
        g_szResumeFilePath[0] = L'\0';
        UpdateTitle();  // Remove [Resumed] indicator
    }
}
```

**Why:** Explicit save means user has chosen a permanent location. Resume file no longer needed.

**Implementation Location:** SaveFile() around line 1670

### Global Variables (Phase 2.6)

```cpp
WCHAR g_szResumeFilePath[EXTENDED_PATH_MAX];   // Current resume temp file path
WCHAR g_szOriginalFilePath[EXTENDED_PATH_MAX]; // Original file (for resumed files)
BOOL g_bIsResumedFile;                          // TRUE if opened from resume
BOOL g_bAutoSaveUntitledOnClose;                // Setting from INI (default: FALSE)
```

**Line Numbers:** Global declarations around line 35-50 in main.cpp

### Key Functions

**Resume File Management:**
- `GetRichEditorTempDir()` - Returns `%TEMP%\RichEditor\` path (line ~1272)
- `EnsureRichEditorTempDirExists()` - Creates directory if needed (line ~1285)
- `GenerateResumeFileName()` - Creates unique filenames with overflow protection (line ~1302)
- `SaveToResumeFile()` - Core save logic with reuse pattern (line ~1443)
- `DeleteResumeFile()` - Returns BOOL for error checking (line ~1555)

**INI Management:**
- `WriteResumeToINI()` - Writes `[Resume]` section (line ~1578)
- `ReadResumeFromINI()` - Reads and returns resume info (line ~1612)
- `ClearResumeFromINI()` - Atomic clear operation (line ~1641)

**Window Message Handlers:**
- WM_QUERYENDSESSION - Phase 1 shutdown (line ~620)
- WM_ENDSESSION - Phase 2 shutdown confirmation/cancellation (line ~651)
- WM_CLOSE - Updated with `AutoSaveUntitledOnClose` logic (line ~697)

### INI Configuration (Phase 2.6)

```ini
[Settings]
AutoSaveUntitledOnClose=0     ; 0=prompt (default), 1=auto-save without prompt

[Resume]
File=C:\Users\...\Temp\RichEditor\document_resume.txt
OriginalFile=C:\Documents\document.txt
WasResumed=1
```

**Section Lifecycle:**
- Created by WriteResumeToINI() during WM_ENDSESSION
- Read once by WinMain on startup
- Cleared immediately by ClearResumeFromINI() after reading
- Empty until next shutdown

### Important Implementation Notes

**Buffer Overflow Protection:**
```cpp
// In GenerateResumeFileName():
if (wcslen(szBase) + wcslen(L"_resume") + wcslen(szExt) + 1 > cchResumeFile) {
    // Truncate base name to fit
    size_t maxBaseLen = cchResumeFile - wcslen(L"_resume") - wcslen(szExt) - 1;
    szBase[maxBaseLen] = L'\0';
}
```

**Unicode Text Extraction:**
```cpp
// Use EM_GETTEXTLENGTHEX for proper Unicode length (not EM_GETLEXTLENGTH)
GETTEXTLENGTHEX gtl = {GTL_DEFAULT, 1200};  // UTF-16LE
int nLen = SendMessage(g_hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
```

**WriteFile Error Handling:**
```cpp
if (!WriteFile(hFile, utf8.c_str(), utf8.length(), &dwWritten, NULL)) {
    CloseHandle(hFile);
    DeleteFileW(szResumeFilePath);  // Cleanup on failure
    return FALSE;
}
```

**Title Bar Updates:**
```cpp
// Always call UpdateTitle() after changing g_bIsResumedFile
g_bIsResumedFile = TRUE;
UpdateTitle();  // Shows "filename [Resumed] - RichEditor"
```

### AutoSaveUntitledOnClose Behavior

**Setting:** `AutoSaveUntitledOnClose=0` (default)

**When 0 (Default - Traditional Behavior):**
- User closes untitled modified document
- Prompt: "Save changes to Untitled?" Yes/No/Cancel
- User must explicitly choose

**When 1 (Auto-Save):**
- User closes untitled modified document
- Automatically saved to resume file (no prompt)
- Recovers on next startup

**Implementation in WM_CLOSE:**
```cpp
if (g_bModified) {
    if (g_szFileName[0] == L'\0' && g_bAutoSaveUntitledOnClose) {
        // Untitled + auto-save enabled = save to resume without prompt
        SaveToResumeFile(...);
        WriteResumeToINI(...);
    } else {
        // Named file or auto-save disabled = show prompt
        int nRes = MessageBox(..., MB_YESNOCANCEL);
        // ... handle response ...
    }
}
```

**Why Default is OFF:** Traditional editor behavior. Users expect prompts for unsaved work. Auto-save is opt-in.

### Testing Checklist (Phase 2.6)

**Basic Resume:**
- [ ] Modify untitled file, trigger shutdown, restart → file recovered with [Resumed]
- [ ] Modify named file, trigger shutdown, restart → file recovered with [Resumed]
- [ ] Explicitly save resumed file → [Resumed] disappears, resume file deleted

**Shutdown Cancellation:**
- [ ] Modify file, start shutdown, cancel shutdown → no resume file in INI
- [ ] Check %TEMP%\RichEditor\ → temp file exists but not registered

**Resume File Reuse:**
- [ ] Resume file, modify, shutdown again → same resume file reused (check timestamp)
- [ ] Repeat 5 times → still only one resume file in %TEMP%\RichEditor\

**Multi-Instance:**
- [ ] Resume file registered, start two instances → first gets resume, second blank
- [ ] INI checked immediately → [Resume] section empty after first instance reads

**AutoSaveUntitledOnClose:**
- [ ] Set to 0, close untitled modified → prompt shown
- [ ] Set to 1, close untitled modified → saved automatically, no prompt

**Edge Cases:**
- [ ] Unicode filenames with emoji → resume file created correctly
- [ ] Very long filenames (>200 chars) → truncated safely
- [ ] UNC paths → resume works correctly
- [ ] Read-only resume file → error handling works

### Bugs Fixed During Implementation

1. ✅ **WM_QUERYENDSESSION/WM_ENDSESSION race** - Split pattern prevents stale INI entries
2. ✅ **Buffer overflow in GenerateResumeFileName** - Added truncation logic
3. ✅ **Unicode length calculation** - Use EM_GETTEXTLENGTHEX not EM_GETLEXTLENGTH
4. ✅ **WriteFile error handling** - Cleanup temp file on write failure
5. ✅ **AutoSaveUntitledOnClose scope** - Only applies to untitled files (check `g_szFileName[0] == L'\0'`)
6. ✅ **Title bar not updating** - Added UpdateTitle() call after loading resume file
7. ✅ **Resume file proliferation** - Reuse pattern prevents %TEMP% pollution
8. ✅ **IDS_RESUMED localization** - Added to both LANG_ENGLISH and LANG_CZECH

### String Resources (Phase 2.6)

```cpp
#define IDS_RESUMED 2089
```

**Localization:**
- English: "Resumed"
- Czech: "Obnoveno"

**Usage in UpdateTitle():**
```cpp
if (g_bIsResumedFile) {
    WCHAR szResumed[32];
    LoadString(GetModuleHandle(NULL), IDS_RESUMED, szResumed, 32);
    wcscat(szTitle, L" [");
    wcscat(szTitle, szResumed);
    wcscat(szTitle, L"]");
}
```

### Important Don'ts (Phase 2.6 Specific)

1. **Never write to INI in WM_QUERYENDSESSION** - Shutdown may be cancelled
2. **Never create new resume files if g_bIsResumedFile is TRUE** - Reuse existing path
3. **Never forget to call UpdateTitle() after changing g_bIsResumedFile** - UI won't update
4. **Never apply AutoSaveUntitledOnClose to named files** - Check `g_szFileName[0] == L'\0'`
5. **Never use EM_GETLEXTLENGTH for Unicode text** - Use EM_GETTEXTLENGTHEX with GTL_DEFAULT
6. **Never leave orphaned temp files** - Always cleanup on WriteFile failures
7. **Never forget to clear INI after reading** - Multi-instance will break

## Phase 2.7: Template System Implementation (Important Patterns)

### Overview

Phase 2.7 implements a template system for quick text insertion with variable expansion. Similar to Markdown editors (Typora, Obsidian) and IDEs (Visual Studio Code snippets).

**Key Design Goals:**
- Fast template insertion with keyboard shortcuts
- **Ctrl+Shift+T** opens template picker popup menu at cursor (quick access to all templates)
- Variable expansion (%cursor%, %date%, %selection%, etc.)
- File type filtering (Markdown templates only in .md files)
- Category organization (similar to filter system)
- Dynamic File→New submenu (create files from templates)
- Accessibility-first (menu descriptions, screen reader support)

### Template System Architecture

**Storage:**
- Templates stored in `[Templates]` section of INI file
- Up to 100 templates supported (MAX_TEMPLATES constant)
- Auto-generated defaults: 15 Markdown templates

**Template Structure:**
```ini
[Template1]
Name=Heading 1
Name.cs_CZ=Nadpis 1                    ; Optional Czech translation
Description=Insert a level 1 heading
Description.cs_CZ=Vložit nadpis úrovně 1
Category=Markdown                      ; Menu grouping
FileExtension=md                       ; Filter by file type
Template=# %cursor%                    ; Template text with variables
Shortcut=Ctrl+1                        ; Optional keyboard shortcut
```

**Core Components:**
- `TemplateInfo` struct - Holds template metadata (name, description, shortcut, etc.)
- `g_Templates[]` - Global array of loaded templates (MAX_TEMPLATES=100)
- `g_nTemplateCount` - Number of loaded templates
- `g_szCurrentFileExtension` - Current file's extension (affects filtering)

### Critical Pattern: Dynamic Accelerator Table

**Problem Solved:** Template keyboard shortcuts must work alongside built-in shortcuts (Ctrl+S, Ctrl+N, etc.) without conflicts.

**Solution:** Build dynamic accelerator table at startup combining built-in + template shortcuts.

**Implementation:**
```cpp
// In WinMain (line ~205):
LoadTemplates(szIniPath);  // Load templates from INI
g_hAccel = BuildAcceleratorTable();  // Build dynamic table

// Message loop:
while (GetMessage(&msg, NULL, 0, 0)) {
    if (!TranslateAccelerator(g_hWndMain, g_hAccel, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// Cleanup:
if (g_hAccel) DestroyAcceleratorTable(g_hAccel);
```

**BuildAcceleratorTable() Function (~line 922):**
```cpp
HACCEL BuildAcceleratorTable() {
    ACCEL accels[128];  // 14 built-in + up to 100 templates
    int count = 0;
    
    // Add built-in shortcuts (14 total)
    accels[count++] = {FVIRTKEY | FCONTROL, 'N', ID_FILE_NEW};
    accels[count++] = {FVIRTKEY | FCONTROL, 'O', ID_FILE_OPEN};
    // ... etc
    
    // Add template shortcuts (skip if conflicts with built-in)
    for (int i = 0; i < g_nTemplateCount; i++) {
        if (g_Templates[i].vkKey != 0) {  // Has shortcut
            if (!IsShortcutReserved(g_Templates[i].vkKey, g_Templates[i].dwModifiers)) {
                accels[count].fVirt = FVIRTKEY | g_Templates[i].dwModifiers;
                accels[count].key = g_Templates[i].vkKey;
                accels[count].cmd = ID_TOOLS_TEMPLATE_BASE + i;
                count++;
            }
        }
    }
    
    return CreateAcceleratorTable(accels, count);
}
```

**Why This Pattern:**
- Static RC accelerator table replaced with dynamic generation
- Template shortcuts loaded from INI at runtime
- Conflicts automatically prevented via IsShortcutReserved()
- User can add/remove template shortcuts without recompiling

### Critical Pattern: Variable Expansion

**Problem Solved:** Templates need dynamic content (cursor position, current date, selected text, clipboard contents).

**Solution:** String replacement with special variable handling for `%cursor%`.

**ExpandTemplateVariables() Function (~line 1107):**
```cpp
LPWSTR ExpandTemplateVariables(const WCHAR *szTemplate, LONG *pCursorOffset) {
    size_t len = wcslen(szTemplate);
    size_t bufferSize = len * 4 + 4096;  // Extra space for expansions
    LPWSTR pszResult = (LPWSTR)malloc(bufferSize * sizeof(WCHAR));
    LPWSTR pDest = pszResult;
    const WCHAR *pSrc = szTemplate;
    
    *pCursorOffset = -1;  // No cursor marker found yet
    
    while (*pSrc) {
        if (wcsncmp(pSrc, L"%cursor%", 8) == 0) {
            if (*pCursorOffset < 0) {
                // First occurrence - mark position
                *pCursorOffset = (LONG)(pDest - pszResult);
            }
            // Skip "%cursor%" (don't insert literal text)
            pSrc += 8;
        }
        else if (wcsncmp(pSrc, L"%selection%", 11) == 0) {
            // Get current selection from RichEdit
            CHARRANGE cr;
            SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
            int nSelLen = cr.cpMax - cr.cpMin;
            if (nSelLen > 0) {
                WCHAR *szSelection = GetSelectedText();
                wcscpy(pDest, szSelection);
                pDest += wcslen(szSelection);
                free(szSelection);
            }
            pSrc += 11;
        }
        else if (wcsncmp(pSrc, L"%date%", 6) == 0) {
            // Insert current date (YYYY-MM-DD)
            SYSTEMTIME st;
            GetLocalTime(&st);
            swprintf(pDest, 16, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
            pDest += wcslen(pDest);
            pSrc += 6;
        }
        // ... similar for %time%, %datetime%, %clipboard%
        else {
            *pDest++ = *pSrc++;  // Copy literal character
        }
    }
    
    *pDest = L'\0';
    return pszResult;  // Caller must free!
}
```

**Key Details:**
- `%cursor%` is special: first occurrence marks position, rest removed
- Caller receives cursor offset in `*pCursorOffset` parameter
- Returned string must be freed by caller (malloc'd memory)
- Buffer oversized to prevent overflow during expansion
- Unknown variables left as literals (e.g., `%foo%` stays `%foo%`)

### Critical Pattern: File Extension Tracking

**Problem Solved:** Template menu must filter by current file type (Markdown templates only in .md files).

**Solution:** Track current extension globally, rebuild menu when it changes.

**Global Variable:**
```cpp
WCHAR g_szCurrentFileExtension[MAX_TEMPLATE_FILEEXT] = L"txt";  // Default
```

**UpdateFileExtension() Function (~line 1063):**
```cpp
void UpdateFileExtension(const WCHAR *szFilePath) {
    if (szFilePath == NULL || szFilePath[0] == L'\0') {
        wcscpy(g_szCurrentFileExtension, L"txt");  // Default for untitled
    } else {
        ExtractFileExtension(szFilePath, g_szCurrentFileExtension, MAX_TEMPLATE_FILEEXT);
    }
    
    // Rebuild template menu with new filter
    if (g_hWndMain) {
        BuildTemplateMenu(g_hWndMain);
        BuildFileNewMenu(g_hWndMain);  // Also rebuild File→New
    }
}
```

**Called From:**
- `WM_CREATE` - Initialize to "txt"
- `FileNew()` - Reset to "txt" for untitled
- `LoadTextFile()` - Extract extension from opened file
- `SaveTextFile()` - Update if extension changed (Save As)
- `FileNewFromTemplate()` - Set based on template's FileExtension field

**Why This Pattern:**
- Menu automatically updates when file type changes
- No manual refresh needed
- Works seamlessly with Save As (change .txt → .md)
- User sees only relevant templates

### Critical Pattern: File→New Submenu Conversion

**Problem Solved:** Convert File→New from menu item to submenu without breaking Ctrl+N.

**Solution:** Replace menu item with submenu, add "Blank Document" as first item, map both IDs to FileNew().

**BuildFileNewMenu() Function (~line 1320):**
```cpp
void BuildFileNewMenu(HWND hwnd) {
    HMENU hMenu = GetMenu(hwnd);
    HMENU hFileMenu = GetSubMenu(hMenu, 0);  // File menu is first
    
    // Find existing "New" item
    int newPos = -1;
    for (int i = 0; i < GetMenuItemCount(hFileMenu); i++) {
        if (GetMenuItemID(hFileMenu, i) == ID_FILE_NEW) {
            newPos = i;
            break;
        }
    }
    
    // Convert to submenu if needed
    HMENU hNewMenu = NULL;
    MENUITEMINFO mii = {0};
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_SUBMENU;
    GetMenuItemInfo(hFileMenu, newPos, TRUE, &mii);
    
    if (mii.hSubMenu == NULL) {
        // Create new submenu
        hNewMenu = CreatePopupMenu();
        mii.fMask = MIIM_SUBMENU | MIIM_STRING;
        mii.hSubMenu = hNewMenu;
        mii.dwTypeData = L"&New";
        SetMenuItemInfo(hFileMenu, newPos, TRUE, &mii);
    } else {
        hNewMenu = mii.hSubMenu;
        // Clear existing items
        while (GetMenuItemCount(hNewMenu) > 0) {
            DeleteMenu(hNewMenu, 0, MF_BYPOSITION);
        }
    }
    
    // Add "Blank Document" as first item (preserves Ctrl+N behavior)
    WCHAR szBlankDoc[64];
    LoadString(GetModuleHandle(NULL), IDS_BLANK_DOCUMENT, szBlankDoc, 64);
    WCHAR szMenuItem[80];
    swprintf(szMenuItem, 80, L"&%s\tCtrl+N", szBlankDoc);
    AppendMenu(hNewMenu, MF_STRING, ID_FILE_NEW_BLANK, szMenuItem);
    
    AppendMenu(hNewMenu, MF_SEPARATOR, 0, NULL);
    
    // Group templates by file extension
    // Add "Markdown Document", "Text Document", etc.
    // (uses first template of each type)
    // ...
    
    DrawMenuBar(hwnd);
}
```

**WM_COMMAND Handling:**
```cpp
case ID_FILE_NEW:
case ID_FILE_NEW_BLANK:  // Both IDs call FileNew()
    FileNew();
    break;

// In default case:
else if (wmId >= ID_FILE_NEW_TEMPLATE_BASE && wmId < ID_FILE_NEW_TEMPLATE_BASE + 32) {
    int templateIdx = wmId - ID_FILE_NEW_TEMPLATE_BASE;
    if (templateIdx >= 0 && templateIdx < g_nTemplateCount) {
        FileNewFromTemplate(templateIdx);
    }
}
```

**Why This Pattern:**
- Ctrl+N accelerator still works (mapped to ID_FILE_NEW)
- ID_FILE_NEW and ID_FILE_NEW_BLANK both call FileNew() (backwards compatible)
- Template items use separate ID range (ID_FILE_NEW_TEMPLATE_BASE = 8000)
- Menu can be converted at runtime without RC file changes

### Global Variables (Phase 2.7)

```cpp
// Template system
TemplateInfo g_Templates[MAX_TEMPLATES];  // Array of loaded templates
int g_nTemplateCount = 0;                 // Number of loaded templates
WCHAR g_szCurrentFileExtension[MAX_TEMPLATE_FILEEXT] = L"txt";  // Current file extension
HACCEL g_hAccel = NULL;                   // Dynamic accelerator table
```

**Line Numbers:** Global declarations around line 40-50 in main.cpp

### Key Functions (Phase 2.7)

**Template Loading & Parsing:**
- `LoadTemplates()` - Load templates from INI with localization (~line 652)
- `ParseShortcut()` - Parse "Ctrl+Shift+F1" strings to VK codes (~line 540)
- `IsShortcutReserved()` - Check if shortcut conflicts with built-in (~line 636)
- `UnescapeTemplateString()` - Handle \n, \t, \\ escapes (~line 593)

**Template Expansion & Insertion:**
- `ExpandTemplateVariables()` - Replace all variables with values (~line 1107)
- `InsertTemplate()` - Main insertion logic with cursor positioning (~line 1221)
- `ExtractFileExtension()` - Extract extension from file path (~line 1028)
- `UpdateFileExtension()` - Update global extension, rebuild menus (~line 1063)

**Menu Building:**
- `BuildAcceleratorTable()` - Dynamic accelerator table (~line 829)
- `ShowTemplatePickerMenu()` - Ctrl+Shift+T popup menu at cursor (~line 1137)
- `BuildTemplateMenu()` - Tools→Insert Template submenu (~line 1275)
- `BuildFileNewMenu()` - File→New submenu with templates (~line 1429)

**File Creation:**
- `FileNewFromTemplate()` - Create new file with template (~line 3822)

### Important Implementation Notes

**Shortcut Parsing:**
```cpp
// Supports: Ctrl+Key, Ctrl+Shift+Key, Ctrl+Alt+Key, F1-F12, etc.
BOOL ParseShortcut(const WCHAR *szShortcut, WORD *pvkKey, DWORD *pdwModifiers) {
    *pvkKey = 0;
    *pdwModifiers = 0;
    
    // Parse modifiers
    if (wcsstr(szShortcut, L"Ctrl+")) *pdwModifiers |= FCONTROL;
    if (wcsstr(szShortcut, L"Shift+")) *pdwModifiers |= FSHIFT;
    if (wcsstr(szShortcut, L"Alt+")) *pdwModifiers |= FALT;
    
    // Find last '+' to get key part
    const WCHAR *pKey = wcsrchr(szShortcut, L'+');
    if (pKey) pKey++; else pKey = szShortcut;
    
    // Map key name to VK code using KeyMapping table
    for (int i = 0; i < KeyMappingCount; i++) {
        if (_wcsicmp(pKey, KeyMapping[i].name) == 0) {
            *pvkKey = KeyMapping[i].vkCode;
            return TRUE;
        }
    }
    
    return FALSE;  // Unknown key
}
```

**Reserved Shortcuts Table:**
```cpp
struct ReservedShortcut {
    WORD vkKey;
    DWORD dwModifiers;
    const WCHAR *description;
};

ReservedShortcut g_ReservedShortcuts[] = {
    {VK_N, FCONTROL, L"Ctrl+N (File→New)"},
    {VK_O, FCONTROL, L"Ctrl+O (File→Open)"},
    {VK_S, FCONTROL, L"Ctrl+S (File→Save)"},
    // ... 14 total
};
```

**Template Insertion Behavior:**
```cpp
void InsertTemplate(int nTemplateIndex) {
    TemplateInfo *pTemplate = &g_Templates[nTemplateIndex];
    
    // Check file extension filter
    if (pTemplate->szFileExtension[0] != L'\0') {
        if (_wcsicmp(pTemplate->szFileExtension, g_szCurrentFileExtension) != 0) {
            return;  // Silently fail (wrong file type)
        }
    }
    
    // Expand variables
    LONG nCursorOffset = -1;
    LPWSTR pszExpanded = ExpandTemplateVariables(pTemplate->szTemplate, &nCursorOffset);
    
    if (pszExpanded) {
        // Replace selection with expanded template
        SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszExpanded);
        
        // Position cursor if %cursor% was found
        if (nCursorOffset >= 0) {
            CHARRANGE cr;
            SendMessage(g_hWndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
            cr.cpMin = cr.cpMax - wcslen(pszExpanded) + nCursorOffset;
            cr.cpMax = cr.cpMin;
            SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
        }
        
        free(pszExpanded);
        g_bModified = TRUE;
        UpdateTitle();
    }
}
```

### String Resources (Phase 2.7)

```cpp
#define IDS_BLANK_DOCUMENT              2106
#define IDS_MARKDOWN_DOCUMENT           2108
#define IDS_TEXT_DOCUMENT               2110
#define IDS_HTML_DOCUMENT               2112
#define IDS_NO_TEMPLATES                2114
#define IDS_NO_TEMPLATES_FOR_FILETYPE   2116
```

**Localization:**
- English: "Blank Document", "Markdown Document", etc.
- Czech: "Prázdný dokument", "Markdown dokument", etc.

**Usage:** BuildFileNewMenu() and BuildTemplateMenu() use LoadString() for all display text.

### Important Don'ts (Phase 2.7 Specific)

1. **Never override reserved shortcuts** - Check IsShortcutReserved() before adding
2. **Never forget to rebuild menus after extension change** - Call BuildTemplateMenu() in UpdateFileExtension()
3. **Never assume %cursor% exists** - Check nCursorOffset >= 0 before positioning
4. **Never forget to free expanded template** - ExpandTemplateVariables() returns malloc'd memory
5. **Never use static accelerator table** - Build dynamically with BuildAcceleratorTable()
6. **Never show error dialogs for wrong file type** - Silently skip (better UX)
7. **Never forget to call UpdateFileExtension() in file operations** - Affects menu filtering

## Phase 2.9.1: Find Dialog with Escape Sequences (Important Patterns)

### Overview

Phase 2.9.1 implements a modeless Find dialog with history, checkbox persistence, and escape sequence support. Similar to Notepad++, Sublime Text, and VS Code find functionality.

**Key Design Goals:**
- Modeless dialog (non-blocking, can edit while open)
- Search history (MRU list, max 20 items)
- Checkbox state persistence (Match case, Whole word, Use escapes)
- Escape sequence support (\n, \t, \xNN, \uNNNN)
- Keyboard shortcuts (Ctrl+F, F3, Shift+F3)
- No search wrapping (user requirement)

### Search System Architecture

**Dialog Type:** Modeless (created once, shown/hidden with ShowWindow)

**Global State:**
```cpp
HWND g_hDlgFind = NULL;                    // Dialog handle (NULL = not created)
WCHAR g_szFindWhat[MAX_SEARCH_TEXT];       // Current search term
BOOL g_bFindMatchCase;                     // Checkbox states
BOOL g_bFindWholeWord;
BOOL g_bFindUseEscapes;
BOOL g_bSearchDown = TRUE;                 // Search direction
BOOL g_bSelectAfterFind = TRUE;            // INI setting
WCHAR g_szFindHistory[20][256];            // MRU history
int g_nFindHistoryCount;                   // History count
```

**String Resources:**
- English: IDS_FIND_TITLE (2140) through IDS_FIND_NOTFOUND_TITLE (2151)
- Czech: Fully localized with diacritics

### Critical Pattern: Modeless Dialog Integration

**Problem Solved:** Modeless dialogs require special message loop handling and careful lifetime management.

**Solution:** Message loop filter + WM_DESTROY cleanup

**Message Loop Integration (WinMain ~line 637):**
```cpp
while (GetMessage(&msg, NULL, 0, 0)) {
    // CRITICAL: Check modeless dialog before TranslateAccelerator
    if (g_hDlgFind && IsDialogMessage(g_hDlgFind, &msg)) {
        continue;  // Dialog handled the message (Tab, Enter, etc.)
    }
    
    if (!TranslateAccelerator(g_hWndMain, g_hAccel, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
```

**Dialog Lifecycle:**
```cpp
// In WM_COMMAND for ID_SEARCH_FIND:
if (g_hDlgFind == NULL) {
    // Create dialog once
    g_hDlgFind = CreateDialog(hInst, MAKEINTRESOURCE(IDD_FIND), hwnd, DlgFindProc);
    ShowWindow(g_hDlgFind, SW_SHOW);
} else {
    // Already exists - just focus it
    ShowWindow(g_hDlgFind, SW_SHOW);
    SetForegroundWindow(g_hDlgFind);
}

// In WM_DESTROY:
if (g_hDlgFind) {
    SaveFindHistory();  // Persist before destroy
    DestroyWindow(g_hDlgFind);
    g_hDlgFind = NULL;
}
```

**Why This Pattern:**
- IsDialogMessage() must run BEFORE TranslateAccelerator() to handle Tab/Enter properly
- Dialog created once, reused across multiple open/close cycles
- SaveFindHistory() called on main window destroy (not dialog close) to persist state

### Critical Pattern: Enhanced Escape Sequence Parsing

**Problem Solved:** Find needs to support more escape sequences than templates (\xNN hex bytes, \uNNNN Unicode).

**Solution:** Enhanced ParseEscapeSequences() function replaces old UnescapeTemplateString()

**Implementation (~line 735):**
```cpp
LPWSTR ParseEscapeSequences(LPCWSTR pszInput) {
    size_t nLen = wcslen(pszInput);
    LPWSTR pszOutput = (LPWSTR)malloc((nLen + 1) * sizeof(WCHAR));
    
    const WCHAR *pSrc = pszInput;
    WCHAR *pDst = pszOutput;
    
    while (*pSrc) {
        if (*pSrc == L'\\' && *(pSrc + 1)) {
            pSrc++;  // Skip backslash
            switch (*pSrc) {
                case L'n':  *pDst++ = L'\n'; pSrc++; break;
                case L'r':  *pDst++ = L'\r'; pSrc++; break;
                case L't':  *pDst++ = L'\t'; pSrc++; break;
                case L'\\': *pDst++ = L'\\'; pSrc++; break;
                
                case L'x':  // Hex byte: \xNN
                    if (iswxdigit(pSrc[1]) && iswxdigit(pSrc[2])) {
                        WCHAR hex[3] = {pSrc[1], pSrc[2], 0};
                        *pDst++ = (WCHAR)wcstol(hex, NULL, 16);
                        pSrc += 3;
                    } else {
                        *pDst++ = L'\\'; *pDst++ = *pSrc++;  // Invalid
                    }
                    break;
                
                case L'u':  // Unicode: \uNNNN
                    if (iswxdigit(pSrc[1]) && iswxdigit(pSrc[2]) &&
                        iswxdigit(pSrc[3]) && iswxdigit(pSrc[4])) {
                        WCHAR unicode[5] = {pSrc[1], pSrc[2], pSrc[3], pSrc[4], 0};
                        *pDst++ = (WCHAR)wcstol(unicode, NULL, 16);
                        pSrc += 5;
                    } else {
                        *pDst++ = L'\\'; *pDst++ = *pSrc++;  // Invalid
                    }
                    break;
                
                default:  // Unknown escape - preserve literally
                    *pDst++ = L'\\';
                    *pDst++ = *pSrc++;
                    break;
            }
        } else {
            *pDst++ = *pSrc++;  // Copy normal character
        }
    }
    
    *pDst = L'\0';
    return pszOutput;  // Caller must free!
}
```

**Escape Sequences Supported:**
- `\n` - Line feed (LF)
- `\r` - Carriage return (CR)
- `\t` - Tab
- `\\` - Backslash
- `\xNN` - Hex byte (00-FF) e.g., `\x41` = 'A'
- `\uNNNN` - Unicode codepoint (0000-FFFF) e.g., `\u00E9` = 'é'
- Unknown escapes preserved literally (e.g., `\q` → `\q`)

**Memory Management:**
- Returns malloc'd memory - **caller must free!**
- Used in DoFind() and LoadTemplates() (both free after use)

### Critical Pattern: RichEdit EM_FINDTEXTEXW Search

**Problem Solved:** Need case-sensitive, whole-word, and forward/backward search in RichEdit control.

**Solution:** Use EM_FINDTEXTEXW message with FINDTEXTEXW structure

**FindTextInDocument() Implementation (~line 1675):**
```cpp
LONG FindTextInDocument(LPCWSTR pszSearchText, BOOL bMatchCase, 
                        BOOL bWholeWord, BOOL bSearchDown, LONG nStartPos)
{
    if (!pszSearchText || !pszSearchText[0]) {
        return -1;  // Empty search
    }
    
    FINDTEXTEXW ft = {0};
    
    // Set search range
    if (bSearchDown) {
        ft.chrg.cpMin = nStartPos;
        ft.chrg.cpMax = -1;  // -1 = end of document
    } else {
        ft.chrg.cpMin = nStartPos;
        ft.chrg.cpMax = 0;   // Search backward to start
    }
    
    ft.lpstrText = pszSearchText;
    
    // Build flags
    DWORD dwFlags = 0;
    if (bMatchCase) dwFlags |= FR_MATCHCASE;
    if (bWholeWord) dwFlags |= FR_WHOLEWORD;
    if (!bSearchDown) dwFlags |= FR_DOWN;  // Backward search
    
    // Perform search
    LRESULT result = SendMessage(g_hWndEdit, EM_FINDTEXTEXW, dwFlags, (LPARAM)&ft);
    
    if (result != -1) {
        return ft.chrgText.cpMin;  // Return match start position
    }
    
    return -1;  // Not found
}
```

**Why EM_FINDTEXTEXW:**
- Built-in RichEdit functionality (fast, reliable)
- Handles Unicode correctly (wide strings)
- Supports all needed flags (case, whole word, direction)
- Returns exact match position in ft.chrgText

### Critical Pattern: MRU History Management

**Problem Solved:** Store up to 20 most recent searches, newest first, no duplicates.

**Solution:** AddToFindHistory() with deduplication and rotation

**AddToFindHistory() Implementation (~line 1951):**
```cpp
void AddToFindHistory(LPCWSTR pszText) {
    if (!pszText || !pszText[0]) return;
    
    // Check if already in history (remove duplicates)
    for (int i = 0; i < g_nFindHistoryCount; i++) {
        if (wcscmp(g_szFindHistory[i], pszText) == 0) {
            // Move to front
            WCHAR szTemp[MAX_SEARCH_TEXT];
            wcscpy(szTemp, g_szFindHistory[i]);
            
            // Shift items down
            for (int j = i; j > 0; j--) {
                wcscpy(g_szFindHistory[j], g_szFindHistory[j - 1]);
            }
            
            wcscpy(g_szFindHistory[0], szTemp);
            return;
        }
    }
    
    // Not in history - add to front
    if (g_nFindHistoryCount < MAX_FIND_HISTORY) {
        g_nFindHistoryCount++;
    }
    
    // Shift all items down
    for (int i = g_nFindHistoryCount - 1; i > 0; i--) {
        wcscpy(g_szFindHistory[i], g_szFindHistory[i - 1]);
    }
    
    wcscpy(g_szFindHistory[0], pszText);
}
```

**Behavior:**
- Duplicate entries moved to front (MRU)
- Max 20 items (oldest dropped)
- Newest always at index 0

### Global Variables (Phase 2.9.1)

```cpp
// Search System (Phase 2.9)
#define MAX_FIND_HISTORY 20
#define MAX_SEARCH_TEXT 256

HWND g_hDlgFind = NULL;
WCHAR g_szFindWhat[MAX_SEARCH_TEXT] = L"";
BOOL g_bFindMatchCase = FALSE;
BOOL g_bFindWholeWord = FALSE;
BOOL g_bFindUseEscapes = FALSE;
BOOL g_bSearchDown = TRUE;
BOOL g_bSelectAfterFind = TRUE;
WCHAR g_szFindHistory[MAX_FIND_HISTORY][MAX_SEARCH_TEXT];
int g_nFindHistoryCount = 0;
```

**Line Numbers:** Global declarations around line 310 in main.cpp

### Key Functions (Phase 2.9.1)

**Core Search Functions:**
- `ParseEscapeSequences()` - Enhanced escape parsing with \xNN and \uNNNN (~line 735)
- `FindTextInDocument()` - RichEdit search with EM_FINDTEXTEXW (~line 1675)
- `DoFind()` - Main find logic with escape parsing and selection (~line 1716)

**Dialog Management:**
- `DlgFindProc()` - Modeless dialog procedure (~line 1818)
- `LoadFindHistory()` - Load history from INI [FindHistory] section (~line 1884)
- `SaveFindHistory()` - Save history and checkbox states (~line 1915)
- `AddToFindHistory()` - MRU list management with deduplication (~line 1951)

### INI Configuration (Phase 2.9.1)

```ini
[Settings]
SelectAfterFind=1             ; 1=select match, 0=move cursor only
FindMatchCase=0               ; Checkbox state persistence
FindWholeWord=0
FindUseEscapes=0

[FindHistory]
Count=3
Item0=function
Item1=\n\n
Item2=TODO
```

**History Format:**
- Count field specifies number of items
- Item0 is most recent (MRU)
- Max 20 items saved
- Loaded on dialog open, saved on main window destroy

### Important Implementation Notes

**Modeless Dialog Message Handling:**
```cpp
// WRONG order (breaks Tab/Enter):
if (!TranslateAccelerator(...)) {
    if (g_hDlgFind && IsDialogMessage(...)) ...
}

// CORRECT order:
if (g_hDlgFind && IsDialogMessage(...)) {
    continue;  // Must be BEFORE TranslateAccelerator
}
if (!TranslateAccelerator(...)) ...
```

**SelectAfterFind Behavior:**
```cpp
if (g_bSelectAfterFind) {
    // Select the found text
    cr.cpMin = foundPos;
    cr.cpMax = foundPos + matchLength;
} else {
    // Position cursor based on search direction (bidirectional smart positioning)
    if (bSearchDown) {
        // Forward search: position cursor after match
        cr.cpMin = foundPos + matchLength;
        cr.cpMax = foundPos + matchLength;
    } else {
        // Backward search: position cursor before match
        cr.cpMin = foundPos;
        cr.cpMax = foundPos;
    }
}
SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
SendMessage(g_hWndEdit, EM_SCROLLCARET, 0, 0);
```

**Why bidirectional cursor positioning when SelectAfterFind=0:**
- **Forward search (F3):** Cursor positioned **after** match → next F3 finds next occurrence
- **Backward search (Shift+F3):** Cursor positioned **before** match → next Shift+F3 finds previous occurrence
- Without this logic, pressing F3 or Shift+F3 would find the same match repeatedly (infinite loop)
- This matches PSPad's smart behavior for non-selecting search navigation

**No Search Wrapping:**
```cpp
// User requirement: Do NOT wrap around document
if (foundPos < 0) {
    // Show "not found" message, don't continue from start
    WCHAR szMsg[512];
    WCHAR szPrefix[64];
    LoadString(GetModuleHandle(NULL), IDS_FIND_NOTFOUND_PREFIX, szPrefix, 64);
    wcscpy(szMsg, szPrefix);
    wcscat(szMsg, pszSearchText);
    wcscat(szMsg, L"\"");
    
    WCHAR szTitle[64];
    LoadString(GetModuleHandle(NULL), IDS_FIND_NOTFOUND_TITLE, szTitle, 64);
    MessageBox(g_hDlgFind, szMsg, szTitle, MB_OK | MB_ICONINFORMATION);
}
```

### String Resources (Phase 2.9.1)

```cpp
#define IDS_FIND_TITLE                  2140
#define IDS_FIND_WHAT                   2141
#define IDS_MATCH_CASE                  2142
#define IDS_WHOLE_WORD                  2143
#define IDS_USE_ESCAPES                 2144
#define IDS_FIND_NEXT_BTN               2145
#define IDS_FIND_PREV_BTN               2146
#define IDS_FIND_NOTFOUND_PREFIX        2147
#define IDS_FIND_NOTFOUND_TITLE         2148
```

**Localization:**
- English: "Find", "Match &case", "Find &Next →", etc.
- Czech: "Najít", "Rozlišovat &velikost písmen", "&Další →", etc.

### Important Don'ts (Phase 2.9.1 Specific)

1. **Never call IsDialogMessage after TranslateAccelerator** - Tab/Enter won't work
2. **Never forget to free ParseEscapeSequences result** - Returns malloc'd memory
3. **Never wrap search around document** - User requirement, show "not found" instead
4. **Never use DestroyWindow in dialog close button** - Use ShowWindow(SW_HIDE)
5. **Never forget SaveFindHistory in WM_DESTROY** - History lost if not saved
6. **Never create dialog multiple times** - Check g_hDlgFind != NULL first
7. **Never assume checkbox states are synced** - Always read from dialog controls in DoFind()

## Phase 2.10: Configurable Date/Time Format (Important Patterns)

### Overview

Phase 2.10 implements customizable date/time formatting for the F5 key and template variables. Users can configure formats using either internal variables (locale-aware dwFlags) or custom format strings (Windows GetDateFormatEx/GetTimeFormatEx).

**Key Design Goals:**
- Locale-aware formatting with internal variables
- Custom format strings for power users (ISO dates, European formats, etc.)
- F5 key uses template system (can combine multiple variables)
- Backward-compatible (defaults match old hardcoded behavior)
- No `%datetime%` variable (users combine `%date% %time%` manually)

### Date/Time System Architecture

**Two Variable Types:**

**1. Internal Variables (Predefined, dwFlags-based):**
- `%shortdate%` → `DATE_SHORTDATE` (e.g., "1/20/2026")
- `%longdate%` → `DATE_LONGDATE` (e.g., "Monday, January 20, 2026")
- `%yearmonth%` → `DATE_YEARMONTH` (e.g., "January 2026")
- `%monthday%` → `DATE_MONTHDAY` (e.g., "January 20")
- `%longtime%` → 0 flag (e.g., "10:30:45 PM")
- `%shorttime%` → `TIME_NOSECONDS` (e.g., "10:30 PM")

**2. User-Configurable Variables:**
- `%date%` → Uses `DateFormat=` INI setting
- `%time%` → Uses `TimeFormat=` INI setting

**Global State:**
```cpp
WCHAR g_szDateTimeTemplate[256] = L"%shortdate% %shorttime%";  // F5 key template
WCHAR g_szDateFormat[128] = L"%shortdate%";                    // %date% variable format
WCHAR g_szTimeFormat[128] = L"HH:mm";                          // %time% variable format
```

**Line Numbers:** Global declarations around line 332 in main.cpp

### Critical Pattern: Dual-Mode Variable Expansion

**Problem Solved:** Support both internal variables (dwFlags) and custom format strings for the same variable (`%date%`, `%time%`).

**Solution:** Check if format matches internal variable name first, otherwise use custom format string.

**Implementation in ExpandTemplateVariables() (~line 1376):**
```cpp
else if (wcsncmp(pSrc, L"%date%", 6) == 0) {
    // Check if DateFormat is an internal variable
    if (wcscmp(g_szDateFormat, L"%shortdate%") == 0) {
        pszFormatted = FormatDateByFlag(DATE_SHORTDATE);
    } else if (wcscmp(g_szDateFormat, L"%longdate%") == 0) {
        pszFormatted = FormatDateByFlag(DATE_LONGDATE);
    } else if (wcscmp(g_szDateFormat, L"%yearmonth%") == 0) {
        pszFormatted = FormatDateByFlag(DATE_YEARMONTH);
    } else if (wcscmp(g_szDateFormat, L"%monthday%") == 0) {
        pszFormatted = FormatDateByFlag(DATE_MONTHDAY);
    } else {
        // Custom format string
        pszFormatted = FormatDateByString(g_szDateFormat);
    }
    wcscpy(pDest, pszFormatted);
    pDest += wcslen(pszFormatted);
    free(pszFormatted);
    pSrc += 6;
}
```

**Why This Pattern:**
- Internal variables are locale-aware (respects user's Windows settings)
- Custom format strings allow precise control (ISO dates, European formats)
- Exclusive check (either internal OR custom, not mixed)
- Falls back gracefully if GetDateFormatEx fails

### Critical Pattern: Helper Function Fallbacks

**Problem Solved:** Windows API calls might fail in catastrophic scenarios. How to handle?

**Solution:** No fallback - functions always succeed with valid inputs.

**Implementation (~line 1102-1180):**
```cpp
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
    // No error checking - with valid SYSTEMTIME and locale constant, this always succeeds
}

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
    // No error checking - per Microsoft docs: "returns no errors for bad format strings"
}
```

**Why No Fallback:**
- **Invalid format strings don't cause API errors** - Per Microsoft: "returns no errors for bad format string, just forms best possible date/time string"
- **Invalid formats produce garbage output** - e.g., `INVALID_FORMAT` → `INVALID_FOR1AT` (M replaced with month)
- **All parameters are guaranteed valid:**
  - Locale: `LOCALE_NAME_USER_DEFAULT` (always valid constant)
  - SYSTEMTIME: From `GetLocalTime()` (always valid)
  - Buffer: 128 bytes (sufficient for any date/time)
  - Flags: Validated constants (DATE_SHORTDATE, etc.)
- **Only catastrophic failures cause errors** - Out of memory, corrupted locale data (unrealistic)
- **Honest behavior** - Garbage in = garbage out (user sees what Windows produces)

**Error Conditions That Would Return 0:**
- ERROR_INSUFFICIENT_BUFFER - Impossible (128-byte buffer always sufficient)
- ERROR_INVALID_FLAGS - Impossible (we use validated constants)
- ERROR_INVALID_PARAMETER - Impossible (GetLocalTime always returns valid SYSTEMTIME)
- ERROR_OUTOFMEMORY - Unrealistic (catastrophic system failure)

**Empty Format Handling:**
- Empty `DateFormat=` → Uses `FormatDateByFlag(DATE_SHORTDATE)` (not an error)
- Empty `TimeFormat=` → Uses `GetTimeFormatEx` with `TIME_NOSECONDS` and `"HH:mm"` format

### Critical Pattern: F5 Key Integration

**Problem Solved:** Old EditInsertTimeDate() hardcoded format. New system should use template expansion.

**Solution:** Replace entire function with template system call.

**Old Code (20 lines):**
```cpp
void EditInsertTimeDate() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    WCHAR szDate[128];
    GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, szDate, 128);
    
    WCHAR szTime[128];
    GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, szTime, 128);
    
    WCHAR szDateTime[256];
    swprintf(szDateTime, 256, L"%s %s", szDate, szTime);
    
    SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)szDateTime);
    g_bModified = TRUE;
    UpdateTitle();
}
```

**New Code (8 lines, ~line 5677):**
```cpp
void EditInsertTimeDate() {
    LONG nCursorOffset = -1;
    LPWSTR pszExpanded = ExpandTemplateVariables(g_szDateTimeTemplate, &nCursorOffset);
    if (pszExpanded) {
        SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszExpanded);
        free(pszExpanded);
        g_bModified = TRUE;
        UpdateTitle();
    }
}
```

**Why This Pattern:**
- Reuses existing template expansion infrastructure
- F5 can now use ANY variables (date, time, selection, clipboard, cursor, etc.)
- User can customize via `DateTimeTemplate=` in INI
- Supports literal text: `DateTimeTemplate='Today is '%longdate%`
- **Default uses configurable variables:** F5 respects DateFormat/TimeFormat by default
- No code duplication

### Global Variables (Phase 2.10)

```cpp
// Date/Time Formatting (Phase 2.10, ToDo #3)
WCHAR g_szDateTimeTemplate[256] = L"%date% %time%";            // F5 key format (uses configurable variables)
WCHAR g_szDateFormat[128] = L"%shortdate%";                    // %date% variable
WCHAR g_szTimeFormat[128] = L"HH:mm";                          // %time% variable
```

**Line Numbers:** Global declarations around line 332 in main.cpp

### Key Functions (Phase 2.10)

**Helper Functions (~line 1090):**
- `FormatDateByFlag()` - Format date using dwFlags (DATE_SHORTDATE, etc.) (~30 lines)
- `FormatTimeByFlag()` - Format time using dwFlags (TIME_NOSECONDS, etc.) (~30 lines)
- `FormatDateByString()` - Format date using custom format string (~30 lines)
- `FormatTimeByString()` - Format time using custom format string (~30 lines)

**Variable Expansion:**
- Modified `ExpandTemplateVariables()` - Added 6 internal variables + 2 configurable (~90 lines added around line 1298-1420)

**F5 Key Handler:**
- Refactored `EditInsertTimeDate()` - Now uses template system (~line 5677, reduced from 20 to 8 lines)

### INI Configuration (Phase 2.10)

```ini
[Settings]
; F5 key / Edit→Time/Date menu insertion format
DateTimeTemplate=%date% %time%

; Custom format strings for %date% and %time% variables
; Can be either:
;   - Internal variable (exclusive): %shortdate%, %longdate%, etc.
;   - Custom format string: yyyy-MM-dd, dd.MM.yyyy, HH:mm, h:mm tt, etc.
DateFormat=%shortdate%        ; Default: system short date
TimeFormat=HH:mm              ; Default: 24-hour without seconds
```

**Design Principle:**
- **Single source of truth:** Change `DateFormat=` and `TimeFormat=`, F5 automatically uses them
- **Intuitive behavior:** User customizes date/time format once, affects F5 and all templates
- **Override when needed:** Power users can set `DateTimeTemplate=%longdate%` to override default

**Format Behavior:**

**Exclusive Internal Variable Mode:**
- If `DateFormat=%longdate%` (exact match) → uses DATE_LONGDATE flag
- No mixing allowed (e.g., `%longdate% extra text` treated as custom format, will fail)

**Custom Format String Mode:**
- If `DateFormat=yyyy-MM-dd` (not internal variable) → custom format
- Supports literal text: `'Day 'dd' of 'MMMM` → "Day 20 of January"
- Windows API handles all specifiers + literals
- See Microsoft documentation for full list

**Available Format Specifiers:**

**Date (GetDateFormatEx):**
- `d` - Day of month (1-31)
- `dd` - Day of month with leading zero (01-31)
- `ddd` - Abbreviated day name (Mon, Tue, etc.)
- `dddd` - Full day name (Monday, Tuesday, etc.)
- `M` - Month (1-12)
- `MM` - Month with leading zero (01-12)
- `MMM` - Abbreviated month name (Jan, Feb, etc.)
- `MMMM` - Full month name (January, February, etc.)
- `y` - Year without century (0-99)
- `yy` - Year without century, leading zero (00-99)
- `yyyy` - Year with century (2026)
- `g`, `gg` - Era string (AD, BC in English)

**Time (GetTimeFormatEx):**
- `h` - Hour 12-hour format (1-12)
- `hh` - Hour 12-hour format with leading zero (01-12)
- `H` - Hour 24-hour format (0-23)
- `HH` - Hour 24-hour format with leading zero (00-23)
- `m` - Minute (0-59)
- `mm` - Minute with leading zero (00-59)
- `s` - Second (0-59)
- `ss` - Second with leading zero (00-59)
- `t` - Single-character AM/PM (A/P)
- `tt` - Multi-character AM/PM (AM/PM)

**Literal Text:**
- Enclose in single quotes: `'Day 'dd' of 'MMMM`
- Escape single quote with double: `''` → `'`

### Important Implementation Notes

**Internal Variable Expansion Order:**
```cpp
// Check internal variables FIRST (in ExpandTemplateVariables)
// Order in function (~line 1298):
1. %shortdate%   (line ~1298)
2. %longdate%    (line ~1312)
3. %yearmonth%   (line ~1326)
4. %monthday%    (line ~1340)
5. %longtime%    (line ~1354)
6. %shorttime%   (line ~1362)
7. %date%        (line ~1376) - Uses g_szDateFormat
8. %time%        (line ~1398) - Uses g_szTimeFormat
```

**Memory Management:**
```cpp
// All helper functions return malloc'd memory - CALLER MUST FREE!
LPWSTR pszDate = FormatDateByFlag(DATE_SHORTDATE);
wcscpy(pDest, pszDate);
free(pszDate);  // CRITICAL: Don't leak memory!
```

**dwFlags Constants:**
```cpp
// From winnls.h (Windows API)
#define DATE_SHORTDATE     0x00000001  // Short date format
#define DATE_LONGDATE      0x00000002  // Long date format
#define DATE_YEARMONTH     0x00000008  // Year-month format
#define DATE_MONTHDAY      0x00000080  // Month-day format
#define TIME_NOSECONDS     0x00000002  // Time without seconds
// 0 flag for time = include seconds
```

**Locale Awareness:**
- All internal variables respect user's Windows locale settings
- `LOCALE_NAME_USER_DEFAULT` used in GetDateFormatEx/GetTimeFormatEx
- Custom format strings also use user locale (month/day names translated)
- Example: Czech user with %longdate% sees "pondělí 20. ledna 2026"

### Important Don'ts (Phase 2.10 Specific)

1. **Never use swprintf for string concatenation** - Use wcscpy/wcscat (MinGW issues)
2. **Never forget to free helper function results** - All return malloc'd memory (where applicable)
3. **Never add fallback code for GetDateFormatEx/GetTimeFormatEx** - APIs don't fail for invalid format strings (see Critical Pattern above)
4. **Never mix internal variables with text in INI format settings** - Either `%shortdate%` OR `yyyy-MM-dd`, not both
5. **Never use %datetime% variable** - Removed per user request, users combine manually
6. **Never hardcode date/time in EditInsertTimeDate()** - Use template system
7. **Never validate format strings** - Let Windows handle them; garbage in = garbage out (honest behavior)

### Testing Checklist (Phase 2.10)

**Basic Functionality:**
- [ ] F5 inserts date/time with default template `%shortdate% %shorttime%`
- [ ] Each of 6 internal variables works individually in templates
- [ ] %date%, %time% work with default settings

**Internal Variable Mode:**
- [ ] DateFormat=%shortdate% → uses DATE_SHORTDATE flag (e.g., "1/20/2026")
- [ ] DateFormat=%longdate% → uses DATE_LONGDATE flag (e.g., "Monday, January 20, 2026")
- [ ] DateFormat=%yearmonth% → uses DATE_YEARMONTH flag (e.g., "January 2026")
- [ ] DateFormat=%monthday% → uses DATE_MONTHDAY flag (e.g., "January 20")
- [ ] TimeFormat=%longtime% → includes seconds (e.g., "10:30:45 PM")
- [ ] TimeFormat=%shorttime% → excludes seconds (e.g., "10:30 PM")

**Custom Format Mode:**
- [ ] DateFormat=yyyy-MM-dd → ISO format (e.g., "2026-01-20")
- [ ] DateFormat=dd.MM.yyyy → European format (e.g., "20.01.2026")
- [ ] DateFormat=MMMM d, yyyy → US format (e.g., "January 20, 2026")
- [ ] TimeFormat=HH:mm → 24-hour without seconds (e.g., "22:30")
- [ ] TimeFormat=h:mm tt → 12-hour with AM/PM (e.g., "10:30 PM")
- [ ] TimeFormat=HH:mm:ss → 24-hour with seconds (e.g., "22:30:45")

**Literal Text:**
- [ ] DateFormat='Day 'dd → "Day 20"
- [ ] TimeFormat='at 'HH:mm → "at 22:30"
- [ ] DateTimeTemplate='Today: '%longdate%' at '%shorttime% → "Today: Monday, January 20, 2026 at 10:30 PM"

**Edge Cases:**
- [ ] Empty DateFormat= → falls back to %shortdate%
- [ ] Empty TimeFormat= → falls back to HH:mm
- [ ] Invalid format string → produces garbage output (e.g., "INVALID_FORMAT" → "INVALID_FOR1AT")
- [ ] Very long DateTimeTemplate (200+ chars) → doesn't crash

**Locale Testing:**
- [ ] Test on English (US) locale
- [ ] Test on Czech locale (month/day names translated)
- [ ] Test on other locale (German, French, etc.)

**Template Integration:**
- [ ] F5 key can use %cursor% placeholder
- [ ] F5 key can use %selection% variable
- [ ] F5 key can use %clipboard% variable
- [ ] Template system: Insert Template menu items use new %date%/%time%

### Bugs Fixed During Implementation

1. ✅ **Old EditInsertTimeDate() hardcoded format** - Now uses template system
2. ✅ **No locale awareness for custom formats** - Uses LOCALE_NAME_USER_DEFAULT
3. ✅ **Invalid fallback assumption** - Removed fallback code; Windows APIs don't fail for invalid format strings
4. ✅ **Memory leaks in variable expansion** - All malloc results freed properly
5. ✅ **No way to combine date+time in F5** - DateTimeTemplate supports any combination
6. ✅ **%datetime% variable confusion** - Removed, users combine manually

## Common Tasks

### Adding a New Setting

1. **Add global variable:**
   ```cpp
   BOOL g_bNewSetting = TRUE;  // Default value
   ```

2. **Update CreateDefaultINI():**
   ```cpp
   "NewSetting=1              ; Comment explaining the setting\r\n"
   ```

3. **Update LoadSettings():**
   ```cpp
   ReadINIValue(szIniPath, L"Settings", L"NewSetting", szValue, 256, L"");
   if (szValue[0] == L'\0') {
       WriteINIValue(szIniPath, L"Settings", L"NewSetting", L"1");
       g_bNewSetting = TRUE;
   } else {
       g_bNewSetting = ReadINIInt(szIniPath, L"Settings", L"NewSetting", 1);
   }
   ```

4. **Update README.md** with documentation

### Adding a New Filter Action Type

1. **Add enum value:**
   ```cpp
   enum FilterAction {
       FILTER_ACTION_INSERT = 0,
       FILTER_ACTION_DISPLAY = 1,
       FILTER_ACTION_CLIPBOARD = 2,
       FILTER_ACTION_NONE = 3,
       FILTER_ACTION_YOURNEW = 4  // Add here
   };
   ```

2. **Add mode enum if needed:**
   ```cpp
   enum FilterYourNewMode {
       FILTER_YOURNEW_MODE1 = 0,
       FILTER_YOURNEW_MODE2 = 1
   };
   ```

3. **Update FilterInfo struct:**
   ```cpp
   struct FilterInfo {
       // ... existing fields ...
       FilterYourNewMode yourNewMode;
   };
   ```

4. **Update LoadFilters()** to parse the new action

5. **Add ExecuteFilterYourNew() function**

6. **Update ExecuteFilter()** to call your handler

7. **Update README.md** and CreateDefaultINI() with examples

### Adding a Menu Item

1. **Add resource ID in resource.h:**
   ```cpp
   #define ID_YOUR_COMMAND 1234
   ```

2. **Add to menu in resource.rc:**
   ```rc
   MENUITEM "Your Command\tCtrl+Key", ID_YOUR_COMMAND
   ```

3. **Add to accelerator table (if keyboard shortcut):**
   ```rc
   VK_KEY, ID_YOUR_COMMAND, VIRTKEY, CONTROL
   ```

4. **Handle in WndProc:**
   ```cpp
   case WM_COMMAND:
       switch (LOWORD(wParam)) {
           case ID_YOUR_COMMAND:
               YourFunction();
               return 0;
       }
   ```

5. **Localize in both LANG_ENGLISH and LANG_CZECH sections**

## Testing

### Build Test
```bash
cd /home/peer/RichEditor
make clean
make
```

Expected output: `RichEditor.exe` (~1.15 MB) with no warnings.

### Manual Testing Checklist

After making changes:

**Basic Functionality:**
- [ ] New file, Open, Save, Save As work
- [ ] Cut, Copy, Paste, Undo, Redo work
- [ ] Word wrap toggle works (Ctrl+W)
- [ ] Status bar updates correctly
- [ ] Modified state tracked (asterisk in title)

**Filter System:**
- [ ] Tools → Select Filter shows categorized menu
- [ ] Right-click shows context menu with filters
- [ ] Ctrl+Enter executes filter
- [ ] All action types work (insert, display, clipboard, none)

**Accessibility:**
- [ ] Menu items show descriptions when ShowMenuDescriptions=1
- [ ] Menu items show only names when ShowMenuDescriptions=0
- [ ] Colon-space separator used: "Name: Description"

**INI File:**
- [ ] Delete INI file, run program → creates new INI with all settings
- [ ] Missing settings are auto-added with defaults
- [ ] All 8 example filters are created

**Localization:**
- [ ] Czech diacritics display correctly (not garbled)
- [ ] Both English and Czech strings present in binary

## Known Issues and Limitations

### Current Limitations

- No Replace functionality (planned for Phase 2.9.2)
- No Bookmark system (planned for Phase 2.9.3)
- No Go to Line dialog (planned for Phase 2.9.4)
- No font selection dialog
- No print support
- Plain text only (no RTF formatting used)
- Single document interface (no tabs/MDI)
- Filter timeout fixed at 30 seconds

### Platform-Specific Notes

**Windows 7+:**
- Requires Msftedit.dll (RichEdit 4.1)
- Included in Windows Vista and later

**UNC Paths:**
- Fully supported for file operations
- Custom INI functions used (GetPrivateProfile* APIs don't work reliably)

**Unicode:**
- UTF-8 without BOM for text files
- Full Unicode support including emoji and surrogate pairs
- Alt+X conversion works for all Unicode planes

## Git Workflow

### Files to Never Commit

```gitignore
*.ini           # Auto-generated
*.exe           # Build artifact
*.o             # Object files
*.lnk           # Windows shortcuts
```

### Commit Message Style

Follow existing patterns:

```
Add feature X with Y support

- Bullet point for each major change
- Implementation details
- User-facing improvements
- Documentation updates
```

Examples from history:
- "Add accessibility features with configurable menu descriptions"
- "Document command-line argument support in README"
- "Add MRU (Most Recently Used) file list feature"

### Before Committing

```bash
# Build test
make clean && make

# Check what's staged
git status
git diff --cached --stat

# Review changes
git diff --cached

# Commit (no INI file!)
git add .gitignore README.md src/main.cpp
git commit -m "Your message"
```

## Important Don'ts

1. **Never commit RichEditor.ini** - It's auto-generated
2. **Never use `swprintf` for string concatenation** - Use wcscpy/wcscat
3. **Never rely on status bar alone for accessibility** - Use menu item text
4. **Never use GetPrivateProfile* APIs** - Use custom INI functions for UNC support
5. **Never forget the UTF-8 BOM pragma in RC file** - Czech will break
6. **Never hardcode paths** - Always use EXTENDED_PATH_MAX for UNC support
7. **Never skip LoadSettings auto-generation** - All settings must appear in INI
8. **Never add filters without descriptions** - Accessibility requirement
9. **Never use tooltips on menu items** - Not supported in standard Win32 menus
10. **Never implement owner-drawn menus** - Screen reader support is unreliable

## Useful Resources

### Windows API References

- RichEdit Control: https://learn.microsoft.com/en-us/windows/win32/controls/rich-edit-controls
- Common Controls: https://learn.microsoft.com/en-us/windows/win32/controls/common-controls-intro
- Menus: https://learn.microsoft.com/en-us/windows/win32/menurc/menus

### Project-Specific

- README.md: Comprehensive user and developer documentation
- resource.h: All resource IDs
- Makefile: Build configuration and flags

### Testing Accessibility

- NVDA: https://www.nvaccess.org/ (free, open source)
- Windows Narrator: Built into Windows (Win+Ctrl+Enter)
- JAWS: https://www.freedomscientific.com/products/software/jaws/ (commercial)

## Questions to Ask Before Making Changes

1. **Does this affect accessibility?**
   - Will screen readers still work correctly?
   - Are menu descriptions still announced?

2. **Does this work with UNC paths?**
   - Are you using custom INI functions?
   - Is EXTENDED_PATH_MAX used for buffers?

3. **Is this localized?**
   - Are strings in both LANG_ENGLISH and LANG_CZECH?
   - Is UTF-8 BOM pragma present?

4. **Is this documented?**
   - README.md updated?
   - INI file comments added?
   - Example filters provided?

5. **Is this self-documenting?**
   - Settings auto-written to INI?
   - Validation error messages helpful?

6. **Does this maintain the single-file architecture?**
   - All code in main.cpp?
   - No new dependencies?

## Contact

For questions about this project, refer to:
- README.md for user-facing documentation
- Git commit history for implementation rationale
- This file (AGENTS.md) for AI agent guidance

---

**Last Updated:** January 2026  
**Project Version:** Phase 2.9.1 Complete (Find Dialog with Escape Sequences)  
**Code Size:** ~8,050 lines (main.cpp)  
**Binary Size:** ~314 KB (MinGW stripped), ~962 KB (MinGW with symbols)

