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

## Where to Look

- `src/main.cpp`: all primary logic
- `README.md`: user-facing behavior and usage
- `docs/notes/AGENTS_APPENDIX.md`: historical deep dives and patterns
- `docs/CHANGE_CHECKLIST.md`: post-change documentation checklist

## Quick Build

```bash
make
```

Warnings about GETTEXTEX/SETTEXTEX should not appear; structs are explicitly initialized.

When adding or adjusting features, prefer refactors over new code to keep the binary small, and report old/new binary sizes.
