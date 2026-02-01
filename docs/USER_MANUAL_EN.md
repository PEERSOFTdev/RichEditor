# RichEditor User Manual (English)

RichEditor is a lightweight, accessible Win32 text editor focused on fast plain-text editing and powerful automation via external filters.

## Getting Started

- Launch: `RichEditor.exe`
- Open a file: `File -> Open` or `Ctrl+O`
- Save: `Ctrl+S` (Save As if the file is untitled)
- Read-only mode: `File -> Read-Only` or `/readonly` on the command line

### Command-Line Options

```
RichEditor.exe [options] [filename]

Options:
  /nomru     Open without adding to MRU
  /readonly  Open in read-only mode
```

## Interface Basics

### Status Bar

Shows:
- Line and column (tab-aware)
- Character at cursor (Unicode aware)
- Active filter name

### Word Wrap

Toggle via `View -> Word Wrap` or `Ctrl+W`. When enabled, the status bar shows both visual and physical positions.

## Editing

- Undo/Redo labels reflect the last operation
- Insert Time/Date: `F5`
- Standard edit shortcuts: Cut/Copy/Paste/Select All

### Time/Date Templates

The `F5` output is controlled by INI settings:

```
[Settings]
DateFormat=%shortdate%
TimeFormat=HH:mm
DateTimeTemplate=%date% %time%
```

## Find & Replace

- Find: `Ctrl+F`
- Find Next: `F3`
- Find Previous: `Shift+F3`
- Replace: `Ctrl+H`

Options:
- Match case
- Whole word
- Use escapes

Supported escape sequences:
- `\n`, `\r`, `\t`, `\\`
- `\xNN` (hex byte)
- `\uNNNN` (Unicode)

Replace placeholders:
- `%0` inserts the matched text
- `%%` inserts a literal `%`

History updates when you run Find/Replace/Replace All.
Replace All is fully undoable.

## Filters (Power Feature)

Filters run external commands on selected text (or current line) and can:
- Insert output
- Display output
- Copy output to clipboard
- Run for side effects only

### Running a Filter

1) Select a filter: `Tools -> Select Filter`
2) Execute: `Ctrl+Enter`

### Interactive REPL Mode

- Start: `Ctrl+Shift+I`
- Exit: `Ctrl+Shift+Q`
- Press Enter on a prompt line to send input to the REPL

## Templates

- Insert template: `Tools -> Insert Template`
- Template picker: `Ctrl+Shift+T`
- File -> New submenu includes template-based documents

Template variables include:
- `%cursor%`, `%selection%`, `%date%`, `%time%`, `%clipboard%`

## Autosave & Session Resume

- Timer autosave (default: 1 minute)
- Autosave on focus loss (when switching to another application)
- Autosave skips untitled files by default

Session resume:
- On shutdown, unsaved work is preserved
- On next launch, content is recovered with a `[Resumed]` indicator
- Optional: `AutoSaveUntitledOnClose=1` for note-taking workflows

## URL Handling

- URLs are detected automatically
- Press `Enter` on a URL to open it
- Right-click on a URL for Open/Copy actions

## Configuration (RichEditor.ini)

The INI file is created next to the executable on first run. It is safe to edit.

Key settings:

```
[Settings]
WordWrap=1
TabSize=8
ShowMenuDescriptions=1
AutosaveEnabled=1
AutosaveIntervalMinutes=1
AutosaveOnFocusLoss=1
SelectAfterPaste=0
AutoSaveUntitledOnClose=0
```

## Keyboard Shortcuts (Core)

```
Ctrl+N  New
Ctrl+O  Open
Ctrl+S  Save
Ctrl+Z  Undo
Ctrl+Y  Redo
Ctrl+X  Cut
Ctrl+C  Copy
Ctrl+V  Paste
Ctrl+A  Select All
Ctrl+W  Word Wrap
F5      Insert Time/Date
Ctrl+F  Find
F3      Find Next
Shift+F3 Find Previous
Ctrl+H  Replace
Ctrl+Enter  Execute Filter
Ctrl+Shift+I Start REPL
Ctrl+Shift+Q Exit REPL
Ctrl+Shift+T Template Picker
```

## Troubleshooting

- Filters do nothing: check the filter command in `RichEditor.ini`.
- Find doesn't wrap: wrapping is intentionally disabled.
- Read-only mode blocks editing and Replace by design.
