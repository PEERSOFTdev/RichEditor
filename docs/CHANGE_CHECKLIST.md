# Change Checklist

Use this checklist after code changes. It is meant to keep docs and behavior in sync.

## Documentation Updates

* If user-facing behavior changed, update:

  * README.md
  * docs/USER\_MANUAL\_EN.md
  * docs/USER\_MANUAL\_CS.md

* If developer/agent behavior changed, update:

  * AGENTS.md
  * docs/notes/AGENTS\_APPENDIX.md (if deeper context is needed)
  * commit message guidance if process expectations were updated

* If INI settings/defaults changed, update:

  * README.md (configuration section)
  * both user manuals
  * default INI documentation in CreateDefaultINI (if applicable)

* If command-line options changed, update README + both manuals.
* If menu labels, dialogs, or shortcuts changed, update manuals and resources.
* If a phase is completed or a new phase is planned, update:

  * docs/PHASES.md
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
