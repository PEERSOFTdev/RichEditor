# RichEditor

RichEditor is a lightweight, accessible Win32 text editor built around the RichEdit control. It focuses on fast plain-text editing, strong accessibility, and an extensible filter system for power users.

## Highlights

- Plain-text editor with UTF-8 files (no BOM)
- Accessibility-first UI (screen reader friendly menus, accurate status bar data)
- Fast Find/Replace with history and undo support
- External filter system (transform, display, clipboard, and side-effect actions)
- URL auto-detection with keyboard activation
- Autosave (timer + app switch)
- Optional session resume for unsaved work
- English + Czech resources in one binary

## Quick Start

Run the editor:

```cmd
RichEditor.exe
```

Open a file in read-only mode:

```cmd
RichEditor.exe /readonly "C:\path\file.txt"
```

Open without adding to MRU:

```cmd
RichEditor.exe /nomru "C:\path\file.txt"
```

## Features (User-Facing)

### Editing

- New/Open/Save/Save As
- Undo/Redo with descriptive labels
- Word wrap toggle
- Time/Date insertion (F5) with configurable templates
- Read-only mode with UI protection

### Find & Replace

- Find Next/Previous (F3 / Shift+F3)
- Replace and Replace All with undo support
- History (MRU) for find/replace terms
- Options (match case, whole word, escapes) persist on toggle

### Filters (Power Feature)

Filters run external commands and operate on selected text (or current line). Actions:

- **Insert**: replace / below / append
- **Display**: status bar / message box
- **Clipboard**: copy / append
- **None**: side-effect only

Filters are configured in `RichEditor.ini` (auto-generated on first run).

### Accessibility

- Screen reader friendly menu labels
- Accurate status bar line/column (tab aware)
- URL detection announced as links

## Configuration

RichEditor auto-creates `RichEditor.ini` on first run. The INI lives next to the executable. The file is self-documenting and safe to edit.

Key settings include:

```ini
[Settings]
WordWrap=1
TabSize=8
AutosaveEnabled=1
AutosaveIntervalMinutes=1
AutosaveOnFocusLoss=1
SelectAfterPaste=0
ShowMenuDescriptions=1
AutoSaveUntitledOnClose=0
```

### Find/Replace History

History is stored as:

```ini
[FindHistory]
Count=3
Item1=foo
Item2=bar
Item3=baz
```

`Item1` is always the most recent.

## Build

### MinGW-w64

```bash
make
```

### MSVC

See `BUILD_MSVC.md` for the Windows toolchain build.

## Localization

The binary ships with English and Czech resources. Windows selects the UI language automatically.

## Repository Notes

- `src/main.cpp` contains most of the application logic.
- Agent/developer guidance: `AGENTS.md` and `docs/notes/AGENTS_APPENDIX.md`.

## User Manual

- English: `docs/USER_MANUAL_EN.md`
- Czech: `docs/USER_MANUAL_CS.md`

## Development Phases

- `docs/PHASES.md`

## Change Checklist

- `docs/CHANGE_CHECKLIST.md`

## License

TBD
