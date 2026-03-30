|# RichEditor

A lightweight, accessible Win32 text editor built around the RichEdit control (default MSFTEDIT.DLL) with optional library overrides. This reference keeps the original phase narrative and stays in sync with current behavior; the user manuals focus on end‑user workflows.

Last updated: 2026‑02‑19.

## Features

### Phase 1 (Complete)

**Core Editing:**
- Plain text editing with UTF-8 support (no BOM on save; BOM stripped on load if present)
- File operations: New, Open, Save, Save As
- Edit operations: Undo, Redo, Cut, Copy, Paste, Select All
- Time/Date insertion (F5) with locale-specific formatting
- Full keyboard shortcut support
- RichEdit built-in shortcuts (Alt+X for Unicode conversion, Ctrl+Up/Down for paragraph navigation, etc.)
- Accessibility support for screen readers (MSAA/UI Automation)

**Display & Navigation:**
- Status bar showing:
  - Line and column position
  - Character at cursor
- Word wrap toggle (Ctrl+W) with dual position display:
  - Visual position: includes soft-wrapped lines
  - Physical position: actual line/column in file
  - Format: `Ln X, Col Y / A,B` when word wrap is on
- Modified state tracking with asterisk (*) in title bar
- Proper focus management (returns to editor after dialogs)

**Autosave:**
- Timer-based autosave (default: 1 minute)
- Autosave on focus loss (when switching to another application)
- Only autosaves files with a filename (skips "Untitled")

**File Format:**
- UTF-8 encoding without BOM (BOM stripped on load if present)
- Handles Windows (CRLF), Unix (LF), and Mac (CR) line endings
- Default save format: Windows CRLF

### Phase 2 (Complete)

**INI Configuration System:**
- Auto-creation of default `RichEditor.ini` on first run
- 10 example filters included demonstrating common action types
- Lines starting with `;` or `#` are treated as comments in the INI file
- All application settings configurable via INI file:
  - Word wrap default state
  - Autosave enable/disable
  - Autosave interval (minutes)
  - Autosave on focus loss behavior
  - Show menu descriptions (accessibility)
  - Select after paste (power user feature, default: off)
- Auto-generation of missing settings with defaults (self-documenting)
- No need to edit source code for customization

**Autosave:**
- Timer-based autosave (configurable interval, default: 1 minute)
- Autosave on focus loss (when switching to another application)
- Only autosaves files with a filename (skips "Untitled")
- Configurable via INI file (AutosaveEnabled, AutosaveIntervalMinutes, AutosaveOnFocusLoss)

**Command-Line Arguments:**
- Command-line argument support (open file on startup)
- `/nomru` option to prevent MRU list pollution (perfect for file associations)
- UNC path support throughout

**MRU (Most Recently Used):**
- Up to 10 recent files in File menu
- Numbered 1-10 with keyboard accelerators
- Most recent file always at top (File1)
- Auto-updates on open/save operations
- Supports UNC paths



**Filter System:**
- External filter/utility execution (Ctrl+Enter)
- Multiple configurable filters with category organization
- Process input/output via stdin/stdout pipes with UTF-8 encoding
- INI-based filter configuration (`RichEditor.ini`)
- Dynamic categorized menu (Transform, Statistics, Clipboard, Utility)
- Four action types: Insert (replace/below/append), Display (statusbar/messagebox/pane), Clipboard (copy/append), None (side effects)
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
- `ShowMenuDescriptions` setting (enabled by default for accessibility)
- When enabled: menu items show "Filter Name: Description"
- When disabled: menu items show only "Filter Name" for clean appearance
- Filter descriptions announced automatically by NVDA, JAWS, and Windows Narrator
- No manual status bar querying required

**Undo/Redo Labels:**
- Undo/Redo with dynamic labels showing operation type:
  - "Undo Typing", "Redo Paste", "Undo Delete", etc.
  - Tracks both RichEdit operations (typing, paste, cut, delete, drag-drop) and filter actions
  - Uses Windows RichEdit `EM_GETUNDONAME` API for standard operations

**Display Enhancements:**
- Tab-aware column calculation for accurate position reporting
  - Accounts for tab character expansion (configurable tab size, default: 8)
  - Example: "hello\t" at column 5 → jumps to column 9
  - Critical for accessibility: accurate positions in source code and tabular data
- Character at cursor with Unicode support:
  - BMP characters: `Char: 'A' (Dec: 65, U+0041)`
  - Emoji/supplementary: `Char: '😀' (Dec: 128512, U+1F600)`
  - Handles UTF-16 surrogate pairs correctly
  - Control characters: `Char: (Dec: 10, U+000A)`
- Undo buffer overflow notification:
  - Warning dialog when RichEdit undo buffer becomes full
  - Allows user to continue editing or close without saving
  - Prevents silent loss of undo capability

**Example Filter Use Cases:**
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


**URL Autodetection:**
- Automatic detection and highlighting of URLs as you type
- Supported protocols: http, https, ftp, mailto, file, tel, callto
- Visual feedback: blue underline and hand cursor on hover
- Keyboard activation: Press `Enter` when cursor is in URL to open
- Mouse activation: Click URL to open in default browser/handler
- Context menu integration:
  - Right-click on URL shows "Open URL" and "Copy URL" as first menu items
  - Right-click outside URL shows standard context menu
- Silent operation (no status bar messages)
- Opens URLs with Windows default application via ShellExecute
- Full accessibility: URLs announced as "link" by NVDA, JAWS, Narrator
- Enabled by default; set `DetectURLs=0` in `[Settings]` and restart to disable (see performance note below)
- When disabled, status bar shows "URL: off" and no URL features are available
- **Performance note:** RichEdit's URL scanner runs on every cursor movement. On very large files (13 MB+) this causes multi-second freezes. Add `DetectURLs=0` to the INI for large-file sessions.



### Phase 2.5 (Complete)

**Interactive Mode (REPL) Core:**
- Full REPL (Read-Eval-Print Loop) support for interactive command-line tools
- Execute commands interactively with persistent shell sessions
- Keyboard shortcuts:
  - `Ctrl+Shift+I`: Start interactive mode with selected REPL filter
  - `Ctrl+Shift+Q`: Exit interactive mode
  - `Enter`: Send command to REPL (when on line with prompt or at document end)
  - `Shift+Enter`: Always inserts newline (bypass REPL mode)
- Title bar shows "[Interactive Mode]" when active
- Prompt detection system:
  - Define prompt end string per filter (e.g., `> ` for PowerShell, `$ ` for bash)
  - Automatically extracts user input after prompt
  - Supports custom prompts for any shell or REPL tool
- Configurable line ending modes per filter:
  - `auto`: LF for input (output detected for info only)
  - `lf`: Unix line endings (bash, python, node, etc.)
  - `crlf`: Windows line endings (cmd.exe, PowerShell)
  - `cr`: Legacy Mac line endings (rarely used)
- Process lifecycle management:
  - Process terminated on exit (TerminateProcess)
  - No zombie processes or leaked handles
  - Proper cleanup of stdin/stdout/stderr pipes
- Exit notifications:
  - Warns if REPL process exits unexpectedly (ExitNotification=1)
  - Silent exit when user intentionally closes (Ctrl+Shift+Q or menu)
  - Clear feedback for troubleshooting

**Phase 2.5.1 (Notebook Workflow):**
- Smart Enter key behavior:
  - Detects prompt on current line and sends command to REPL
  - Works with ANY previous command line - edit old commands and re-run them
  - Press Enter at document end to recall prompt (safety feature if text deleted)
  - Otherwise works normally: insert newline or open URLs
  - No need to remember "where the prompt was" - just find any prompt and use it
- Notebook-style workflow:
  - Output appears directly below the command that generated it
  - Edit any previous command and press Enter to re-run it
  - Clean and organize by deleting old output blocks
