# RichEditor

A lightweight, accessible Win32 text editor built with the RichEdit 4.1 control and MinGW-w64.

## Features

### Phase 1 (Complete)

**Core Editing:**
- Plain text editing with UTF-8 support (no BOM)
- File operations: New, Open, Save, Save As
- Edit operations: Undo, Redo, Cut, Copy, Paste, Select All
- Time/Date insertion (F5) with locale-specific formatting
- Full keyboard shortcut support
- Accessibility support for screen readers (MSAA/UI Automation)

**Display & Navigation:**
- Status bar showing:
  - File name (or "Untitled")
  - Line and column position
  - Character at cursor (decimal and hex: e.g., 'A' Dec: 65, Hex: 0x0041)
  - Current filter status (placeholder for Phase 2)
- Word wrap toggle (Ctrl+W) with dual position display:
  - Visual position: includes soft-wrapped lines
  - Physical position: actual line/column in file
  - Format: `Ln X, Col Y / A,B` when word wrap is on
- Modified state tracking with asterisk (*) in title bar
- Proper focus management (returns to editor after dialogs)

**Autosave:**
- Timer-based autosave (default: 1 minute interval)
- Autosave on focus loss (when switching to another application)
- Only autosaves files with a filename (skips "Untitled")
- Configurable interval via `g_nAutosaveIntervalMinutes` constant

**File Format:**
- UTF-8 encoding without BOM
- Handles Windows (CRLF), Unix (LF), and Mac (CR) line endings
- Default save format: Windows CRLF

### Phase 2 (Planned)

**Filter System:**
- External filter/utility execution (Ctrl+Enter)
- Multiple configurable filters (calculator, scripts, AI agents, etc.)
- Process input/output via stdin/stdout pipes
- INI-based filter configuration (`RichEditor.ini`)
- Dynamic filter menu populated from configuration
- Error handling with stderr capture
- Selected text (or current line if no selection) sent to filter
- Filter output inserted below input

**Example Use Cases:**
- Calculator: `2 + 2 + 3` → `7`
- Text processing: uppercase, lowercase, sorting
- Code execution: Python, PowerShell one-liners
- AI text generation/completion
- Custom utilities and scripts

### Future Phases (Ideas)
- Find/Replace functionality
- Font selection dialog
- Print support
- Multiple document interface (MDI) or tabs
- RTF file format support
- Markdown syntax highlighting and preview
- Plugin system
- Configurable themes/colors

## Building

### Requirements
- **Build Environment:** MinGW-w64 cross-compiler (e.g., MXE, MSYS2)
- **Compiler:** x86_64-w64-mingw32-g++ or i686-w64-mingw32-g++
- **Target OS:** Windows 7 or later
- **Runtime Dependency:** Msftedit.dll (RichEdit 4.1, included in Windows Vista+)

### Build Commands

**Standard build (with MXE cross-compiler):**
```bash
make CROSS=x86_64-w64-mingw32.static-
```

**32-bit build:**
```bash
make CROSS=i686-w64-mingw32.static-
```

**Native Windows build (MSYS2/MinGW-w64):**
```bash
make
```

**Czech language build:**
```bash
make CROSS=x86_64-w64-mingw32.static- LANG=cs
# or
make czech CROSS=x86_64-w64-mingw32.static-
```

**Clean build:**
```bash
make clean
make CROSS=x86_64-w64-mingw32.static-
```

**Output:** `RichEditor.exe` (standalone static executable, ~166KB)

### Localization

The application supports multiple languages through separate resource files:
- **English (default):** `src/resource.rc` - Language ID: 0x0409 (en-US)
- **Czech:** `src/resource_cs.rc` - Language ID: 0x0405 (cs-CZ)

To build a localized version, use the `LANG` parameter:
- English: `make` or `make LANG=en`
- Czech: `make LANG=cs`

