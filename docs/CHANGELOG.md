# Changelog

All notable changes to RichEditor are listed here, newest first.
Within each version, entries are in chronological (oldest-to-newest) order.

---

## Unreleased

- The Open dialog now offers an "All Supported Types" filter that combines every registered file extension into a single entry, making it easier to browse mixed-content folders. The Save As dialog is unaffected — it still lists individual types so the chosen extension is unambiguous. (`6e50787`)
- Addon system: filters and templates can now be distributed as self-contained packs in the `addons/` directory. Each subfolder contains its own `filters.ini` and/or `templates.ini`; RichEditor loads them at startup alongside the built-in definitions. A "Reload Addons" item under the Tools menu rescans without restarting. (`b03c174`)
- Fixed a crash when the `addons/` directory exists but contains no addon packs. Also fixed a potential crash when reloading addons while REPL mode is active. (`08009d7`)
- Addon templates now use a compound key (source + name + file extension) for duplicate detection, so identically named templates from different packs or for different file types can coexist instead of silently overriding each other. Override warning messages now include the type label and source pack name. (`3a9c3af`)
- Fixed the addon status bar message showing wrong counts — the pack count was silently dropped, shifting all subsequent numbers by one position. (`76e934a`)
- The addon summary in the status bar now persists after opening a file via resume or command-line argument, where previously it was overwritten by the file-load status update. (`6a42d45`)
- Addon filters whose `Command=` uses a relative path (e.g. `mytools\convert.exe`) now resolve correctly against the addon's own directory. Previously, the executable was not found even though the addon's folder was set as the working directory — Windows requires the full path to locate it. Bare executable names like `powershell.exe` are left to the system PATH as before. (`f17d6cb`, `482e165`)
- New `FilterDebug=1` INI option (under `[Settings]`) logs every filter and REPL command execution to the output pane — including the full command that was run, its working directory, and any error output. Useful for troubleshooting addon filters that fail silently. (`b7ee0a1`)
- REPL mode now supports a `\raw:` input prefix (when `FilterDebug=1` is active) that sends text with escape-sequence expansion (`\n`, `\t`, `\xNN`, `\uNNNN`, etc.) and no automatic end-of-line, for low-level protocol testing. (`fc5e93f`)
- Per-Monitor V2 DPI awareness: the window, title bar, scroll bars, status bar, and output pane now scale correctly when moved between monitors with different DPI settings, or when the display scale is changed at runtime. An embedded application manifest also enables Common Controls v6 visual styles and the UTF-8 active code page. Falls back gracefully on older Windows versions. (`0eb3247`)
- Screen reader fix: NVDA now correctly announces "System menu" when navigating the menu bar with the keyboard. Newer versions of the RichEdit control (shipped with Office and Windows 11) were interfering with the announcement by reporting the document title instead. The editor now temporarily suppresses this interference while a menu is open. (`2fe6d0c`)
- Internal code size reduction: consolidated repeated patterns across the codebase into shared helpers. Net effect is roughly 2 KB smaller binary with no user-visible behavior change. (`23a145a`, `584f1eb`, `f978f0b`)
- REPL tab completion: pressing Tab on a prompt line sends the partial input to the child process for completion. The editor suppresses the echoed text that the child sends back and tracks what has already been sent, so that subsequent Tab or Enter presses transmit only the new portion. Requires a cooperating REPL filter that reads input without waiting for a full line. (`5164e3b`)
- Fixed "Cannot find" and "Replace All" message boxes stealing focus into an invisible window when the Find dialog had been closed (hidden) before pressing F3. The message box owner now falls back to the main window whenever the Find dialog is not visible. (`e83e7cc`)
- Autocorrection tables: you can now define search → replace pairs in `autocorrections.ini` files (in addon packs or in the main `RichEditor.ini`) and have them applied while you type, to incoming REPL output, or on demand via **Tools → Apply Autocorrections**. Activation per table is controlled by a new `[AutocorrectionSettings]` section in `RichEditor.ini`. See `docs/autocorrection_tables.md` for the full format reference. (`8f08430`, `eea5050`)
- Fixed autocorrection table names and descriptions not appearing in their translated form when running under a non-English locale. The editor now follows the same two-pass lookup used by filters and templates: it first checks the full locale key (e.g. `Name.cs_CZ`), then falls back to the language-only key (e.g. `Name.cs`), before using the English name.
- The addon loading summary shown in the status bar now includes the number of autocorrection tables loaded from addon packs, alongside the existing filter and template counts.

