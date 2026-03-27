# Changelog

All notable changes to RichEditor are listed here, newest first.
Within each version, entries are in chronological (oldest-to-newest) order.

---

## Unreleased

- Bump version to v2.9.0
- Update BUILD_MSVC.md: fix GPL to MIT, remove external comparisons, update stale sizes
- Add README_CS.md: Czech signpost with manual pointer and archive contents
- Add source code URL to README.md
- Add release zip workflow: make dist target and CI packaging with docs
- Add All Supported Types filter to Open dialog; exclude from Save As

## v2.9.0 (2026-03-26)

- Add script: filter prefix with embedded JScript engine (Phase 2.12)
- Fix script: filter: call SetScriptState(STARTED) before expression eval
- Fix Sort Lines script: split/join on CR, drop semicolon
- Remove MigrateBuiltinFilters: migration too aggressive, no auto-migration policy
- Change INI inline comment delimiter from bare ; to whitespace-preceded ;
- Add localized category display names to filter and template menus
- Remove redundant Category.cs from Filter10 default configuration
- Add output pane: Display=pane with Pane=append/focus, F6, Ctrl+Shift+Delete, context menu
- Annotate RichEdit controls with accessible names via IAccPropServices (MSAA Dynamic Annotation)
- Replace wcstok with manual comma tokeniser in LoadFilters for cross-compiler portability
- Add Pane=start token: place caret at start of newly written output; fix replace-mode scroll-to-top
- Fix MinGW linker flags: move --gc-sections to LDFLAGS; add -flto -Os -fno-exceptions
- Remove redundant /Os and /Gy from MSVC release flags (implied by /O1)

---

## v2.8.0 (2026-03-05)

- Expand About dialog and document version bump rules
- Add zoom support with word wrap fix and Reset Zoom command
- Fix zoom reset when loading or creating a document
- Fix Braille/screen-reader confusion on MRU open with slow RichEdit
- Fix Replace button requiring two presses
- Report not-found in Replace All; silence post-replace find
- Fix adjacent URLs on one line parsed as a single URL
- Document Find/Replace escape sequences and Replace placeholders in Reference.md
- Fix Save/AutoSave silently blocked after discarding changes in open/new dialog
- Fix Save doing nothing on untitled documents (File > New then Ctrl+S)
- Fix caret reset to position 0 after failed MRU load (file not found)
- Show selection stats in status bar when text is selected

---

## v2.7.0 (2026-01-14)

