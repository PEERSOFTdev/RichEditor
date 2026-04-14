# AGENTS.md - AI Agent Guide for RichEditor

This is the concise, current guide for contributors and AI agents. The detailed design notes live in `docs/notes/AGENTS_APPENDIX.md`.

## Project Snapshot

- **App:** RichEditor - lightweight Win32 text editor
- **Core:** Single-file architecture (most logic in `src/main.cpp`)
- **UI:** Win32 + RichEdit (plain text, UTF-8)
- **Build:** MinGW-w64 or MSVC, static linking
- **Languages:** English + Czech resources in one binary
- **Accessibility:** Screen reader support is non-negotiable

## Non-Negotiables

- **Accessibility first:** menu labels must remain screen-reader friendly.
- **Single-file bias:** new functionality generally lives in `src/main.cpp`.
- **UTF-8 (no BOM):** for text files written by the app.
- **UNC paths:** avoid Win32 INI APIs (no `GetPrivateProfileString`).
- **Small binary size:** prefer refactors over new code; report old/new sizes for feature changes.

## Version Number Management

- **Only a human developer may bump the version number or create Git tags.** Agents must never change a version number without an explicit human instruction to do so.
- When a human developer explicitly instructs a version change, update **all** of the following locations in the same commit:

  | File | What to update |
  |------|----------------|
  | `src/resource.rc` | `FILEVERSION` and `PRODUCTVERSION` quads (e.g. `2,8,0,0`) |
  | `src/resource.rc` | `VALUE "FileVersion"` in both language blocks (`040904B0` EN, `040504B0` CS) |
  | `src/resource.rc` | `VALUE "ProductVersion"` in both language blocks |
  | `src/resource.rc` | `LTEXT "RichEditor vX.Y.Z"` in both `IDD_ABOUT` dialog definitions (English + Czech) |
  | `README.md` | Any version references |
  | `Reference.md` | Any version references |
  | `docs/USER_MANUAL_EN.md` | If it contains version references |
  | `docs/USER_MANUAL_CS.md` | If it contains version references |

## INI System (Current Behavior)

- INI content is cached in memory (`g_IniCache`).
- `ReadINIValue` / `WriteINIValue` / `ReplaceINISection` operate on the cache.
- Disk writes only happen via `FlushIniCache()` (on exit and critical paths).
- External edits are **not** detected during runtime by design.
- **Inline comment convention:** a `;` is treated as an inline comment delimiter only when it is preceded by a space or tab (e.g. `Key=Value   ; comment`). A bare `;` not preceded by whitespace is kept as part of the value. This allows JScript `Command=` values to contain semicolons freely. The one residual limitation: a JScript string or regex that literally contains ` ;` (space + semicolon) would still be truncated there.

## Find / Replace Rules (Current)

- **History is saved only when an action happens** (Find/Replace/Replace All).
- **Checkbox options** persist immediately on toggle (cache-only write).
- **INI ordering:** Find/Replace history is written as `Item1..ItemN`, with **Item1 = newest**.

## Autosave Rules (Current)

- Autosave on focus loss uses `WM_ACTIVATEAPP` (external app switch only).
- Timer autosave runs unless a dialog from this app is foreground.
- Autosave is blocked while `g_bSaveInProgress` is set.

## Save Pipeline (Current)

