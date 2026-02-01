# Phase 2.10 Implementation Plan - Configurable Date/Time Format (ToDo #3)

**Status:** Awaiting approval  
**Date:** January 21, 2026  
**Target:** RichEditor v2.10

---

## Executive Summary

This phase implements fully configurable date/time formatting using Windows GetDateFormatEx/GetTimeFormatEx APIs, eliminating all hardcoded date/time formats. Users gain complete control over date/time insertion through internal variables, custom format strings, and literal text mixing.

---

## Design Overview

### Three Variable Types

#### 1. Internal Variables (Predefined, dwFlags-based)
Locale-aware system formats:
- `%shortdate%` → `DATE_SHORTDATE` (e.g., "1/20/2026")
- `%longdate%` → `DATE_LONGDATE` (e.g., "Monday, January 20, 2026")
- `%yearmonth%` → `DATE_YEARMONTH` (e.g., "January 2026")
- `%monthday%` → `DATE_MONTHDAY` (e.g., "January 20")
- `%longtime%` → 0 flag (e.g., "10:30:45 PM")
- `%shorttime%` → `TIME_NOSECONDS` (e.g., "10:30 PM")

#### 2. User-Configurable Variables
Custom format strings:
- `%date%` → Uses `DateFormat=` INI setting
- `%time%` → Uses `TimeFormat=` INI setting

**Note:** `%datetime%` variable has been removed. Users can combine variables manually in templates (e.g., `%date% %time%`).

#### 3. F5 Key Template
- `DateTimeTemplate=` INI setting
- Can use **both** internal variables (dwFlags-based) **and** user-defined variables (`%date%`, `%time%`)
- Cannot use raw format specifiers directly (e.g., `yyyy-MM-dd` not allowed in DateTimeTemplate)

**Examples:**
- ✅ `DateTimeTemplate=%shortdate% %shorttime%` (internal variables)
- ✅ `DateTimeTemplate=%date% %time%` (user-defined variables)
- ✅ `DateTimeTemplate=%longdate%, %longtime%` (internal variables)
- ❌ `DateTimeTemplate=yyyy-MM-dd HH:mm` (raw specifiers not allowed)

---

## INI Configuration

```ini
[Settings]
; F5 key / Edit→Time/Date menu insertion format
; Use internal variables: %shortdate%, %longdate%, %yearmonth%, %monthday%,
;                        %longtime%, %shorttime%, %date%, %time%
DateTimeTemplate=%shortdate% %shorttime%

; Custom format strings for %date% and %time% variables
; Can be either:
;   - Internal variable (exclusive): %shortdate%, %longdate%, etc.
;   - Custom format string: yyyy-MM-dd, dd.MM.yyyy, HH:mm, h:mm tt, etc.
;   - Mixed with literals: 'Day 'dd' of 'MMMM', 'yyyy
; Format documentation:
;   https://learn.microsoft.com/en-us/windows/win32/intl/day--month--year--and-era-format-pictures
; See README.md for complete list of format specifiers
DateFormat=%shortdate%        ; Default: system short date
TimeFormat=HH:mm              ; Default: 24-hour without seconds
```

### Format Modes

**Exclusive Internal Variable Mode:**
```ini
DateFormat=%longdate%    ; Must be exact match, uses dwFlags
```

**Custom Format String Mode:**
```ini
DateFormat=yyyy-MM-dd               ; Format specifiers only
DateFormat='Day 'dd' of 'MMMM       ; With literal text (single quotes)
DateFormat=dd.MM.yyyy               ; With punctuation
```

**Rule:** Internal variables must appear alone (exclusive). If not an internal variable, entire string is passed to Windows API for custom formatting.

---

## Implementation Tasks

### Task 1: Add Global Variables
**Location:** After line 331 (after Find history section)

```cpp
//============================================================================
// Date/Time Configuration (Phase 2.10, ToDo #3)
//============================================================================
WCHAR g_szDateTimeTemplate[256] = L"%shortdate% %shorttime%";  // F5/menu template
WCHAR g_szDateFormat[128] = L"%shortdate%";                    // %date% format
WCHAR g_szTimeFormat[128] = L"HH:mm";                          // %time% format
```

---

### Task 2: Add Function Declarations
**Location:** After line 411 (after template declarations)