- Template system: Core infrastructure + keyboard shortcuts
- Template system: Variable expansion and insertion logic
- Template system: Tools menu integration
- Template system: File→New submenu integration
- Template system: Localization support
- Bump version to 2.7.0 (Template System)
- Fix template system menu localization and file dialog bugs
- Filter unknown extensions and make Save As dialog dynamic
- Fix 'New' menu localization and ensure Text Files filter always present
- Improve file dialog filters: localize and reorder
- Refactor file dialogs: category-based filters with proper localization
- Remove 'General' category: show uncategorized templates at root
- Remove 'General' category for filters: show uncategorized at root
- Implement Ctrl+Shift+T template picker menu
- Fix template menu synchronization and eliminate code duplication
- Fix template localization: support both full locale and language-only keys
- Fix Save As dialog filter selection for untitled files
- Phase 2.8.1: Add RichEdit version detection infrastructure
- Phase 2.8: Add configurable RichEdit library selection
- Fix swprintf Unicode bug in About dialog (Phase 2.8.5)
- Add RichEditD2DPT support with graceful fallback chain (Phase 2.8.5)
- Add enhanced About dialog and INI class override (Phase 2.8.5)
- Localize session resume save prompt (Phase 2.6 fix)
- Add MSVC build system - 18% smaller executables
- Add Find dialog with escape sequences and history (Phase 2.9.1)
- Improve cursor positioning for bidirectional search (Phase 2.9.1)
- Add configurable date/time formatting (Phase 2.10, ToDo #3)
- Change DateTimeTemplate default to use configurable variables
- Document Insert/Overtype mode without status bar indicator
- Add read-only mode feature with comprehensive UI protection
- Fix autosave to respect read-only mode
- Fix MRU list not updating when files opened via command line
- Flatten template picker popup menu for easier selection
- Add Replace functionality with optimized Replace All (Phase 2.9.2)
- Fix critical bugs in DoReplaceAll (Phase 2.9.2)
- Unify Replace All with custom word boundaries (Phase 2.9.2 final)
- Fix undo support for Replace All (Phase 2.9.2)
- Fix Replace All undo, autosave race, and dialog performance (Phase 2.9.2)
- Add custom undo name for Replace All operation
- Fix autosave dialog detection using WM_ACTIVATEAPP
- Refactor INI section saves and disable Replace in read-only mode
- Cache INI reads, save find options immediately, and clean history saves
- Reorganize documentation and add user manuals
- Add MIT license and project philosophy
- Make word wrap live and fix status bar lines for RichEdit 8+
- Refine manuals and format shortcuts
- Add Go to Line dialog
- Remove filter help menu
- Document RichEditLibraryPath examples
- Add bookmark navigation and persistence
- Replace Speak Text with Auto Indent
- Handle elevated save dialogs and close flow (ToDo #2)
- Document elevated save (Phase 2.9.5) across all relevant docs
- Replace EM_EXLINEFROMCHAR with TOM for status bar line queries
- Replace TOM/O(N) line queries with cached line-starts index
- Disable AURL_ENABLEURL above 512 K threshold to fix large-file cursor lag
- Replace AURL threshold logic with a single DetectURLs INI setting

---

## v2.6.0 (2026-01-09)

- Add Phase 2.6: Session Resume with automatic recovery
- Fix: Clear [Resumed] indicator when loading a new file
- Change precedence: Command-line arguments now override resume files
- Fix: Clear [Resumed] indicator when user discards resumed file
- Fix: Clear [Resumed] indicator immediately after saving file
- Fix: Don't delete resume file when user cancels close operation
- Fix: Eliminate code duplication in resume file I/O - fixes data loss bug
- Refactor: Fix resume file regressions and eliminate code duplication
- Fix: REPL filter string formatting and localization issues
- Fix: Use localized filter names in all user-facing displays
- Improve: Use dynamic window sizing based on work area instead of hardcoded 800x600
- Fix: Autosave now properly clears modified flag after successful save
- Fix: Edit menu Cut/Copy/Paste items now correctly enable/disable based on state
- Fix: Prevent autosave infinite loop when save fails

---

## v2.1.0 (2025-12-19)

- Add URL autodetection with keyboard and mouse activation (v2.1)
- Fix URL context menu commands not working
- Fix URL detection boundary bugs and Enter key handler
- Optimize URL detection performance in large documents
- Further optimize URL detection using EM_FINDWORDBREAK
- Add EN_STOPNOUNDO notification handler
- Fix critical bug: Always prompt to save when document is modified
- Fix autosave-on-focus-loss interfering with save prompt
- Add /nomru command-line option to prevent MRU list pollution
- Add SelectAfterPaste feature (configurable, default off)
- Phase 2.5b: Add user interaction layer for REPL filters with dual-filter system
- Fix REPL exit notification shown on intentional close
- Add stderr support for REPL mode to capture error output
- Fix REPL line ending defaults for Unix shells (WSL, bash)
- Add WSL Bash example filter with interactive mode
- Use pseudo-TTY for WSL Bash to enable prompt display
- Strip ANSI escape sequences from REPL output
- Handle OSC sequences in ANSI escape stripper
- Fix AUTO mode EOL detection causing duplicate prompts in bash
- Remove manual newline insertion after sending REPL command
- Insert newline after sending command to separate from shell echo
- Disable terminal echo in WSL Bash to prevent command duplication
- Revert to simple script command, avoid quoting issues
- Insert REPL output after current command line, not at document end
- Fix Ctrl+Shift+I shortcut intercepted by RichEdit control
- Suppress TAB insertion when Ctrl+Shift+I/Q shortcuts are pressed
- Send REPL input only when Enter pressed on line with prompt
- Fix URL detection for query parameters with = and & characters
- Fix URL detection including trailing non-URL characters
- Restore word break optimization for URL detection in large documents
- Fix URL boundary detection with proper scanning and fallback
- Force AutoURL detection on file load
- Revert slow AutoURL trigger and reorder initialization
- Handle Windows shutdown/logoff with save prompts
- Fix: Actually close editor window on Windows shutdown
- Use ShutdownBlockReason API to prevent 5-second timeout
- Fix: Close editor immediately after user confirms during shutdown

---

## v1.0.0 (2025-12-04)

- Initial project structure with README
- Add resource definitions (menus, accelerators, About dialog)
- Add window framework (WinMain, window registration, basic WndProc)
- Add RichEdit control integration (load Msftedit.dll, create control, handle resize)
- Add status bar with line/column tracking and file info display
- Add UTF-8 conversion and file I/O functions (load/save text files)
- Implement File menu operations (New, Open, Save, Save As, save prompts)
- Implement Edit menu operations and modified state tracking (EN_CHANGE)
- Implement About dialog
- Add filter system architecture (Phase 2 stubs with documentation)
- Add Makefile for MinGW-w64 build system
- Fix compilation errors and add MXE cross-compilation support
- Fix focus and modified flag issues
- Add WM_SETFOCUS handler to restore focus to edit control
- Add character at cursor display in status bar (dec and hex)
- Implement autosave with configurable interval and focus-loss trigger
- Fix autosave timer by passing hwnd parameter to StartAutosaveTimer
- Implement word wrap toggle with Ctrl+W shortcut
- Fix word wrap to work correctly on initial startup
- Add dual position display for word wrap mode
- Fix word wrap physical position calculation
- Add time/date insertion feature (F5)
- Add comprehensive version resource to executable
- Add Czech (cs-CZ) localization
- Convert to universal executable with automatic language selection
- Fix Czech character encoding with UTF-8 code page pragma
- Implement Phase 2: External filter system with process pipes
- Fix BuildFilterMenu timing issue - pass hwnd parameter
- Restore dynamic menu finding instead of hardcoded positions
- Add comprehensive filter collection - 20 useful filters
- Add filter output modes - Replace/Append/Below options
- Add filter categories - organize filters into submenus
- Make application settings configurable via INI file
- Create default INI file with sensible defaults on first run
- Rename Manage Filters to Filter Help with updated documentation
- Translate all MessageBox strings to Czech using string resources
- Translate 'Untitled' and skip save prompt for empty documents
- Localize 'No filters configured' menu message
- Replace legacy GetPrivateProfile APIs with direct INI file reading for UNC path support
- Fix filter REPLACE mode to properly select and replace lines without adding newlines
- Add filter persistence, status bar display, and custom INI writer for UNC path support
- Remove filename from status bar (already shown in title bar)
- Add command-line argument support for opening files
- Add MRU (Most Recently Used) file list feature
- Add Unicode surrogate pair support to status bar
- Add accessibility features with configurable menu descriptions
- Localize error messages for better user experience
- Add localized filter names and descriptions
- Localize filter execution messages and context menu items
- Localize file dialogs and status bar strings
- Add context-aware undo/redo menu labels
- Refactor undo/redo tracking to use EM_GETUNDONAME API
- Add tab-aware column calculation for status bar

