# UR-003 — HiDPI Support

**Status:** Done
**Requested by:** GitHub issue #1
**Date:** 2026-04-07

---

## Background

RichEditor had no DPI awareness declaration. On high-DPI displays, Windows applied bitmap
scaling, making the entire application look blurry. The GitHub issue included a screenshot
showing fuzzy text on a HiDPI monitor.

---

## Scope

Add Per-Monitor V2 DPI awareness so the app renders sharply on any display scale factor,
including multi-monitor setups with mixed DPIs.

---

## Changes

### Application manifest (`src/RichEditor.manifest`)

New file, embedded as resource `1 24` in `src/resource.rc`. Declares:

- `PerMonitorV2` DPI awareness (Win10 1703+), fallback `PerMonitor` (Win8.1+), legacy
  `true/pm` (Win7/Vista system-DPI-aware).
- Common Controls v6 dependency (enables modern visual styles).
- UTF-8 active code page (Win10 1903+).
- `<supportedOS>` entries for Vista through Windows 10+.

### DPI infrastructure (`src/main.cpp`)

- `g_nDpi` global tracks the current effective DPI (initialized in `WM_CREATE`).
- `ScaleDpi(value, dpi)` scales 96-DPI design values to the current DPI.
- `GetDpiForHwnd(hwnd)` dynamically loads `GetDpiForWindow` (Win10 1607+); falls back
  to `GetDeviceCaps(LOGPIXELSX)`.
- `InitDpiApis()` loads function pointers at startup via `GetProcAddress`.

### WndProc handlers

- `WM_NCCREATE`: calls `EnableNonClientDpiScaling` for Per-Monitor V1 fallback.
- `WM_DPICHANGED`: updates `g_nDpi`, resets output pane line height cache, applies the
  suggested window rect. Triggers `WM_SIZE` for full re-layout.
- `WM_GETMINMAXINFO`: enforces DPI-scaled 640x480 minimum window size.

### Scaled pixel constants

| Constant | Design value (96 DPI) | Locations |
|----------|-----------------------|-----------|
| Minimum window size | 640 x 480 | wWinMain, WM_GETMINMAXINFO |
| Status bar right part | 200 | WM_SIZE, CreateStatusBar |
| Output pane padding | 15 | WM_SIZE |
| Output pane min/max margin | 20 | WM_SIZE |

---

## File Summary

| File | Change |
|------|--------|
| `src/RichEditor.manifest` | New — DPI + visual styles + UTF-8 manifest |
| `src/resource.rc` | Embed manifest as resource `1 24` |
| `src/main.cpp` | DPI globals, helpers, WM_NCCREATE/WM_DPICHANGED/WM_GETMINMAXINFO, scale constants |
| `AGENTS.md` | DPI awareness section for future agents |

---

## Testing Notes

- Verify on a HiDPI display (150%/200%) that text, menus, status bar, and dialogs are sharp.
- Drag the window between monitors with different DPIs to confirm smooth re-scaling.
- Verify the output pane and status bar partition scale proportionally.
- Verify the app runs without error on Windows 7 (graceful system-DPI-aware fallback).
- Binary size delta: 359 936 → 362 496 bytes (+2 560 bytes stripped).
- PowerShell DPI verification (see `docs/notes/DPI_NOTES.md` for the full script) confirmed
  `Awareness: 2` (PerMonitor) and `DPI: 120` (125% scaling) on the developer's machine.
- GitHub issue #1 closed as completed by the issue author after testing the built artifact.