- URL support in REPL output:
  - Press Enter on URLs (e.g., in PowerShell Get-Help output) to open them
  - Works seamlessly with interactive mode

**Phase 2.5.2 (Streams & Terminal Hygiene):**
- Automatic output capture:
  - Stdout: command output displayed in editor
  - Stderr: error output displayed in editor (same stream as stdout)
  - Both streams shown as output arrives
- ANSI escape sequence stripping:
  - Removes color codes and cursor positioning sequences
  - Clean output without terminal control characters
  - Handles CSI and OSC sequences
- Stderr capture (separate thread) added after initial REPL support

**Phase 2.5.3 (WSL Bash Integration):**
- EOL auto detection tuned for WSL/bash behavior
- WSL bash PTY handling and echo behavior documented

**Supported REPL Environments:**
- **PowerShell** (`powershell.exe` or `pwsh.exe`)
  - Full cmdlet support with tab completion (via shell)
  - Get-Help output with clickable URLs
  - Multi-line commands supported
  - Example prompt: `PS C:\> `
- **Command Prompt** (`cmd.exe`)
  - All built-in commands (dir, cd, echo, etc.)
  - Batch file execution
  - Example prompt: `C:\> `
- **WSL Bash** (`wsl.exe script -qfc bash /dev/null`)
  - Full Linux environment access from Windows
  - Pseudo-TTY support for proper prompt display
  - Color code stripping for clean output
  - Example prompt: `user@host:~/path$ `
- **Python REPL** (`python -i`)
  - Interactive Python interpreter
  - Import modules and test code
  - Example prompt: `>>> `
- **Node.js REPL** (`node`)
  - JavaScript evaluation
  - Example prompt: `> `
- **Custom REPLs**: Any command-line tool with stdin/stdout interaction

**INI Configuration for REPL Filters:**

Each REPL filter requires these settings in `RichEditor.ini`:

```ini
[Filter1]
Name=PowerShell
Command=powershell.exe -NoLogo -NoExit
Description=Interactive PowerShell session with full cmdlet support
Category=Interactive
Action=repl
PromptEnd=> 
EOLDetection=auto
ExitNotification=1
ContextMenu=0
```

**Key Settings Explained:**
- `Action=repl`: Marks filter as REPL mode (must be lowercase)
- `PromptEnd=> `: String that marks end of prompt (used to extract user input)
- `EOLDetection`: Line ending mode (auto/lf/crlf/cr)
- `ExitNotification`: Show dialog when REPL exits unexpectedly
- `ContextMenu=0`: REPL filters typically not shown in right-click menu

**Example REPL Filters (included in default INI):**

1. **PowerShell**:
   ```ini
   Command=powershell.exe -NoLogo -NoExit
   PromptEnd=> 
   EOLDetection=auto
   ```

2. **WSL Bash** (with pseudo-TTY for prompts):
   ```ini
   Command=wsl.exe script -qfc bash /dev/null
   PromptEnd=$ 
   EOLDetection=lf
   ```

3. **Python Interactive**:
   ```ini
   Command=python -i
   PromptEnd=>>> 
   EOLDetection=lf
   ```

**Typical Workflow:**
1. Select REPL filter: `Tools → Select Filter → PowerShell` (or any REPL filter)
2. Start interactive mode: Press `Ctrl+Shift+I` or `Tools → Start Interactive Mode`
3. Wait for prompt to appear (e.g., `PS C:\> `)
4. Type command and press `Enter` to execute
5. Output appears below your command
6. Continue entering commands as needed
7. Edit any previous command line and press `Enter` to re-run it
8. Exit: Press `Ctrl+Shift+Q` or `Tools → Exit Interactive Mode`

**Advanced Features:**
- **Multi-command editing**: Edit multiple old commands and run them sequentially
- **Clipboard integration**: Copy interesting output with standard Ctrl+C
- **URL interaction**: Click or press Enter on URLs in REPL output
- **Clean workspace**: Delete old output blocks to reduce clutter
- **Safety recovery**: Delete all text and press Enter at end to get fresh prompt
- **Concurrent editing**: Edit document normally while in REPL mode (non-REPL Enter keys work)

**Technical Notes:**
- REPL mode uses Windows pipes (CreateProcess with STARTF_USESTDHANDLES)
- Separate background threads read stdout and stderr without blocking
- Output inserted at cursor position for notebook-style workflow
- Process terminated on exit (TerminateProcess)
- All handles (pipes, threads, process) properly cleaned up
- UTF-8 encoding for all input/output
- No 30-second timeout (REPL runs until explicitly closed)
- Stderr handling is not configurable (always captured and inserted)

**Troubleshooting:**
- **No prompt appears**: Check `Command` path is valid and executable
- **Command echo appears twice**: Normal PTY behavior with WSL bash
- **Wrong line endings**: Adjust `EOLDetection` setting (try auto or lf)
- **Garbled output**: ANSI codes should be stripped automatically
- **Process won't start**: Check command-line arguments and permissions
- **Can't recall prompt**: Press Enter at document end (safety feature)

### Phase 2.6 (Complete)

**Session Resume (Automatic Recovery):**
- Windows 11 Notepad-style automatic session recovery
- Automatically saves unsaved work during system shutdowns
- Recovers content on next application startup
- Works for both untitled documents and saved files with unsaved changes
- Title bar shows `[Resumed]` indicator (localized: English/Czech) for recovered files

**How It Works:**
1. **During Windows Shutdown**:
   - All unsaved changes automatically saved to temporary resume file
   - No prompts or dialogs - seamless background operation
   - Resume file stored in `%TEMP%\RichEditor\` directory
   - Original files remain untouched until you explicitly save
   
2. **On Next Startup**:
   - Editor automatically detects and loads resume file
   - Title bar shows `*Untitled [Resumed] - RichEditor` or `*filename.txt [Resumed] - RichEditor`
   - Document marked as modified (asterisk shown)
   - You can continue editing where you left off
   
3. **After Recovery**:
   - **Save (Ctrl+S)**: For untitled files, opens "Save As" dialog
   - **Save (Ctrl+S)**: For saved files, prompts whether to save to original location
   - Resume file automatically deleted after successful save
   - Regular autosave continues working as normal

**Optional Auto-Save on Close:**
- INI setting: `AutoSaveUntitledOnClose=0` (default: off)
- When enabled (`=1`): Untitled files auto-save to resume file on normal close (no prompt)
- When disabled (`=0`): Traditional "Save changes?" prompt shown
- Only applies to **untitled** files (saved files always prompt)
- Perfect for note-taking workflow where you never lose content

**Resume File Management:**
- Resume files reused across sessions (same file updated, not recreated)
- Format: `Untitled_YYYYMMDD_HHMMSS_resume.txt` (for untitled files)
- Format: `filename_resume.ext` (for saved files with changes)
- Automatic cleanup when you explicitly save or close without saving
- Windows temp cleanup eventually removes old resume files
- One resume file per document session

**Multi-Instance Behavior:**
- First instance opened recovers the resume file
- Second instance starts with blank document
- No conflicts or confusion between instances
- Each instance maintains independent state

**Shutdown Handling (Microsoft Guidelines):**
- No prompts during system shutdown (`WM_QUERYENDSESSION`)
- Resume file only registered if shutdown completes (`WM_ENDSESSION`)
- If shutdown cancelled, resume file cleaned up automatically
- REPL mode exits silently during shutdown
- Fast, non-blocking shutdown behavior

**Technical Details:**
- Resume files stored as UTF-8 without BOM (consistent with RichEditor format)
- INI file tracks resume file path in `[Resume]` section (cleared after recovery)
- Uses accurate Unicode text extraction (`EM_GETTEXTLENGTHEX`) for emoji support
- Buffer overflow protection for very long filenames/paths
- Error handling for disk full, permission denied, etc.
- One-time recovery semantics (resume file cleared after loading)

**User Experience:**
- Transparent operation - works automatically, no configuration needed
- Clear visual indicator (`[Resumed]`) when working with recovered content
- Prompts only when necessary (save location for resumed files)
- Never lose work due to unexpected shutdown or system restart
- Compatible with Windows Update restarts and power management

**Configuration:**
```ini
[Settings]
AutoSaveUntitledOnClose=0     ; 1=auto-save untitled files on close (no prompt), 0=prompt as usual (default: 0)

