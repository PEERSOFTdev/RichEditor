# Filter Debug Logging — Design Notes

## Overview

`FilterDebug=1` in `[Settings]` enables diagnostic output for filter and REPL
execution. All debug messages are appended to the output pane. The feature is
opt-in and the key is not auto-written to the INI.

## Regular filter debug output

When a regular filter (non-REPL) runs, the output pane shows:

- `[Filter] Command: <resolved command>` — the full command passed to
  `CreateProcess`.
- `[Filter] Working dir: <path>` — only when a working directory is set
  (addon filters).
- `[Filter] Exit code: <N>` — process exit code.
- `[Filter] stderr:` — header + captured stderr, when present.

Display=pane filters are forced to `bAppend=TRUE` while debug is active so
that filter output does not overwrite the debug messages.

## REPL filter debug output

REPL sessions log five event types:

| Tag | When |
|-----|------|
| `[REPL] Started: <command>` | After process and threads are created |
| `[REPL] Working dir: <path>` | If a working directory is set |
| `[REPL] >> <text>` | Each line sent to REPL stdin |
| `[REPL] << (raw) <text>` | Stdout chunk before ANSI stripping |
| `[REPL] << <text>` | Stdout chunk after ANSI stripping |
| `[REPL] << (stderr raw) <text>` | Stderr chunk before ANSI stripping |
| `[REPL] << (stderr) <text>` | Stderr chunk after ANSI stripping |
| `[REPL] Exited (code N)` | Process exited on its own |
| `[REPL] Stopped by user` | User exited via menu/shortcut |

## Thread safety

REPL stdout and stderr threads cannot call `LogFilterDebug()` directly
(Win32 UI calls must be on the main thread). Instead they `malloc` a
formatted message and `PostMessage(WM_FILTER_DEBUG, 0, (LPARAM)ptr)`.
The `WM_FILTER_DEBUG` handler on the main thread calls `LogFilterDebug()`
and frees the memory.

`WM_FILTER_DEBUG` is `WM_USER + 102`.

## INI behaviour

- Key: `FilterDebug` in `[Settings]`.
- Default: `0` (off).
- Not written to INI when absent (keeps INI clean for non-developers).
- Loaded via `ReadINIInt(..., -1)` sentinel: `-1` means absent, treat as off.

## Binary size impact

+1 536 bytes stripped (358 400 -> 359 936).

## Raw REPL input (`\raw:` prefix)

When `FilterDebug=1` and a REPL session is active, typing a line starting with
`\raw:` sends the text after the prefix through escape expansion and directly
to REPL stdin **without appending any EOL**. The user must include `\n`, `\r\n`,
etc. explicitly. This uses the same `ParseEscapeSequences()` function as
Find/Replace.

Examples (typed after the REPL prompt):

| Input | Bytes sent to stdin |
|-------|-------------------|
| `\raw:hello` | `hello` (no newline) |
| `\raw:hello\n` | `hello` + LF |
| `\raw:hello\r\n` | `hello` + CR LF |
| `\raw:\x1b[2J` | ESC `[2J` (ANSI clear) |
| `\raw:\t\t\n` | TAB TAB LF |

The debug log shows `[REPL] >> (raw debug) <escaped form>` for each raw send.

When `FilterDebug=0`, the `\raw:` prefix is sent as normal text with automatic
EOL — no special handling occurs.
