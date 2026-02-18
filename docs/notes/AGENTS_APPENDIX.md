# AGENTS_APPENDIX.md - RichEditor Deep Notes

This file holds extended design notes, historical context, and larger implementation patterns. Keep `AGENTS.md` concise and current.

## String Handling

- Prefer wide Win32 APIs.
- Avoid fragile formatting with user-provided Unicode.
- `_snwprintf` is acceptable for fixed-format, non-user input; use `wcscpy`/`wcscat` for user strings.

## INI System

- Custom INI parsing is required for UNC paths.
- INI reads/writes are cached in `g_IniCache` and flushed via `FlushIniCache()`.
- Cache is invalidated when `CreateDefaultINI()` writes the default file.

## Find/Replace History

- MRU ordering: newest is `Item1`.
- History is saved only on actions; checkbox state is saved on toggle.

## Elevated Save (Phase 2.x Pattern)

- On `ERROR_ACCESS_DENIED` during save, the editor stages content to `%TEMP%\RichEditor\` and re-launches itself with `/elevated-save "<staging>" "<target>"` via `ShellExecuteEx` with `runas` verb.
- The child process runs `ElevatedSaveWorker` headlessly (no window), copies the staging file to the target, exits with 0 on success or `GetLastError()` on failure.
- The parent waits synchronously, reads the exit code as a Win32 error, and either calls `FinalizeSuccessfulSave` or `ShowSaveTextFailure`.
- `RestoreForegroundAfterElevation` is `static`; it does a best-effort `SetForegroundWindow` after UAC completes.
- `OFN_NOTESTFILECREATE` is set in Save As to prevent the shell from refusing to show the dialog for paths the user cannot write (e.g., `C:\Windows\`). A manual overwrite check follows.

- Two-phase shutdown: use `WM_QUERYENDSESSION` to prepare, `WM_ENDSESSION` to commit.
- Only write resume entry in `WM_ENDSESSION` when shutdown is confirmed.
- Clear resume entry immediately after reading on startup (multi-instance safe).

## Template System

- Templates are filtered by file extension.
- Template shortcuts are combined into a dynamic accelerator table.

## Localization

- `resource.rc` uses `#pragma code_page(65001)` and must remain UTF-8 with BOM.
- English + Czech resources are compiled into one universal binary.
