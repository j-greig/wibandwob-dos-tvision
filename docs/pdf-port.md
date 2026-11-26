# PDF port

WibWob-DOS runs inside a PDF file: the same Turbo Vision app as the browser pages (`docs/web-port.md`),
linked as plain JavaScript (asm.js) and run by the PDF viewer's own JavaScript engine. The screen is
form fields; the keyboard is a text field plus on-page buttons; the mouse is a grid of invisible buttons.

The built file is committed as `pdf/wibwob-dos.pdf`. Open it in Chrome, Edge or Brave (their built-in
viewer, PDFium). Other viewers: see the table at the end.

## Build

```
kernel/build-pdf.sh            # --clean wipes build-pdf/ first; other arguments go to make_pdf.py
cp dist-web/wwdos.pdf pdf/wibwob-dos.pdf
```

Needs emsdk 6.0.13 (7.0.1's libc++ no longer pulls in `<cstdlib>`/`<algorithm>` transitively, and several
app and tvision sources stop compiling) and fontTools (`pip install fonttools`). The script configures
`kernel/` with `-DWWDOS_PDF=ON` into `build-pdf/`, which links every target with `-sWASM=0
-sENVIRONMENT=shell` instead of WebAssembly, then runs `kernel/tools/make_pdf.py` for `wwdos` and `hello`.
Linking the app through wasm2js takes about two minutes.

`make_pdf.py` options: `--no-mouse` (leave out the 2,000 click buttons), `--debug` (timer, key, click and
frame counts in the status line), `--size` (font size in points), `--font` (monospace TTF), `--draw-ms`.

## How it works

```
Turbo Vision ─▶ WasmGridDisplay ─cells─▶ Module.wwdosGrid ─▶ pdf-host.js ─▶ field.value per colour layer
yellow field keystrokes / key buttons / cell buttons ─▶ terminal bytes ─▶ Module.wwdosQueue ─▶ TermIO
timers: one app.setInterval (10 ms) runs every setTimeout the Emscripten output and the asyncify wait ask for
```

The platform layer is the browser one (patch 0002, `WasmGridDisplay` and the byte queue); nothing in tvision
or the app changed. `kernel/pages/pdf-host.js` is the host:

- **Globals.** PDFium gives ECMAScript, typed arrays, Promises, `app.setInterval`/`app.setTimeOut` and
  `Date`, and no WebAssembly, DOM, `setTimeout` or `performance`. The host defines `setTimeout`,
  `clearTimeout`, `performance.now`, `crypto.getRandomValues` and `console.log`. All timeouts run from one
  `app.setInterval` pump: chained `app.setTimeOut` calls stopped firing now and then in Chrome's viewer, which
  froze the app for good.
- **Screen.** Per row, 33 transparent text fields on the same rectangle: `r<y>.b0`..`b15` hold U+2588 FULL
  BLOCK where a cell's background is that colour, `r<y>.f0`..`f15` the characters drawn in that colour,
  `r<y>.c` the caret. Each field's colour is fixed in its `/DA`; colours are quantised to the 16 BIOS colours.
  Only fields whose text changed are set. The font is DejaVu Sans Mono, embedded as a Type0 font with a
  ToUnicode map (the standard Courier has no box drawing); its line height is the full block's height, and the
  block elements are stretched down a little so rows overlap instead of leaving seams. A character the font
  lacks (kaomoji, CJK) draws as `?`.
- **Keyboard.** The yellow field's keystroke action reads `event.change`; an empty change with a selection is
  Backspace, an empty change at the caret is Enter. A form field never reports arrows, Esc, Tab or F-keys,
  so those are buttons. Alt and Ctrl are sticky buttons that apply to the next key, click or button.
- **Mouse.** One invisible push button per cell sends an SGR press and release. Chrome's viewer runs the
  previous button's action again when the next mouse-down lands on another widget; the host holds each click
  for 40 ms and drops a repeat of the last cell that another widget's action follows. Clicks only: no drag,
  no right button, no wheel.
- **Sound** is off (`WWDOS_SOUND_OUT=off`); the TR-808 samples are not embedded.

## Measured

Headless Chromium 1243 (Playwright), Linux container:

- `pdf/wibwob-dos.pdf`: 2.75 MB (asm.js 9.8 MB before Flate), 2,856 fields.
- Script parse to first frame: 120-370 ms. A changed row costs a few ms to draw; a full repaint about 10-15 ms.
- No-JIT V8 runs a tight loop at about 30 M iterations a second, so heavy animated views are slower than in
  the browser page.

## What works (checked in headless Chromium)

- Boot to the desktop with the start-screen primers.
- Menus by click, Alt+letter (sticky Alt), Esc; menu items by click.
- Window menu > Text Editor; typing, Backspace, Enter into the editor.
- Ctrl+N (sticky Ctrl) opens a Test Pattern window.
- 16 colours, box drawing and shading.

Not checked yet: dialogs with many controls, the primer browser, animated views over minutes, window drag
(not possible: clicks only).

## Cut or missing in the PDF

Everything cut in the browser build (`docs/web-port.md` § Cut) plus: sound, mouse drag/right button/wheel,
physical arrow/Esc/F-keys (use the buttons), paste, glyphs outside DejaVu Sans Mono, colours beyond the 16.

## Viewers

| viewer | result |
|---|---|
| Chrome, Edge, Brave (PDFium) | runs |
| Firefox (PDF.js 4.10, tested as the generic viewer in Chromium) | draws layered fields and runs timers, but its QuickJS sandbox fails with "memory access out of bounds" while loading the 10 MB script, before the app starts |
| Adobe Acrobat / Reader | not tested. Its JavaScript engine predates `async`/`await` and `??=`, which the Emscripten output uses; it would need the glue transpiled to ES5 |
| macOS Preview, Apple Books, most mobile viewers | no PDF JavaScript: the page shows a static note saying to open it in Chrome |
