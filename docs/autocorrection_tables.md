# Autocorrection Tables

Autocorrection tables let you define search → replace pairs that RichEditor
applies automatically in one or more of three modes:

| Mode | When it fires |
|------|---------------|
| `typing` | After each keystroke in the editor (including the REPL prompt) |
| `repl` | To each chunk of output received from the REPL process, before ANSI colours are stripped |
| manual | Via **Tools → Apply Autocorrections → _Table name_** |

---

## INI format

Tables are defined in `autocorrections.ini` files placed in addon pack
subdirectories under `addons/`, **or** directly in `RichEditor.ini`.

### Section `[AutocorrectionTables]`

```ini
[AutocorrectionTables]
Count=2
```

`Count` is optional; if omitted, numbered table sections are probed
sequentially until a section with no `Name=` key is found.

### Section `[AutocorrectionTableN]`

One section per table, numbered from 1:

```ini
[AutocorrectionTable1]
Name=Emoticons
Name.cs=Emotikony
Description=Replaces ASCII emoticons with Unicode emoji
Description.cs=Nahrazuje ASCII emotikony unicode emoji
```

| Key | Description |
|-----|-------------|
| `Name` | Table name shown in the Tools menu and referenced in `[AutocorrectionSettings]`. Required. |
| `Name.xx` | Localised name for language code `xx` (e.g. `Name.cs` for Czech). |
| `Description` | Short description, shown in the menu as a tooltip where supported. |
| `Description.xx` | Localised description. |

### Section `[_TableName_]`

The actual entries, one per line in `Search=Replace` format:

```ini
[Emoticons]
:-)=☺
:-(=☹
:-D=😀
;-)=😉
```

Search and replace values both support escape sequences: `\n`, `\r`, `\t`,
`\\`, `\xNN` (hex byte), `\uNNNN` (Unicode code point).

Replace values also support placeholders:

| Placeholder | Expands to |
|-------------|------------|
| `%0` | The matched text (useful when the table is also used manually with
regex-like patterns) |
| `%%` | A literal `%` |

Entries are matched and applied in the order they appear in the section (top
to bottom).

---

## Activation: `[AutocorrectionSettings]`

Activation is configured in **`RichEditor.ini`** (not in addon INI files):

```ini
[AutocorrectionSettings]
Emoticons=typing,repl
MyOtherTable=typing
```

The value is a comma-separated list of modes: `typing`, `repl`, or both.
A table with no entry in `[AutocorrectionSettings]` is available for manual
use only (it appears in the submenu but is never applied automatically).

---

## Addon example layout

```
addons/
  my-corrections/
    autocorrections.ini
```

An addon `autocorrections.ini` uses the same `[AutocorrectionTables]` /
`[AutocorrectionTableN]` / `[_TableName_]` structure as the main INI.
Activation (`[AutocorrectionSettings]`) is always read from the **main**
`RichEditor.ini`, never from an addon file.

If an addon defines a table with the same `Name=` as an existing one, the
addon version overwrites it (last loaded wins). The override is logged to
the output pane.

---

## How typing autocorrection works

After every character is inserted, RichEditor looks back up to the length of
the longest search string and tests each entry (longest first). The first
match at the end of that window is replaced as a single undoable operation.

- The replacement is only attempted when the editor is not in read-only mode.
- In REPL mode the typing hook still fires for text you type in the prompt.
- A bare caret is required; autocorrection is skipped if text is selected.

---

## How REPL autocorrection works

Before ANSI colour codes are stripped from REPL output, every `repl`-enabled
table is applied to the raw output chunk. This means autocorrections see the
full output including any embedded escape sequences, so they can also match
and replace control characters.

---

## Manual application

**Tools → Apply Autocorrections → _Table name_** applies the chosen table to:

- the current **selection**, if any text is selected; or
- the **entire document**, if nothing is selected.

The replacement is performed as a single undoable operation.

The submenu item is greyed out when the editor is in read-only mode or when
no tables have been loaded.
