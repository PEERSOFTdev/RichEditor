# RichEditor User Manual (English)

RichEditor is a lightweight, accessible Win32 text editor for plain text. It is designed to stay fast, keep the interface predictable, and let power features live in external tools (filters and templates). If you read this manual once, you will likely discover features that are intentionally hidden from the surface UI.

## Quick Start

Open and save like any classic editor: `File -> Open` (`Ctrl+O`) and `Ctrl+S`. Toggle word wrap with `View -> Word Wrap` (`Ctrl+W`). Find, Replace, and Go to Line live under `Search` (`Ctrl+F`, `Ctrl+H`, `Ctrl+G`).

## UI Basics and Status Bar

### Title Bar Indicators

RichEditor is very explicit about state. The title bar uses:

- `*` for unsaved changes
- `[Read-Only]` when editing is blocked
- `[Resumed]` when a document was recovered after shutdown
- `[Interactive Mode]` while a REPL filter is running

### Status Bar

The status bar is built for accessibility and precision. It shows the cursor **position** (tab-aware), the **character** under the caret, and the **currently selected filter**.

When word wrap is ON, the position shows **visual** and **physical** values (soft-wrap vs. real line breaks). This is important for screen readers and for exact column work.

Advanced: the character field includes Unicode codes, control characters, surrogate pairs, and EOF. This makes the editor useful for inspecting files that contain invisible characters.

## Editing

Undo/Redo labels describe the last action (Typing, Paste, Replace All, etc.). Insert/Overtype mode toggles with the **Insert** key. `F5` inserts a time/date string based on your configuration.

Files are saved as UTF‑8 without BOM. Line endings are preserved when possible; default save uses Windows CRLF.

If saving fails because the target path requires administrator access, the editor prompts you to retry with elevated permissions. A UAC dialog will appear; if you approve, the file is saved using a temporary staging copy. This applies to both Save (`Ctrl+S`) and Save As.

Advanced: `SelectAfterPaste=1` selects pasted text automatically. Typing replaces the selection; this is a power feature, not a default.

## Word Wrap and Positions

Word wrap changes only how lines are **displayed**, not how they are saved.

- **Wrap ON**: lines wrap to the window width.
- **Wrap OFF**: long lines stay on one physical line.

Note: With Word Wrap off, RichEdit 8+ may still visually segment long lines around ~1000 characters without inserting line breaks. This matches Windows 11 Notepad behavior and is a display-only effect.

## Find and Replace

Find and Replace behave like classic editors (`Ctrl+F`, `Ctrl+H`, `F3`, `Shift+F3`). History is saved only when you perform Find/Replace/Replace All.

Go to Line (`Ctrl+G`) jumps to a specific line number and scrolls it into view.
Line counting follows Word Wrap mode: visual lines when wrap is ON, physical lines when wrap is OFF.

Advanced:
- Escape sequences: `\n`, `\r`, `\t`, `\\`, `\xNN`, `\uNNNN`
- Replace placeholders: `%0` inserts matched text, `%%` inserts a literal `%`
- Replace All can be fully undone.

## Autosave and Session Resume

Autosave runs on a timer (default: 1 minute) and when you switch to another application. Untitled files are not autosaved by default.

If Windows shuts down or restarts, RichEditor can recover unsaved work. Recovered files show `[Resumed]` in the title bar.

Advanced: `AutoSaveUntitledOnClose=1` saves untitled work on close without prompting.

## Read-Only Mode

Read-only mode prevents edits but still allows viewing and navigation. You can open files, use Save As, copy and select text, run Find, and use non-destructive filters.

## Filters (External Tools)

Filters run external commands on selected text (or the current line if nothing is selected). They can insert output, display it, copy it to the clipboard, or run for side effects only. This lets you build features without bloating the editor itself. Interactive (REPL, Read‑Eval‑Print Loop) filters exist for advanced use and are described in the INI section.

Examples include Uppercase/Lowercase, Sort Lines, Line Count, and Word Count. These are meant as starting points; the rest is for you to explore in the menus.

Advanced: filters and categories are defined in `RichEditor.ini`. Categories are user-defined and can be renamed or replaced. You can also control whether a filter appears in the context menu.

The `Command` field accepts a `script:` prefix to run a JScript expression in-process (no external process spawned). The special variable `INPUT` holds the selected text. Example: `script:INPUT.toLocaleUpperCase()`. This correctly handles all Unicode characters including diacritics. JScript reference: https://learn.microsoft.com/en-us/previous-versions//hbxc2t98(v=vs.85)

Three things to keep in mind when writing `script:` expressions:

