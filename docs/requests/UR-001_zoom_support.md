# UR-001 — Zoom Support

**Status:** Planned  
**Requested by:** User (visually impaired friend report)  
**Date:** 2026-03-07

---

## Background

A visually impaired user reported that Ctrl+mouse wheel zoom (built into RichEdit, not implemented
by the app) breaks Word Wrap: after zooming in, wrapped lines overflow the visible window width and
the horizontal scrollbar is absent in Word Wrap mode, making the overflow inaccessible.

Additionally, zoom level is not indicated anywhere, not persisted across sessions, and has no
reset shortcut or menu item.

---

## Root Cause

`ApplyWordWrap` is called once (startup, resize, wrap toggle) but never after a zoom change.

- **Older RichEdit:** wrap target is a fixed twip width calculated at 100% zoom. At 200% zoom those
  twips occupy twice the screen pixels — lines overflow the window.
- **RichEdit 8+:** `EM_SETTARGETDEVICE` with `widthTwips=0` ("wrap to window") is evaluated at
  send time only; zoom changes do not trigger re-evaluation automatically.

Narrator works correctly (MSAA structure is fine); the bug is purely in the wrap width not
tracking zoom.

---

## Scope

Five self-contained sub-features, all in `src/main.cpp` except the menu additions.

---

## Changes

### 1 — Core fix: re-apply Word Wrap after Ctrl+wheel zoom

**`EditSubclassProc` — add `WM_MOUSEWHEEL` case (before final `CallWindowProc`):**

```cpp
if (msg == WM_MOUSEWHEEL) {
    if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) {
        LRESULT r = CallWindowProc(g_pfnOriginalEditProc, hwnd, msg, wParam, lParam);
        if (g_bWordWrap) ApplyWordWrap(hwnd);
        UpdateStatusBar();
        return r;
    }
}
```

**`ApplyWordWrap` older-RE branch — divide twip width by zoom factor:**

```cpp
LONG widthTwips = GetTwipsForPixels(hEdit, rcClient.right - rcClient.left);
UINT zoomNum = 0, zoomDen = 0;
if (SendMessage(hEdit, EM_GETZOOM, (WPARAM)&zoomNum, (LPARAM)&zoomDen)
    && zoomNum > 0 && zoomDen > 0)
    widthTwips = MulDiv(widthTwips, (int)zoomDen, (int)zoomNum);
SetRichEditWordWrap(hEdit, widthTwips);
```

RichEdit 8+ path (`widthTwips=0`): re-sending `EM_SETTARGETDEVICE` after zoom is sufficient
to trigger re-layout; no twip math needed.

---

### 2 — Zoom indicator in status bar

**`UpdateStatusBar` — append zoom % when not at 100%:**

```cpp
UINT zNum = 0, zDen = 0;
WCHAR szZoom[16] = L"";
if (SendMessage(g_hWndEdit, EM_GETZOOM, (WPARAM)&zNum, (LPARAM)&zDen)
    && zNum > 0 && zDen > 0)
    _snwprintf(szZoom, 16, L"    %d%%", MulDiv((int)zNum, 100, (int)zDen));
// append szZoom to both szStatus format strings
```

Result: `Ln 1, Col 1    U+0041    200%`. Absent when at 100%.

---

### 3 — View → Reset Zoom (Ctrl+0)

**`src/resource.h`:** `#define ID_VIEW_ZOOM_RESET 1111`

**`src/resource.rc`** — both language blocks:
- EN: `MENUITEM "Reset &Zoom\tCtrl+0", ID_VIEW_ZOOM_RESET`
- CS: `MENUITEM "Obnovit &přiblížení\tCtrl+0", ID_VIEW_ZOOM_RESET`

**`BuildAcceleratorTable`:** `BUILTIN_COUNT` 23 → 24; add:
```cpp
pAccel[idx].fVirt = FCONTROL | FVIRTKEY; pAccel[idx].key = '0'; pAccel[idx++].cmd = ID_VIEW_ZOOM_RESET;
```

**`WM_COMMAND`:** `case ID_VIEW_ZOOM_RESET: ViewZoomReset(); break;`

**`WM_INITMENUPOPUP` View block:** gray item when already at 100%:
```cpp
UINT zNum = 0, zDen = 0;
BOOL bZoomed = (BOOL)SendMessage(g_hWndEdit, EM_GETZOOM, (WPARAM)&zNum, (LPARAM)&zDen)
               && zNum > 0 && zDen > 0;
EnableMenuItem(hMenu, ID_VIEW_ZOOM_RESET, bZoomed ? MF_ENABLED : MF_GRAYED);
```

**New `ViewZoomReset()`:**
```cpp
void ViewZoomReset()
{
    SendMessage(g_hWndEdit, EM_SETZOOM, 0, 0);
    if (g_bWordWrap) ApplyWordWrap(g_hWndEdit);
    UpdateStatusBar();
}
```

**`EditSubclassProc` `WM_CHAR` — add NUL guard** (Ctrl+digit generates NUL on some layouts;
accelerator already handles the command so no `WM_KEYDOWN` intercept needed here):
```cpp
if (wParam == 0 && (GetKeyState(VK_CONTROL) & 0x8000))
    return 0;
```

---

### 4 — INI-persisted zoom level

**Global:** `int g_nZoomPercent = 100;`

**`LoadSettings`:** read `Zoom=` as integer percentage, default 100.

**`CreateRichEditControl`** — apply before `ApplyWordWrap` so first layout is zoom-aware:
```cpp
if (g_nZoomPercent != 100)
    SendMessage(hwndEdit, EM_SETZOOM, (WPARAM)g_nZoomPercent, 100);
ApplyWordWrap(hwndEdit);   // existing line — now zoom-aware from first layout
```

**`WM_DESTROY`** — save before `FlushIniCache()`:
```cpp
UINT zNum = 0, zDen = 0;
int pct = 100;
if (SendMessage(g_hWndEdit, EM_GETZOOM, (WPARAM)&zNum, (LPARAM)&zDen)
    && zNum > 0 && zDen > 0)
    pct = MulDiv((int)zNum, 100, (int)zDen);
// WriteINIValue Zoom = pct
```

**`CreateDefaultINI`:** `Zoom=100  ; Zoom percentage (100 = default)`

---

## File summary

| File | Lines (net) |
|------|-------------|
| `src/resource.h` | +1 |
| `src/resource.rc` | +2 |
| `src/main.cpp` | ~60 |

---

## Testing notes

- Ctrl+wheel zoom in / out with Word Wrap ON → lines must stay within window at all zoom levels.
- Ctrl+wheel with Word Wrap OFF → scroll behavior unchanged (no `ApplyWordWrap` called).
- View → Reset Zoom grayed at 100%; active when zoomed; Ctrl+0 equivalent.
- Status bar shows e.g. `200%` when zoomed; blank when at 100%.
- Exit at 150% → reopen → zoom restored to 150%, wrap correct on first paint.
- RichEdit 8+ and older RE paths both exercised if possible.