[Resume]
ResumeFile=C:\Users\...\AppData\Local\Temp\RichEditor\Untitled_20260109_230000_resume.txt
OriginalPath=                 ; Empty for untitled, or original file path for saved files
```

**Common Scenarios:**

1. **Power outage while editing**:
   - Next boot: Editor opens with `*Untitled [Resumed]`, content preserved ✅

2. **Windows Update restart overnight**:
   - Next morning: All unsaved work automatically recovered ✅

3. **Note-taking mode** (`AutoSaveUntitledOnClose=1`):
   - Close editor anytime, no prompts
   - Next open: Resume exactly where you left off ✅

4. **Working on saved file**:
   - Make changes, shutdown happens
   - Next start: Changes recovered, original file untouched
   - Save prompts: "Save to original location?" ✅

5. **Shutdown cancelled by another app**:
   - Continue working normally
   - No stale resume file registered ✅

**Limitations:**
- Multiple instances during shutdown: Only last one writes resume file
  - Acceptable: Windows temp cleanup handles orphaned files
  - Most users run single instance
- Resume file visible in `%TEMP%\RichEditor\` until explicitly saved
  - This is by design: serves as backup until you save

### Phase 2.7 (Complete)

**Template System Core:**
- Pre-defined text snippets with variable expansion
- Keyboard shortcuts for quick template insertion (Ctrl+1, Ctrl+B, etc.)
- Default template set loaded from INI

**Template Variables (Phase 2.7.1):**
- `%cursor%` - Positions cursor after template insertion (first occurrence)
- `%selection%` - Inserts current text selection
- `%date%` - Inserts date
- `%time%` - Inserts time
- `%datetime%` - Inserts combined date/time
- `%clipboard%` - Inserts clipboard contents
- Unknown variables preserved as literals

**Phase 2.10 (Template Variables):**
- `%shortdate%`, `%longdate%`, `%yearmonth%`, `%monthday%`
- `%shorttime%`, `%longtime%`
- `%date%` and `%time%` become configurable via `DateFormat`/`TimeFormat`

**Phase 2.7.1 (Insertion & File Type Tracking):**
- Template insertion replaces current selection (or inserts at cursor)
- Document marked as modified after insertion
- File type associations (templates filter by current file extension)
- Wrong file type silently ignored (no insertion)

**Phase 2.7.2 (Tools Menu Integration):**
- Tools → Insert Template menu (insert templates into current document)
- Category organization (Markdown, Plain Text, HTML, etc.)
- Menu rebuilds when file extension changes (automatic filtering)
- Template descriptions shown in menu items when enabled
- "No templates" / "No templates for this file type" messages

**Phase 2.7.3 (File → New Submenu):**
- Dynamic File → New submenu (create files from templates)
- File types derived from templates and grouped by extension
- Unknown extensions filtered from File → New list

**Phase 2.7.4 (Localization):**
- Full localization support (English + Czech)
- Template localization fallback: full locale → language-only → English

**File → New Menu:**
- **Blank Document** (Ctrl+N) - Traditional empty document (default)
- **Markdown Document** - Creates file with Front Matter template
- **Text Document** - Creates file with Plain Text template
- **HTML Document** - Creates file with HTML boilerplate template
- Dynamic menu items based on available templates
- Document extension set automatically (affects template filtering)
- Document marked as modified (prompts to save on first save)

**Tools → Insert Template Menu:**
- Grouped by category (e.g., "Markdown" with 15 templates)
- Filtered by current file extension (Markdown templates only show in .md files)
- Shows template descriptions for accessibility (configurable)
- Individual keyboard shortcuts for common templates (Ctrl+1, Ctrl+B, Ctrl+I, etc.)
- **Ctrl+Shift+T** - Opens template picker popup menu at cursor position (quick access to all templates)

**Phase 2.7.5 (Template Picker & Menu Sync):**
- Ctrl+Shift+T shows a template picker popup menu at cursor position
- Tools → Insert Template and Ctrl+Shift+T menus stay synchronized
- Picker menu flattened with category headers (no cascading)
- Uncategorized templates appear at root level (no "General" category)
- File Open/Save As dialogs build filters from template file types
- Open dialog prepends "All Supported Types" combining every template extension plus `*.txt`
- Save As omits the combined entry (shows only per-category filters for precise extension choice)
- Save As default extension follows current file type

**15 Default Markdown Templates:**
1. **Heading 1** (Ctrl+1) - `# %cursor%`
2. **Heading 2** (Ctrl+2) - `## %cursor%`
3. **Heading 3** (Ctrl+3) - `### %cursor%`
4. **Bold Text** (Ctrl+B) - `**%selection%**` (wraps selected text)
5. **Italic Text** (Ctrl+I) - `*%selection%*`
6. **Bold Italic** - `***%selection%***`
7. **Strikethrough** - `~~%selection%~~`
8. **Inline Code** - `` `%selection%` ``
9. **Code Block** (Ctrl+Shift+C) - Fenced code block with cursor inside
10. **Unordered List** - `- %cursor%`
11. **Ordered List** - `1. %cursor%`
12. **Task List** (Checkbox) - `- [ ] %cursor%`
13. **Link** - `[%selection%](url)`
14. **Blockquote** - `> %cursor%`
15. **Front Matter** - YAML front matter with `%date%` variable

**How It Works:**
1. **File → New → Markdown Document**:
   - Creates new untitled document
   - Inserts Front Matter template:
     ```yaml
     ---
     title: [cursor here]
     date: 2026-01-14
     author: 
     ---
     
     ```
   - Extension set to "md" (enables Markdown templates in menu and filters)
   - Document marked as modified

2. **Tools → Insert Template → Markdown → Bold** (or Ctrl+B):
   - Selects text: "important"
   - Press Ctrl+B
   - Text becomes: "**important**"
   - Works like traditional Markdown editors

3. **Custom Template Creation**:
   - Edit `RichEditor.ini` (same folder as .exe)
   - Add template to `[Templates]` section
   - Supports template variables (see list above)
   - Restart editor to reload templates

**INI Configuration:**
```ini
[Templates]
Count=15

[Template1]
Name=Heading 1
Name.cs_CZ=Nadpis 1
Description=Insert a level 1 heading
Description.cs_CZ=Vložit nadpis úrovně 1
Category=Markdown
Category.cs=Markdown
FileExtension=md
Template=# %cursor%
Shortcut=Ctrl+1

[Template5]
Name=Bold
Name.cs_CZ=Tučný text
Description=Wrap selected text in bold formatting
Description.cs_CZ=Udělat text tučným (obalí výběr nebo vloží šablonu)
Category=Markdown
FileExtension=md
Template=**%selection%**
Shortcut=Ctrl+B

[Template15]
Name=Front Matter
Description=Insert YAML front matter with title, date, and author fields
Description.cs_CZ=Vložit YAML záhlaví s aktuálním datem
Category=Markdown
FileExtension=md
Template=---\ntitle: %cursor%\ndate: %date%\nauthor: \n---\n\n
Shortcut=Ctrl+Shift+F
```