- The value after `script:` must be a single expression — bare statements such as `var x = 1` are not valid at the top level; for multi-step logic, use an IIFE that returns its result: `(function(){var n=INPUT.length;return String(n)})()`.
- If the expression evaluates to `undefined`, `null`, or an empty string, the filter silently produces no output and no error — wrapping the result in `String(…)` is a safe habit.
- A semicolon preceded by a space or tab anywhere in `Command=` is treated as the start of an INI comment and silently drops everything after it, so keep semicolons tight against their preceding token: write `return x})()` not `return x })()`.

## Templates

Templates insert snippets with variables. You can insert them from `Tools -> Insert Template`, use the template picker (`Ctrl+Shift+T`), or create new documents from templates under `File -> New`.

### Template Variables

Templates support these variables:

- `%cursor%` — cursor position after insertion
- `%selection%` — current selection
- `%clipboard%` — clipboard text
- `%date%`, `%time%` — configurable formats
- `%shortdate%`, `%longdate%`, `%yearmonth%`, `%monthday%`
- `%shorttime%`, `%longtime%`

Advanced: templates are editable in `RichEditor.ini`. You can localize template names and descriptions using `Name.xx` and `Description.xx` keys (for example, `Name.cs`). Unknown variables are preserved as literals.

## Addons (Filter and Template Packs)

You can extend RichEditor by placing addon packs in an `addons` folder next to the executable. Each addon is a subdirectory containing `filters.ini` and/or `templates.ini`.

Example layout:

```
RichEditor.exe
addons/
  my-tools/
    filters.ini
    tools/
      myfilter.exe
  snippets/
    templates.ini
```

- Addon INI files use the same `[Filter1]`/`[Template1]` section format as the main `RichEditor.ini`. The `Count=` key is optional; if omitted, sections are probed sequentially.
- Addon directories are loaded alphabetically by name.
- If an addon defines a filter or template with the same `Name=` as an existing one, the addon version overwrites it (last loaded wins). The override is logged to the output pane.
- Filter commands from addons are executed with their addon directory as the working directory, so relative paths to bundled tools work.
- Use `Tools -> Reload Addons` to reload all addons without restarting.

## URL Handling

URLs are detected automatically. Press Enter on a URL to open it, or right-click to open/copy. If cursor movement feels slow on a very large file, set `DetectURLs=0` in `[Settings]` and restart.

## Configuration (INI)

On first run, RichEditor creates `RichEditor.ini` next to the executable. The file is self‑documenting with comments, and you can safely edit it while the app is closed. Lines starting with `;` or `#` are treated as comments. If you remove the comments and want them back, delete the INI file and let RichEditor recreate it.

Below is a complete, structured overview of all INI sections and keys currently used by the editor. Defaults shown here match the autogenerated INI.

### [Settings]

Editor behavior and defaults.

- `WordWrap` (default `1`): 1 = wrap enabled, 0 = disabled; controls whether long lines wrap to the window width.
- `AutosaveEnabled` (default `1`): master autosave switch.
- `AutosaveIntervalMinutes` (default `1`): autosave interval; 0 disables timer autosave even if enabled.
- `AutosaveOnFocusLoss` (default `1`): autosave when switching to another app.
- `ShowMenuDescriptions` (default `1`): show filter descriptions in menus (accessibility).
- `SelectAfterPaste` (default `0`): select pasted text automatically.
- `AutoSaveUntitledOnClose` (default `0`): save untitled work on close without prompting.
- `DetectURLs` (default `1`): 1 = detect and highlight URLs (blue underline, click/Enter to open, screen reader announces as link). Set to 0 if cursor movement is slow on very large files — RichEdit scans the document on every keystroke when URL detection is on. Requires restart. Status bar shows "URL: off" when disabled.
- `TabSize` (default `8`): tab width in spaces for column calculation (valid range 1–32).
- `SelectAfterFind` (default `1`): keep found text selected after a successful find.
- `FindMatchCase` (default `0`): match case by default in Find/Replace.
- `FindWholeWord` (default `0`): whole‑word search by default.
- `FindUseEscapes` (default `0`): interpret escape sequences (\n, \t, \xNN, \uNNNN) in Find/Replace.

Date and time formatting.

