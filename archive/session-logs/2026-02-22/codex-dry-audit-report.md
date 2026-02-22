# TUI DRY / Refactor Audit (`app/`)

## High-Priority Opportunities

1. **JSON string escaping is duplicated across multiple production paths (and repeated inside one function)**
   - Files/lines: `app/command_registry.cpp:83`, `app/scramble_engine.cpp:94`, `app/test_pattern_app.cpp:3301`, `app/llm/providers/claude_code_sdk_provider.cpp:29`, `app/llm/providers/anthropic_api_provider.cpp:388` (loops at `:399`, `:439`, `:475`)
   - Why it matters: Same logic is implemented 4+ times; `AnthropicAPIProvider::buildSimpleRequestJson` repeats near-identical escape loops 3 times in one function.
   - Effort: **S-M**
   - Suggested approach: Extract shared `json_escape_string(...)` utility (e.g. `app/json_util.h/.cpp`) and reuse everywhere. Then simplify `buildSimpleRequestJson()` to call it for system/history/user/tool-result fields.

2. **Ad hoc JSON string parsing/unescape logic is duplicated**
   - Files/lines: `app/browser_view.cpp:905`, `app/browser_view.cpp:962`, `app/llm/providers/claude_code_sdk_provider.cpp:52`, `app/llm/base/auth_config.cpp:177`
   - Why it matters: Multiple custom parsers/unescapers for string fields; easy to drift/bug on escapes/unicode handling.
   - Effort: **M**
   - Suggested approach: Extract a tiny shared helper for “extract JSON string field(s)” (or move to a real JSON parser in LLM/browser paths if acceptable).

3. **`TScrambleView` still has a local `wordWrap()` despite shared `text_wrap`**
   - Files/lines: `app/scramble_view.cpp:223`
   - Why it matters: This is the same class of duplication as the recent `wrapText` extraction.
   - Effort: **S**
   - Suggested approach: Replace `TScrambleView::wordWrap()` with `wrapText()` from `app/text_wrap.h` (or document why bubble wrapping intentionally differs).

4. **Remaining `popen()` sites are inconsistent with stdin redirect rule**
   - Findings:
     - Only one explicit `</dev/null` seen: `app/scramble_engine.cpp:259`
     - `popen()` call sites without stdin redirect (inspected): `app/text_editor_view.cpp:401`, `app/browser_view.cpp:689`, `app/scramble_engine.cpp:134`, `app/scramble_engine.cpp:189`, `app/scramble_engine.cpp:333` (CLI path depends on builder), `app/clipboard_read.cpp:30`, `app/llm/base/auth_config.cpp:154`, `app/llm/providers/anthropic_api_provider.cpp:103`, `app/llm/providers/anthropic_api_provider.cpp:339`
   - Effort: **S-M**
   - Suggested approach: Centralize shell command suffixing via helper (e.g. `append_popen_redirects(cmd)`), enforce `</dev/null 2>/dev/null` for all shell-backed subprocesses.

5. **Remaining `fork()` in app code (`NodeBridge`)**
   - Files/lines: `app/llm/providers/claude_code_sdk_provider.cpp:109`
   - Why it matters: Conflicts with your established PTY/process pattern direction.
   - Effort: **M-L** (depends on whether PTY semantics are actually needed here)
   - Suggested approach: Extract/standardize process spawn helper; migrate this path to repo-canonical spawn API (or explicitly document why pipe+fork is retained).

## Medium-Priority Refactor Opportunities

1. **`TTextFileView` manually reimplements scrolling + scrollbar sync**
   - Files/lines: `app/frame_file_player_view.h:117`, `app/frame_file_player_view.cpp:414`, `app/frame_file_player_view.cpp:479`, `app/frame_file_player_view.cpp:538`
   - Why it matters: Manual `topLine`, key handling, scrollbar params, broadcast handling duplicate `TScroller` responsibilities.
   - Effort: **M**
   - Suggested approach: Convert text content pane to `TScroller` (or embed a `TScroller` child) and move scroll state to `delta`/`setLimit`.

2. **`BrowserWindow` is a raw `TView` with manual scroll state/arrow handling**
   - Files/lines: `app/browser_window.h:22`, `app/browser_window.cpp:28`
   - Why it matters: Matches the anti-pattern you called out (`scrollY` + key handling on raw `TView`); also appears unused.
   - Effort: **S**
   - Suggested approach: Either delete as dead prototype or convert to `TScroller` before reuse.