```cpp
// Date/Time formatting functions (Phase 2.10, ToDo #3)
void FormatDateByFlag(SYSTEMTIME* pst, DWORD dwFlags, WCHAR* pszOutput, size_t cchMax);
void FormatTimeByFlag(SYSTEMTIME* pst, DWORD dwFlags, WCHAR* pszOutput, size_t cchMax);
void FormatDateByString(SYSTEMTIME* pst, LPCWSTR pszFormat, WCHAR* pszOutput, size_t cchMax);
void FormatTimeByString(SYSTEMTIME* pst, LPCWSTR pszFormat, WCHAR* pszOutput, size_t cchMax);
```

---

### Task 3: Implement Helper Functions
**Location:** Before `ExpandTemplateVariables()` (~line 1075)

**Four functions to add:**

1. **FormatDateByFlag** - Uses GetDateFormatEx with dwFlags
2. **FormatTimeByFlag** - Uses GetTimeFormatEx with dwFlags
3. **FormatDateByString** - Uses GetDateFormatEx with custom format string
4. **FormatTimeByString** - Uses GetTimeFormatEx with custom format string

**Key features:**
- Empty format fallback to defaults (%shortdate%, HH:mm)
- Error handling with ISO format fallback
- Support for literal text (passed through to API)

**Total:** ~120 lines

---

### Task 4: Add Internal Variable Handlers
**Location:** In `ExpandTemplateVariables()`, BEFORE existing %date% handler (~line 1171)

**Add 6 handlers:**
- `%shortdate%` - Calls FormatDateByFlag with DATE_SHORTDATE
- `%longdate%` - Calls FormatDateByFlag with DATE_LONGDATE
- `%yearmonth%` - Calls FormatDateByFlag with DATE_YEARMONTH
- `%monthday%` - Calls FormatDateByFlag with DATE_MONTHDAY
- `%longtime%` - Calls FormatTimeByFlag with 0 (includes seconds)
- `%shorttime%` - Calls FormatTimeByFlag with TIME_NOSECONDS

**Total:** ~90 lines

---

### Task 5: Replace Existing %date%, %time%, %datetime% Handlers
**Location:** Lines 1171-1207 in `ExpandTemplateVariables()`

**DELETE:** Existing hardcoded ISO format handlers (36 lines)

**REPLACE with:** Configurable versions that:
1. Check if format setting is an internal variable (exact match)
2. If yes → use FormatDateByFlag/FormatTimeByFlag
3. If no → use FormatDateByString/FormatTimeByString

**Logic for %date%:**
```
if (g_szDateFormat == "%shortdate%") → FormatDateByFlag(DATE_SHORTDATE)
else if (g_szDateFormat == "%longdate%") → FormatDateByFlag(DATE_LONGDATE)
else if (g_szDateFormat == "%yearmonth%") → FormatDateByFlag(DATE_YEARMONTH)
else if (g_szDateFormat == "%monthday%") → FormatDateByFlag(DATE_MONTHDAY)
else → FormatDateByString(g_szDateFormat)  // Custom format
```

**Similar logic for %time%:**
```
if (g_szTimeFormat == "%longtime%") → FormatTimeByFlag(0)
else if (g_szTimeFormat == "%shorttime%") → FormatTimeByFlag(TIME_NOSECONDS)
else → FormatTimeByString(g_szTimeFormat)  // Custom format
```

**Note:** `%datetime%` variable has been **removed** per user request. Users combine manually in templates.

**Total:** ~60 lines (reduced by removing %datetime%)

---

### Task 6: Refactor EditInsertTimeDate()
**Location:** Replace lines 5462-5482

**DELETE:** Old function (20 lines with hardcoded GetDateFormat/GetTimeFormat)

**REPLACE with:**
```cpp
void EditInsertTimeDate()
{
    // Expand configured template (uses internal variables)
    LONG nCursorOffset = -1;
    LPWSTR pszExpanded = ExpandTemplateVariables(g_szDateTimeTemplate, &nCursorOffset);
    
    if (pszExpanded) {
        SendMessage(g_hWndEdit, EM_REPLACESEL, TRUE, (LPARAM)pszExpanded);
        free(pszExpanded);
    }
}
```

**Total:** 8 lines (-12 lines net)

---