**Template Fields:**
- `Name=` - Template name (required)
- `Name.cs=` - Czech translation (optional, for localization)
- `Name.cs_CZ=` - Czech translation (optional, full locale)
- `Description=` - Template description (optional, for accessibility)
- `Description.cs=` - Czech description (optional)
- `Description.cs_CZ=` - Czech description (optional, full locale)
- `Category=` - Category for menu grouping (empty = uncategorized at root)
- `Category.cs=` - Czech category display name (optional)
- `Category.cs_CZ=` - Czech category display name, full locale (optional)
- `FileExtension=` - File type filter (e.g., "md", "txt", "html")
  - Leave empty for universal templates (available in all file types)
- `Template=` - Template text with variables (required)
  - Use `\n` for newlines, `\t` for tabs, `\\` for literal backslash
- `Shortcut=` - Keyboard shortcut (optional)
  - Format: `Ctrl+Key`, `Ctrl+Shift+Key`, `Ctrl+Alt+Key`, `F1`-`F12`
  - Cannot override built-in shortcuts (Ctrl+S, Ctrl+N, etc.)

**Reserved Keyboard Shortcuts (Cannot Be Used):**
- File: Ctrl+N, Ctrl+O, Ctrl+S
- Edit: Ctrl+Z, Ctrl+Y, Ctrl+X, Ctrl+C, Ctrl+V, Ctrl+A
- View: Ctrl+W
- Tools: Ctrl+Enter, Ctrl+Shift+I, Ctrl+Shift+Q, Ctrl+Shift+T
- Other: F5, Ctrl+G, Alt+F4

**Accessibility:**
- Template descriptions shown in menu items by default
- Format: "Heading 1: Insert a level 1 heading"
- Screen readers (NVDA, JAWS, Narrator) read descriptions automatically
- Configurable via `ShowMenuDescriptions=1` in INI
- Keyboard shortcuts shown in menu (e.g., "Ctrl+1")
- Full support for tab navigation and arrow key navigation

**User Workflows:**

1. **Markdown Note-Taking**:
   - File → New → Markdown Document
   - Front matter inserted automatically (title, date, author)
   - Type title after "title: " field
   - Press Ctrl+1 for main heading
   - Insert list items with the Unordered List template
   - Save as `notes.md`

2. **Quick Formatting**:
   - Type "important concept"
   - Select the text
   - Press Ctrl+B → becomes "**important concept**"
   - Press Ctrl+I → becomes "***important concept***"
   - Undo works normally (Ctrl+Z)

3. **Custom Templates**:
   - Create email signature template
   - Add to INI: `Template=Best regards,\n%clipboard%` (inserts name from clipboard)
   - Assign Ctrl+Shift+S shortcut
   - Use in any text file

**Technical Details:**
- Up to 100 templates supported
- Template text up to 4,096 characters
- Dynamic accelerator table (combines built-in + template shortcuts)
- Variable expansion is case-insensitive
- Multiple `%cursor%` markers: first one used, rest removed
- Undo/Redo fully supported
- Reserved shortcut warnings not shown (silently ignored)

**Limitations:**
- Template shortcuts cannot override built-in editor shortcuts
- Template names limited to 64 characters
- Category names limited to 64 characters
- File extensions limited to 16 characters
- Template text limited to 4,096 characters

### Phase 2.8 (Complete)

**RichEdit Library Selection:**
- Optional `RichEditLibraryPath` and `RichEditClassName` overrides
- `RichEditLibraryPath` accepts absolute or relative paths
- Example Windows Notepad DLL: `C:\Program Files\WindowsApps\Microsoft.WindowsNotepad_11.x.x.x_x64__8wekyb3d8bbwe\Notepad\riched20.dll`
- Default DLL cascade: MSFTEDIT.DLL → RICHED20.DLL → RICHED32.DLL
- Class fallback chain includes RichEditD2DPT, RichEdit60W, RICHEDIT50W, RICHEDIT
- RichEdit version detection exposed in the About dialog

### Phase 2.8.5 (Complete)

**About Dialog Enhancements:**
- Shows detected RichEdit version and class
- Shows the loaded RichEdit DLL path
- Adds RichEditD2DPT support with a fallback chain for older libraries

### Phase 2.9.1 (Complete)

**Find Dialog:**
- Find dialog with escape sequences and history; when Use Escapes is enabled: `\n` (newline), `\r` (carriage return), `\t` (tab), `\\` (literal backslash), `\xNN` (hex byte), `\uNNNN` (Unicode code point)
- Forward and backward searching (Find Next / Find Previous)
- F3 = Find Next, Shift+F3 = Find Previous
- Options persisted: Match case, Whole word, Use escapes, Select after find
- Find history stored in `[FindHistory]` (Item1 is newest)
- Find/Replace default options saved in INI
- Find Previous searches upward from the current cursor position
- When SelectAfterFind=0: cursor lands after forward matches and before backward matches

### Phase 2.9.2 (Complete)

**Replace:**
- Replace and Replace All with undo support
- Optimized Replace All path with custom word-boundary logic
- Replace history stored in `[ReplaceHistory]` (Item1 is newest)
- Replace supports escape sequences when enabled (same rules as Find); replacement string placeholders: `%0` inserts the matched text, `%%` inserts a literal `%`

### Phase 2.9.3 (Complete)

**Bookmarks:**
- Ctrl+F2 toggles a bookmark on the current line
- F2 jumps to the next bookmark, Shift+F2 to the previous
- Clear All Bookmarks removes all bookmarks for the current file
- Bookmarks persist per file (stored in INI under a hashed section)
- Line interpretation follows Word Wrap mode: visual lines when ON, physical lines when OFF
- When a bookmarked line is deleted, the bookmark relocates to the replacement line

### Phase 2.9.4 (Complete)

**Go to Line:**
- Ctrl+G opens a modal line jump dialog
- Shows current line range and validates input (1..line count)
- Moves caret to start of the chosen line and scrolls into view
- Line counting follows Word Wrap mode: visual lines when ON, physical lines when OFF

### Phase 2.9.5 (Complete)