## v2.9.0 (2026-03-26)

- Zoom support: Ctrl+mouse wheel zooms in and out; word wrap adjusts correctly at all zoom levels. View → Reset Zoom (Ctrl+0) returns to 100 %. The current zoom level is shown in the status bar and persisted in the INI file. (`853e71f`, `912b6d1`)
- Fixed a screen reader issue where opening a large file from the recent-files list left the Braille display stuck on the menu item or reporting the editor as unavailable, instead of showing the document with the cursor. (`ee2a041`)
- Fixed the Replace button requiring two presses: the first press found the match but did not replace it; a second press was needed to actually perform the replacement. (`f36bef2`)
- Replace All now shows a "Cannot find" message when the search term is not in the document, instead of silently doing nothing. Also, replacing the last occurrence no longer immediately shows a "not found" message — that message only appears on the next press. (`6f00c37`)
- Fixed two adjacent URLs on the same line being merged into a single invalid URL when clicked. (`f35e061`)
- Fixed Save and AutoSave becoming permanently blocked after discarding changes (pressing No) in the File → Open or File → New save prompt. The editor gave no indication that saving was disabled. (`afd37cf`)
- Fixed Save (Ctrl+S) doing nothing on untitled documents created with File → New. (`cbb9c39`)
- Fixed the cursor jumping to the beginning of the document after trying to open a file from the recent-files list that no longer exists. (`50ba6ef`)
- The status bar now shows selection statistics (line count, character count, and total document length) when text is selected. (`3c7e7e7`)
- Embedded JScript engine: filters can now use a `script:` prefix to run JScript expressions directly, without spawning an external process. The result obeys the same Action/Insert/Display/Clipboard routing as pipe-based filters. (`d74e304`, `d7d399c`)
- The inline-comment delimiter in INI values changed from a bare `;` to `;` preceded by a space or tab. This means JScript command values containing semicolons (e.g. `Command=script: a; b`) no longer get truncated. Existing INI files are unaffected because all built-in comments already used the spaced form. (`08ca1c3`)
- Removed the automatic migration of built-in filter definitions. Users who upgraded from an older version and want the new `script:` filters should delete their `RichEditor.ini` and let the editor recreate it with current defaults. (`98e4f76`)
- Filter and template menus now show localized category names (e.g. the Czech translation of "Transform") when a `Category.cs` or `Category.cs_CZ` key is present in the INI section. (`7da2866`)
- New output pane: filters with `Display=pane` send their results to a dedicated read-only pane below the editor instead of a message box. Press F6 to move focus between the editor and the pane, Ctrl+Shift+Delete to clear it, or right-click for a context menu. The pane supports `Pane=append` (add to existing content), `Pane=focus` (append and move focus), and `Pane=start` (append and place the cursor at the beginning of the new output). (`6fdeea6`, `a1bd0cc`)
- Both the editor and the output pane now have accessible names ("Editor" / "Output Pane") that screen readers can announce. (`df48a78`)
- Build size improvements: reorganised linker flags and enabled link-time optimisation in the MinGW build; removed redundant MSVC optimisation flags. (`a1fa7dd`)

---

## v2.8.0 (2026-03-05)