### Task 7: Add INI Settings Loading
**Location:** In `LoadSettings()` after line 6555 (after FindUseEscapes)

**Add before `LoadFindHistory();` call:**

Three settings to load:
1. `DateTimeTemplate=` - Default: `%shortdate% %shorttime%`
2. `DateFormat=` - Default: `%shortdate%`
3. `TimeFormat=` - Default: `HH:mm`

All three auto-write defaults if missing.

**Total:** ~30 lines

---

### Task 8: Add INI Documentation
**Location:** In `CreateDefaultINI()` after line 6096 (after "Display settings")

**Add documentation section (keep terse/concise):**
- Brief explanation of DateTimeTemplate (F5 key behavior)
- List available internal variables
- Explain DateFormat/TimeFormat modes (internal vs custom)
- Link to Microsoft documentation
- Reference README.md for detailed guide

**Total:** ~20 lines (kept minimal as requested)

---

### Task 9: Update AGENTS.md
**Location:** After Phase 2.9.1 section

**Add Phase 2.10 section covering:**
- Overview and key features
- Variable types and architecture
- INI configuration examples
- Implementation notes (helper functions, ExpandTemplateVariables changes)
- Global variables
- Key functions with line numbers
- Important Don'ts
- Testing checklist

**Total:** ~100 lines

---

### Task 10: Add README.md Documentation
**Location:** After Autosave section, before Filter System

**Add comprehensive section:**
- Quick start (F5 key usage)
- Configuration overview
- Complete table of internal variables with examples
- Complete table of date/time format specifiers
- Literal text usage rules
- 10+ practical examples (ISO, European, US, verbose, day book)
- Configurable variables explanation
- F5 key configuration
- Troubleshooting Q&A
- Links to Microsoft technical documentation

**Total:** ~300 lines

---

### Task 11: Build and Test
**Steps:**
1. Build with `make clean && make`
2. Test each internal variable individually
3. Test custom format strings (specifiers + literals)
4. Test %date% and %time% in both modes
5. Test F5 key with various templates
6. Test empty format fallbacks
7. Test on English and Czech locales
8. Test error handling (invalid formats)

---

## Code Impact Summary

### Main Changes (src/main.cpp)

| Operation | Lines | Description |
|-----------|-------|-------------|
| **Add** | +3 | Global variables |
| **Add** | +4 | Function declarations |
| **Add** | +120 | Four helper functions |
| **Add** | +90 | Six internal variable handlers |
| **Delete** | -36 | Old hardcoded handlers |
| **Add** | +60 | Two new configurable handlers (%date%, %time% only) |
| **Delete** | -20 | Old EditInsertTimeDate |
| **Add** | +8 | New EditInsertTimeDate |
| **Add** | +30 | INI loading code |
| **Add** | +20 | INI documentation (terse) |
| **Net** | **+279** | Total main.cpp changes |

**Current:** 8,050 lines  
**After:** ~8,329 lines

### Documentation Changes

| File | Lines | Description |
|------|-------|-------------|
| AGENTS.md | +100 | Phase 2.10 documentation |
| README.md | +300 | Comprehensive formatting guide |
| **Total** | **+400** | Documentation |

### Binary Size
- **Current:** 314 KB (stripped)
- **Expected:** 318-320 KB (stripped)
- **Increase:** +4-6 KB

---

## Key Design Decisions (Confirmed)

✅ **Default DateTimeTemplate:** `%shortdate% %shorttime%`  
✅ **Default DateFormat:** `%shortdate%` (locale-aware)  
✅ **Default TimeFormat:** `HH:mm` (24-hour without seconds)  
✅ **Empty format handling:** Fall back to defaults  
✅ **Internal variable mode:** Exclusive (exact match only)  
✅ **Literal text support:** Via single quotes (Windows API native)  
❌ **%datetime% variable:** Removed per user request (users combine manually)  
✅ **Error handling:** Silent fallback to ISO format  
✅ **Variable naming:** Lowercase (%shortdate%, not %SHORTDATE%)

---

## Windows API Usage

### GetDateFormatEx
```cpp
GetDateFormatEx(
    LOCALE_NAME_USER_DEFAULT,
    dwFlags,              // DATE_SHORTDATE, DATE_LONGDATE, etc. OR 0 for custom
    &st,                  // SYSTEMTIME
    pszFormat,            // NULL for dwFlags, custom string otherwise
    pszOutput,
    cchMax,
    NULL
);
```