- `FileSave` / `FileSaveAs`: attempt silent save via `SaveTextFileSilently`; on `ERROR_ACCESS_DENIED` → `PerformElevatedSave`.
- `SaveTextFileSilently` → `SaveTextFileInternal(..., bShowErrors=FALSE)`: writes file without showing errors; returns failure type and Win32 error via out-params.
- `SaveTextFileInternal`: canonical implementation; all save wrappers delegate here.
- `SaveTextFile`: thin wrapper for `SaveTextFileInternal` with error display enabled (used by autosave and resume save).
- `PerformElevatedSave`: stages content to `%TEMP%\RichEditor\`, launches self with `/elevated-save` via `ShellExecuteEx` (`runas`), waits, restores foreground.
- `FinalizeSuccessfulSave`: single place that updates `g_szFileName`, clears modified flag, updates title and MRU after any successful save.
- `ShowSaveTextFailure`: single place that shows save error dialogs by failure type.
- `g_bSaveInProgress`: guard set by `FileSave`, `FileSaveAs`, and `DoAutosave`; prevents re-entrant saves. `WM_CLOSE` clears it before the close sequence. `PromptSaveChanges` IDNO path sets `g_bModified=FALSE` and `g_bSaveInProgress=TRUE` to block post-discard saves.
- `OFN_NOTESTFILECREATE` is set on Save As dialog flags intentionally, with a manual overwrite prompt, to bypass the shell's refusal to present the dialog for protected paths.

## DPI Awareness and Visual Styles

- DPI awareness is declared via `src/RichEditor.manifest`, embedded as resource `1 24` in `src/resource.rc`. The manifest also enables Common Controls v6 visual styles and the UTF-8 active code page. Do not remove or modify it without understanding that it controls all three features.
- The app is Per-Monitor V2 DPI-aware (Win10 1703+) with fallback to Per-Monitor V1 (Win8.1+) and system-DPI-aware (Win7/Vista).
- `g_nDpi` holds the current effective DPI. It is initialized in `WM_CREATE` and updated in `WM_DPICHANGED`.
- **Never hardcode pixel constants.** Use `ScaleDpi(value, g_nDpi)` for any pixel measurement that must adapt to DPI (status bar widths, padding, minimum sizes, etc.).
- `GetDpiForHwnd(hwnd)` dynamically loads `GetDpiForWindow` (Win10 1607+); falls back to `GetDeviceCaps(LOGPIXELSX)`.
- `EnableNonClientDpiScaling` is called in `WM_NCCREATE` for correct title bar / scroll bar scaling on Per-Monitor V1.
- New dialogs must use `DIALOGEX` with `FONT 8, "MS Shell Dlg"` and DLU-based coordinates. The Per-Monitor V2 dialog manager handles scaling automatically for such dialogs.
- `WM_GETMINMAXINFO` enforces DPI-scaled minimum window size.

## Menu Bar Accessibility (RichEdit UIA Suppression)

- Modern RichEdit (v8.0+, Office/Win11 DLL) has a native UIA provider that interferes with NVDA's menu bar navigation, causing the system menu position to announce the document title instead of "System menu".
- `g_bInMenuLoop` tracks menu bar activity via `WM_ENTERMENULOOP` / `WM_EXITMENULOOP`.
- Both `EditSubclassProc` and `OutputPaneSubclassProc` return 0 for `WM_GETOBJECT` when `g_bInMenuLoop` is true, suppressing the UIA provider during menu navigation.
- **Any new RichEdit-based child window** must apply the same `WM_GETOBJECT` suppression in its subclass procedure.

## Reference.md Maintenance (Agent Guidelines)

- Preserve the legacy README tone and structure (see `README.md` at commit `e2567a9`); avoid reorganizing sections.
- Only change text when it is inaccurate; otherwise move existing bullets to the correct phase.
- Use commit history (oldest to newest) to place features and split phased evolutions.
- Prefer minimal edits: move bullets, add small clarifying lines, avoid rewrites.
- Keep examples international and neutral; do not introduce locale-specific slang.
- If defaults changed over time, note the change explicitly in the correct phase.
- Do not reintroduce duplicate phase blocks after Usage.
- Verify behavior against `src/main.cpp` before updating Reference.md.
- **Phase numbering is a human decision.** Agents must never introduce a new `### Phase X.Y` heading in `Reference.md` without an explicit instruction from the human developer. Document new work inside the most recent accepted phase section, or as a subsection of it, until a human assigns a new phase number.

## Commit Message Guidelines

- Subject line: imperative verb + object, no prefix tags, no period, <= 72 chars.
- Be specific: if replacing/migrating, name both old and new.
- One logical change per commit; split unrelated edits.
- Body: add a blank line, then 1-5 short verb-led bullets for intent/impact when non-trivial.
- Docs-only changes: use verbs like Document/Update/Refine.
- Avoid vague subjects ("misc", "fix stuff").
- Tests: if run, add a final "Tests: ..." line; if not run and notable, add "Tests: not run".
- Never commit secrets or ignored/local files unless explicitly requested.
- **All commits must be GPG-signed** (`git commit -S`). The repository enforces signed commits via branch protection. Never use `--no-gpg-sign`; if GPG signing fails, stop and alert the user.
- **Never push to the remote.** Agents must only commit locally. Pushing is always a human action.

## Where to Look

- `src/main.cpp`: all primary logic
- `README.md`: user-facing behavior and usage
- `Reference.md`: full phase-by-phase feature reference and usage details; update for every user-facing or architectural change
- `docs/notes/AGENTS_APPENDIX.md`: historical deep dives and patterns
- `docs/CHANGE_CHECKLIST.md`: post-change documentation checklist
- `docs/requests/`: user-requested features tracked as individual `UR-NNN_*.md` files

## User Request Workflow

When a user reports a desired feature or behaviour change that does not belong to an
existing planned phase:

1. **Check `docs/requests/`** — a `UR-NNN_*.md` file may already describe the request.
   The `docs/requests/README.md` index lists all requests and their current status.
2. **If the request is new**, create `docs/requests/UR-NNN_short_description.md`
   following the format of existing files (Background, Scope, Changes with code
   snippets, File summary, Testing notes). Set status to `Planned`.
3. **While implementing**, set status to `In Progress`.
4. **When done and building cleanly**, set status to `Done`.
5. After implementation, also update `docs/requests/README.md` index and follow
   `docs/CHANGE_CHECKLIST.md` for any other required doc updates.

## Changelog Maintenance

`docs/CHANGELOG.md` is the user-facing changelog. After any batch of commits that adds
user-facing or architectural changes, prepend the new entries under `## Unreleased`.

### Structure rules

- **Sections:** newest first — `## Unreleased` at the top, then versions descending.
- **Entries within a section:** chronological, oldest-to-newest commit order.
- **Entry format:** human-readable prose (see Writing style below), with commit hashes
  appended in parentheses for traceability, e.g. `(\`abc1234\`)`.
