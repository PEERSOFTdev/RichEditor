# Manual Guidelines

These rules keep the user manuals consistent, accurate, and maintainable.

## Scope

- The manuals are user-facing. Avoid implementation details unless they explain behavior.
- Use short, practical explanations with examples when helpful.
- Keep command-line and INI-only features in "Advanced" callouts.

## Consistency Rules

- **Update both manuals together:** `docs/USER_MANUAL_EN.md` and `docs/USER_MANUAL_CS.md`.
- **Keep the same section order and headings** (translated, but aligned).
- **Verify behavior against current code and default INI** before documenting.
- **Do not copy old notes** verbatim without validation.

## Terminology

- Follow UI strings in `src/resource.rc` for Czech terminology.
- **Czech wording must match `src/resource.rc` exactly.**

## Style

- Prefer paragraphs and short examples over long bullet lists.
- Use lists only when they clearly improve readability (shortcuts, key lists, variables).
- Use the same names as menu items and dialogs.
- Prefer simple language for users, with brief Advanced callouts when needed.

## Content Boundaries

- Do not describe future features; add them only when implemented.
- Keep long technical explanations in phase notes or developer docs.
- Avoid duplicating all INI comments; link users to the INI file itself.

## Common Advanced Callouts

- Command-line options
- INI-only settings and filters
- Escape sequences and placeholders
- Encoding and line endings
- REPL configuration
