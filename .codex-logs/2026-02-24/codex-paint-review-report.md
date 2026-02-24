Structured markdown review report written to `paint-workspace-review-report.md`.

Key findings:
- No proven crash/corruption bug in the reviewed paths.
- Major functional regressions in workspace preview parsing (`bounds` schema mismatch, scans past `windows[]`, fragile brace matching).
- Paint window resize rebuild wipes canvas/state.
- Composite paint draw still blanks pixel data in `Text` mode.
- Paint command IDs `500–502` collide with existing `tvterm` commands.

If you want, I can patch the top 2-3 issues directly next.