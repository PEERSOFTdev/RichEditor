# RichEditor

A lightweight, accessible Win32 text editor built with the RichEdit 4.1 control and MinGW-w64.

## Features

### Phase 1 (Complete)

**Core Editing:**
- Plain text editing with UTF-8 support (no BOM)
- File operations: New, Open, Save, Save As
- Command-line argument support (open file on startup)
- Edit operations: Undo, Redo, Cut, Copy, Paste, Select All
- Time/Date insertion (F5) with locale-specific formatting
- Full keyboard shortcut support
- RichEdit built-in shortcuts (Alt+X for Unicode conversion, Ctrl+Up/Down for paragraph navigation, etc.)
- Accessibility support for screen readers (MSAA/UI Automation)

**Display & Navigation:**
- Status bar showing:
  - Line and column position
  - Character at cursor with Unicode support:
    - BMP characters: `Char: 'A' (Dec: 65, U+0041)`
    - Emoji/supplementary: `Char: '😀' (Dec: 128512, U+1F600)`
    - Handles UTF-16 surrogate pairs correctly
    - Control characters: `Char: (Dec: 10, U+000A)`
  - Current active filter (e.g., "[Filter: Calculator]")
- Word wrap toggle (Ctrl+W) with dual position display:
  - Visual position: includes soft-wrapped lines
  - Physical position: actual line/column in file
  - Format: `Ln X, Col Y / A,B` when word wrap is on
- Modified state tracking with asterisk (*) in title bar
- Proper focus management (returns to editor after dialogs)
- MRU (Most Recently Used) file list:
  - Up to 10 recent files in File menu
  - Numbered 1-10 with keyboard accelerators
  - Most recent file always at top (File1)
  - Auto-updates on open/save operations
  - Supports UNC paths

**Autosave:**
- Timer-based autosave (configurable interval, default: 1 minute)
- Autosave on focus loss (when switching to another application)
- Only autosaves files with a filename (skips "Untitled")
- Configurable via INI file (AutosaveEnabled, AutosaveIntervalMinutes, AutosaveOnFocusLoss)

**File Format:**
- UTF-8 encoding without BOM
- Handles Windows (CRLF), Unix (LF), and Mac (CR) line endings
- Default save format: Windows CRLF
- UNC path support throughout

### Phase 2 (Complete)

**INI Configuration System:**
- Auto-creation of default `RichEditor.ini` on first run
- 8 example filters included demonstrating all action types
- All application settings configurable via INI file:
  - Word wrap default state
  - Autosave enable/disable
  - Autosave interval (minutes)
  - Autosave on focus loss behavior
  - Show menu descriptions (accessibility)
- Auto-generation of missing settings with defaults (self-documenting)
- Filter Help dialog (Tools → Filter Help) with comprehensive documentation
- No need to edit source code for customization

**Filter System:**
- External filter/utility execution (Ctrl+Enter)
- Multiple configurable filters with category organization
- Process input/output via stdin/stdout pipes with UTF-8 encoding
- INI-based filter configuration (`RichEditor.ini`)
- Dynamic categorized menu (Transform, Statistics, Extract, Web)
- Four action types: Insert (replace/below/append), Display (statusbar/messagebox), Clipboard (copy/append), None (side effects)
- Context menu integration (right-click to access filters)
- Configurable context menu appearance (ContextMenu=1, ContextMenuOrder=N)
- Error handling with stderr capture and display
- Selected text (or current line if no selection) sent to filter
- Up to 100 filters supported (Tools → Select Filter menu)
- Process timeout (30 seconds) to prevent hanging
- Proper pipe and process handle cleanup
- INI validation with helpful error messages

**Accessibility Features:**
- Screen reader friendly menu descriptions (configurable)
- Filter descriptions announced automatically by NVDA, JAWS, and Windows Narrator
- `ShowMenuDescriptions` setting (enabled by default for accessibility)
- When enabled: menu items show "Filter Name: Description"
- When disabled: menu items show only "Filter Name" for clean appearance
- No manual status bar querying required
- Auto-generation of missing INI settings with defaults
- All settings self-documenting in INI file

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

**Output:** `RichEditor.exe` (universal executable with English and Czech, ~1.1MB)

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

### Command-Line Arguments

RichEditor supports opening files from the command line:

```bash
RichEditor.exe [filename]
```

**Examples:**
```bash
RichEditor.exe document.txt
RichEditor.exe "C:\My Documents\notes.txt"
RichEditor.exe \\server\share\file.txt    # UNC paths supported
```

**Behavior:**
- If a filename is provided, it will be opened on startup
- Relative paths are resolved from the current working directory
- Supports UNC network paths
- File is opened with UTF-8 encoding (no BOM)

### Keyboard Shortcuts

