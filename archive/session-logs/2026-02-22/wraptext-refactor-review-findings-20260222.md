# wrapText DRY Refactor Review (2026-02-22)

## Summary

No blocking correctness issues found in the shared `wrapText()` extraction or the `TTransparentTextView` `TScroller` usage pattern.

## Findings

1. Medium: Visible wrapping behavior changed in `wibwob_scroll_test` (hard-break -> word-boundary wrap)
- `app/wibwob_scroll_test.cpp:142`, `app/wibwob_scroll_test.cpp:176`, `app/wibwob_scroll_test.cpp:315` now call shared `wrapText()`.
- Previous test-view implementations hard-broke at fixed width; shared `wrapText()` prefers word boundaries with hard-break fallback (`app/text_wrap.cpp:42`-`app/text_wrap.cpp:59`).
- Expected effect: fewer lines for prose, different line breaks/scroll positions, and possibly changed visual expectations in the scroll test view.
- This is not a correctness bug, but it is a behavior change and could affect screenshots/manual comparisons.

2. Low: `TScrambleMessageView` empty/very-narrow wrap behavior changed slightly
- `app/scramble_view.cpp:459` now uses shared `wrapText()`.
- Old `simpleWordWrap()` returned an empty vector for `text.empty()` or `width <= 0`; shared `wrapText()` returns `{""}` for those cases (`app/text_wrap.cpp:10`-`app/text_wrap.cpp:13`, `app/text_wrap.cpp:32`-`app/text_wrap.cpp:34`).
- Expected effect: in extremely narrow layouts (or empty message content), scramble history may render an extra blank wrapped content line where it previously rendered none.

## Checklist Results

1. `text_wrap.h/cpp` correctness/completeness
- `empty text`: handled, returns one empty line (`app/text_wrap.cpp:32`-`app/text_wrap.cpp:34`, fallback at `app/text_wrap.cpp:68`-`app/text_wrap.cpp:69`).
- `newlines`: handled by segment split on `\n`, including consecutive/trailing newline cases (`app/text_wrap.cpp:19`-`app/text_wrap.cpp:26`, `app/text_wrap.cpp:63`-`app/text_wrap.cpp:66`).
- `long words`: handled by hard-break fallback when no usable space/tab break exists (`app/text_wrap.cpp:42`-`app/text_wrap.cpp:53`).
- `trailing \r`: stripped per segment (`app/text_wrap.cpp:28`-`app/text_wrap.cpp:30`).
- `width = 0`: handled, returns one empty line (`app/text_wrap.cpp:10`-`app/text_wrap.cpp:13`).

2. Old implementations removed / dead declarations removed
- Removed from `.cpp`: browser, scramble (`simpleWordWrap` + member wrapper), transparent text view, wibwob message view, and both scroll test views (A/B).
- Removed from headers: no remaining per-class `wrapText(...)` declarations in `app/wibwob_view.h`, `app/scramble_view.h`, `app/transparent_text_view.h`, `app/browser_view.h`, or `app/wibwob_scroll_test.h`.

3. Callers use shared `wrapText()`
- Remaining `wrapText(` symbols are only the shared declaration/definition and call sites in the affected `.cpp` files.
- No remaining `simpleWordWrap` symbols found.

4. `TTransparentTextView` and `TScroller` usage
- Class derives from `TScroller` and constructor accepts/passes scrollbars (`app/transparent_text_view.h:34`, `app/transparent_text_view.cpp:16`-`app/transparent_text_view.cpp:21`).
- `changeBounds()` calls `TScroller::changeBounds(...)` then rebuilds wrapped display lines (`app/transparent_text_view.cpp:87`-`app/transparent_text_view.cpp:91`).
- `draw()` uses `delta.y` and `delta.x` for vertical/horizontal scrolling (`app/transparent_text_view.cpp:128`, `app/transparent_text_view.cpp:137`).

5. Behavioral differences vs old implementations
- `scramble`: now gains hard-break fallback for long words and preserves explicit newline segmentation more directly; can change line layout (generally positive, but visible).
- `scroll_test`: now word-wraps at boundaries instead of always hard-breaking (visible, likely intended but should be acknowledged for any golden/screenshot expectations).
- `browser`: now also handles embedded newlines/CR consistently via shared helper (behavioral improvement).

6. CMakeLists update
- `app/text_wrap.cpp` added to target sources (`app/CMakeLists.txt:57`).

7. Include issues
- Shared helper included where used: `app/wibwob_view.cpp`, `app/scramble_view.cpp`, `app/transparent_text_view.cpp`, `app/browser_view.cpp`, `app/wibwob_scroll_test.cpp`.
- No obvious include/declaration gaps found in static review.

## Notes

- Review was static only (grep/diff/source inspection). I did not run a build/test pass in this review.