**Elevated Save:**
- When Save (`Ctrl+S`) or Save As fails with access denied, the editor prompts to retry with administrator permissions
- On confirmation, content is staged to `%TEMP%\RichEditor\` and the editor re-launches itself with `/elevated-save` via `ShellExecuteEx` (`runas`), waits synchronously, then restores foreground
- `Save As` dialog uses `OFN_NOTESTFILECREATE` so the shell presents the dialog even for protected paths; a manual overwrite prompt follows
- `/elevated-save` is an internal command-line mode only; it is not intended for direct user invocation

### Phase 2.10 (Complete)

**Configurable Date/Time Formatting:**
- `DateFormat`, `TimeFormat`, and `DateTimeTemplate` settings
- `DateTimeTemplate` default: `%date% %time%` (changed from `%shortdate% %shorttime%`)
- `%date%` and `%time%` are configurable aliases

**INI Behavior:**
- INI is cached in memory; external edits are not detected during runtime by design



### Phase 2.11 (Planned)

- Wrap ruler (twips/column-based wrap) plan in `docs/notes/PHASE_2.11_WRAP_RULER_PLAN.md`

### Phase 2.12 (Complete)

**Embedded JScript filter engine (`script:` prefix):**

- Adds a `script:` prefix for filter `Command` values that runs a JScript expression in-process via the Windows `IActiveScript` / `IActiveScriptParse` COM interfaces (no external process spawned).
- The special variable `INPUT` holds the selected text (or current line). The expression result is treated as filter output and is subject to the same `Action` / `Insert` / `Display` / `Clipboard` settings as any other filter.
- Fixes the Unicode/diacritics corruption bug in the built-in PowerShell filters: PowerShell read stdin using the system ANSI code page (e.g. CP1250 on Czech Windows) rather than UTF-8, so characters like `á é í ó ú ů ž š č` were mangled before `.ToUpper()` / `.ToLower()` ran.
- The built-in filter commands are not migrated automatically. Users with an existing INI should delete it and let RichEditor recreate it (or update the `Command=` values manually) to get the `script:` equivalents.
- Script errors are reported in a message box with line and column number.
- JScript reference: https://learn.microsoft.com/en-us/previous-versions//hbxc2t98(v=vs.85)

**`script:` gotchas:**

- **Top-level must be an expression.** `ParseScriptText` is called with `SCRIPTTEXT_ISEXPRESSION`; bare statements (`var x = 1`, `if (…) …`) at the top level are a syntax error. For multi-step logic use an IIFE that `return`s the result: `(function(){var n=INPUT.length;return String(n)})()`.
- **`undefined`, `null`, or empty result → silent no output, no error.** `ExecuteFilterDisplay` returns immediately on an empty string (`src/main.cpp`: `if (outputData.empty()) return`). An IIFE without `return`, or any expression that evaluates to `undefined`/`null`, coerces to an empty BSTR and falls through this guard silently. Wrap in `String(…)` to be safe.
- **Space or tab before `;` in `Command=` is an INI comment delimiter.** The INI parser treats ` ;` (space or tab followed by semicolon) as the start of an inline comment and discards the rest of the line. Code such as `return x; })()` (space before `;`) is silently truncated. Keep semicolons tight: `return x})()`.

**Built-in filter commands after migration:**

| Filter | Command |
|--------|---------|
| Uppercase | `script:INPUT.toLocaleUpperCase()` |
| Lowercase | `script:INPUT.toLocaleLowerCase()` |
| Sort Lines | `script:INPUT.split('\r').sort(function(a,b){return a.localeCompare(b)}).join('\r')` |

**Binary size delta (MinGW stripped):** 347 648 → 351 744 bytes (+4 096 bytes, +1.2%)

### Phase 2.13 (Complete)

**Output pane (`Display=pane`):**

- Adds a secondary RichEdit panel that appears below the main edit area when a filter first writes to it; stays visible for the rest of the session.
- Created at startup without `WS_VISIBLE`; shown automatically on first `Display=pane` use; not destroyed or hidden again while the app runs.
- INI settings in `[Settings]`:
  - `OutputPaneLines=5` — pane height as integer lines (default `5`) or percentage of available area (e.g. `20%`)
  - `OutputPaneReadOnly=0` — `0` = editable (default), `1` = read-only
- Per-filter INI (chained from `Action=display`):

```ini
Action=display
Display=pane
Pane=append,focus    ; optional comma-separated values
```

  | `Pane=` value | Meaning |
  |---|---|
  | *(absent)* | Replace pane content; scroll to top; stay in main edit |
  | `append` | Append to existing pane content |
  | `focus` | Move focus to pane after writing |
  | `start` | Place caret at start of newly written output (position 0 for replace; start of appended chunk for append) |
  | `append,focus` | Both modifiers combined |

- **F6**: Switches focus between the main edit area and the output pane (only when pane is visible).
- **Output pane keyboard shortcuts:**
  - `F6` → return focus to main edit
  - `Ctrl+Shift+Delete` → clear pane
- **Output pane context menu** (right-click): "Copy All" / "Clear"
- Word wrap disabled in the pane; horizontal and vertical scroll bars shown.
- Czech UI label: **panel výstupu**

**Accessible names (MSAA Dynamic Annotation + UIA label):**

- The main edit area is annotated as **"Text Editor"** (CS: "Textový editor") and the output pane as **"Output Pane"** (CS: "Panel výstupu").
- **MSAA path** (`IAccPropServices::SetHwndPropStr`): effective for MSAA-based screen readers with the default RichEdit library (`MSFTEDIT.DLL` / `RICHEDIT50W`).
- **UIA path** (1×1 `WS_VISIBLE` STATIC label preceding each RichEdit in Z-order): the Win32 UIA HWND composition layer resolves `UIA_NamePropertyId` from the text of the immediately-preceding STATIC sibling when the native provider returns `VT_EMPTY`. Covers modern RichEdit DLLs (Windows 11 Notepad, Office) whose native UIA provider does not consult MSAA annotations. The label appears as an extra navigation stop in AT object trees; annotating it with `STATE_SYSTEM_INVISIBLE` via `IAccPropServices::SetHwndProp` has no effect on UIA-mode screen readers.
- Linked with `oleacc.lib`; GUIDs (`CLSID_AccPropServices_`, `IID_IAccPropServices_`, `PROPID_ACC_NAME_`) defined as static constants in `main.cpp` (MXE sysroot does not export them from `liboleacc.a`).

**Binary size delta:** 1 035 771 → 1 044 437 bytes (+8 666 bytes total for this phase)

### Phase 2.14 (Complete)

**Addon system** — user-extensible filter and template packs.

**Directory layout:**

```
RichEditor.exe
RichEditor.ini              ← main INI (Count= optional, still supported)
addons/
  my-suite/
    filters.ini             ← addon filters (Count= optional)
    templates.ini           ← addon templates (Count= optional)
    tools/
      myfilter.exe          ← relative path resolved via working directory
  another-pack/
    filters.ini
```

**Loading behaviour:**

- On startup and via `Tools → Reload Addons`, the editor scans `addons\` next to the executable.
- Addon directories are loaded in alphabetical order by directory name.
- Within each addon directory, `filters.ini` and `templates.ini` are loaded using the same `[Filter1]`..`[FilterN]` / `[Template1]`..`[TemplateN]` section format as the main INI.
- `Count=` in `[Filters]` or `[Templates]` is optional everywhere (main INI and addon files). If present, it defines the loop bound; if absent, sections are probed sequentially until an empty `Name=` is encountered.
- Duplicate names (across main INI and all addons): last loaded wins. The previous definition is completely overwritten in place, and a warning is logged to the output pane.

**Working directory resolution:**

- Filter commands from addon packs are executed with `lpCurrentDirectory` set to the addon pack directory (e.g., `addons\my-suite\`). This allows addon `Command=` values to use relative paths to bundled executables.
- Main INI filters continue to use `NULL` (inherit from process), preserving backward compatibility.

**Reload Addons:**

- `Tools → Reload Addons` performs a full reload: clears all filters and templates, re-reads the main INI and all addon packs, rebuilds all menus and the accelerator table. Filter and REPL selection is restored by name.

**Status bar and output pane:**

- After addon loading, a summary (`N packs, N addon filters, N addon templates`) is shown in the status bar.
- The output pane is auto-shown only if warnings or errors were logged during loading (e.g., duplicate name overrides, limit reached).

**INI parsing refactor:**

- `ReadINIValueFromData(pszData, section, key, ...)` extracted as the core parsing routine, operating on an arbitrary wide-string buffer. `ReadINIValue` is now a thin wrapper that delegates to the cache.
- `LoadINIFileToBuffer(path, outData)` reads a UTF-8 INI file into a `std::wstring` with BOM stripping.
- `LoadFilters` and `LoadTemplates` accept a `std::vector<INISource>` (data pointer + source directory), enabling unified loading from multiple INI sources.

**Binary size delta:** 348 672 → 356 352 bytes (+7 680 bytes stripped for this phase)

**Filter debug logging (`FilterDebug=1`):**

- `FilterDebug=` in `[Settings]` — default `0` (off). Not auto-written to the INI when absent.
- When active, regular filter execution logs to the output pane: resolved command, working directory (if set), exit code, and captured stderr.
- REPL sessions additionally log: start command + working directory, every input line sent (`>>`), raw and ANSI-stripped stdout/stderr chunks (`<<`), and exit code or "Stopped by user".
- Display=pane filters are forced to append mode while debug is active so filter output does not overwrite debug messages.
- REPL background threads post debug messages via `WM_FILTER_DEBUG` (`WM_USER + 102`) for thread-safe main-thread logging.
- **Raw REPL input:** typing `\raw:<text>` on a REPL prompt line sends `<text>` through escape expansion (`\n`, `\r`, `\t`, `\\`, `\xNN`, `\uNNNN`) directly to stdin with no automatic EOL. The user must include line endings explicitly. Only active when `FilterDebug=1`.

**Binary size delta (with FilterDebug):** 358 400 → 359 936 bytes (+1 536 bytes)

## Building

### Option 1: MSVC Build (Recommended for Windows) ✅

**Why MSVC?** Produces smaller executables (see `BUILD_MSVC.md`).

**Requirements:**
- Visual Studio 2019+ or Build Tools 2022
- Windows 10/11 SDK
- See `BUILD_MSVC.md` for detailed instructions

**Quick Build:**
```cmd
build_msvc.bat
```

**Output:** `msvc\RichEditor.exe`

### Option 2: MinGW-w64 Build (Cross-platform)

**Requirements:**
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

**Output:** `RichEditor.exe` (universal executable with English and Czech)

### Localization

The application is a **universal binary** containing both English and Czech resources in a single executable. Windows automatically selects the appropriate language based on the system's UI language settings.

**Supported Languages:**
- **English (en-US):** Default fallback language - Language ID: 0x0409
- **Czech (cs-CZ):** Full localization - Language ID: 0x0405

**What's Localized:**
- All menus (File/Soubor, Edit/Úpravy, View/Zobrazit, Tools/Nástroje, Help/Nápověda)
- Context-aware menu labels (Undo/Redo with operation type)
- Dialog boxes (About dialog, file open/save dialogs)
- Status bar messages (line, column, character info, EOF)
- Error messages (file operations, INI parsing, filter execution)
- Filter system messages (execution success/failure, context menu)
- Default filter names and descriptions with locale fallback (cs_CZ → cs → en)
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

RichEditor supports opening files from the command line with optional flags:

```bash
RichEditor.exe [options] [filename]
```

**Options:**
- `/nomru` - Open file without adding it to the Most Recently Used (MRU) list
- `/readonly` - Open file in read-only mode (prevents accidental modifications)
- `/elevated-save "<staging>" "<target>"` - **Internal use only.** Used by the editor itself when retrying a save with administrator permissions; not intended for direct invocation.

**Examples:**
```bash
# Normal file open (adds to MRU)
RichEditor.exe document.txt
RichEditor.exe "C:\My Documents\notes.txt"
RichEditor.exe \\server\share\file.txt    # UNC paths supported