- `DateTimeTemplate` (default `%date% %time%`): inserted by F5 and the Insert Time/Date command. Use `%date%`/`%time%` to honor `DateFormat`/`TimeFormat`, or use `%shortdate%`, `%longdate%`, `%shorttime%`, `%longtime%` directly.
- `DateFormat` (default `%shortdate%`): format used by `%date%` in templates. Set it to a built‑in variable (like `%shortdate%`) or a custom date format string.
- `TimeFormat` (default `HH:mm`): format used by `%time%` in templates. Set it to a built‑in variable (like `%shorttime%`) or a custom time format string.
- `OutputPaneLines` (default `5`): height of the output pane. Use an integer for a fixed number of lines (for example, `10`) or append `%` for a percentage of the available area (for example, `20%`).
- `OutputPaneReadOnly` (default `0`): `1` makes the output pane read-only; `0` keeps it editable.
- `FilterDebug` (default `0`): `1` enables debug logging for filter and REPL execution. When active, the output pane shows the resolved command, working directory, exit code, and stderr for each filter run. REPL sessions additionally log every input line sent and output chunk received. In REPL mode, typing `\raw:<text>` on a prompt line sends the text with escape expansion and no automatic EOL — useful for testing exact byte sequences. This key is not written to the INI by default; add `FilterDebug=1` manually when needed for troubleshooting.

Date/time variables and custom formats (Windows date/time picture tokens).

- Built‑in variables:
  - `%shortdate%`: system short date (for example, 1/20/2026).
  - `%longdate%`: system long date (for example, Monday, January 20, 2026).
  - `%yearmonth%`: year and month (for example, January 2026).
  - `%monthday%`: month and day (for example, January 20).
  - `%shorttime%`: short time without seconds (for example, 10:30 PM).
  - `%longtime%`: long time with seconds (for example, 10:30:45 PM).
- Date tokens: `d dd ddd dddd M MM MMM MMMM y yy yyyy g gg`.
- Time tokens: `h hh H HH m mm s ss t tt`.
- Example templates: `DateTimeTemplate=%date% 'at' %time%`, `DateTimeTemplate=%longdate% 'at' %shorttime%`.
- Custom date formats (examples): `yyyy-MM-dd`, `dd.MM.yyyy`, `MMMM d, yyyy`.
- Custom time formats (examples): `HH:mm`, `h:mm tt`, `HH:mm:ss`.
- Literals: wrap in single quotes (example: `DateTimeTemplate=%longdate% 'at' %shorttime%`).

RichEdit library selection (advanced).

- `RichEditLibraryPath` (default empty): custom RichEdit DLL path; empty uses auto‑detection.
- `RichEditClassName` (default empty): optional class override (e.g., `RichEditD2DPT`, `RichEdit60W`).

Internal state (usually not edited by hand).

- `CurrentFilter` (default empty): last selected filter by name.
- `CurrentREPLFilter` (default empty): last selected REPL filter by name.

### [Filters] and [FilterN]

`[Filters]` stores an optional key:

- `Count` (optional): number of filter entries. If omitted, filter sections are probed sequentially (`[Filter1]`, `[Filter2]`, ...) until an empty or missing `Name=` is found.

Each `[FilterN]` defines one filter. Required fields:

- `Name`: display name.
- `Command`: command line to execute.

Common optional fields:

- `Description`: short help text (shown in menus when enabled).
- `Category`: submenu grouping (user‑defined; can be any label).
- `Name.xx` / `Description.xx`: localized strings (for example, `Name.cs`).
- `Action` (default `insert`): behavior type. `insert` = insert output, `display` = show output, `clipboard` = put in clipboard, `none` = side effects only, `repl` = interactive (REPL).

Action‑specific fields:

- `Insert` (default `below`): `replace`, `below`, or `append`.
- `Display` (default `messagebox`): `messagebox`, `statusbar`, or `pane`.
  - `pane` shows output in the output pane below the editor. The pane appears on the first use and stays visible for the session.
  - Optional `Pane` key (comma‑separated): `append` (add to existing content instead of replacing), `focus` (move focus to pane after writing), `start` (place caret at start of newly written output: position 0 for replace mode, start of appended chunk for append mode). Example: `Pane=append,focus`.
- `Clipboard` (default `copy`): `copy` or `append`.
- `PromptEnd` (default `> `): REPL prompt terminator string (for example, `> ` or `$ `).
- `EOLDetection` (default `auto`): end‑of‑line (EOL) detection for REPL output. `auto` detects from the first output; `crlf` = Windows, `lf` = Unix/Linux/WSL, `cr` = classic Mac.
- `ExitNotification` (default `1`): 1 shows a dialog when a REPL filter exits, 0 is silent.

Context menu placement:

- `ContextMenu` (default `0`): show in right‑click menu (1) or only in Tools (0).
- `ContextMenuOrder` (default `999`): sort order; lower numbers appear first.

### [Templates] and [TemplateN]

`[Templates]` stores an optional key:

- `Count` (optional): number of template entries. If omitted, template sections are probed sequentially until an empty or missing `Name=` is found.