- Configurable RichEdit library: a new `RichEditLibraryPath` INI setting lets you load a specific RichEdit DLL (e.g. the one shipped with Microsoft Office) instead of the system default. The editor detects the DLL's version automatically and picks the best window class, with a fallback chain from the newest class down to the oldest. An optional `RichEditClassName` override is available if auto-detection chooses the wrong class. (`5547256`, `589d029`, `3c5b8ab`, `891096e`, `51d3f84`, `805b5a0`, `40eff75`, `fa92ce8`)
- The About dialog now shows the loaded RichEdit version, DLL filename, and full path for troubleshooting. It also lists the editor's key features and has correct version numbers in both English and Czech. (`891096e`, `fbde613`)
- Template system: 15 default Markdown templates with keyboard shortcuts (Ctrl+1/2/3 for headings, Ctrl+B/I for bold/italic, etc.), variable expansion (`%cursor%`, `%date%`, `%selection%`, `%clipboard%`, etc.), categorised menus filtered by the current file type, and full English/Czech localisation. Templates are defined in the INI file and can be customised or extended without recompilation. (`4015e90`, `d3af9ef`, `9bf4d27`, `87ff888`, `efd92fb`, `6b8d020`)
- File → New is now a submenu offering "Blank Document" plus one entry per template file type (e.g. "Markdown Document"). Choosing a template type creates a new untitled document pre-filled with the template content, with the cursor placed at the `%cursor%` marker. Ctrl+N still creates a blank document. (`87ff888`)
- Ctrl+Shift+T opens a quick-pick template popup at the text cursor, showing a flat list with category headers. The Tools → Insert Template menu remains a cascading submenu. (`3535257`, `10b9236`)
- Fixed template localisation not working on systems with full locale codes (e.g. `cs_CZ`): the editor now tries the full locale first, then the language-only key (e.g. `Name.cs`), before falling back to English. (`b7d3ca3`)
- File dialogs now group extensions by template category (e.g. "Markdown Files (*.md)") instead of showing raw uppercase extensions. Multiple extensions in the same category are combined. The "Text Files" and "All Files" filters are always present. Save As correctly preselects the filter matching the current document type, even for untitled files. (`9767392`, `3283c4d`, `3f82f96`, `3709409`, `9d0a0d4`, `60be69c`, `b9efb4d`)
- Templates and filters without a `Category` field now appear at the root level of their respective menus instead of being grouped under a "General" submenu. (`d5f03a5`, `07ea4c2`)
- Find dialog (Ctrl+F, F3, Shift+F3) with 20-item search history, escape sequence support (`\n`, `\t`, `\xNN`, `\uNNNN`), match case, whole word, and bidirectional search. Checkbox states and history persist across sessions. (`6e4e34b`, `6249112`)
- Replace dialog (Ctrl+H) with Replace Next and Replace All. Replace All uses a fast single-pass in-memory algorithm — typically instant even on large documents. Supports `%0` (matched text) and `%%` (literal percent) placeholders in the replacement string. Whole-word Replace All has been optimised from ~1.5 seconds to ~50 ms. A single Undo step reverts the entire Replace All. (`5f07330`, `7d284be`, `8ed0df7`, `e358e29`, `84bff49`, `508256e`, `c949c83`, `f3a8774`, `e0d86e1`)
- Configurable date/time formatting: six built-in variables (`%shortdate%`, `%longdate%`, etc.) plus two user-configurable ones (`%date%` and `%time%`) whose formats are set via `DateFormat` and `TimeFormat` INI settings. F5 now uses the `DateTimeTemplate` setting, which defaults to `%date% %time%` so it automatically respects your configured formats. Custom Windows format strings (e.g. `yyyy-MM-dd`, `HH:mm`) and literal text in single quotes are supported. (`2cb98cd`, `e9e804e`, `b3bb623`)
- Read-only mode: toggle via File → Read-Only or launch with `/readonly` on the command line. Disables saving, editing, undo/redo, insert/REPL filters, and template insertion. Display and clipboard filters still work. A `[Read-Only]` indicator appears in the title bar, and autosave is automatically suppressed. (`65e70f0`, `cccb3c5`)
- Fixed the MRU (recently used files) list not updating when files were opened via the command line. (`02eff9f`)
- Word wrap can now be toggled live without recreating the editor control. Fixed status bar line/column display on RichEdit 8+ where wrap-off mode reported incorrect values due to internal visual segmentation. (`c5867a0`)
- Go to Line dialog (Ctrl+G). (`1d28d50`)
- Bookmark navigation: set or clear a bookmark on the current line, then jump between bookmarks. Bookmarks persist across sessions. (`2a7f395`)
- The "Speak Text" default filter example has been replaced with "Auto Indent". Default PowerShell filter examples no longer use semicolons, avoiding truncation by the INI comment delimiter. (`38111d8`)
- Elevated save: when a normal save fails due to insufficient permissions (e.g. editing a system file), the editor automatically retries with a UAC administrator prompt. The save-on-close and Save As flows also support this elevation. (`4be5272`)
- Fixed autosave firing during the Find/Replace dialog and other internal dialogs. Autosave on focus loss now only triggers when switching to a different application, not when an internal dialog opens. (`8cff22b`)
- INI values are now cached in memory; disk writes happen only on exit or critical saves. Find/Replace checkbox options save immediately on toggle; history saves only on an actual search or replace action, eliminating a half-second delay when closing the dialog. (`dc6e107`, `e1589f8`)
- The session resume save prompt is now properly localised in Czech. (`d685da0`)
- Large-file performance: a new `DetectURLs` INI setting (default 1) lets you disable URL auto-detection entirely, which avoids cursor lag in very large documents. Replaces the previous automatic threshold approach, which had timing issues. (`c77a7ac`, `ee14113`)
- Status bar performance on large files: replaced a slow line-counting method with a cached index, reducing per-keystroke delays from seconds to near-instant on files with tens of thousands of lines. (`9bc6d15`, `893ffd2`)
- Added MSVC build system (`build_msvc.bat`) producing ~18 % smaller executables than MinGW. Both builds are functionally identical. (`3dafb97`)

