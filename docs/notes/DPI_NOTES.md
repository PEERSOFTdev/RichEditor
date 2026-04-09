# DPI Awareness Implementation Notes

## Overview

RichEditor is Per-Monitor V2 DPI-aware (Win10 1703+) with fallback to Per-Monitor V1
(Win8.1+) and system-DPI-aware (Win7/Vista). The implementation is manifest-driven with
minimal code: ~50 lines of new logic in `src/main.cpp`.

## Architecture

### Manifest (`src/RichEditor.manifest`)

Embedded as resource `1 24` in `resource.rc`. The manifest serves triple duty:

1. **DPI awareness** — `<dpiAwareness>PerMonitorV2,PerMonitor</dpiAwareness>` for Win10
   1703+, with `<dpiAware>true/pm</dpiAware>` for legacy fallback.
2. **Visual styles** — Common Controls v6 dependency enables themed controls.
3. **UTF-8 code page** — `<activeCodePage>UTF-8</activeCodePage>` for Win10 1903+.

### Dynamic API loading

`GetDpiForWindow` and `EnableNonClientDpiScaling` are loaded from `user32.dll` via
`GetProcAddress` in `InitDpiApis()` (called before any window is created). This avoids
import failures on older Windows. The `(void*)` intermediate cast suppresses MinGW's
`-Wcast-function-type` warning.

### DPI state

`g_nDpi` (type `UINT`) holds the current effective DPI. Initialized to
`USER_DEFAULT_SCREEN_DPI` (96), set properly in `WM_CREATE` via `GetDpiForHwnd()`,
and updated in `WM_DPICHANGED`.

### Message handlers

| Message | Purpose |
|---------|---------|
| `WM_NCCREATE` | Calls `EnableNonClientDpiScaling()` for V1 fallback |
| `WM_CREATE` | Sets `g_nDpi = GetDpiForHwnd(hwnd)` |
| `WM_DPICHANGED` | Updates `g_nDpi`, resets pane line cache, applies suggested rect |
| `WM_GETMINMAXINFO` | Enforces `ScaleDpi(640)` x `ScaleDpi(480)` minimum |

## What does NOT need DPI code

- **RichEdit control** — fills WM_SIZE-provided area; font/rendering is DPI-aware once
  the manifest is present.
- **Dialogs** — DIALOGEX + "MS Shell Dlg" + DLU-based coordinates; the Per-Monitor V2
  dialog manager handles scaling.
- **Status bar** — auto-sizes on WM_SIZE (common control behavior).
- **GetTwipsForPixels()** — already uses `GetDeviceCaps(LOGPIXELSX)`; returns the real
  DPI once the process is DPI-aware.

## Verifying DPI awareness at runtime (PowerShell)

The following PowerShell snippet queries the running RichEditor process to confirm that
Per-Monitor V2 DPI awareness is active. This was used for initial testing by a screen
reader user who could not visually verify rendering sharpness but needed to confirm the
manifest and DPI infrastructure were functioning correctly.

```powershell
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class DpiCheck {
    [DllImport("user32.dll")]
    public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern int GetAwarenessFromDpiAwarenessContext(IntPtr value);

    [DllImport("user32.dll")]
    public static extern uint GetDpiForWindow(IntPtr hwnd);
}
"@

$proc = Get-Process RichEditor -ErrorAction Stop
$hwnd = $proc.MainWindowHandle
$ctx = [DpiCheck]::GetWindowDpiAwarenessContext($hwnd)
$awareness = [DpiCheck]::GetAwarenessFromDpiAwarenessContext($ctx)
$dpi = [DpiCheck]::GetDpiForWindow($hwnd)

# Awareness: 0=Unaware, 1=System, 2=PerMonitor
Write-Host "Awareness: $awareness (0=Unaware, 1=System, 2=PerMonitor)"
Write-Host "DPI: $dpi (96=100%, 120=125%, 144=150%, 192=200%)"
```

Expected output for a correctly configured build at 125% scaling:

```
Awareness: 2 (0=Unaware, 1=System, 2=PerMonitor)
DPI: 120 (96=100%, 120=125%, 144=150%, 192=200%)
```

## Binary size impact

359 936 → 362 496 bytes stripped (+2 560 bytes / +0.7%).
Mostly the manifest XML embedded in the resource section.