**Application Shortcuts:**
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
| `Context Menu Key` | Open context menu with filters |
| `Alt+F4` | Exit |

**RichEdit Control Built-in Shortcuts:**

These shortcuts are provided by the Windows RichEdit control and work automatically:

| Shortcut | Action |
|----------|--------|
| `Alt+X` | Convert hex to Unicode / Unicode to hex |
| `Ctrl+Up` | Move to previous paragraph |
| `Ctrl+Down` | Move to next paragraph |
| `Ctrl+Left` | Move to previous word |
| `Ctrl+Right` | Move to next word |
| `Ctrl+Home` | Move to start of document |
| `Ctrl+End` | Move to end of document |
| `Shift+Arrow` | Extend selection |
| `Ctrl+Shift+Arrow` | Extend selection by word/paragraph |
| `Home` | Move to start of line |
| `End` | Move to end of line |
| `Page Up` | Scroll up one page |
| `Page Down` | Scroll down one page |

**Unicode Conversion (Alt+X):**
- Type hex code (e.g., `03B1`) then press `Alt+X` → converts to `α`
- Place cursor after character and press `Alt+X` → converts to hex code
- Supports full Unicode range including emoji:
  - `1F600` + `Alt+X` → 😀
  - Works with BMP and supplementary planes

### Configuration

**First Run:**
On first launch, RichEditor automatically creates a default `RichEditor.ini` file with:
- Sensible default settings (word wrap on, autosave enabled, menu descriptions on)
- Empty MRU list (populated as you open/save files)
- 8 example filters demonstrating all action types:
  - Uppercase (Transform, Insert/Replace, Context Menu)
  - Lowercase (Transform, Insert/Replace, Context Menu)
  - Sort Lines (Transform, Insert/Replace, Context Menu)
  - Add Line Numbers (Transform, Insert/Below, Context Menu)
  - Line Count (Statistics, Display/MessageBox)
  - Word Count (Statistics, Display/StatusBar)
  - Copy Reversed (Clipboard, Clipboard/Copy)
  - Speak Text (Utility, None - uses text-to-speech)

**Application Settings** (`RichEditor.ini`):

```ini
[Settings]
; Editor settings
WordWrap=1                    ; 1=enabled, 0=disabled (default: 1)

; Accessibility settings
ShowMenuDescriptions=1        ; 1=show filter descriptions in menus (accessible), 0=names only (default: 1)

; Autosave settings
AutosaveEnabled=1             ; 1=enabled, 0=disabled (default: 1)
AutosaveIntervalMinutes=1     ; Autosave interval in minutes, 0=disabled (default: 1)
AutosaveOnFocusLoss=1         ; 1=save when window loses focus, 0=don't (default: 1)

; Most recently used files (auto-managed)
CurrentFilter=Calculator      ; Last selected filter (auto-saved)

[MRU]
; Most recently used files (auto-populated, up to 10 entries)
File1=C:\path\to\most\recent.txt
File2=C:\path\to\second.txt
File3=C:\path\to\third.txt
; ... entries managed automatically by the application
```

**Note on Settings:**
- All settings are automatically written to the INI file with defaults if missing
- This makes the configuration self-documenting
- You can always see what settings are available by opening the INI file

**Filter Configuration** (`RichEditor.ini`):

The filter system reads from `RichEditor.ini` in the same directory as the executable. Filters are organized into categories that appear as submenus.

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

[Filter2]
Name=Lowercase
Command=powershell -NoProfile -Command "$input | ForEach-Object { $_.ToLower() }"
Description=Converts selected text to lowercase letters
Category=Transform
Action=insert
Insert=replace
ContextMenu=1
ContextMenuOrder=2

