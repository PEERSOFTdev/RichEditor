# UR-002 — Selection Stats in Status Bar

**Status:** Done  
**Requested by:** User  
**Date:** 2026-03-11

---

## Background

When text is selected, the status bar shows the same `Ln N, Col N` cursor position as
when nothing is selected. The user asked for selection-specific feedback: how many visual
lines are spanned by the selection, how many characters are selected, and the total
document character count — matching the behaviour of editors such as VS Code and
Notepad++.

---

## Scope

Single function change in `src/main.cpp` (`UpdateStatusBar`) plus four new string
resources (EN + CS).

---

## Behaviour

- **No selection:** status bar is unchanged — `Ln N, Col N    Char: ...    [zoom%]`
- **Selection active:** the `Ln/Col` portion is replaced by:

  ```
  Sel: N ln, N ch    Total: N    Char: 'X' (Dec: N, U+XXXX)    [zoom%]
  ```

  | Field | Value |
  |-------|-------|
  | `N ln` | Number of visual lines spanned by the selection |
  | `N ch` | `cpMax − cpMin` (UTF-16 code units) |
  | `Total: N` | Cached document length (`g_nLastTextLen`, O(1)) |

- **Edge case:** if `cpMax` falls exactly at the start of a visual line (e.g. after
  Shift+End to line end), that trailing line is **not** counted — consistent with
  VS Code / Notepad++ (Shift+Down once from col 1 = 1 line, not 2).

- **Czech locale:** labels become `Výb: N ř, N zn    Celkem: N`.

---

## Line count algorithm

### Word wrap OFF (or RichEdit 8+ physical mode)

Uses the pre-built `g_lineStarts` physical line index (two `upper_bound` binary searches,
O(log N)):

```cpp
auto it1 = std::upper_bound(g_lineStarts.begin(), g_lineStarts.end(), (LONG)cr.cpMin);
if (it1 != g_lineStarts.begin()) --it1;
int lineOfStart = (int)(it1 - g_lineStarts.begin());

auto it2 = std::upper_bound(g_lineStarts.begin(), g_lineStarts.end(), (LONG)cr.cpMax);
if (it2 != g_lineStarts.begin()) --it2;
int lineOfEnd = (int)(it2 - g_lineStarts.begin());

if (lineOfEnd > lineOfStart && (LONG)cr.cpMax == g_lineStarts[lineOfEnd])
    lineOfEnd--;
selLines = lineOfEnd - lineOfStart + 1;
```

### Word wrap ON — TOM path (primary)

Uses the same `ITextRange::GetIndex(tomLine)` pattern already established in the
cursor-position block above:

```cpp
ITextRange *pR1, *pR2;
g_pTextDoc->Range(cr.cpMin, cr.cpMin, &pR1);  pR1->GetIndex(tomLine, &lineAtStart);
g_pTextDoc->Range(cr.cpMax, cr.cpMax, &pR2);  pR2->GetIndex(tomLine, &lineAtEnd);
// edge case: collapse pR2 to visual line start; if it equals cpMax, subtract 1
pR2->StartOf(tomLine, 0, &delta);
pR2->GetStart(&visLineStart);
if (visLineStart == (LONG)cr.cpMax && lineAtEnd > lineAtStart) lineAtEnd--;
selLines = (int)(lineAtEnd - lineAtStart) + 1;
```

### Word wrap ON — message-based fallback (TOM unavailable)

`EM_EXLINEFROMCHAR` × 2 + `EM_LINEINDEX` for the edge-case check.

---

## Changes

| File | Change |
|------|--------|
| `src/resource.h` | +4 string ID constants (2170–2173) |
| `src/resource.rc` | +4 entries in EN STRINGTABLE, +4 entries in CS STRINGTABLE |
| `src/main.cpp` | `UpdateStatusBar()` — replaced 16-line `posInfo` block with 80-line selection-aware block |

Binary size delta: `+~2 KB` (string resources + code).

---

## Testing notes

- No selection: verify `Ln/Col` display is unchanged.
- Select a few characters on one line: `Sel: 1 ln, N ch    Total: N`.
- Select from mid-line 1 to mid-line 3: `Sel: 3 ln, N ch`.
- Select from col 1 of line 1 to col 1 of line 2 (Shift+Down from start):
  `Sel: 1 ln, N ch` (not 2).
- Select to end of last line: verify `Total` matches document length.
- Word wrap ON, long wrapped line: visual line count matches visual extent.
- Czech locale: labels use `Výb`, `ř`, `zn`, `Celkem`.
- Screen reader: status bar text is announced on selection change (no change to
  accessibility plumbing needed — `SB_SETTEXT` already triggers MSAA notification).
