# Phase 2.11 Plan - Word Wrap Ruler (Twips and Column-Based)

Status: Draft
Goal: Support word wrap by fixed twips width and by character count (ruler-style wrap).

## Summary

RichEditor already supports live word wrap using EM_SETTARGETDEVICE. This phase adds a column-based wrap mode so users can wrap at a fixed column (monospace and proportional fonts), while keeping a twips-based API for future UI controls (ruler).

## Proposed API

- Set wrap width in twips (0 = no wrap):
  - `SetRichEditWordWrap(HWND hEdit, LONG widthTwips)`

- Set wrap width by character count (0 = no wrap):
  - `SetRichEditWrapWidthChars(HWND hEdit, int charCount)`

## Column-Based Wrap (Draft)

Key idea: compute the pixel width of N characters based on the current font, then convert to twips and call EM_SETTARGETDEVICE.

Notes:
- Use the control font (WM_GETFONT).
- Use wide APIs (GetTextExtentPoint32W) to keep Unicode safe.
- TMPF_FIXED_PITCH is inverted: 0 means monospaced.

### Draft Helper (Unicode-safe)

```cpp
// Helper: Check if font is monospaced
static BOOL IsMonospacedFont(HDC hdc)
{
    TEXTMETRIC tm;
    if (!GetTextMetrics(hdc, &tm))
        return TRUE; // Assume monospaced if we can't tell
    return (tm.tmPitchAndFamily & TMPF_FIXED_PITCH) == 0;
    // Note: TMPF_FIXED_PITCH flag is inverted: 0 = monospaced
}

// Set wrap width in characters, accurate for mono and proportional fonts
void SetRichEditWrapWidthChars(HWND hEdit, int charCount)
{
    if (!hEdit) return;

    if (charCount <= 0) {
        // Disable wrapping
        SetRichEditWordWrap(hEdit, 0);
        return;
    }

    HDC hdc = GetDC(hEdit);
    if (!hdc) return;

    // Select the control's current font into the DC
    HFONT hFont = (HFONT)SendMessage(hEdit, WM_GETFONT, 0, 0);
    HFONT hOldFont = NULL;
    if (hFont) {
        hOldFont = (HFONT)SelectObject(hdc, hFont);
    }

    int totalWidthPx = 0;

    if (IsMonospacedFont(hdc)) {
        // Fast path: monospaced font
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        totalWidthPx = tm.tmAveCharWidth * charCount;
    } else {
        // Proportional font: measure actual string width
        // Use 'M' as it is usually widest, repeated charCount times
        WCHAR* testStr = (WCHAR*)malloc((charCount + 1) * sizeof(WCHAR));
        if (testStr) {
            for (int i = 0; i < charCount; i++) {
                testStr[i] = L'M';
            }
            testStr[charCount] = L'\0';

            SIZE sz;
            if (GetTextExtentPoint32W(hdc, testStr, charCount, &sz)) {
                totalWidthPx = sz.cx;
            } else {
                // Fallback to average char width
                TEXTMETRIC tm;
                GetTextMetrics(hdc, &tm);
                totalWidthPx = tm.tmAveCharWidth * charCount;
            }
            free(testStr);
        }
    }

    // Convert pixels to twips
    int widthTwips = MulDiv(totalWidthPx, 1440, GetDeviceCaps(hdc, LOGPIXELSX));

    // Restore old font
    if (hOldFont) {
        SelectObject(hdc, hOldFont);
    }
    ReleaseDC(hEdit, hdc);

    // Apply wrap width
    SetRichEditWordWrap(hEdit, widthTwips);
}
```

## Open Questions

- Settings: new INI keys (WrapMode=Window|Column|Twips, WrapColumn=NN)?
- UI: menu toggle or optional ruler control?
- Behavior on font change: should wrap width be recalculated automatically?

## Notes

- EM_SETTARGETDEVICE uses twips for the line width. Use 0 to disable wrapping.
- Keep Unicode intact (W APIs only). RichEdit 1.0 (ANSI) is not acceptable for this feature.
