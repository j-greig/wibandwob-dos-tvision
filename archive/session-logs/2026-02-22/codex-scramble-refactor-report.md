Implemented a Scramble-only fix for the post-async reply freeze and verified build/tests.

**Root Cause**
- Async delivery path was already main-thread safe (`cmScrambleReply`), but the reply text itself was still rendered unsanitized.
- `app/test_pattern_app.cpp:1607` delivers the reply, then renders it at:
  - `app/test_pattern_app.cpp:1615` (`TScrambleView::say(reply)`)
  - `app/test_pattern_app.cpp:1618` (`TScrambleMessageView::addMessage(...)`)
- `app/scramble_engine.cpp:569` (`voiceFilter`) modifies text style but does not strip control/ANSI bytes before callback (`app/scramble_engine.cpp:539`, `app/scramble_engine.cpp:541`).
- Scramble views were storing/rendering raw text directly:
  - `app/scramble_view.cpp:190` / `app/scramble_view.cpp:192`
  - `app/scramble_view.cpp:461` / `app/scramble_view.cpp:465`

This means hidden terminal control sequences from CLI/LLM output could corrupt terminal state during draw and make the TUI appear frozen right after the response is shown.

**Fix**
- Added Scramble-local text sanitizer in `app/scramble_view.cpp:20` (`sanitizeScrambleDisplayText`).
- Applied it at both render boundaries:
  - `app/scramble_view.cpp:192` (`bubbleText = sanitize...`)
  - `app/scramble_view.cpp:465` (`msg.text = sanitize...`)
- It strips ANSI/ESC sequences and unsafe control bytes, normalizes `\r`, and keeps normal text intact.

**Build / Test**
- `cd build && make -j8 test_pattern` ✅
- `cd build && ctest -R scramble --output-on-failure` ✅ (`scramble_engine` passed)

**Report File**
- Wrote structured markdown findings report to `scramble-freeze-fix-report.md`

If you want, next step is an interactive smoke test in the TUI with a reply known to include escape/control bytes to confirm the freeze is gone end-to-end.