All UI elements are localized:
- Menus (File, Edit, View, Tools, Help)
- Dialogs (About dialog)
- Version information strings
- Keyboard shortcuts remain the same across all languages

### Build Configuration

The `Makefile` uses:
- `-O2` optimization
- `-std=c++11` standard
- Static linking (`-static -static-libgcc -static-libstdc++`)
- Unicode support (`-DUNICODE -D_UNICODE -municode`)
- Required libraries: `comctl32`, `comdlg32`, `ole32`, `oleaut32`, `shell32`

## Usage

### Keyboard Shortcuts
| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New file |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save file |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+X` | Cut |
| `Ctrl+C` | Copy |
| `Ctrl+V` | Paste |
| `Ctrl+A` | Select all |
| `Ctrl+W` | Toggle word wrap |
| `F5` | Insert time/date |
| `Ctrl+Enter` | Execute filter (Phase 2) |
| `Alt+F4` | Exit |

### Configuration

**Autosave Settings** (in `src/main.cpp`):
```cpp
// Line 28: Autosave interval in minutes
const int g_nAutosaveIntervalMinutes = 1;

// Line 29: Enable/disable autosave timer
BOOL g_bAutosaveEnabled = TRUE;

// Line 30: Enable/disable autosave on focus loss
BOOL g_bAutosaveOnFocusLoss = TRUE;
```

**Word Wrap Default** (in `src/main.cpp`):
```cpp
// Line 24: Word wrap enabled by default
BOOL g_bWordWrap = TRUE;
```

### Status Bar Information

The status bar displays (from left to right):
1. **Filename:** Current file or "Untitled"
2. **Position:**
   - Word wrap OFF: `Ln X, Col Y`
   - Word wrap ON: `Ln X, Col Y / A,B` (visual / physical)
3. **Character:** Character at cursor (e.g., `Char: 'A' (Dec: 65, Hex: 0x0041)`)
   - Shows `Char: EOF` at end of file
4. **Filter:** `[Filter: None]` (placeholder for Phase 2)

### Word Wrap Position Display

When word wrap is enabled:
- **First number pair** (Ln X, Col Y): Visual position including soft wraps
- **Second number pair** (A,B): Physical position in actual file
- Example: `Ln 12, Col 64 / 11,204`
  - Visual: Line 12 (wrapped), Column 64
  - Physical: Line 11 (actual file), Column 204

## Architecture

### Code Structure
```
RichEditor/
├── src/
│   ├── main.cpp       (~1050 lines) - Main application logic
│   ├── resource.h     - Resource IDs and constants
│   ├── resource.rc    - English resources (menus, dialogs, version info)
│   └── resource_cs.rc - Czech resources (localized UI)
├── Makefile           - Build configuration with language support
├── README.md          - This file
└── .gitignore         - Git ignore patterns
```

### Key Components

**Main Window (`WndProc`):**
- Message handling: WM_CREATE, WM_SIZE, WM_COMMAND, WM_NOTIFY, WM_TIMER
- Focus management: WM_SETFOCUS, WM_KILLFOCUS
- Autosave triggers

**RichEdit Control:**
- Class: `MSFTEDIT_CLASS` (RichEdit 4.1)
- Mode: Plain text (`TM_PLAINTEXT`)
- Event mask: `ENM_CHANGE | ENM_SELCHANGE`
- Undo limit: 100 operations
- Text limit: 2GB (`0x7FFFFFFE`)

**Status Bar:**
- Updated on: `EN_SELCHANGE`, file operations, window activation
- Multiple parts for different information sections

**Autosave Timer:**
- Timer ID: `IDT_AUTOSAVE` (1)
- Interval: Configurable (default 60 seconds)
- Triggered by: WM_TIMER message

### Filter System Architecture (Phase 2)

**INI Configuration (`RichEditor.ini`):**
```ini
[Filters]
Count=3

[Filter1]
Name=Calculator
Command=calc.exe
Description=Mathematical expression evaluator

