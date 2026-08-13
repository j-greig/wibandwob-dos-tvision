# wwdos_app.cpp split plan (opus, 2026-08-13)

tl;dr: 6095-line monolith → real wwdos_app.h + 11 modules in 10 revertable
stages. Of ~60 friend api_* declarations, ONE survives (getAppRuntimeApiKey).
Everything else becomes public members + six small accessors
(nextWindowNumber, publishEvent, forgetWindow, syncWindowRegistry,
takeLastRegisteredWindowId, noteInput + scramble/chat/gallery getters).

Stages (M=mechanical/delegable, J=judgement):
0  M wwdos_commands.h — cm* constants (lines 178-290)
1  M windows/pattern_windows.{h,cpp} — TTestPattern*/TGradientWindow (630-751)
2  M ww_view_utils.h — ww_get_child_view template + findPrimerDir decl
3  J wwdos_app.h — class body verbatim (753-1040), friends intact, prove
     the include set closes; delete dead setKaomojiMood
4  J widen API surface: promote members public, add the six accessors,
     rewrite ~15 raw private accesses, delete friend lines (signature stage —
     test stubs in command_registry_test/scramble_engine_test must update in
     the SAME commit)
5  J→M declaration headers api_{windows,desktop,chat,paint,figlet}.h +
     workspace_io.h replace extern blocks in command_registry/api_ipc/
     window_type_registry (catches arity drift; declare ALL overloads:
     api_spawn_backrooms_tv, api_open_animation_path, api_spawn_test/gradient)
6  M move bodies: a desktop, b paint, c figlet, d chat, e windows (largest last)
7  M a workspace_manager_dialog.cpp (4529-4867), b workspace_io.cpp
8  M app_chrome.{h,cpp} — menubar/statusline/menus; delete getAppIpcStatus shim
9  M optional app_spawn.cpp (member spawns 1900-2500)
10 J optional handleEvent split (779 lines) — design task, NOT code motion

Hard gotchas:
- command_registry.cpp / api_ipc.cpp / window_type_registry.cpp must NEVER
  include wwdos_app.h (tests define their own class TWwdosApp {};) — only
  the stage-5 declaration headers with forward decls.
- findPrimerDir: definition stays in wwdos_app.cpp (two test TUs stub it).
- USE_CONTINUOUS_PATTERN: extern in test_pattern.h, define once, never static.
- getPalette stays put (cpAppColor macro dependency, positional entries).
- New sources into add_executable(wwdos...) NOT TV_COMMON_SOURCES.
- Menu char* leaks are deliberate (tvision owns strings) — don't fix in motion.
- Command-id constants triplicated (wibwob_background/figlet_text_view/
  scramble_view) — separate dedup commit; cmScrambleToggle must keep coming
  from scramble_view.h.

Full function→module map with line ranges lives in the git history of this
file (first commit) — regenerate anchors with grep before executing a stage;
the file drifts.