# Open without adding to MRU (perfect for temporary/reference files)
RichEditor.exe data.json /nomru
RichEditor.exe /nomru data.json           # Order doesn't matter
RichEditor.exe "C:\Logs\debug.log" /nomru

# Open in read-only mode
RichEditor.exe config.ini /readonly
RichEditor.exe /readonly config.ini       # Order doesn't matter

# Combine options
RichEditor.exe /readonly /nomru server.log
RichEditor.exe reference.txt /nomru /readonly

# File associations (e.g., for JSON viewer)
# Configure Windows file association:
RichEditor.exe "%1" /nomru
```

**Behavior:**
- If a filename is provided, it will be opened on startup
- Relative paths are resolved from the current working directory
- Supports UNC network paths
- File is opened with UTF-8 encoding (no BOM)
- `/nomru` prevents the file from appearing in File menu's recent files list
- `/readonly` opens the file in read-only mode (can be toggled later via File menu)
- Options can appear before or after the filename and can be combined

### Read-Only Mode (Phase 2.10.1+)

RichEditor supports read-only mode to prevent accidental file modifications while allowing safe viewing and non-destructive operations.

**Activating Read-Only Mode:**
- **Menu:** File → Read-Only (checkbox toggle)
- **Command-line:** `RichEditor.exe /readonly filename.txt`
- **Title bar indicator:** `[Read-Only]` appears before `[Resumed]` and `[Interactive Mode]`

**Disabled in Read-Only Mode:**
- File → Save (grayed out)
- All editing operations:
  - Edit → Undo, Redo, Cut, Paste (grayed out)
  - Edit → Time/Date insertion (grayed out)
  - Typing, Backspace, Delete (blocked by RichEdit control)
- Tools → Execute Filter (only for insert/REPL filters)
- Tools → Start Interactive Mode (grayed out)
- Tools → Insert Template (grayed out)
- Tools → Select Filter menu:
  - Insert filters appear grayed out
  - REPL filters appear grayed out
- Context menu:
  - Undo, Cut, Paste (grayed out)
  - Insert filters (hidden)
  - REPL filters (hidden)
- Autosave (both timer-based and focus-loss autosave are disabled)

**Still Available in Read-Only Mode:**
- File operations: Open, Save As, New, Exit
- File → Read-Only toggle (can turn read-only mode on/off)
- Copy and Select All operations
- Search → Find, Find Next, Find Previous, Go to Line, Bookmarks
- View → Word Wrap
- Filters with non-destructive actions:
  - Display filters (messagebox/statusbar)
  - Clipboard filters (copy/append to clipboard)
  - None filters (side effects only)
- Navigation and cursor movement
- Text selection
- URL opening (Enter key on URLs)

**Use Cases:**
- Viewing configuration files without risk of accidental changes
- Reading log files or reference documentation
- Reviewing code without editing
- Opening files in multiple instances (one read-only, one editable)
- File associations for viewing (combine with `/nomru`)

**Example Workflows:**
```bash
# View server configuration without editing risk
RichEditor.exe /readonly C:\Server\config.ini

# Open log file for viewing (no MRU, no editing)
RichEditor.exe /readonly /nomru C:\Logs\application.log

# Review code with statistics filter (display-only, non-destructive)
RichEditor.exe /readonly source.cpp
# Then use Tools → Select Filter → Word Count (works in read-only mode)
```

**Notes:**
- Read-only mode can be toggled on/off at any time via File → Read-Only
- Creating a new file (File → New) automatically disables read-only mode
- Opening a different file preserves the current read-only state
- Save As is allowed (creates a new copy, original remains unmodified)

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
| `Ctrl+0` | Reset zoom to 100% |
| `F5` | Insert time/date |
| `Ctrl+Enter` | Execute current filter |
| `Ctrl+G` | Go to Line |
| `Ctrl+F2` | Toggle bookmark |
| `F2` | Next bookmark |
| `Shift+F2` | Previous bookmark |
| `Ctrl+Shift+T` | Open template picker menu |
| `Enter` | Open URL (when cursor is in URL) |
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
| `Insert` | Toggle insert/overtype mode |

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
- 10 example filters demonstrating all action types

**Application Settings** (`RichEditor.ini`):

```ini
[Settings]
; Editor settings
WordWrap=1                    ; 1=enabled, 0=disabled (default: 1)
TabSize=8                     ; Tab size in spaces for column calculation (default: 8, range: 1-32)
Zoom=100                      ; Zoom percentage (default: 100, range: 1-6400)

; Editor behavior settings
SelectAfterPaste=0            ; 1=select pasted text, 0=cursor after paste (default: 0)
SelectAfterFind=1             ; 1=keep found text selected (default: 1)

; Accessibility settings
ShowMenuDescriptions=1        ; 1=show filter descriptions in menus (accessible), 0=names only (default: 1)

; Autosave settings
AutosaveEnabled=1             ; 1=enabled, 0=disabled (default: 1)
AutosaveIntervalMinutes=1     ; Autosave interval in minutes, 0=disabled (default: 1)
AutosaveOnFocusLoss=1         ; 1=save when window loses focus, 0=don't (default: 1)

; Find/Replace defaults
FindMatchCase=0               ; 1=match case, 0=ignore case (default: 0)
FindWholeWord=0               ; 1=whole word, 0=substring (default: 0)
FindUseEscapes=0              ; 1=enable escape sequences (default: 0)

; Date/time formatting
DateTimeTemplate=%date% %time%
DateFormat=%shortdate%
TimeFormat=HH:mm

