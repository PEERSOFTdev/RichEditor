# Change Checklist

Use this checklist after code changes. It is meant to keep docs and behavior in sync.

## Documentation Updates

* If user-facing behavior changed, update:

  * README.md
  * Reference.md (phase entry under Features + Usage section if applicable)
  * docs/USER\_MANUAL\_EN.md
  * docs/USER\_MANUAL\_CS.md

* If developer/agent behavior changed, update:

  * AGENTS.md
  * docs/notes/AGENTS\_APPENDIX.md (if deeper context is needed)
  * commit message guidance if process expectations were updated

* If implementing a user request (from `docs/requests/`):

  * Set the `UR-NNN_*.md` status to `Done`
  * Update the index in `docs/requests/README.md`

* If INI settings/defaults changed, update:

  * README.md (configuration section)
  * Reference.md (configuration section)
  * both user manuals
  * default INI documentation in CreateDefaultINI (if applicable)

* If command-line options changed, update README, Reference.md + both manuals.
* If menu labels, dialogs, or shortcuts changed, update manuals and resources.
* If a phase is completed or a new phase is planned, update:

  * docs/PHASES.md
  * Reference.md (add/update phase entry under Features)
  * docs/notes/ (plan or final status doc)

## UI/Accessibility

* Keep menu labels screen-reader friendly.
* Keep English and Czech resources in sync.
* Avoid owner-drawn menus or non-standard UI that breaks screen readers.

## Build/Quality

* Avoid committing code containing compiler warnings
* Run a build when behavior or resources change.

## Repo Hygiene

* Do not commit generated files (ini, exe, obj).
* Keep docs in docs/ and notes in docs/notes/.

## Version Bump (human-initiated only)

* Only bump the version when a human developer explicitly instructs it.
* Update all locations in the same commit (see Version Number Management in `AGENTS.md`):
  * `FILEVERSION` and `PRODUCTVERSION` quads in `src/resource.rc`
  * `VALUE "FileVersion"` and `VALUE "ProductVersion"` in both language blocks
  * `LTEXT "RichEditor vX.Y.Z"` in both `IDD_ABOUT` dialog definitions (EN + CS)
  * `README.md`, `Reference.md`, and both user manuals if they reference the version
