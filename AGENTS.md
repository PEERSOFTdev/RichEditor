# AGENTS.md - AI Agent Instructions for RichEditor

This document provides context and guidelines for AI agents working on the RichEditor project.

## Project Overview

**RichEditor** is a lightweight, accessible Win32 text editor built with the RichEdit 4.1 control and MinGW-w64. It features a powerful external filter system that allows text transformation through command-line tools.

**Key Design Principles:**
- Accessibility first (screen reader support is mandatory)
- Single-file architecture (main.cpp is ~3,900 lines)
- Universal binary (English + Czech in one executable)
- UTF-8 without BOM for all text files
- UNC path support throughout
- Static linking (no external DLLs except Windows system libraries)

## Architecture

### File Structure

```
RichEditor/
├── src/
│   ├── main.cpp          # Main application (~3,900 lines)
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

- No Find/Replace functionality (planned for Phase 3)
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

**Last Updated:** December 2025
**Project Version:** Phase 2 Complete (Accessibility Features + Command-Line Options)
**Code Size:** ~3,900 lines (main.cpp)
**Binary Size:** ~933 KB (static)