; Output pane settings
OutputPaneLines=5             ; Height: integer lines (e.g. "5") or percentage of available area (e.g. "20%")
OutputPaneReadOnly=0          ; 1=read-only pane, 0=editable (default: 0)
; FilterDebug=0               ; 1=log filter/REPL execution to output pane (default: 0, not auto-written)

; Most recently used files (auto-managed)
CurrentFilter=Calculator      ; Last selected filter (auto-saved)

[MRU]
; Most recently used files (auto-populated, up to 10 entries)
File1=C:\path\to\most\recent.txt
File2=C:\path\to\second.txt
File3=C:\path\to\third.txt
; ... entries managed automatically by the application
```

**SelectAfterPaste Feature:**

When `SelectAfterPaste=1`, pasted text is automatically selected after the paste operation. This enables quick positioning:
- Paste text → text is selected
- Press **Up/Down arrow** → cursor jumps to start/end of pasted text
- Press **Left/Right arrow** → cursor jumps to start/end of pasted text

**Use case**: Paste notes at cursor, press Up to position cursor before pasted block, start elaborating.

**⚠️ Important Caveat**: When pasted text is selected, typing ANY character will **replace the entire selection**. This is standard Windows behavior for selected text, but can be surprising if you don't realize the text is selected. If you accidentally overwrite, press `Ctrl+Z` to undo.

**Recommendation**: Leave at default (0) unless you specifically want this power-user feature.

**Note on Settings:**
- All settings are automatically written to the INI file with defaults if missing
- This makes the configuration self-documenting
- You can always see what settings are available by opening the INI file

**Filter Configuration** (`RichEditor.ini`):

The filter system reads from `RichEditor.ini` in the same directory as the executable. Filters are organized into categories that appear as submenus.

```ini
[Filters]
Count=10

[Filter1]
Name=Uppercase
Name.cs=Velká písmena
Name.cs_CZ=Velká písmena
Command=powershell -NoProfile -Command "$input | ForEach-Object { $_.ToUpper() }"
Description=Converts selected text to UPPERCASE letters
Description.cs=Převede vybraný text na VELKÁ písmena
Description.cs_CZ=Převede vybraný text na VELKÁ písmena
Category=Transform
Category.cs=Transformace
Action=insert
Insert=replace
ContextMenu=1
ContextMenuOrder=1

[Filter2]
Name=Lowercase
Name.cs=Malá písmena
Name.cs_CZ=Malá písmena
Command=powershell -NoProfile -Command "$input | ForEach-Object { $_.ToLower() }"
Description=Converts selected text to lowercase letters
Description.cs=Převede vybraný text na malá písmena
Description.cs_CZ=Převede vybraný text na malá písmena
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
  - `Display=pane` - Shows output in the output pane (panel výstupu); pane appears on first use and stays visible for the session
    - `Pane=` - Optional comma-separated modifiers:
      - `append` - Append to existing pane content (default: replace)
      - `focus` - Move focus to pane after writing
      - `start` - Place caret at start of newly written output (position 0 for replace; start of appended chunk for append)
      - `append,focus` - Both modifiers combined

- **`Action=clipboard`** - Copies output to clipboard silently
  - `Clipboard=copy` - Replaces clipboard contents
  - `Clipboard=append` - Appends to existing clipboard contents

- **`Action=none`** - Executes command for side effects (e.g., logging, text-to-speech)

**Context Menu Integration:**

- `ContextMenu=1` - Filter appears in right-click context menu
- `ContextMenu=0` - Filter only in Tools menu (default)
- `ContextMenuOrder=N` - Sort order in context menu (lower numbers first)

**Filter Localization:**
- `Name.cs` / `Description.cs` - Czech translations (language-only)
- `Name.cs_CZ` / `Description.cs_CZ` - Czech translations (full locale)
- Fallback order: full locale → language-only → English

**Filter Categories:**

Filters are automatically organized into submenus based on their `Category=` setting:

- **Transform** (Czech: Transformace) - Text manipulation
- **Statistics** (Czech: Statistiky) - Analysis tools
- **Clipboard** (Czech: Schránka) - Clipboard operations
- **Utility** (Czech: Utility) - Miscellaneous utilities
- **Interactive** (Czech: Interaktivní) - REPL filters

Category display names are localized via `Category.cs=` (or `Category.cs_CZ=`). Only the first filter in a category needs the localized key; subsequent filters in the same category may omit it. User-defined filters can override the display name by providing their own `Category.cs=` for that category.

**Filter Usage:**
1. On first run, RichEditor creates `RichEditor.ini` with example filters demonstrating all action types
2. Edit the INI file to add more filters
3. Restart RichEditor to load the filters (see the manual or INI examples for documentation)
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
- Stderr output is captured and shown in a warning message box
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
3. **Zoom:** Appended to part 1 when zoom is not 100%, e.g. `    200%`
4. **Filter:** Current active filter name (e.g., `[Filter: Calculator]` or `[Filter: None]`)

### Insert/Overtype Mode

RichEditor supports toggling between insert and overtype modes using the **Insert key**:

- **Insert mode (default):** Typed characters are inserted at the cursor position, pushing existing text forward
- **Overtype mode:** Typed characters replace existing characters at the cursor position

**To toggle modes:** Press the **Insert** key

**Note:** The status bar does not display the current mode. This is standard behavior in Windows text editors (Notepad, WordPad). The Insert key toggle is handled internally by the RichEdit control.

**Tips:**
- Overtype mode is useful for quick text replacement without selecting
- To return to insert mode, press **Insert** again
- At the end of a line or document, characters are always inserted (nothing to overtype)
- Before a newline, characters are inserted even in overtype mode (preserves line breaks)

### Word Wrap Position Display

When word wrap is enabled:
- **First number pair** (Ln X, Col Y): Visual position including soft wraps
- **Second number pair** (A,B): Physical position in actual file
- Example: `Ln 12, Col 64 / 11,204`
  - Visual: Line 12 (wrapped), Column 64
  - Physical: Line 11 (actual file), Column 204

**RichEdit 8+ Note:** Word wrap uses Notepad-like internal segmentation (~1000-character visual chunks). This does not change the underlying file content; it only affects display and the visual line count.

### Zoom

RichEditor supports zooming the editor text in and out:

- **Ctrl+mouse wheel** — zoom in/out; word wrap layout is recalculated automatically after each step
- **View → Reset Zoom (Ctrl+0)** — returns to 100%; grayed out when already at 100%
- **Status bar** — shows the current zoom percentage (e.g. `200%`) appended to the position info whenever zoom is not 100%
- **Persisted** — zoom level is saved to `RichEditor.ini` as `Zoom=NNN` on exit and restored on the next launch
- **INI setting:** `Zoom=100` (integer percentage, default 100, range 1–6400)

**Word wrap interaction:** At zoom levels above 100%, RichEditor compensates the wrap point so text wraps at the visible window edge rather than overflowing it.

## Date/Time Formatting

RichEditor provides powerful and flexible date/time formatting for both the F5 key (Insert Time/Date) and template variables. You can choose between predefined locale-aware formats or create custom format strings.

### Quick Start

**Default Behavior (F5 key):**
```
Pressing F5 inserts: 1/20/2026 22:30
```
This uses `%date%` and `%time%` variables with their default formats (`%shortdate%` and `HH:mm`).

**Change date/time format once, affects F5 automatically:**
```ini
[Settings]
DateFormat=yyyy-MM-dd          ; ISO date
TimeFormat=HH:mm:ss            ; 24-hour with seconds
; DateTimeTemplate=%date% %time%  (uses formats above)
```

**Now F5 inserts:**
```
2026-01-20 22:30:45
```

**Override F5 behavior specifically:**
```ini
[Settings]
DateFormat=yyyy-MM-dd          ; Used by %date% variable
TimeFormat=HH:mm:ss            ; Used by %time% variable
DateTimeTemplate=%longdate% 'at' %shorttime%  ; F5 uses different format
```