3. **Timer lifecycle boilerplate duplicated across many animated/generative views**
   - Files/lines (examples): `app/animated_blocks_view.cpp:56`, `app/animated_gradient_view.cpp:46`, `app/game_of_life_view.cpp:101`, `app/generative_orbit_view.cpp:54`, `app/generative_cube_view.cpp:68`, `app/generative_mycelium_view.cpp:54`, `app/generative_monster_cam_view.cpp:66`, `app/generative_monster_portal_view.cpp:63`, `app/generative_monster_verse_view.cpp:66`, `app/generative_torus_view.cpp:40`, `app/generative_verse_view.cpp:106`, `app/token_tracker_view.cpp:96`
   - Why it matters: Repeated `startTimer()/stopTimer()`, restart-on-speed-change, expose/hide behavior.
   - Effort: **M-L**
   - Suggested approach: Introduce a small `TAnimatedTimedView` base/mixin for timer ownership and restart semantics.

4. **Repeated line-buffer draw pattern (`lineBuf.resize(W)` + `setCell` + `writeLine`)**
   - Files/lines (examples): `app/animated_blocks_view.cpp:80`, `app/animated_gradient_view.cpp:83`, `app/game_of_life_view.cpp:221`, `app/snake_view.cpp:194`, `app/quadra_view.cpp:294`, `app/deep_signal_view.cpp:559`, `app/rogue_view.cpp:643`
   - Why it matters: Common rendering scaffold repeated across grid/cell views.
   - Effort: **M**
   - Suggested approach: Shared helper/base for row-buffer management (`ensureLineBufWidth`, `drawRows`) without constraining per-view render logic.

5. **Shell escaping logic is duplicated/inconsistent**
   - Files/lines: `app/text_editor_view.cpp:382`, `app/browser_view.cpp:670`, `app/scramble_engine.cpp:121`, `app/scramble_engine.cpp:126`, `app/scramble_engine.cpp:250`, `app/scramble_engine.cpp:255`
   - Why it matters: Different quoting rules for different contexts, repeated loops, easy correctness/security drift.
   - Effort: **M**
   - Suggested approach: Extract explicit shell quoting helpers (`single_quote_shell_arg`, `double_quote_shell_arg`) and reuse.

## Low-Priority Observations

1. **Common message/wrapped-line data shapes duplicated**
   - Files/lines: `app/wibwob_view.h:33` (`ChatMessage`), `app/scramble_view.h:117` (`ScrambleMessage`), `app/scramble_view.h:136`, `app/wibwob_view.h:73`, `app/wibwob_scroll_test.h:98` (`WrappedLine`)
   - Effort: **S-M**
   - Suggested approach: Share only if interfaces converge; otherwise keep local structs (current duplication is small and domain-specific).

2. **ANSI16 palette + quantizer duplicated in image viewer and POC**
   - Files/lines: `app/ascii_image_view.cpp:48`, `app/ascii_image_view.cpp:53`, `app/tvision_ascii_view_poc.cpp:128`, `app/tvision_ascii_view_poc.cpp:169`
   - Effort: **S**
   - Suggested approach: Extract only if POC code is still active; otherwise leave as-is.

3. **Naming/style inconsistency around the same utility concept**
   - Files/lines: `jsonEscape`, `json_escape`, `escapeJsonString` across files above
   - Effort: **S**
   - Suggested approach: Standardize naming once shared utility exists.

## Subprocess / Spawn Rule Check Summary

- `popen()` sites found in `app/` + `app/llm`: **9**
- `forkpty()` calls found: **0**
- `fork()` calls found: **1** (`app/llm/providers/claude_code_sdk_provider.cpp:109`)
- `</dev/null` redirect found at inspected command-build sites: only `app/scramble_engine.cpp:259` (async CLI builder)

## Notes

- No broad duplicated UTF-8 length helper family found in `app/` (mostly localized UTF-8 handling).
- ANSI sanitization/strip helper duplication appears minimal (mainly `app/scramble_view.cpp:21`).

If you want, I can turn this into a concrete refactor sequence (smallest-to-largest wins first) and start with the lowest-risk pair: shared JSON escaping + `popen` redirect helper.