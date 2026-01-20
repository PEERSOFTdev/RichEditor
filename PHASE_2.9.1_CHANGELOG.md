# Phase 2.9.1 Changelog

## User Feedback & Improvements

### UX Enhancement: Default Button in Find Dialog

**Issue:** When pressing Enter in the "Find what" field, nothing happened. Users expected Enter to trigger a search (forward direction by default).

**Solution:** Changed "Find Next" button from `PUSHBUTTON` to `DEFPUSHBUTTON` in both English and Czech dialogs.

**Effect:**
- ✅ Pressing Enter in the "Find what" field now searches forward (Find Next)
- ✅ Button has visual indication (bold border) showing it's the default action
- ✅ Standard Windows dialog behavior (matches Notepad, VS Code, etc.)
- ✅ Shift+Enter still triggers "Find Previous" button

**Technical Change:**
```rc
// Before:
PUSHBUTTON      "Find &Next →", IDC_FIND_NEXT_BTN, 197, 7, 56, 14

// After:
DEFPUSHBUTTON   "Find &Next →", IDC_FIND_NEXT_BTN, 197, 7, 56, 14
```

**Files Modified:**
- `src/resource.rc` (line 110 - English dialog)
- `src/resource.rc` (line 388 - Czech dialog)

**Build Result:** Clean rebuild, same binary size (321,536 bytes)

---

### UX Enhancement #2: Cursor Positioning with SelectAfterFind=0

**Issue:** When `SelectAfterFind=0`, the cursor was positioned at the **start** of the matched string. Pressing F3 to find next would find the same match again (infinite loop), because the search started from the cursor position which was still at the beginning of the current match.

**Solution:** Position cursor **after** the matched string (at `cpMax` instead of `cpMin`) when `SelectAfterFind=0`.

**Effect:**
- ✅ F3 now correctly finds the next occurrence (moves past current match)
- ✅ SelectAfterFind=0 is now usable for navigating through matches
- ✅ Cursor positioned logically after the match (ready for next search)

**Technical Change:**
```cpp
// Before:
if (g_bSelectAfterFind) {
    // Select the found text
    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
} else {
    // Just move cursor to start of match
    cr.cpMax = cr.cpMin;
    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
}

// After:
if (g_bSelectAfterFind) {
    // Select the found text
    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
} else {
    // Move cursor to end of match (after the matched string)
    // This allows F3 to find the next occurrence correctly
    cr.cpMin = cr.cpMax;
    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
}
```

**Files Modified:**
- `src/main.cpp` (line 1735-1742 - FindTextInDocument function)

**Build Result:** Clean rebuild, same binary size (321,536 bytes)

---

### Polish #3: Better Checkbox Label

**Issue:** "Use escapes" was too abbreviated - not immediately clear what "escapes" means.

**Solution:** Changed to full phrase "Use escape sequences" for better clarity.

**Effect:**
- ✅ More descriptive and professional label
- ✅ Clearer to users unfamiliar with the term "escapes"
- ✅ English: "Use &escape sequences"
- ✅ Czech: "Použít &escape sekvence"

**Technical Change:**
```rc
// Before:
"Use &escapes (\\n, \\t, \\xNN, \\uNNNN)"
"Použít &escape (\\n, \\t, \\xNN, \\uNNNN)"

// After:
"Use &escape sequences (\\n, \\t, \\xNN, \\uNNNN)"
"Použít &escape sekvence (\\n, \\t, \\xNN, \\uNNNN)"
```

**Files Modified:**
- `src/resource.rc` (lines 115, 278 - English)
- `src/resource.rc` (lines 393, 509 - Czech)

**Build Result:** Clean rebuild, same binary size (321,536 bytes)

---

## Complete Feature List

### Find Dialog Features
- ✅ Modeless dialog (non-blocking)
- ✅ Ctrl+F to open, F3 for next, Shift+F3 for previous
- ✅ **Enter key searches forward (default button)** ⭐ Enhancement #1
- ✅ **Cursor positioned after match when SelectAfterFind=0** ⭐ Enhancement #2
- ✅ **"Use escape sequences" (clearer label)** ⭐ Polish #3
- ✅ Escape closes dialog
- ✅ 20-item MRU search history
- ✅ Match case, Whole word, Use escape sequences options
- ✅ Escape sequences: \n, \t, \xNN, \uNNNN
- ✅ Checkbox state persistence
- ✅ Full Czech localization

### Keyboard Shortcuts Summary
| Shortcut | Action |
|----------|--------|
| **Ctrl+F** | Open Find dialog |
| **F3** | Find next |
| **Shift+F3** | Find previous |
| **Enter** | Find next (when dialog focused) ⭐ |
| **Shift+Enter** | Find previous (when dialog focused) |
| **Escape** | Close dialog |
| **Alt+N** | Focus "Find Next" button |
| **Alt+P** | Focus "Find Previous" button |
| **Alt+C** | Toggle "Match case" |
| **Alt+W** | Toggle "Whole word" |
| **Alt+E** | Toggle "Use escape sequences" |

---

**Date:** January 20, 2026  
**Feedback By:** User  
**Status:** ✅ Implemented and tested (build successful)
