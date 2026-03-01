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

## Template System

- Templates are filtered by file extension.
- Template shortcuts are combined into a dynamic accelerator table.

## Localization

- `resource.rc` uses `#pragma code_page(65001)` and must remain UTF-8 with BOM.
- English + Czech resources are compiled into one universal binary.