[Filter2]
Name=Uppercase
Command=powershell -Command "$input | ForEach-Object { $_.ToUpper() }"
Description=Convert text to uppercase

[Filter3]
Name=AI Agent
Command=ai-agent.exe --stdin
Description=AI text processor
```

**Execution Flow:**
1. User selects text (or cursor on line if no selection)
2. Presses Ctrl+Enter (or selects filter from menu)
3. Selected text sent to filter process via stdin
4. Filter processes input and writes to stdout
5. Output inserted below original text
6. Errors (stderr) shown in message box

**Process Management:**
- Uses `CreateProcess` with redirected stdin/stdout/stderr
- Anonymous pipes for IPC
- 30-second timeout per execution
- Asynchronous I/O to prevent blocking
- Proper handle cleanup

## Known Issues & Limitations

### Current Limitations
- No Find/Replace functionality yet
- No font selection (uses system default)
- No print support
- Single document interface (no tabs/MDI)
- Plain text only (no RTF formatting)
- Fixed status bar layout

### Known Issues
- **Word wrap recreates control:** Toggling word wrap recreates the RichEdit control to change styles, which briefly loses and restores focus
- **Autosave behavior:** Autosave triggers on any focus loss, including opening dialogs within the application (by design, but could be refined)
- **Large files:** Performance not tested with files >100MB

### Platform-Specific Notes
- **Windows 7+:** Requires Msftedit.dll (RichEdit 4.1)
- **Screen readers:** Tested with NVDA/JAWS, works with standard RichEdit accessibility
- **High DPI:** Uses system DPI settings, not DPI-aware

## Development Notes

### Commit History
The repository contains ~30 clean, incremental commits documenting the development process:
- Initial Win32 window and RichEdit setup
- File I/O with UTF-8 support
- Edit menu implementation
- Status bar with position and character display
- Autosave with timer and focus-loss triggers
- Word wrap with dual position calculation
- Time/Date insertion feature

### Coding Style
- Hungarian notation for Win32 types (e.g., `hwnd`, `sz`, `g_`)
- Function headers with separator comments
- Descriptive variable names
- Comprehensive comments for complex logic

### Testing Methodology
- Manual testing with various text files
- UTF-8 files with Czech characters (multi-byte UTF-8)
- Long lines for word wrap testing
- Focus management testing with dialogs
- Autosave testing with timer and focus loss

## Future Enhancements

### High Priority (Phase 2)
- [ ] Implement filter system with INI configuration
- [ ] Add process execution with stdin/stdout piping
- [ ] Create dynamic filter menu
- [ ] Integrate calculator utility example
- [ ] Add error handling for filter execution

### Medium Priority
- [ ] Find/Replace dialog
- [ ] Font selection dialog
- [ ] Print support
- [ ] Status bar click to jump to line/column
- [ ] Recent files list (MRU)
- [ ] Drag & drop file support

### Low Priority
- [ ] RTF format support
- [ ] Markdown preview
- [ ] Multiple document interface or tabs
- [ ] Customizable keyboard shortcuts
- [ ] Themes/color schemes
- [ ] Plugin system

## Contributing

This is a personal project for learning Win32 programming and building an accessible text editor. Feel free to use, modify, or learn from the code.

### Code Standards
- Win32 API without frameworks (no MFC/WTL)
- Plain C++ (C++11 standard)
- Static linking for standalone executable
- Unicode-only (no ANSI support)
- Accessibility-first design

## License

(To be determined)

## Author

Created with focus on:
- Accessibility for screen reader users
- Lightweight, standalone executable
- Extensibility through external filters
- Clean, readable Win32 code

---

**Version:** 1.0.0 - Phase 1 Complete (December 2025)  
**Build:** ~166KB static executable  
**Lines of Code:** ~1,248 lines (main.cpp), ~1,526 total  
**Languages:** English, Czech (Čeština)