---

## v2.7.0 (2026-01-14)

- Fixed the `[Resumed]` indicator not clearing when loading a new file, discarding changes, or saving. (`31338c7`, `7f3bf63`, `75b83fa`)
- Command-line files now take priority over resume files. The resumed content is preserved for the next launch without arguments. (`e35f7f8`)
- Fixed a resume-file bug where the last line could lose characters, caused by duplicated file-writing code that miscalculated the byte count. (`a3a8f1f`)
- Fixed the resume file being deleted when the user pressed Cancel on the close prompt — the editor stays open but the safety net was removed. (`8e03881`)
- Fixed several resume-related regressions: autosave was not clearing the modified indicator, the MRU list showed temporary file paths, and re-launching the editor multiple times could lose the resumed content. (`1505022`)
- Fixed a filter name display bug where only the first letter appeared in the status bar and error dialogs (a text-formatting issue specific to the MinGW build). All filter-related messages are now fully localised. (`a46a670`, `b8512d8`)
- The editor window now sizes itself to 80 % of the available screen area (excluding the taskbar) instead of a fixed 800 × 600. (`b0502c3`)
- Fixed Edit menu Cut/Copy/Paste items always appearing enabled even when no text was selected or the clipboard was empty. They now grey out correctly, matching the context menu. (`fa3ae0d`)
- Fixed autosave appearing to do nothing: the file was saved to disk but the title bar kept showing the unsaved-changes asterisk. (`f00df6e`)
- Fixed an autosave infinite loop when saving a file with no write permission — the error dialog triggered another autosave attempt, which failed again, endlessly. (`ba35b2a`)

---

## v2.6.0 (2026-01-09)

- URL auto-detection improvements: fixed URLs with query parameters (`?`, `=`, `&`) being truncated at special characters; fixed trailing punctuation (periods, spaces) being included in the URL; restored fast detection in large documents by narrowing the scan area; and fixed URLs not being clickable immediately after loading a file (they previously required editing near the URL first). (`64d56e9`, `97890b4`, `30f5765`, `14bf0e4`, `7cc2e8d`, `af7f5fc`)
- The editor now warns when the undo buffer is full and asks whether you want to continue editing or close. (`3e968fa`)
- Fixed a critical bug where autosave silently saved the file before you could respond to the "Save changes?" prompt on close, making the No and Cancel buttons ineffective. (`402b723`, `9c09a02`)
- `/nomru` command-line option: opens a file without adding it to the recent-files list — useful for file associations where you view temporary or reference files. (`bb6198a`)
- `SelectAfterPaste` setting (default off): when enabled, pasted text is automatically selected so you can immediately press an arrow key to jump before or after the pasted block. (`4f94681`)
- Interactive mode (REPL): start a shell session (PowerShell, Bash, Python, etc.) inside the editor with Ctrl+Shift+I. Commands are sent on Enter; output appears inline. Includes prompt detection, error stream capture, ANSI colour-code stripping, and an example WSL Bash filter. Close the session with Ctrl+Shift+Q. (`b5d4bba`, `94cef0b`, `8f7e855`, `a45a108`, `0b388d0`, `2de1435`, `2dd7bcd`, `d3a26e3`, `ac04681`)
- Fixed REPL mode showing an unwanted exit notification when the user intentionally closed the session. (`aa98fab`)
- Fixed REPL keyboard shortcuts (Ctrl+Shift+I/Q) being swallowed by the editor control instead of reaching the menu. (`a9ffe01`, `c516c88`)
- Windows shutdown and log-off handling: the editor now prompts to save unsaved changes when the system shuts down, using the system's "app is blocking shutdown" mechanism to prevent a timeout while you respond. Previously the editor either blocked shutdown silently or closed without saving. (`fd15b4e`, `e775b39`, `ed76733`, `7236abd`)
- Session resume: unsaved work is automatically preserved during Windows shutdown, power loss, or normal close (if `AutoSaveUntitledOnClose=1`). On the next launch, the editor recovers the content and shows a `[Resumed]` indicator in the title bar. The resume file is stored in the system temp directory and cleaned up after an explicit save. (`8eb21ba`)

