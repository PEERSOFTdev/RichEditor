# RichEditor

A lightweight Win32 text editor built with the RichEdit 4.1 control and MinGW-w64.

## Features

### Phase 1 (Current)
- Plain text editing with UTF-8 support (no BOM)
- File operations: New, Open, Save, Save As
- Edit operations: Undo, Redo, Cut, Copy, Paste, Select All
- Status bar showing file name, line/column position
- Modified state tracking with save prompts
- Keyboard shortcuts for all operations
- Accessibility support for screen readers

### Phase 2 (Planned)
- External filter/utility execution (Ctrl+Enter)
- Multiple configurable filters (calculator, scripts, AI agents, etc.)
- Process input/output via stdin/stdout pipes
- INI-based filter configuration
- Optional plain Enter key execution mode

### Future Phases
- RTF file format support
- Markdown syntax highlighting and preview
- Find/Replace functionality
- Font selection
- Additional enhancements

## Building

### Requirements
- MinGW-w64 (tested with x86_64-w64-mingw32-g++)
- Windows 7 or later (target OS)
- Msftedit.dll (included in Windows Vista+)

### Build Commands

**From Linux (MXE or MinGW-w64 cross-compiler):**
```bash
make
```

**From Windows (MSYS2/MinGW-w64):**
```bash
make
```

**Clean build:**
```bash
make rebuild
```

Output: `RichEditor.exe` (standalone executable, ~150KB)

## Usage

### Keyboard Shortcuts
- `Ctrl+N` - New file
- `Ctrl+O` - Open file
- `Ctrl+S` - Save file
- `Ctrl+Z` - Undo
- `Ctrl+Y` - Redo
- `Ctrl+X` - Cut
- `Ctrl+C` - Copy
- `Ctrl+V` - Paste
- `Ctrl+A` - Select all
- `Ctrl+Enter` - Execute filter (Phase 2)

### File Format
- UTF-8 encoding without BOM
- Plain text (.txt files)
- Line endings: Windows (CRLF)

## Architecture

### Filter System (Phase 2)
Filters are external programs that read from stdin and write to stdout:

**Example filter (calculator):**
```
Input: 2 + 2 + 3
[User presses Ctrl+Enter]
Output:
2 + 2 + 3
7
```

**Configuration:** `RichEditor.ini`
```ini
[Filters]
Count=3

[Filter1]
Name=Calculator
Command=calc.exe
Description=Mathematical expression evaluator

[Filter2]
Name=Python
Command=python -c
Description=Python one-liner

[Filter3]
Name=AI Agent
Command=ai-agent.exe --stdin
Description=AI text processor
```

**Behavior:**
- Selected text (or current line) sent to filter as stdin
- Filter output appears below input
- Multi-line selections processed as whole
- Errors shown with "Error: " prefix
- 30-second timeout per execution

## Development Roadmap

**Phase 1:** Basic text editor with RichEdit control ✓ (in progress)
**Phase 2:** Filter execution system
**Phase 3:** RTF format support
**Phase 4:** Markdown support
**Phase 5:** Advanced features (Find/Replace, printing, etc.)

## License

(To be determined)

## Author

Created for accessibility-focused text editing with screen reader support.