# ... more filters (see auto-generated RichEditor.ini for full collection)
```

**Filter Actions:**

Filters use an action-based architecture. The `Action=` setting determines what happens with the output:

- **`Action=insert`** - Modifies the document by inserting filter output
  - `Insert=replace` - Replaces selected text
  - `Insert=below` - Inserts output on new line below selection
  - `Insert=append` - Appends output to end of selection

- **`Action=display`** - Shows output without modifying document
  - `Display=messagebox` - Shows output in a message box dialog
  - `Display=statusbar` - Shows output in status bar for 30 seconds

- **`Action=clipboard`** - Copies output to clipboard silently
  - `Clipboard=copy` - Replaces clipboard contents
  - `Clipboard=append` - Appends to existing clipboard contents

- **`Action=none`** - Executes command for side effects (e.g., logging, text-to-speech)

**Context Menu Integration:**

- `ContextMenu=1` - Filter appears in right-click context menu
- `ContextMenu=0` - Filter only in Tools menu (default)
- `ContextMenuOrder=N` - Sort order in context menu (lower numbers first)

**Filter Categories:**

Filters are automatically organized into submenus based on their `Category=` setting:

- **Transform** - Text manipulation
- **Statistics** - Analysis tools
- **Clipboard** - Clipboard operations
- **Utility** - Miscellaneous utilities

**Filter Usage:**
1. On first run, RichEditor creates `RichEditor.ini` with 8 example filters demonstrating all action types
2. Edit the INI file to add more filters
3. Restart RichEditor to load the filters (or use Tools → Filter Help for documentation)
4. Navigate to Tools → Select Filter → [Category] → [Filter Name]
5. Checkmark shows the currently active filter
7. Select text or place cursor on a line
8. Press `Ctrl+Enter` to execute the filter
9. Output behavior depends on the filter's `Action=` and action-specific settings:
   - Insert: Replace/Below/Append
   - Display: MessageBox/StatusBar
   - Clipboard: Copy/Append
   - None: Command executes for side effects only

**Filter Requirements:**
- Command must read from stdin (pipe input)
- Command must write to stdout (results)
- Stderr output is captured and shown in a message box
- Process timeout: 30 seconds
- UTF-8 encoding for input/output
- Up to 100 filters supported (IDs 1300-1399)

### Status Bar Information

The status bar displays (from left to right):
1. **Position:**
   - Word wrap OFF: `Ln X, Col Y`
   - Word wrap ON: `Ln X, Col Y / A,B` (visual / physical)
2. **Character:** Character at cursor with full Unicode support
   - BMP characters: `Char: 'A' (Dec: 65, U+0041)`
   - Emoji (surrogate pairs): `Char: '😀' (Dec: 128512, U+1F600)`
   - Control characters: `Char: (Dec: 10, U+000A)`
   - End of file: `Char: EOF`
3. **Filter:** Current active filter name (e.g., `[Filter: Calculator]` or `[Filter: None]`)

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
│   ├── main.cpp       (~2,320 lines) - Main application logic + filter system + MRU
│   ├── resource.h     - Resource IDs and constants
│   └── resource.rc    - Universal resources (English + Czech UI)
├── Makefile           - Build configuration
├── README.md          - This file
├── RichEditor.ini     - Configuration file (auto-created on first run)
└── .gitignore         - Git ignore patterns
```

### Key Components

**Main Window (`WndProc`):**
- Message handling: WM_CREATE, WM_SIZE, WM_COMMAND, WM_NOTIFY, WM_TIMER
- Focus management: WM_SETFOCUS, WM_KILLFOCUS
- Autosave triggers
- MRU list management

**RichEdit Control:**
- Class: `MSFTEDIT_CLASS` (RichEdit 4.1)
- Mode: Plain text (`TM_PLAINTEXT`)
- Event mask: `ENM_CHANGE | ENM_SELCHANGE`
- Undo limit: 100 operations
- Text limit: 2GB (`0x7FFFFFFE`)

**Status Bar:**
- Updated on: `EN_SELCHANGE`, file operations, window activation
- Two parts: main info (left) and filter name (200px right)
- Unicode surrogate pair support for character display

**MRU System:**
- Storage: `[MRU]` section in INI file
- Format: `File1=path` (File1 is most recent)
- Atomic section writing (replaces entire [MRU] section)
- Menu integration: Inserts before Exit with separator
- Keyboard accelerators: 1-9, 0 for quick access

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
The repository contains ~48 clean, incremental commits documenting the development process:
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
- MRU (Most Recently Used) file list implementation
- Unicode surrogate pair support in status bar

### Coding Style
- Hungarian notation for Win32 types (e.g., `hwnd`, `sz`, `g_`)
- Function headers with separator comments
- Descriptive variable names
- Comprehensive comments for complex logic

### Testing Methodology
- Manual testing with various text files
- UTF-8 files with Czech characters (multi-byte UTF-8)
- Unicode characters beyond BMP (emojis, supplementary planes)
- Long lines for word wrap testing
- Focus management testing with dialogs
- Autosave testing with timer and focus loss
- Filter execution with PowerShell commands (calculator, uppercase, lowercase)
- Process pipe I/O with UTF-8 encoding
- Error handling with stderr capture
- INI file auto-creation on first run
- Settings and filter loading from INI
- Filter categories and output modes
- MRU list persistence and menu updates
- UTF-16 surrogate pair handling in status bar

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
- [ ] Drag & drop file support
- [ ] Command-line arguments for opening files

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

**Version:** 2.1.0 - Phase 2+ Complete (December 2024)  
**Build:** ~1.1MB universal executable (includes C++ string library)  
**Lines of Code:** ~2,320 lines (main.cpp), ~2,580 total  
**Languages:** English + Czech (universal build with automatic selection)  
**Features:** Full text editing + MRU list + Unicode surrogate pairs + INI-configurable filter system  
**Configuration:** Auto-created default INI with 3 example filters on first run