- **Grouping:** tightly related commits (e.g. a feature + its follow-up fixes) should be
  consolidated into a single entry. List all relevant hashes together.
- **Version header format:** `## vX.Y.Z (YYYY-MM-DD)` where the date is the date of the
  first commit that belongs to that version.
- **Unreleased header:** `## Unreleased` with no date.
- **Only a human developer may release a version** (rename `## Unreleased` to
  `## vX.Y.Z (YYYY-MM-DD)` and open a fresh `## Unreleased` above it). Same rule as
  version number bumps.

### Writing style

The changelog is read by users, not developers. Every entry must be understandable by
someone who has never seen the source code.

1. **Write from the user's perspective:** describe what changed in terms of visible
   behaviour, menus, shortcuts, dialogs, or settings — not function names, data
   structures, or internal mechanisms.
2. **For bug fixes, describe the symptom and trigger**, not the root cause.
   Good: "Fixed the Replace All message box appearing behind the main window when the
   Find dialog had been closed before pressing F3."
   Bad: "Fix MessageBox owner HWND falling through to NULL when hDlgFind is hidden."
3. **Avoid developer/internal jargon.** Do not use terms like: uninitialized buffer,
   bounds check, format placeholder, access violation, format specifier, UIA provider,
   child process's working directory, exit code, stderr, stdout, stdin, sync-point
   mechanism, output chunks, NULL, HWND, subclass procedure, EM\_\* messages, etc.
   If a technical detail genuinely helps the user, explain it briefly in plain language.
4. **For internal-only changes** (refactoring with no user-visible effect), keep entries
   short: "Internal code reorganisation; no user-visible behavior change."
5. **Keep entries concise** — aim for one or two sentences. Add a third only when the
   context would be unclear otherwise (e.g. explaining what the old behaviour was).

Example of a good entry:

```
- The Open dialog now offers an "All Supported Types" filter that combines every
  registered file extension into a single entry, making it easier to browse
  mixed-content folders. The Save As dialog is unaffected. (`6e50787`)
```

Example of a bad entry (too technical):

```
- Add BuildCombinedFilterString() helper; populate nFilterIndex for OFN struct
  in FileOpen() using aggregated extension list from g_Templates[]. (`6e50787`)
```

### How version boundaries are identified

There are no git tags in this repository. Version boundaries are found by inspecting
`FILEVERSION` in `src/resource.rc` across the commit history:

```bash
git log --oneline                                        # list all commits
git show <hash>:src/resource.rc | grep FILEVERSION       # check version at a commit
```

The first commit where `FILEVERSION` changes to `X,Y,0,0` is the start of that version's
section; its commit date becomes the section date.

Known boundaries (as of 2026-03-14):

| Section    | Boundary commit | Date       | FILEVERSION |
|------------|----------------|------------|-------------|
| Unreleased | `e7e09b4`      | 2026-03-13 | 2,8,0,0 (unreleased work) |
| v2.8.0     | `fbde613`      | 2026-03-05 | 2,8,0,0 |
| v2.7.0     | `359d682`      | 2026-01-14 | 2,7,0,0 |
| v2.6.0     | `8eb21ba`      | 2026-01-09 | 2,6,0,0 |
| v2.1.0     | `4ae175c`      | 2025-12-19 | 2,1,0,0 |
| v1.0.0     | `1d06689`      | 2025-12-04 | (initial) |

Note: versions 2.2–2.5 were development phases shipped under the `2,1,0,0` FILEVERSION
label; they are folded into the v2.1.0 section.

To list commits in a version range:

```bash
git log --oneline fbde613..e7e09b4    # example: all v2.8.0 commits
```

### What to include and what to skip

**Keep** commits that are user-facing or architecturally significant:
- New features, behaviour changes, bug fixes visible to the user
- Changes to INI keys, defaults, keyboard shortcuts, menu items
- New build targets or significant toolchain changes
- Refactors that change observable behaviour (e.g. save pipeline rewrites)

**Drop** commits that are purely internal with no user impact:
- Debug output added or removed
- GCC / compiler warning fixes with no behaviour change
- AGENTS.md-only updates
- CI / build-system-only tweaks (unless they introduce a new build target)
- Pure README/doc reformats that only reorganise without adding content
- Internal refactors with zero user-visible effect

When in doubt, include the entry — it is easier to prune later than to reconstruct.

### Adding entries for new work

1. Run `git log <last-recorded-hash>..HEAD` (without `--oneline`) to get full commit
   messages (subject + body). The body often contains intent, user-visible impact, and
   context that the subject alone omits — use all of it when writing entries.
2. Filter using the keep/drop rules above.
3. Rewrite each kept commit into human-readable prose following the Writing style rules,
   drawing on the full commit message for context.
4. Consolidate tightly related commits into single entries where appropriate.
5. Prepend the new entries (oldest-to-newest) under `## Unreleased` in
   `docs/CHANGELOG.md`.

## Quick Build

```bash
make
```

Warnings about GETTEXTEX/SETTEXTEX should not appear; structs are explicitly initialized.

When adding or adjusting features, prefer refactors over new code to keep the binary small, and report old/new binary sizes.
