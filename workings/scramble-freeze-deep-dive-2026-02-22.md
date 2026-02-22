# Scramble Freeze Deep Dive — 2026-02-22

## The Bug

After Scramble receives an async LLM response and displays it, the **entire TUI freezes**. No clicks, no typing, no window moves. This has persisted through 4 fix attempts.

## What We Know For Certain

1. The async call works: `exit=0 result_len=45` — response arrives fine
2. The response IS displayed: user sees Scramble's answer on screen
3. The freeze happens AFTER display, not during the async call
4. Wib&Wob Chat does NOT freeze (same app, different async mechanism)
5. The ANSI sanitizer was added but freeze persists — so it's NOT terminal corruption

## Fix Attempts So Far (all failed to fix it)

1. **Moved callback out of idle()** — callback now posts `cmScrambleReply` event via `putEvent()`, handled in `handleEvent()`. Freeze persists.
2. **ANSI sanitizer** — strips escape sequences before rendering. Freeze persists.
3. **markCalled() ordering** — moved after startAsync success. Irrelevant to freeze.

## What's Different Between Scramble and Wib&Wob

| Aspect | Wib&Wob Chat | Scramble |
|--------|-------------|----------|
| Async mechanism | SDK background thread → streamQueue → poll() delivers chunks | popen + O_NONBLOCK → poll() in idle() → putEvent cmScrambleReply |
| Delivery | poll() called from TWibWobWindow timer (cmTimerExpired) | poll() called from TTestPatternApp::idle() |
| UI update | Callback updates messageView from timer handler | deliverScrambleReply() called from handleEvent(cmScrambleReply) |
| View ownership | TWibWobWindow owns its views | TScrambleWindow owns views, but TTestPatternApp owns the engine |
| Timer | TWibWobWindow has its own pollTimerId timer | TScrambleView has its own timerId (100ms for idle poses/bubble fade) |

## Key Question: Is the TUI Actually Frozen or Just Visually Stuck?

This is critical. If the event loop is still running but the terminal is corrupted, that's different from a true deadlock/infinite loop.

**Evidence for "visually stuck":**
- The ANSI sanitizer was supposed to fix this but didn't
- But the sanitizer only catches ESC sequences — what about other control chars?
- `strings` on the log shows binary data — something non-text is in the output

**Evidence for "truly frozen":**
- User can't interact AT ALL — not just visual glitches
- Wib&Wob Chat works fine in the same session before opening Scramble

## Hypotheses To Test

### H1: pclose() blocks in poll() or the event cascade

`ScrambleHaikuClient::poll()` calls `pclose(activePipe)` on the main thread when EOF is detected. `pclose()` calls `waitpid()` which should return immediately since the child already exited (we got EOF). BUT: if the child process spawned its OWN children (claude CLI spawns node, which spawns other processes), `pclose` might wait for the shell to reap them.

**Test:** Add timing around pclose:
```cpp
auto t0 = std::chrono::steady_clock::now();
int exitCode = pclose(activePipe);
auto t1 = std::chrono::steady_clock::now();
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
fprintf(stderr, "[scramble] pclose took %lldms exit=%d\n", ms, exitCode);
```

### H2: putEvent() from idle() drops events or causes re-entrancy

Turbo Vision's `putEvent()` has a single-slot pending event buffer. If something else already put an event, ours gets dropped. Worse: if the event IS delivered but causes re-entrancy in the event loop (idle() → poll() → callback → putEvent → handleEvent → drawView → ...), we could deadlock.

**Test:** Add logging at every step:
```cpp
fprintf(stderr, "[scramble] callback: about to putEvent cmScrambleReply\n");
// ... putEvent ...
fprintf(stderr, "[scramble] callback: putEvent done\n");
// In handleEvent:
fprintf(stderr, "[scramble] handleEvent: cmScrambleReply received\n");
// In deliverScrambleReply:
fprintf(stderr, "[scramble] deliverScrambleReply: starting\n");
fprintf(stderr, "[scramble] deliverScrambleReply: say() done\n");
fprintf(stderr, "[scramble] deliverScrambleReply: addMessage() done\n");
fprintf(stderr, "[scramble] deliverScrambleReply: complete\n");
```

### H3: Scramble's own timer fires during/after delivery and triggers something bad

TScrambleView has a 100ms timer that:
- Counts down bubble fade
- Rotates idle poses
- Calls `say()` with idle observations

If the timer fires DURING `deliverScrambleReply()` or immediately after, it could:
- Call `say()` which calls `drawView()` while we're already in a draw
- Interact badly with the event dispatch

**Test:** Log timer events:
```cpp
// In TScrambleView::handleEvent timer handler:
fprintf(stderr, "[scramble-timer] tick bubbleVisible=%d idleCounter=%d\n", bubbleVisible, idleCounter);
```

### H4: Focus/selection issue after delivery

`deliverScrambleReply()` calls `say()` and `addMessage()`. If either of these triggers a focus change (e.g., the input view trying to grab focus, or the window trying to select itself), it could disrupt the event loop.

**Test:** Check if any select()/focus() calls happen during delivery.

### H5: The idle() loop itself enters an infinite spin

If `scrambleEngine.poll()` somehow never returns, or returns but the event loop gets stuck in a tight idle() cycle without processing events.

**Test:** Add a counter to idle():
```cpp
static int idleCount = 0;
if (++idleCount % 1000 == 0)
    fprintf(stderr, "[idle] tick %d\n", idleCount);
```

## Recommended Approach

Add ALL the logging from H1-H5 in one pass. Reproduce. Read the log. The answer will be obvious.

## Specific Lines To Instrument

- `app/scramble_engine.cpp:359` — pclose timing
- `app/test_pattern_app.cpp` callback in wireScrambleInput — putEvent logging
- `app/test_pattern_app.cpp` cmScrambleReply handler — entry/exit logging  
- `app/test_pattern_app.cpp:1607` deliverScrambleReply — step-by-step logging
- `app/test_pattern_app.cpp:2554` idle() — tick counter
- `app/scramble_view.cpp` timer handler — tick logging
