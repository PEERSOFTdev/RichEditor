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
  - Current active filter (e.g., "[Filter: Calculator]")
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

### Phase 2 (Complete)

**INI Configuration System:**
- Auto-creation of default `RichEditor.ini` on first run
- 3 example filters included (Uppercase, Lowercase, Line Count)
- All application settings configurable via INI file:
  - Word wrap default state
  - Autosave enable/disable
  - Autosave interval (minutes)
  - Autosave on focus loss behavior
- Filter Help dialog (Tools → Filter Help) with comprehensive documentation
- No need to edit source code for customization

**Filter System:**
- External filter/utility execution (Ctrl+Enter)
- Multiple configurable filters with category organization
- Process input/output via stdin/stdout pipes with UTF-8 encoding
- INI-based filter configuration (`RichEditor.ini`)
- Dynamic categorized menu (Transform, Statistics, Extract, Web)
- Three output modes: Replace, Append, Below
- Error handling with stderr capture and display
- Selected text (or current line if no selection) sent to filter
- Up to 100 filters supported (Tools → Select Filter menu)
- Process timeout (30 seconds) to prevent hanging
- Proper pipe and process handle cleanup

**Example Use Cases:**
- **Calculator**: `2 + 2 + 3` → evaluates to `7`
- **Text transformation**: UPPERCASE, lowercase, Title Case, reverse
- **Line operations**: Sort, remove duplicates, number lines, reverse order
- **Data extraction**: Extract URLs, email addresses from documents
- **Encoding**: Base64 encode/decode
- **Web content**: Download webpage by URL
- **JSON formatting**: Prettify minified JSON
- **Statistics**: Count lines, words, characters
- **Cleanup**: Remove empty lines, trim whitespace
- **Code execution**: Python, PowerShell, Bash one-liners
- **AI integration**: Any AI agent via command-line interface
- **Custom utilities**: Any program that reads stdin and writes stdout

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

**Clean build:**
```bash
make clean
make CROSS=x86_64-w64-mingw32.static-
```

**Output:** `RichEditor.exe` (universal executable with English and Czech, ~752KB)

### Localization

The application is a **universal binary** containing both English and Czech resources in a single executable. Windows automatically selects the appropriate language based on the system's UI language settings.

**Supported Languages:**
- **English (en-US):** Default fallback language - Language ID: 0x0409
- **Czech (cs-CZ):** Full localization - Language ID: 0x0405

**What's Localized:**
- All menus (File/Soubor, Edit/Úpravy, View/Zobrazit, Tools/Nástroje, Help/Nápověda)
- Dialog boxes (About dialog)
- Version information strings (visible in file properties)
- Keyboard shortcuts remain universal (Ctrl+N, Ctrl+S, F5, etc.)

**How It Works:**
- Single executable contains both language resources
- Windows loads the matching language automatically
- If Czech is not available, falls back to English
- No configuration needed - works out of the box

**Technical Implementation:**

The `resource.rc` file uses the following structure:

```rc
#pragma code_page(65001)  // Critical: tells windres to use UTF-8 encoding

LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US
// ... English resources ...

LANGUAGE LANG_CZECH, SUBLANG_DEFAULT
// ... Czech resources with diacritics (ř, č, š, ž, á, í, é, ý, ů) ...
```

**Key Points:**
1. **UTF-8 Encoding:** The RC file must be saved as UTF-8 with BOM
2. **Code Page Pragma:** `#pragma code_page(65001)` is essential for MinGW-w64's `windres` to properly handle non-ASCII characters
3. **Language Sections:** Each `LANGUAGE` directive creates a separate resource set
4. **Automatic Selection:** Windows uses `GetUserDefaultUILanguage()` to pick the right resources at runtime
5. **Proper Encoding:** Czech diacritics are compiled as UTF-16LE in the PE executable (e.g., 'ř' = U+0159 = `59 01` bytes)

**Without the pragma:** MinGW's `windres` would misinterpret UTF-8 bytes, causing garbled characters (mojibake) in Czech text.

**Adding New Languages:**
1. Add a new `LANGUAGE` section in `resource.rc`
2. Duplicate and translate menu/dialog resources
3. Add version info block with appropriate language ID
4. Update `Translation` value in `VarFileInfo` section
5. Rebuild - the new language will be automatically available

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
| `Ctrl+Enter` | Execute current filter |
| `Alt+F4` | Exit |

### Configuration

**First Run:**
On first launch, RichEditor automatically creates a default `RichEditor.ini` file with:
- Sensible default settings (word wrap on, autosave enabled)
- 3 example filters to get you started:
  - Uppercase (Transform category, Replace mode)
  - Lowercase (Transform category, Replace mode)
  - Line Count (Statistics category, Append mode)

You can customize this file or replace it with the full 20-filter collection included in the repository.

**Application Settings** (`RichEditor.ini`):

```ini
[Settings]
; Editor settings
WordWrap=1                    ; 1=enabled, 0=disabled (default: 1)

; Autosave settings
AutosaveEnabled=1             ; 1=enabled, 0=disabled (default: 1)
AutosaveIntervalMinutes=1     ; Autosave interval in minutes, 0=disabled (default: 1)
AutosaveOnFocusLoss=1         ; 1=save when window loses focus, 0=don't (default: 1)
```

**Filter Configuration** (`RichEditor.ini`):

The filter system reads from `RichEditor.ini` in the same directory as the executable. Filters are organized into categories that appear as submenus.

```ini
[Filters]
Count=20

[Filter1]
Name=Calculator
Command=powershell -NoProfile -Command "$input | Invoke-Expression"
Description=Evaluates mathematical expressions
Category=Transform
Mode=Replace

# ... more filters (see included RichEditor.ini for full collection)
```