**Now F5 inserts:**
```
Monday, January 20, 2026 at 10:30 PM
```
(While `%date%` in templates still expands to `2026-01-20`)

### Internal Variables (Locale-Aware)

These variables use Windows locale settings and automatically adapt to your region:

| Variable | Description | Example (US English) | Example (Czech) |
|----------|-------------|---------------------|-----------------|
| `%shortdate%` | Short date format | 1/20/2026 | 20.1.2026 |
| `%longdate%` | Long date format | Monday, January 20, 2026 | pondělí 20. ledna 2026 |
| `%yearmonth%` | Year and month | January 2026 | leden 2026 |
| `%monthday%` | Month and day | January 20 | 20. ledna |
| `%shorttime%` | Time without seconds | 10:30 PM | 22:30 |
| `%longtime%` | Time with seconds | 10:30:45 PM | 22:30:45 |

**Key Features:**
- Automatically translated to your Windows language
- Respect regional date/time format preferences
- No custom format string needed
- Recommended for most users

### User-Defined Variables

Two special variables can be customized via INI settings:

**`%date%` Variable:**
- Controlled by `DateFormat=` setting
- Can be either an internal variable OR a custom format string
- Default: `%shortdate%`

**`%time%` Variable:**
- Controlled by `TimeFormat=` setting
- Can be either an internal variable OR a custom format string
- Default: `HH:mm` (24-hour without seconds)

**Example 1: Use Internal Variable**
```ini
[Settings]
DateFormat=%longdate%
TimeFormat=%longtime%
```

**Example 2: Use Custom Format**
```ini
[Settings]
DateFormat=yyyy-MM-dd          ; ISO 8601 date
TimeFormat=HH:mm:ss            ; 24-hour with seconds
```

### Custom Format Strings

For precise control, you can use Windows date/time format specifiers:

#### Date Format Specifiers

| Specifier | Description | Example |
|-----------|-------------|---------|
| `d` | Day of month (1-31) | 1, 20 |
| `dd` | Day with leading zero (01-31) | 01, 20 |
| `ddd` | Abbreviated day name | Mon, Tue, Wed |
| `dddd` | Full day name | Monday, Tuesday |
| `M` | Month (1-12) | 1, 12 |
| `MM` | Month with leading zero (01-12) | 01, 12 |
| `MMM` | Abbreviated month name | Jan, Feb, Dec |
| `MMMM` | Full month name | January, February |
| `y` | Year without century (0-99) | 0, 26 |
| `yy` | Year without century, padded (00-99) | 00, 26 |
| `yyyy` | Year with century | 2026 |
| `g`, `gg` | Era string | AD, BC |

#### Time Format Specifiers

| Specifier | Description | Example |
|-----------|-------------|---------|
| `h` | Hour 12-hour (1-12) | 1, 10 |
| `hh` | Hour 12-hour padded (01-12) | 01, 10 |
| `H` | Hour 24-hour (0-23) | 0, 22 |
| `HH` | Hour 24-hour padded (00-23) | 00, 22 |
| `m` | Minute (0-59) | 0, 30 |
| `mm` | Minute padded (00-59) | 00, 30 |
| `s` | Second (0-59) | 0, 45 |
| `ss` | Second padded (00-59) | 00, 45 |
| `t` | Single-character AM/PM | A, P |
| `tt` | Multi-character AM/PM | AM, PM |

### Literal Text in Format Strings

Use single quotes to include literal text in your formats:

**Example:**
```ini
DateFormat='Day 'dd' of 'MMMM', 'yyyy
```

**Output:**
```
Day 20 of January, 2026
```

**Escaping Single Quotes:**
```ini
DateFormat='It''s 'dddd'!'
```

**Output:**
```
It's Monday!
```

### F5 Key Template (DateTimeTemplate)

The `DateTimeTemplate` setting controls what F5 inserts. It uses the full template system, so you can:

**Default Behavior (Uses Your Custom Formats):**
```ini
DateTimeTemplate=%date% %time%
```
The default uses `%date%` and `%time%` variables, which automatically respect your `DateFormat=` and `TimeFormat=` settings. Change those settings once, and F5 automatically uses your preferred formats.

**Override with Internal Variables:**
```ini
DateTimeTemplate=%longdate% %shorttime%
```
Use internal variables like `%longdate%`, `%shortdate%`, `%shorttime%`, `%longtime%` to get specific formats regardless of your DateFormat/TimeFormat settings.

**Add Literal Text:**
```ini
DateTimeTemplate='Today is '%longdate%' at '%shorttime%
```

**Use Other Template Variables:**
```ini
DateTimeTemplate=%date% %time% - %selection%
```

**Advanced Example:**
```ini
DateTimeTemplate='## Meeting Notes - '%longdate%'\n\nAttendees: %cursor%\n\n'
```

**Note:** `DateTimeTemplate` can ONLY use template variables (like `%date%`, `%longdate%`, etc.), not raw format specifiers (like `yyyy-MM-dd`). To use custom formats, set them in `DateFormat=` or `TimeFormat=` first, then reference via `%date%` or `%time%`.

### Practical Examples

#### ISO 8601 Format
```ini
[Settings]
DateFormat=yyyy-MM-dd
TimeFormat=HH:mm:ss
DateTimeTemplate=%date%T%time%Z
```
**F5 Output:** `2026-01-20T22:30:45Z`

#### European Format
```ini
[Settings]
DateFormat=dd.MM.yyyy
TimeFormat=HH:mm
DateTimeTemplate=%date% %time%
```
**F5 Output:** `20.01.2026 22:30`

#### US Format with Full Month
```ini
[Settings]
DateFormat=MMMM d, yyyy
TimeFormat=h:mm tt
DateTimeTemplate=%date% at %time%
```
**F5 Output:** `January 20, 2026 at 10:30 PM`

#### Blog Post Header
```ini
[Settings]
DateTimeTemplate='---\ntitle: %cursor%\ndate: '%date%'\nauthor: John Doe\n---\n\n'
DateFormat=yyyy-MM-dd
```
**F5 Output:**
```yaml
---
title: [cursor here]
date: 2026-01-20
author: John Doe
---
```

#### Meeting Notes Template
```ini
[Settings]
DateTimeTemplate='## '%longdate%'\n\n**Time:** '%shorttime%'\n**Attendees:** %cursor%\n\n### Agenda\n\n'
```
**F5 Output:**
```markdown
## Monday, January 20, 2026

**Time:** 10:30 PM
**Attendees:** [cursor here]

### Agenda
```

#### Japanese Format
```ini
[Settings]
DateFormat=yyyy'年'M'月'd'日'
TimeFormat=H:mm
DateTimeTemplate=%date% %time%
```
**F5 Output:** `2026年1月20日 22:30`

### Template System Integration (Phase 2.10+)

Date/time variables work seamlessly with all template features:

**In Insert Template Menu:**
```ini
[Template1]
Name=Daily Journal Entry
Template='# '%longdate%'\n\n## %cursor%\n\n'
Category=General
```

**Combined with Selection:**
```ini
DateTimeTemplate='Modified on '%shortdate%': %selection%'
```

**Combined with Clipboard:**
```ini
DateTimeTemplate='Source: %clipboard%\nRetrieved: '%date%' at '%time%'\n\n'
```

### Troubleshooting

**Q: My custom format shows up literally (e.g., "yyyy-MM-dd" instead of "2026-01-20")**

A: You're using a custom format string in `DateTimeTemplate`. Instead:
```ini
; WRONG:
DateTimeTemplate=yyyy-MM-dd HH:mm

; CORRECT:
DateFormat=yyyy-MM-dd
TimeFormat=HH:mm
DateTimeTemplate=%date% %time%
```