**Flags supported:**
- `DATE_SHORTDATE` - Short date (e.g., "1/20/2026")
- `DATE_LONGDATE` - Long date (e.g., "Monday, January 20, 2026")
- `DATE_YEARMONTH` - Year and month (e.g., "January 2026")
- `DATE_MONTHDAY` - Month and day (e.g., "January 20")

### GetTimeFormatEx
```cpp
GetTimeFormatEx(
    LOCALE_NAME_USER_DEFAULT,
    dwFlags,              // 0 (with seconds), TIME_NOSECONDS
    &st,                  // SYSTEMTIME
    pszFormat,            // NULL for dwFlags, custom string otherwise
    pszOutput,
    cchMax
);
```

**Flags supported:**
- `0` - With seconds (e.g., "10:30:45 PM")
- `TIME_NOSECONDS` - Without seconds (e.g., "10:30 PM")

### Custom Format Specifiers

**Date:** d, dd, ddd, dddd, M, MM, MMM, MMMM, y, yy, yyyy, gg  
**Time:** h, hh, H, HH, m, mm, s, ss, t, tt  
**Literals:** Enclosed in single quotes: `'text'`

**Examples:**
- `yyyy-MM-dd` → 2026-01-20
- `'Day 'dd' of 'MMMM` → Day 20 of January
- `HH:mm:ss` → 22:30:45
- `'at 'h:mm' 'tt` → at 10:30 PM

---

## Testing Scenarios

### Basic Functionality
- [ ] F5 inserts date/time with default template
- [ ] Each of 6 internal variables works individually
- [ ] %date%, %time% work with default settings

### Internal Variable Mode
- [ ] DateFormat=%shortdate% → uses DATE_SHORTDATE flag
- [ ] DateFormat=%longdate% → uses DATE_LONGDATE flag
- [ ] DateFormat=%yearmonth% → uses DATE_YEARMONTH flag
- [ ] DateFormat=%monthday% → uses DATE_MONTHDAY flag
- [ ] TimeFormat=%longtime% → includes seconds
- [ ] TimeFormat=%shorttime% → excludes seconds

### Custom Format Mode
- [ ] DateFormat=yyyy-MM-dd → ISO format
- [ ] DateFormat=dd.MM.yyyy → European format
- [ ] DateFormat=M/d/yyyy → US format
- [ ] TimeFormat=HH:mm → 24-hour without seconds
- [ ] TimeFormat=h:mm tt → 12-hour with AM/PM
- [ ] TimeFormat=HH:mm:ss → 24-hour with seconds

### Literal Text
- [ ] DateFormat='Day 'dd → "Day 20"
- [ ] TimeFormat='at 'HH:mm → "at 22:30"
- [ ] Mixed format with literals works correctly

### Edge Cases
- [ ] Empty DateFormat= → falls back to %shortdate%
- [ ] Empty TimeFormat= → falls back to HH:mm
- [ ] Invalid format string → falls back to ISO
- [ ] Very long format strings handled
- [ ] Unicode in literal text works

### Locale Testing
- [ ] Test on English locale (all internal variables)
- [ ] Test on Czech locale (all internal variables)
- [ ] Verify locale-awareness of %shortdate%, %longdate%, etc.

### F5 Key Configurations
- [ ] DateTimeTemplate=%shortdate% %shorttime% (default)
- [ ] DateTimeTemplate=%date% %time% (uses custom)
- [ ] DateTimeTemplate=%longdate%, %longtime% (verbose)
- [ ] DateTimeTemplate with multiple variables

---

## Error Handling

### Fallback Chain

**FormatDateByString:**
1. If format empty → Fall back to FormatDateByFlag(DATE_SHORTDATE)
2. If GetDateFormatEx fails → Fall back to ISO: `yyyy-MM-dd`

**FormatTimeByString:**
1. If format empty → Fall back to `HH:mm` via GetTimeFormatEx
2. If GetTimeFormatEx fails → Fall back to ISO: `HH:mm`

**ExpandTemplateVariables:**
1. Check for internal variable (exact match)
2. If not found → treat as custom format string
3. Windows API handles all specifiers + literals

