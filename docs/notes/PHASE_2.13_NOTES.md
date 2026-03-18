# Phase 2.13 Implementation Notes

## Output Pane

A secondary read/write RichEdit panel that appears below the main edit area when
a filter first writes to it via `Display=pane`.  Created hidden at startup
(`WM_CREATE` → `CreateOutputPane`) so it is always available; shown automatically
on first use; never hidden or destroyed during the session.

### Key design choices

- **Single creation, never destroyed** — avoids complexity of dynamic show/hide
  and keeps all pane-related state (scroll position, content) alive for the session.
- **Subclassed** for keyboard handling (F6 = return focus to main edit,
  Ctrl+Shift+Delete = clear pane, context menu with Copy All / Clear).
- **Word wrap disabled** — output is typically structured (paths, key=value,
  tabular data); horizontal scroll bar added.
- **Height** configurable via `OutputPaneLines=5` (integer lines) or `20%`
  (fraction of available client area).  Recalculated on `WM_SIZE`.
- **`Pane=` key** accepts comma-separated tokens `append`, `focus`, and `start`; parsed
  with a manual comma tokeniser (see `wcstok` note below).

### `Pane=start` token

`start` positions the caret at the beginning of the newly written output after the
pane is updated, and scrolls that position into view via `EM_SCROLLCARET`.

- **Replace mode** (no `append`): caret always goes to position 0 and the view
  scrolls to the top.  This is also the fix for a pre-existing bug where
  `SetWindowText` left the view scrolled to wherever it was before the write.
  The `start` token has the same effect as the default in replace mode.
- **Append mode + `start`**: the pre-append text length is saved before
  `EM_REPLACESEL`, and the caret is set to that saved offset — i.e. the first
  character of the newly appended chunk.
- **Append mode without `start`**: caret remains at the end of all content
  (existing behaviour, preserved).

The scroll-to-top fix for replace mode is unconditional — it is a behaviour
improvement, not a breaking change, because the view position after a full replace
is not a user-observable setting.

### `wcstok` ABI mismatch (CI fix)

The original code used the C11 three-argument `wcstok(str, delim, &ctx)`.
MXE (the local cross-compiler) provides the C11 form; Ubuntu 24.04 MinGW-w64
11.0.1 exposes only the two-argument Windows MSVCRT form, which caused a CI
build failure.  Fixed by replacing both call sites with a manual `wcschr`-based
comma tokeniser — portable, no dependency on either `wcstok` variant.

---

## Accessible Names for RichEdit Controls

### Problem

Neither the main edit area nor the output pane had an accessible name.  Screen
readers announced the controls as "RichEdit50W" (the window class name) or simply
said nothing, giving no context to blind users.

### Attempt 1 — MSAA Dynamic Annotation (`IAccPropServices::SetHwndPropStr`)

`IAccPropServices` is the MSAA Dynamic Annotation API (`oleacc.dll`).  Calling
`SetHwndPropStr(hwnd, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME_, name)` injects
an accessible name into the MSAA object tree at runtime without subclassing.

**Result: works for default RichEdit only.**

When the default library (`MSFTEDIT.DLL` / `RICHEDIT50W`) is loaded, NVDA/JAWS/
Narrator query the MSAA object tree and pick up the annotation.  When a modern
RichEdit DLL is loaded (e.g. the one shipped with Windows 11 Notepad or Office),
the DLL registers a native UIA provider.  UIA-mode screen readers (which is the
mode used on modern Windows) query the UIA provider chain directly and never
consult MSAA annotations — so the name is silently ignored.

MSAA Dynamic Annotation was kept in place because it covers the default-library
path at zero additional cost.  GUIDs (`CLSID_AccPropServices_`,
`IID_IAccPropServices_`, `PROPID_ACC_NAME_`) are defined as `static const` in
`main.cpp` because the MXE sysroot's `liboleacc.a` only emits `extern`
declarations without storage (INITGUID is not effective there).

### Attempt 2 — `UiaRegisterProviderCallback`

`UiaRegisterProviderCallback` registers a function that UIA calls to obtain a
custom provider for any HWND.  This looked like a clean way to return a provider
that supplies only `UIA_NamePropertyId` and delegates everything else to the
native RichEdit provider.

**Rejected.**  The callback is invoked only when there is *no* native UIA
provider for the HWND.  Modern RichEdit DLLs register a native UIA provider, so
the callback is never reached for those windows.  The approach is effectively the
same dead end as MSAA annotations for the modern-library path.

### Attempt 3 — WM_GETOBJECT subclassing

