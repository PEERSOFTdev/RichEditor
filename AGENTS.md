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

## Reference.md Maintenance (Agent Guidelines)

- Preserve the legacy README tone and structure (see `README.md` at commit `e2567a9`); avoid reorganizing sections.
- Only change text when it is inaccurate; otherwise move existing bullets to the correct phase.
- Use commit history (oldest to newest) to place features and split phased evolutions.
- Prefer minimal edits: move bullets, add small clarifying lines, avoid rewrites.
- Keep examples international and neutral; do not introduce locale-specific slang.
- If defaults changed over time, note the change explicitly in the correct phase.
- Do not reintroduce duplicate phase blocks after Usage.
- Verify behavior against `src/main.cpp` before updating Reference.md.

## Commit Message Guidelines

- Subject line: imperative verb + object, no prefix tags, no period, <= 72 chars.
- Be specific: if replacing/migrating, name both old and new.
- One logical change per commit; split unrelated edits.
- Body: add a blank line, then 1-5 short verb-led bullets for intent/impact when non-trivial.
- Docs-only changes: use verbs like Document/Update/Refine.
- Avoid vague subjects ("misc", "fix stuff").
- Tests: if run, add a final "Tests: ..." line; if not run and notable, add "Tests: not run".
- Never commit secrets or ignored/local files unless explicitly requested.

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

## Quick Build

```bash
make
```

Warnings about GETTEXTEX/SETTEXTEX should not appear; structs are explicitly initialized.

When adding or adjusting features, prefer refactors over new code to keep the binary small, and report old/new binary sizes.