---

## v2.1.0 (2025-12-19)

- URL auto-detection: URLs (http, https, ftp, mailto, file, etc.) are highlighted with a blue underline. Click to open, press Enter when the cursor is inside a URL, or right-click for "Open URL" and "Copy URL" context menu items. (`4ae175c`, `fc6725b`, `e9c1373`, `b5ceeee`, `7a8314a`)
- Most Recently Used (MRU) file list: the File menu shows up to 10 recent files, with the most recent at the top. (`c157fb2`)
- Command-line argument support: pass a filename to open it on launch. Works with file associations, drag-and-drop, and the Send To menu. (`318304e`)
- Unicode surrogate pair support in the status bar: characters outside the Basic Multilingual Plane (e.g. emoji) now display their full code point instead of showing the raw surrogate pair values. (`2d98ce3`)
- Accessibility: a `ShowMenuDescriptions=1` INI setting adds descriptive text to filter and context menu items so screen readers announce what each filter does. Eight built-in example filters now demonstrate all action types (insert, display, clipboard, none). (`3e04f19`)
- Context-aware Undo/Redo menu labels: the Edit menu now shows what will be undone (e.g. "Undo Typing", "Undo Paste") instead of a generic "Undo", matching the style of word processors. (`7538f90`, `36bece9`)
- Tab-aware column calculation: the status bar column number now accounts for tab stops (configurable via `TabSize` INI setting, default 8). (`d1146fb`)
- Localised file dialogs and status bar strings (line/column labels, character display, file type filters) in Czech. (`39fbb1c`, `7d15ae5`, `94078dd`, `9a40520`)
- Localised all error messages, filter names, and user-facing strings in Czech. The "Untitled" document name is also translated. (`c5d40b9`, `dc9ec92`, `0a78a09`)
- External filter system: run any command-line tool on the selected text (or current line) and choose how the result is handled — replace the selection, append below, copy to clipboard, or display in a message box. Filters are defined in the INI file under numbered sections with a command, category, and description. Up to 100 filters supported. (`4df2f0e`, `2e83c50`, `8cf3904`, `cbb9b1e`)
- Filter persistence: the last-selected filter is remembered across sessions. The filter name appears in the status bar. (`88d088e`)
- Replaced the legacy INI file API with a custom reader/writer that supports UNC network paths. (`f8c32cd`, `88d088e`)
- Application settings are now read from a `RichEditor.ini` file next to the executable. A default file with sensible settings and documentation comments is created on first run. (`9d84743`, `4594770`)
- Autosave: configurable timer interval (default 5 minutes) plus automatic save when the editor loses focus. Only fires when the document has unsaved changes and a filename. (`f2980d7`, `6068d4e`)
- Word wrap toggle (Ctrl+W) with a View menu checkmark. When wrap is on, the status bar shows both the visual (wrapped) position and the physical (unwrapped) position. Wrap state is respected on startup. (`076f43f`, `8445da0`, `1bf3084`, `a447167`)
- Time/date insertion (F5): inserts the current date and time in the system locale's short format. (`cfb7979`)
- Character at cursor in status bar: shows the character, its decimal value, and hex code point. (`b07f649`)
- Czech localisation: all menus, dialogs, and string resources available in Czech. The editor builds a single universal executable that automatically selects the language based on the Windows system setting. (`b77bc7d`, `3ed0708`, `20caba9`)
- Comprehensive version resource (company, description, copyright) visible in Windows file properties. (`ab33568`)

---

## v1.0.0 (2025-12-04)

- Initial release: Win32 text editor built on the RichEdit control with UTF-8 file I/O, a status bar showing line/column/character information, File menu operations (New, Open, Save, Save As with save prompts), Edit menu operations with modified-state tracking, and an About dialog. (`1d06689`, `8e62c01`, `10c8704`, `07d6d1b`, `debdb6d`, `37e0972`, `7ffc26f`, `f744b54`, `c491950`)
- MinGW-w64 build system with MXE cross-compilation support. (`cba53f9`, `2360742`)
- Filter system architecture stubs for future Phase 2 implementation. (`038fd21`)

