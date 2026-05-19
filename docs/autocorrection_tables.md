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
Any other character preceded by `\` is emitted literally with the backslash
dropped — so `\=` produces `=`, which is the only way to include a literal
`=` sign in a search key (since `=` is otherwise the key/value separator).

Replace values also support placeholders:

| Placeholder | Expands to |
|-------------|------------|
| `%0` | The matched text (useful when the table is also used manually with
regex-like patterns) |
| `%%` | A literal `%` |

Entries are matched and applied in the order they appear in the section (top
to bottom).

---

## Search key flags

One or both of the following flag characters may be placed at the very start
of a search key to change how it is matched:

| Flag | Meaning |
|------|---------|
| `~` | Case-insensitive match |
| `<` | Whole-word match — the character immediately before **and** after the matched text must not be a letter, digit, or underscore |

Flags may appear in any order and may be combined.  Examples:

```ini
[SmartTyping]
teh=the              ; exact match only (default)
~tEh=the             ; matches teh, TEH, Teh, …
<word=WORD           ; only replaces 'word' when not part of a longer word
~<colour=color       ; case-insensitive whole-word replacement
```

If you need a literal `~` or `<` as the **first character** of a search string,
escape it with a backslash.  `ParseEscapeSequences` drops the backslash and
keeps the character, so the flag scanner never sees it:

```ini
[Symbols]
\~=≈            ; literal ~ at start, no flag
\<=‹            ; literal < at start, no flag
```

Flags apply in all three modes: typing, manual apply, and REPL.

---

## Cursor placement (`\c`)

A `\c` escape sequence in a **replace** string marks where the cursor
(caret) should be positioned after the replacement is inserted.  Text to
the left of `\c` is placed before the cursor; text to the right is placed
after it.

This makes it easy to build character-pair tables:

```ini
[SmartPairs]
(=(^\c)
[=[\c]
{={\c}
"="\c"
\<=<h1>\c</h1>
```

After typing `(`, the editor inserts `()` and places the cursor between
them.  After typing `<h1>` (with `<` escaped so it is not treated as the
whole-word flag), the editor inserts `<h1></h1>` with the cursor inside.

`\c` is only meaningful in the **typing** mode.  In manual-apply and REPL
contexts the sentinel is silently stripped from the result.

### Smart-pair helpers

When a `\c` replacement fires and the closing string is exactly **one
character**, two additional behaviours activate (both require
`SmartPairAssist=1` in `[Settings]`, which is the default):

**Skip-over:** if you type the closing character when the cursor is already
right before it (and no other key has been pressed in between), the editor
moves past the existing closing character instead of inserting a second one.

**Backspace-delete-pair:** if you press Backspace immediately after the pair
was inserted (cursor still right before the closing character), both the
opening and closing characters are deleted together as one undoable step.

These helpers are silently inactive for multi-character closings (e.g.
`</h1>`).

To disable skip-over and Backspace-delete-pair while keeping `\c` cursor
placement working, set `SmartPairAssist=0` in the `[Settings]` section of
`RichEditor.ini`:

```ini
[Settings]
SmartPairAssist=0
```

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
the longest search string (plus one extra character for whole-word boundary
checks) and tests each entry (longest first). The first match at the end of
that window is replaced as a single undoable operation.

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

---

## Sound feedback

To play a WAV file each time a typing autocorrection fires, add the
`AutocorrectionSound` key to the `[Settings]` section of `RichEditor.ini`:

```ini
[Settings]
AutocorrectionSound=sounds\autocorr.wav
```

- **Relative paths** are resolved from the `RichEditor.exe` directory.
- **Absolute paths** are used as-is.
- Leave the key **empty or absent** (the default) to disable sound feedback.
- Only typing autocorrections play the sound; REPL and manual applications are silent.