**No user-facing error messages** - silent fallback ensures usability.

---

## Documentation URLs

**Referenced in implementation:**
- [GetDateFormatEx API](https://learn.microsoft.com/en-us/windows/win32/api/datetimeapi/nf-datetimeapi-getdateformatex)
- [GetTimeFormatEx API](https://learn.microsoft.com/en-us/windows/win32/api/datetimeapi/nf-datetimeapi-gettimeformatex)
- [Date Format Pictures](https://learn.microsoft.com/en-us/windows/win32/intl/day--month--year--and-era-format-pictures)
- [Time Format Pictures](https://learn.microsoft.com/en-us/windows/win32/intl/time-format-pictures)

---

## Implementation Order

**Recommended sequence:**

1. ✅ Add global variables (simple)
2. ✅ Add function declarations (simple)
3. ✅ Implement helper functions (core functionality)
4. ✅ Add internal variable handlers (straightforward)
5. ✅ Replace %date%, %time%, %datetime% handlers (critical)
6. ✅ Refactor EditInsertTimeDate (simple)
7. ✅ Add INI loading (simple)
8. ✅ Add INI documentation (simple)
9. ✅ Build and basic test
10. ✅ Update AGENTS.md (documentation)
11. ✅ Add README.md section (documentation)
12. ✅ Comprehensive testing

**Estimated time:** 2-3 hours for implementation + testing

---

## Success Criteria

Implementation is complete when:

✅ All 11 tasks completed  
✅ Code builds without warnings  
✅ Binary size within expected range  
✅ All internal variables work correctly  
✅ Custom formats work with specifiers + literals  
✅ F5 key uses configurable template  
✅ Empty formats fall back correctly  
✅ Invalid formats handled gracefully  
✅ Documentation complete (AGENTS.md + README.md)  
✅ INI file auto-generates with new settings  
✅ Backward compatibility maintained (existing templates work)

---

## Risk Assessment

### Low Risk
- ✅ Helper functions use standard Windows APIs
- ✅ Internal variables straightforward (dwFlags mapping)
- ✅ Template system already exists (reuse ExpandTemplateVariables)
- ✅ F5 refactor is simple (8 lines)

### Medium Risk
- ⚠️ Custom format string parsing (Windows API handles it)
- ⚠️ Literal text support (Windows API handles it)
- ⚠️ Error handling must be silent (no user interruption)

### Mitigation
- Extensive testing with various format strings
- Fallback chain ensures robustness
- Test on multiple locales
- Invalid formats → silent fallback (no crashes)

---

## Post-Implementation

### Commit Message
```
Add configurable date/time formatting system (Phase 2.10, ToDo #3)

- Add 6 internal variables: %shortdate%, %longdate%, %yearmonth%, %monthday%, %longtime%, %shorttime%
- Make %date%, %time% fully configurable via INI settings
- Remove %datetime% variable (users combine manually in templates)
- Support custom format strings with literal text (single quotes)
- Add DateTimeTemplate for F5 key configuration
- Use Windows GetDateFormatEx/GetTimeFormatEx APIs (locale-aware)
- Add 4 helper functions for date/time formatting
- Refactor EditInsertTimeDate to use template system
- Add comprehensive documentation in AGENTS.md and README.md
- INI settings: DateTimeTemplate, DateFormat, TimeFormat
- Default: %shortdate% %shorttime% (locale-aware)
- Binary size: 314 KB → 318 KB (+4 KB)
```

### Next Steps
- Monitor user feedback on format options
- Consider adding more internal variables if requested
- Potential future: Format preview in settings dialog

---

## Approval Checklist

Before implementation begins, confirm:

- [x] Design approved (three variable types)
- [x] INI settings confirmed (names and defaults)
- [x] Format behavior confirmed (exclusive internal variables)
- [x] Documentation scope confirmed (comprehensive README)
- [x] Error handling strategy confirmed (silent fallback)
- [x] Testing plan reviewed
- [ ] **FINAL APPROVAL TO BEGIN IMPLEMENTATION**

---

**Status:** ⏳ Awaiting final approval  
**Next Action:** Say "begin implementing" or "implement" to start execution

**Prepared by:** OpenCode AI Agent  
**Date:** January 21, 2026  
**Version:** Final v1.0
