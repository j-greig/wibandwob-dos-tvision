Findings written to `workings/scramble-async-freeze-diagnosis-2026-02-22.md`.

Primary diagnosis in the report: the async Scramble callback is invoked from `TTestPatternApp::idle()` and immediately calls Scramble view methods that synchronously `drawView()`, which is consistent with the freeze happening exactly when the first async response is rendered.

I also documented secondary async issues:
- blocking `pclose()` in `poll()` / `cancelAsync()`
- `markCalled()` before `startAsync()` success
- brittle non-blocking `FILE*` + `fread()` polling
- fixed `/tmp/scramble_haiku.json` temp file path collision risk

If you want, I can implement the minimal fix next (queue response + post a UI event, then update views in `handleEvent()` instead of inside the `idle()` callback).