Each `[TemplateN]` defines one template:

- `Name`: template name.
- `Description`: short help text.
- `Category`: submenu grouping (user‑defined; can be any label).
- `Name.xx` / `Description.xx`: localized strings (for example, `Name.cs`).
- `FileExtension` (default empty): limit to a file type; empty means all.
- `Template`: template text; supports `\n`, `\t`, `\r`, `\\`.
- `Shortcut` (default empty): optional hotkey. Use the same format as `Ctrl+1`, `Ctrl+Shift+C`, or `Alt+F2`. Supported key names include letters A–Z, numbers 0–9, F1–F12, and special keys like `Enter`, `Space`, `Tab`, `Backspace`, `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, and arrow keys. If a shortcut collides with built‑in commands, it is ignored; if unsure about key names, check the default template examples in `RichEditor.ini`.

### [MRU]

Stores most recently used files. `File1` is the newest entry, followed by `File2`, `File3`, and so on. The list length depends on recent activity.

### [FindHistory] / [ReplaceHistory]

Each section stores:

- `Count`: number of history items.
- `Item1`, `Item2`, ...: most recent first.

### [Resume]

Autosave recovery data:

- `ResumeFile`: path to the resume file.
- `OriginalPath`: original path (empty for untitled files).

Resume files are stored in `%TEMP%\RichEditor\` by default.

## Screen Reader Support

RichEditor annotates its controls with accessible names using the MSAA Dynamic Annotation API (`IAccPropServices`). NVDA, JAWS, and Windows Narrator announce:

- **"Text Editor"** for the main editing area.
- **"Output Pane"** for the secondary output pane (when present).

Both MSAA and UI Automation clients receive these names as the control's Name property.

## RichEdit Library (Advanced)

RichEditor can load newer RichEdit DLLs for better accessibility and rendering. Windows 11 Notepad ships with a modern RichEdit build that improves plain-text caret/selection behavior (especially near line ends and trailing spaces) and uses DirectWrite and UI Automation for color emoji and screen reader support.

You can point to Office RichEdit DLLs (e.g., Office 2013+), which often include more recent fixes than the Windows system DLL. Use `RichEditLibraryPath` and `RichEditClassName` in the INI if you want to experiment. For Windows Notepad, a typical path looks like: `C:\Program Files\WindowsApps\Microsoft.WindowsNotepad_11.x.x.x_x64__8wekyb3d8bbwe\Notepad\riched20.dll`.

## Command Line (Advanced)

```
RichEditor.exe [options] [filename]

/nomru     Open without adding to MRU
/readonly  Open in read-only mode
```

Note: `/elevated-save` is used internally by the editor when retrying a save with administrator permissions. It is not intended for direct use.

## Keyboard Shortcuts

### Application Shortcuts

| Shortcut | Action |
| --- | --- |
| Ctrl+N | New |
| Ctrl+O | Open |
| Ctrl+S | Save |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+X | Cut |
| Ctrl+C | Copy |
| Ctrl+V | Paste |
| Ctrl+A | Select All |
| Ctrl+W | Word Wrap |
| F3 | Find Next |
| Shift+F3 | Find Previous |
| Ctrl+F | Find |
| Ctrl+H | Replace |
| Ctrl+G | Go to Line |
| Ctrl+F2 | Toggle Bookmark |
| F2 | Next Bookmark |
| Shift+F2 | Previous Bookmark |
| F5 | Insert Time/Date |
| Ctrl+Enter | Execute Filter |
| Ctrl+Shift+T | Template Picker |
| Ctrl+Shift+I | Start Interactive Mode |
| Ctrl+Shift+Q | Exit Interactive Mode |
| F6 | Switch focus between editor and output pane (when pane is visible) |
| Ctrl+Shift+Delete | Clear output pane (when focus is in the pane) |

### RichEdit Native Shortcuts

These are provided by the RichEdit control itself and work in RichEditor:

| Shortcut | Action |
| --- | --- |
| Alt+X | Convert Unicode hex to character (and back) |
| Alt+Shift+X | Convert character to Unicode hex |
| Ctrl+Left/Right | Move by word |
| Ctrl+Up/Down | Move by paragraph |
| Ctrl+Home/End | Start/end of document |
| Ctrl+Page Up/Down | Move by page |
| Shift+Arrow | Extend selection |
| Ctrl+Shift+Arrow | Extend selection by word/paragraph |
| Ctrl+Backspace | Delete previous word |
| Ctrl+Delete | Delete next word |
| Shift+Delete | Cut |
| Shift+Insert | Paste |
| Insert | Toggle overtype |