**Filter Categories:**

Filters are automatically organized into submenus based on their `Category=` setting:

- **Transform** - Text manipulation (11 filters)
  - Uppercase, Lowercase, Title Case
  - Reverse Lines, Reverse Each Line
  - Sort Lines, Remove Duplicates
  - Remove Empty Lines, Trim Whitespace
  - Number Lines, Calculator

- **Statistics** - Analysis tools (3 filters)
  - Line Count, Word Count, Character Count

- **Extract** - Data extraction (4 filters)
  - Extract URLs, Extract Email Addresses
  - Base64 Encode/Decode

- **Web** - Network operations (2 filters)
  - Download URL Content, JSON Format

**Filter Output Modes:**

The `Mode=` setting controls how filter output is inserted:
- `Mode=Replace` - Replaces selected text with output
- `Mode=Append` - Appends output on same line (no newline)
- `Mode=Below` - Inserts output below with newline separator (default)

**Filter Usage:**
1. On first run, RichEditor creates `RichEditor.ini` with 3 example filters
2. Edit the INI file to add more filters or copy the full collection from the repository
3. Restart RichEditor to load the filters
4. Click Tools → Filter Help for comprehensive documentation
5. Navigate to Tools → Select Filter → [Category] → [Filter Name]
6. Checkmark shows the currently active filter
7. Select text or place cursor on a line
8. Press `Ctrl+Enter` to execute the filter
9. Output behavior depends on the filter's `Mode=` setting (Replace/Append/Below)

**Filter Requirements:**
- Command must read from stdin (pipe input)
- Command must write to stdout (results)
- Stderr output is captured and shown in a message box
- Process timeout: 30 seconds
- UTF-8 encoding for input/output
- Up to 100 filters supported (IDs 1300-1399)

### Status Bar Information

The status bar displays (from left to right):
1. **Filename:** Current file or "Untitled"
2. **Position:**
   - Word wrap OFF: `Ln X, Col Y`
   - Word wrap ON: `Ln X, Col Y / A,B` (visual / physical)
3. **Character:** Character at cursor (e.g., `Char: 'A' (Dec: 65, Hex: 0x0041)`)
   - Shows `Char: EOF` at end of file
4. **Filter:** Current active filter name (e.g., `[Filter: Calculator]` or `[Filter: None]`)

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
│   ├── main.cpp       (~1,750 lines) - Main application logic + filter system
│   ├── resource.h     - Resource IDs and constants
│   └── resource.rc    - Universal resources (English + Czech UI)
├── Makefile           - Build configuration
├── README.md          - This file
├── RichEditor.ini     - Filter configuration (auto-created on first run)
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

**Filter System (Phase 2):**
- Data structure: Array of `FilterInfo` structs (max 100 filters)
- Default INI creation: `CreateDefaultINI()` on first run with 3 example filters
- Settings loading: `LoadSettings()` reads [Settings] section
- INI loading: `GetPrivateProfileString` / `GetPrivateProfileInt`
- Dynamic menu: Built from loaded filters with category submenus
- Process execution: `CreateProcess` with pipe redirection
- UTF-8 I/O: UTF16↔UTF8 conversion for pipes
- Error handling: Stderr capture, exit code checking, timeout (30s)
- Output modes: Replace, Append, Below

**Filter Execution Flow:**
1. User selects text (or current line if no selection)
2. Press `Ctrl+Enter` or choose from Tools menu
3. Text extracted with `EM_GETTEXTRANGE`, converted to UTF-8
4. Anonymous pipes created for stdin/stdout/stderr
5. `CreateProcess` launches filter with `STARTF_USESTDHANDLES`
6. Input written to stdin pipe, then closed
7. Read stdout and stderr buffers (4KB chunks)
8. Wait up to 30 seconds for process completion
9. Convert UTF-8 output to UTF-16, insert after selection
10. Show stderr in message box if present
11. Clean up all handles

**Process Management:**
- Uses `CreateProcess` with `CREATE_NO_WINDOW` flag
- Three anonymous pipes: stdin (write), stdout (read), stderr (read)
- Non-inheritable handles for pipe ends used by parent
- Proper handle cleanup prevents resource leaks
- Synchronous I/O (acceptable for 30s timeout)
- Exit code check for error detection

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
The repository contains ~46 clean, incremental commits documenting the development process:
- Initial Win32 window and RichEdit setup
- File I/O with UTF-8 support
- Edit menu implementation
- Status bar with position and character display
- Autosave with timer and focus-loss triggers
- Word wrap with dual position calculation
- Time/Date insertion feature
- Czech localization with UTF-8 pragma fix
- Filter system with INI configuration
- Process execution with pipe redirection
- Filter output modes (Replace/Append/Below)
- Filter categories and submenus
- Configurable application settings via INI
- Auto-creation of default INI on first run
- Filter Help dialog with comprehensive documentation

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
- Filter execution with PowerShell commands (calculator, uppercase, lowercase)
- Process pipe I/O with UTF-8 encoding
- Error handling with stderr capture
- INI file auto-creation on first run
- Settings and filter loading from INI
- Filter categories and output modes

## Future Enhancements

### High Priority (Phase 3)
- [ ] Additional filter examples and templates
- [ ] Filter error recovery and debugging tools
- [ ] Filter timeout configuration per-filter

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

**Version:** 2.0.0 - Phase 2 Complete (December 2025)  
**Build:** ~752KB universal executable (includes C++ string library for filter I/O)  
**Lines of Code:** 1,753 lines (main.cpp), 2,014 total  
**Languages:** English + Czech (universal build with automatic selection)  
**Features:** Full text editing + INI-configurable filter system with categories and output modes  
**Configuration:** Auto-created default INI with 3 example filters on first run
