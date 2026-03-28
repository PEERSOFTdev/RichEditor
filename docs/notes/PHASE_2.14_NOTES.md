# Phase 2.14 Implementation Notes

## Addon System

An addon is a subdirectory of `addons/` (next to the executable) containing
`filters.ini` and/or `templates.ini`.  At startup (and on `Tools -> Reload
Addons`), the editor scans this directory, sorts pack names alphabetically, reads
each INI file into a wide-string buffer, and feeds the combined list of INI
sources (main INI first, then addons in order) to `LoadFilters` / `LoadTemplates`.

### Architecture: INISource vector

The central new abstraction is `INISource`:

```cpp
struct INISource {
    const WCHAR* pszData;           // INI content as wide string
    WCHAR szSourceDir[MAX_PATH];    // empty for main INI; addon dir for addon files
};
```

`LoadFilters(const std::vector<INISource>&)` and
`LoadTemplates(const std::vector<INISource>&)` iterate over the vector, reading
sections from each buffer in order.  The legacy no-arg overloads are preserved
for call sites that only need the main INI.

This design was chosen over alternatives (e.g. merging addon INI text into
`g_IniCache.data`) because it keeps addon data out of the main cache, avoids
section-name collisions (`[Filter1]` in the main INI vs. `[Filter1]` in an
addon), and naturally associates each entry with its source directory.

### ReadINIValueFromData extraction

`ReadINIValue` always reads from `g_IniCache.data` — its `pszIniPath` parameter
is vestigial (used only to ensure the cache is loaded).  Addon INI data lives in
separate buffers, so the core parsing logic was extracted into a new `static`
function:

```cpp
static BOOL ReadINIValueFromData(const WCHAR* pszData, LPCWSTR pszSection,
                                 LPCWSTR pszKey, LPWSTR pszValue,
                                 DWORD cchValue, LPCWSTR pszDefault);
```

`ReadINIValue` now delegates to `ReadINIValueFromData(g_IniCache.data.c_str(), ...)`.
`ReadINIIntFromData` wraps this for integer reads.  Both addon and main-INI paths
use the same parsing code, eliminating duplication.

### LoadINIFileToBuffer

Reads an arbitrary UTF-8 file into `std::wstring`:

- Opens with `CreateFile` / `ReadFile` (no `GetPrivateProfileString`, per
  AGENTS.md UNC rule).
- `MultiByteToWideChar(CP_UTF8, ...)` converts to wide.
- Strips UTF-8 BOM (`0xFEFF` after conversion) if present.
- Returns FALSE on any error; caller skips the file silently.

The buffer lifetime is managed by `std::vector<std::wstring>` in `LoadAddons()`.
Pointers into these strings are stored in `INISource::pszData`.  The `pszData`
fixup is deferred until after all `push_back` calls complete so that the
`std::wstring` objects are at their final addresses (vector reallocation would
invalidate earlier pointers).

### Count= optional / probe mode

Both `LoadFilters` and `LoadTemplates` support two modes per source:

- **Counted mode:** `Count=N` in `[Filters]` or `[Templates]` → loop `idx=1..N`,
  skip holes (empty `Name=`).
- **Probe mode:** no `Count=` or `Count=0` → loop `idx=1,2,...`, stop at first
  empty `Name=`.

This means addon authors can omit `Count=` entirely and just number their
sections sequentially.  The main INI continues to work with `Count=` as before.

### Duplicate detection

Duplicates are checked by `Name` only (case-sensitive `wcscmp`).  When a
duplicate is found:

1. The existing slot is reused (overwritten in place).
2. A log message is sent to the output pane via `LogAddonMessage`.
3. `g_bAddonWarnings` is set, which causes the output pane to auto-show.

Last-loaded-wins means addons loaded later alphabetically can override earlier
addons or the main INI.  This is intentional: it lets a "fixes" addon override
a buggy filter from the main config without editing `RichEditor.ini`.

### Working directory for filter execution

Both `FilterInfo` and `TemplateInfo` gained a `WCHAR szSourceDir[MAX_PATH]`
field.  For main-INI entries this is empty; for addon entries it contains the
addon pack directory (e.g. `C:\...\addons\my-suite`).

Two `CreateProcess` call sites pass this as `lpCurrentDirectory`:

- `RunFilterCommand` (~line 8812): normal filter execution.  Added
  `LPCWSTR pszWorkingDir` parameter; all callers updated.
- `StartREPLFilter` (~line 11136): interactive filter launch.

When `szSourceDir[0] == L'\0'`, `NULL` is passed as `lpCurrentDirectory`,
preserving the original behavior (inherit parent's working directory).

### Reload Addons

`ReloadAddons()` performs a full refresh:

1. Clears the output pane (for fresh log output).
2. Calls `LoadAddons()` which re-scans `addons/`, rebuilds the source vectors,
   and calls `LoadFilters` + `LoadTemplates` (both reset their counts to 0
   before loading).
3. Rebuilds filter menu, template menu, File -> New menu.
4. Destroys and rebuilds the accelerator table (template shortcuts may have
   changed).
5. Updates filter display and menu states.

This is triggered by `ID_TOOLS_RELOAD_ADDONS` in the `WM_COMMAND` handler.

### Status and error reporting

- **Status bar:** on successful load with at least one addon pack,
  `IDS_ADDON_STATUS` is formatted with pack count, filter count, and template
  count, and displayed in status bar part 0.
- **Output pane:** `LogAddonMessage()` appends text to the output pane and sets
  `g_bAddonWarnings = TRUE`.  After `LoadAddons` completes, if warnings were
  logged, the output pane is shown automatically.  Normal load summaries go only
  to the status bar — the output pane is not shown for clean loads.

### Forward declaration requirements

`LoadTemplates` is defined early in the file (~line 1098) while
`ReadINIValueFromData`, `ReadINIIntFromData`, `LogAddonMessage`, and
`LoadINIFileToBuffer` are defined much later (~line 7308+).  Forward declarations
of these four `static` functions were added near line 620 to resolve compile
errors.  The alternative — moving `LoadTemplates` later — would have been a
larger diff for no benefit.

---

## Binary size

Baseline (pre-Phase 2.14): 348,672 bytes (stripped).
After Phase 2.14: 356,352 bytes (stripped).
Delta: +7,680 bytes.

The increase comes from the new `LoadAddons`/`ReloadAddons` functions,
`LoadINIFileToBuffer`, `ReadINIValueFromData`/`ReadINIIntFromData`, the
`LogAddonMessage` helper, the additional `szSourceDir` fields in both structs,
and the string resources.  The refactored `LoadFilters`/`LoadTemplates` functions
are slightly larger than the originals due to the multi-source loop and duplicate
detection, but the core parsing logic is shared rather than duplicated.
