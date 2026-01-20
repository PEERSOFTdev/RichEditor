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

**Issue:** When `SelectAfterFind=0`, the cursor was positioned at the **start** of the matched string for both forward and backward searches. This caused:
- **F3 (forward)**: Would find the same match again (infinite loop)
- **Shift+F3 (backward)**: Would also find the same match again

**Solution:** Position cursor **directionally** based on search direction:
- **Forward search (F3)**: Position cursor **after** match (cpMax)
- **Backward search (Shift+F3)**: Position cursor **before** match (cpMin)

**Effect:**
- ✅ F3 correctly finds the next occurrence (moves forward past current match)
- ✅ Shift+F3 correctly finds the previous occurrence (moves backward past current match)
- ✅ SelectAfterFind=0 now usable for bidirectional navigation through matches
- ✅ Matches PSPad's smart behavior (discovered independently)

**Technical Change:**
```cpp
// Before (broken for both directions):
if (g_bSelectAfterFind) {
    // Select the found text
    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
} else {
    // Just move cursor to start of match (broken!)
    cr.cpMax = cr.cpMin;
    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
}

// After (smart bidirectional positioning):
if (g_bSelectAfterFind) {
    // Select the found text
    SendMessage(g_hWndEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
} else {
    // Position cursor based on search direction
    if (bSearchDown) {
        // Forward search: position cursor after match (allows F3 to continue forward)
        cr.cpMin = cr.cpMax;
    } else {
        // Backward search: position cursor before match (allows Shift+F3 to continue backward)
        cr.cpMax = cr.cpMin;
    }
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
