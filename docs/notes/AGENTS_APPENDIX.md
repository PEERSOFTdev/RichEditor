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

## TOM (Text Object Model) Usage for Status Bar Performance

`UpdateStatusBar` uses the TOM `ITextDocument` interface to avoid O(N) or full-layout costs on RichEdit 8.0.

### Why TOM

- `EM_EXLINEFROMCHAR` on RichEdit 8 with word wrap ON triggers a full D2D layout pass of all lines on every call — 2–30+ seconds on a 70K-line file.
- `EM_LINEINDEX` in an `O(N)` scan (used before commit `b5c329f`) was similarly slow.
- `GetWindowTextLength` on RichEdit 8 is O(N); replaced with cached `g_nLastTextLen`.
- TOM `ITextRange::GetIndex(tomLine/tomParagraph)` uses RichEdit's internal line table and does not trigger layout; near-instant even at end of file.

### Initialization

- `OleInitialize(NULL)` must be called before `WM_CREATE`. `EM_GETOLEINTERFACE` requires COM/OLE initialized; without it `g_pTextDoc` is always `NULL`.
- `ITextDocument*` is acquired once in `WM_CREATE` via `EM_GETOLEINTERFACE` → `QueryInterface(IID_ITextDocument_)`.
- `IID_ITextDocument` is not in MinGW static import libs; it is defined manually as `IID_ITextDocument_` in `src/main.cpp`.
- `g_pTextDoc->Release()` is called in `WM_DESTROY`; `OleUninitialize()` at end of `wWinMain`.

### Patterns Used

- **Physical line** (`getPhysicalLineAndCol` lambda): `Range(pos,pos)` → `GetIndex(tomParagraph, &line)` → `StartOf(tomParagraph, 0, &delta)` → `GetStart(&lineStart)` → fetch only current line text for tab-aware column. Falls back to O(N) `EM_LINEINDEX` scan if `g_pTextDoc` is NULL.
- **Visual (wrapped) line** (word-wrap ON path): `Range(pos,pos)` → `GetIndex(tomLine, &visLine)` → `StartOf(tomLine, 0, &delta)` → `GetStart(&visLineStart)`. Falls back to `EM_EXLINEFROMCHAR` + `EM_LINEINDEX` if `g_pTextDoc` is NULL or `Range` fails.
- `tomLine` (= 5) = visual display line (respects soft wraps); `tomParagraph` (= 4) = hard-break paragraph line.

### Autosave Flash Timer

- `DoAutosave` previously called `Sleep(1000)` on the UI thread, blocking the message pump during large-file saves.
- Replaced with `SetTimer(IDT_AUTOSAVE_FLASH, 1000)` + `WM_TIMER` handler that restores the previous status text via `g_szAutosaveFlashPrevStatus`.

## AURL Performance (large-file cursor lag)

### Problem

`AURL_ENABLEURL` (RichEdit automatic URL detection) does O(cursor-position) work on every `EN_SELCHANGE` notification. On a 13 MB / ~70,000-line file this causes 2–30+ second freezes on every cursor movement.

### Solution (current)

Read the `DetectURLs` INI setting once in `LoadSettings()`. If `DetectURLs=0`, `EM_AUTOURLDETECT` is never called; RichEdit never builds its internal URL state and there is no per-keystroke overhead. If `DetectURLs=1` (the default), `AURL_ENABLEURL` is sent in `CreateRichEditControl` before `TM_PLAINTEXT`.

Users with large files should add `DetectURLs=0` to `[Settings]` in `RichEditor.ini` and restart.

`g_bAutoURLEnabled` reflects the startup decision and is read by `UpdateStatusBar` to show "URL: off" / "URL: vypnuto" when detection is disabled.

### Why the enable-then-disable approach failed

An earlier attempt enabled `AURL_ENABLEURL` at startup, then sent `EM_AUTOURLDETECT 0` after loading a large file. This was unreliable: RichEdit had already scanned the entire document and built internal URL structures during loading. Disabling AURL afterward did not clear that state, so per-cursor overhead persisted. The correct fix is to never enable AURL in the first place when the user does not want it.

### Why `AURL_ENABLEURL` cannot be replaced by manual `CFE_LINK`

- Manual `EM_SETCHARFORMAT` with `CFM_LINK` only triggers `EN_LINK` mouse events.
- Screen readers never see manually-set `CFE_LINK` as a link (no IAccessible/UIA role).
- Context menu "Open URL" does not appear for manual `CFE_LINK`.
- Keyboard/Enter-to-open URL does not work for manual `CFE_LINK`.
- `EM_SETCHARFORMAT` with `CFM_LINK` is blocked in `TM_PLAINTEXT` mode.
- Therefore `AURL_ENABLEURL` is the only correct approach; the trade-off is disabling it for large-file sessions via the INI setting.

### Ordering requirement

`AURL_ENABLEURL` must be sent **before** `EM_SETTEXTMODE TM_PLAINTEXT` in `CreateRichEditControl`. Some RichEdit versions ignore `AURL_ENABLEURL` if sent after `TM_PLAINTEXT`.

## Template System

- Templates are filtered by file extension.
- Template shortcuts are combined into a dynamic accelerator table.

## Localization

- `resource.rc` uses `#pragma code_page(65001)` and must remain UTF-8 with BOM.
- English + Czech resources are compiled into one universal binary.