Subclassing the RichEdit HWND to intercept `WM_GETOBJECT` (with
`UiaReturnRawElementProvider`) and returning a custom `IRawElementProviderSimple`
was considered.  The custom provider could wrap the native one and override
`UIA_NamePropertyId`.

**Rejected.**  The native RichEdit UIA provider exposes rich text patterns
(`ITextProvider`, `ITextProvider2`, `IValueProvider`, `ISelectionProvider`).
These are what screen readers use to read content, announce selections, and
navigate by word/line.  Replacing the provider root with a wrapper risks losing
or breaking those patterns depending on how the DLL's internal provider chain is
constructed.  The risk of degrading the core screen-reader experience was
considered too high for a name-only improvement.

### Attempt 4 (adopted) — 1×1 `WS_VISIBLE` STATIC label

The Win32 UIA HWND composition layer (the in-process UIA infrastructure that
wraps every HWND into a UIA element) resolves `UIA_NamePropertyId` for a control
whose native provider returns `VT_EMPTY` for Name by looking at the
immediately-preceding sibling in Z-order.  If that sibling is a `STATIC` control,
its window text becomes the name of the labelled control — the standard
"label-for" association pattern used by dialog layouts.

By creating a 1×1 pixel `WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_NOPREFIX`
STATIC immediately *before* each RichEdit in the creation sequence (and therefore
lower in Z-order, i.e. the preceding sibling), the UIA infrastructure resolves the
RichEdit's name from the STATIC's text.

```
CreateUiaLabel(hwnd, IDS_ACCNAME_EDITOR);   // STATIC "Text Editor"
CreateRichEditControl(hwnd);               // RichEdit — name resolved from above

CreateUiaLabel(hwndParent, IDS_ACCNAME_OUTPUTPANE);  // STATIC "Panel výstupu"
CreateWindowEx(..., WC_RICHEDIT, ...);               // output pane
```

The STATIC is visually invisible: it is 1×1 px at position (0,0) and is
completely covered by the RichEdit which is created on top of it.  It is *not*
`WS_DISABLED` or `WS_EX_TRANSPARENT` — those do not affect UIA; `WS_VISIBLE` must
be set for the HWND composition layer to include the window in its sibling scan.

**Result: confirmed working with modern RichEdit DLL.**  NVDA and Narrator
announced "Text Editor" / "Panel výstupu" correctly when the modern library was
loaded.  This was the unexpected outcome — the approach was initially tried as a
long shot.

### Attempt 5 — `STATE_SYSTEM_INVISIBLE` MSAA annotation on the STATIC labels

The STATIC labels appear as extra navigation stops in AT object trees
("Text Editor, static" / "Panel výstupu, static").  To suppress them without
removing `WS_VISIBLE` (which would break UIA label resolution), an MSAA Dynamic
Annotation was tried: `IAccPropServices::SetHwndProp` with `PROPID_ACC_STATE_`
and `var.lVal = STATE_SYSTEM_INVISIBLE`.

**Did not work.**  MSAA state annotations are not consulted by UIA-mode screen
readers for STATIC controls.  The UIA provider for a STATIC (`ControlType =
Text`) does not read back MSAA state; it has its own state model.  The annotation
was removed as dead code.  `PROPID_ACC_STATE_` GUID is
`{A8D4D5B0-0A21-42D0-A5C0-5145D304CA99}` — noted here for any future attempt.

### Known limitation and future directions

The STATIC labels remain visible as navigation stops in AT object trees.  A
screen reader user navigating by object encounters "Text Editor, static" then
"Text Editor, edit" (or similar), which is redundant noise.

Possible future approaches if this becomes important enough to revisit:

- **Custom IAccessible on the STATIC** (subclass + `WM_GETOBJECT`) returning
  `STATE_SYSTEM_INVISIBLE` from `get_accState`.  This is MSAA; whether UIA-mode
  screen readers honour it for a STATIC is untested.
- **`UIA_IsOffscreenPropertyId` / `UIA_IsControlElementPropertyId`** — if a
  custom UIA provider can be injected for the STATIC (lower risk than for the
  RichEdit, since STATIC has no complex text patterns to preserve), setting
  `IsControlElement = FALSE` would remove it from the control-view subtree that
  screen readers navigate.
- **`AccSetRunLevel` / `IAccPropServices::SetHwndPropServer`** with a callback
  that synthesises the full accessible object state — heavier but gives full
  control.
- **Invisible owner-drawn STATIC** (`SS_OWNERDRAW` with no painting) + UIA
  provider returning `IsControlElement = FALSE`.

For now the duplicate stop is accepted: the name announcement on the RichEdit is
the primary goal, and that works correctly on both default and modern RichEdit.
