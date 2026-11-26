/* WibWob-DOS inside a PDF: the host glue that runs as a document-level script.
 *
 * The PDF viewer's JavaScript (Chrome's PDFium: V8 with no JIT, no WebAssembly, no DOM) gives us
 * form fields, app.setTimeOut/app.setInterval and Date. This file supplies the few browser globals the
 * Emscripten output touches (setTimeout, performance, crypto, console), starts the app (asm.js, built
 * by kernel/build-pdf.sh), and draws its cell grid into text fields:
 *
 *   per screen row, one text field per colour layer, all on the same rectangle, transparent:
 *     b0..b15  background: U+2588 FULL BLOCK in that colour where a cell's background is it, else space
 *     f0..f15  foreground: the cell's character where its foreground is that colour, else space
 *     c        the text caret, an underline in bright yellow
 *   field names: r<row>.b<n>, r<row>.f<n>, r<row>.c
 *
 * Input: a keyboard field (k) whose keystroke action calls wwKey(event), on-page buttons for keys a
 * text field never reports (wwKeyName('Esc') etc.), and one invisible button per screen cell whose
 * mouse-up calls wwClick(x, y). Everything reaches the app as terminal bytes on Module.wwdosQueue, the
 * same queue the browser pages fill, parsed by Turbo Vision's TermIO.
 *
 * kernel/tools/make_pdf.py prepends `var WWPDF = {...}` (size, palette, glyph set) and appends the
 * Emscripten output (which defines WWDOS) and a final `wwStart();`.
 */
var wwDoc = this;
var wwG = (typeof globalThis === 'object') ? globalThis : this;

// ---------- browser globals the Emscripten output needs ----------
(function (G) {
  // One app.setInterval drives every timeout. Chained app.setTimeOut calls were seen to stop firing now and
  // then in Chrome's viewer, which froze the app (a lost wake-up never comes back); a pump cannot lose one.
  var timers = [], nextId = 1;
  G.__wwPump = function () {
    wwStats.fires++;
    var now = Date.now(), due = [], keep = [], i;
    for (i = 0; i < timers.length; i++) (timers[i].at <= now ? due : keep).push(timers[i]);
    timers = keep;
    due.sort(function (a, b) { return a.at - b.at || a.id - b.id; });
    for (i = 0; i < due.length; i++) {
      try { due[i].fn.apply(null, due[i].args); } catch (e) { wwLog('timer: ' + e + (e && e.stack ? ' ' + e.stack : '')); }
    }
    if (WW.dirty && now - WW.drawnAt >= (WWPDF.drawMs || 40)) wwDraw();
  };
  if (typeof G.setTimeout !== 'function') {
    G.setTimeout = function (fn, ms) {
      var id = nextId++;
      timers.push({ id: id, at: Date.now() + Math.max(0, ms | 0), fn: fn, args: Array.prototype.slice.call(arguments, 2) });
      return id;
    };
    G.clearTimeout = function (id) {
      for (var i = 0; i < timers.length; i++) if (timers[i].id === id) { timers.splice(i, 1); return; }
    };
  }
  if (typeof G.performance !== 'object' || !G.performance) G.performance = { now: function () { return Date.now(); } };
  if (typeof G.crypto !== 'object' || !G.crypto) {
    G.crypto = { getRandomValues: function (a) { for (var i = 0; i < a.length; i++) a[i] = (Math.random() * 256) | 0; return a; } };
  }
  // The WebAudio stream is turned off (WWDOS_SOUND_OUT=off); this only keeps a stray reference harmless.
  if (typeof G.window !== 'object') G.window = { addEventListener: function () {} };
})(wwG);

var wwLogText = [];
var wwStats = { fires: 0, keys: 0, clicks: 0 };
function wwLog(s) {
  wwLogText.push(String(s));
  if (wwLogText.length > 6) wwLogText.shift();
  try { var f = wwDoc.getField('log'); if (f) f.value = wwLogText.join('  |  ').slice(-600); } catch (e) {}
}
// console in a PDF viewer is Acrobat's (println), not the browser's: give Emscripten log/error.
(function (G) {
  var c = (typeof G.console === 'object' && G.console) ? G.console : {};
  var log = function () { wwLog(Array.prototype.join.call(arguments, ' ')); };
  try { if (typeof c.log !== 'function') c.log = log; if (typeof c.error !== 'function') c.error = log; if (typeof c.warn !== 'function') c.warn = log; } catch (e) {}
  try { if (typeof G.console !== 'object' || !G.console) G.console = c; } catch (e) {}
})(wwG);

// ---------- display ----------
var WW = {
  M: null,
  cols: WWPDF.cols, rows: WWPDF.rows,
  cur: null, shown: null,          // Uint32Array copies of the grid: latest from the app, last drawn
  caret: [-1, -1], shownCaret: [-1, -1],
  dirty: false, drawnAt: 0,
  fields: {}, values: {},
  colourOf: {},                     // 0xRRGGBB -> palette index
  glyph: {},                        // codepoint -> true when the embedded font draws it
  frames: 0, lastDrawMs: 0
};
(function () {
  var g = WWPDF.glyphs;
  for (var i = 0; i < g.length; i++) WW.glyph[g.charCodeAt(i)] = true;
})();

function wwField(name) {
  var f = WW.fields[name];
  if (f === undefined) { f = wwDoc.getField(name); WW.fields[name] = f; }
  return f;
}
function wwSet(name, v) {
  if (WW.values[name] === v) return;
  WW.values[name] = v;
  var f = wwField(name);
  if (f) f.value = v;
}

function wwNearest(rgb) {
  var c = WW.colourOf[rgb];
  if (c !== undefined) return c;
  var r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255, best = 0, bd = 1e9, P = WWPDF.palette;
  for (var i = 0; i < P.length; i++) {
    var p = P[i], dr = r - ((p >> 16) & 255), dg = g - ((p >> 8) & 255), db = b - (p & 255);
    var d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
    if (d < bd) { bd = d; best = i; }
  }
  WW.colourOf[rgb] = best;
  return best;
}

function wwTrimRight(a, n) { while (n > 0 && a[n - 1] === 32) n--; return n; }

// Build one row's layer strings and set the fields that changed.
function wwDrawRow(y) {
  var C = WW.cols, cells = WW.cur, base = y * C * 3, nc = WWPDF.palette.length;
  var bg = [], fg = [], usedB = {}, usedF = {}, x, k;
  for (k = 0; k < nc; k++) { bg[k] = null; fg[k] = null; }
  for (x = 0; x < C; x++) {
    var i = base + x * 3, cp = cells[i] & 0x7fffffff, b = wwNearest(cells[i + 2]), f = wwNearest(cells[i + 1]);
    if (!bg[b]) { bg[b] = new Array(C); for (k = 0; k < C; k++) bg[b][k] = 32; }
    bg[b][x] = 0x2588;
    if (cp === 0) cp = 32;
    if (cp !== 32 && cp !== 0xA0) {
      if (cp > 0xFFFF || !WW.glyph[cp]) cp = 63;   // '?': no glyph in the embedded font
      if (!fg[f]) { fg[f] = new Array(C); for (k = 0; k < C; k++) fg[f][k] = 32; }
      fg[f][x] = cp;
    }
  }
  for (k = 0; k < nc; k++) {
    var s = '';
    if (bg[k]) { var n = wwTrimRight(bg[k], C); s = String.fromCharCode.apply(null, bg[k].slice(0, n)); }
    wwSet('r' + y + '.b' + k, s);
    s = '';
    if (fg[k]) { var m = wwTrimRight(fg[k], C); s = String.fromCharCode.apply(null, fg[k].slice(0, m)); }
    wwSet('r' + y + '.f' + k, s);
  }
}

function wwDraw() {
  WW.drawnAt = Date.now();
  if (!WW.cur) return;
  var t0 = Date.now(), C = WW.cols, R = WW.rows, n = C * 3, cur = WW.cur, shown = WW.shown;
  WW.dirty = false;
  for (var y = 0; y < R; y++) {
    var o = y * n, same = true;
    for (var i = 0; i < n; i++) if (cur[o + i] !== shown[o + i]) { same = false; break; }
    if (same) continue;
    wwDrawRow(y);
    for (var j = 0; j < n; j++) shown[o + j] = cur[o + j];
  }
  var cx = WW.caret[0], cy = WW.caret[1];
  if (cx !== WW.shownCaret[0] || cy !== WW.shownCaret[1]) {
    if (WW.shownCaret[1] >= 0) wwSet('r' + WW.shownCaret[1] + '.c', '');
    if (cy >= 0 && cy < R && cx >= 0 && cx < C) {
      var pad = '';
      for (var p = 0; p < cx; p++) pad += ' ';
      wwSet('r' + cy + '.c', pad + '_');
    }
    WW.shownCaret = [cx, cy];
  }
  WW.frames++;
  WW.lastDrawMs = Date.now() - t0;
}

function wwScheduleDraw() {
  WW.dirty = true;   // the pump draws it
}

// ---------- input ----------
var wwMods = { alt: false, ctrl: false };
function wwPush(str) {
  var M = WW.M;
  if (!M) return;
  // UTF-8 encode (no TextEncoder here)
  for (var i = 0; i < str.length; i++) {
    var c = str.charCodeAt(i);
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < str.length) { c = 0x10000 + ((c - 0xD800) << 10) + (str.charCodeAt(++i) - 0xDC00); }
    if (c < 0x80) M.wwdosQueue.push(c);
    else if (c < 0x800) M.wwdosQueue.push(0xC0 | (c >> 6), 0x80 | (c & 63));
    else if (c < 0x10000) M.wwdosQueue.push(0xE0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
    else M.wwdosQueue.push(0xF0 | (c >> 18), 0x80 | ((c >> 12) & 63), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
  }
  if (M.wwdosWake) M.wwdosWake();
}
function wwModLabel() {
  try {
    var a = wwDoc.getField('key.Alt'), c = wwDoc.getField('key.Ctrl');
    if (a) a.buttonSetCaption(wwMods.alt ? '[ALT]' : 'Alt');
    if (c) c.buttonSetCaption(wwMods.ctrl ? '[CTRL]' : 'Ctrl');
  } catch (e) {}
}
// A typed character, with the sticky Alt/Ctrl buttons applied.
function wwTypeChar(ch) {
  if (wwMods.ctrl && ch.length === 1) {
    var c = ch.toUpperCase().charCodeAt(0);
    if (c >= 64 && c < 96) ch = String.fromCharCode(c & 31);
  }
  if (wwMods.alt) ch = '\x1b' + ch.toLowerCase();
  if (wwMods.alt || wwMods.ctrl) { wwMods.alt = wwMods.ctrl = false; wwModLabel(); }
  wwPush(ch);
}
var WW_KEYS = {
  Esc: '\x1b', Tab: '\t', BackTab: '\x1b[Z', Enter: '\r', Bksp: '\x7f', Del: '\x1b[3~', Ins: '\x1b[2~',
  Up: '\x1b[A', Down: '\x1b[B', Right: '\x1b[C', Left: '\x1b[D', Home: '\x1b[H', End: '\x1b[F',
  PgUp: '\x1b[5~', PgDn: '\x1b[6~',
  F1: '\x1b[11~', F2: '\x1b[12~', F3: '\x1b[13~', F4: '\x1b[14~', F5: '\x1b[15~', F6: '\x1b[17~',
  F7: '\x1b[18~', F8: '\x1b[19~', F9: '\x1b[20~', F10: '\x1b[21~', F11: '\x1b[23~', F12: '\x1b[24~'
};
var WW_CSI = { Up: 'A', Down: 'B', Right: 'C', Left: 'D', Home: 'H', End: 'F' };
function wwKeyName(name) {
  wwStats.keys++;
  wwDropEcho();
  if (name === 'Alt') { wwMods.alt = !wwMods.alt; wwModLabel(); wwRefocus(); return; }
  if (name === 'Ctrl') { wwMods.ctrl = !wwMods.ctrl; wwModLabel(); wwRefocus(); return; }
  var s = WW_KEYS[name];
  if (!s) return;
  var mod = 1 + (wwMods.alt ? 2 : 0) + (wwMods.ctrl ? 4 : 0);
  if (mod > 1) {
    if (WW_CSI[name]) s = '\x1b[1;' + mod + WW_CSI[name];
    else if (s.charAt(s.length - 1) === '~') s = s.slice(0, -1) + ';' + mod + '~';
    else if (wwMods.alt) s = '\x1b' + s;
    wwMods.alt = wwMods.ctrl = false; wwModLabel();
  }
  wwPush(s);
  wwRefocus();
}
// Keystroke action of the keyboard field. The field keeps growing in some viewers whatever rc says,
// so the text is read from event.change and the field is emptied now and then.
function wwKey(e) {
  wwDropEcho();
  if (e.willCommit) return;
  var ch = e.change;
  if (ch && ch.length) {
    for (var i = 0; i < ch.length; i++) {
      var c = ch.charAt(i);
      if (c === '\r' || c === '\n') { if (c === '\n' && i > 0 && ch.charAt(i - 1) === '\r') continue; wwTypeChar('\r'); }
      else wwTypeChar(c);
    }
  } else if (e.selEnd > e.selStart) {
    wwPush('\x7f');                                           // a deletion: Backspace
  } else {
    wwPush('\r');                                             // an empty change at the caret: Enter
  }
  try { e.change = ''; e.rc = true; } catch (x) {}
  if (e.value && e.value.length > 200) { try { setTimeout(function () { wwField('k').value = ''; }, 1); } catch (x) {} }
}
function wwRefocus() {
  try { var k = wwField('k'); if (k && k.setFocus) k.setFocus(); } catch (e) {}
}
// Mouse: a click on a cell button. SGR reports are 1-based.
// Chrome's viewer runs the previous button's action again when the next mouse-down lands on another
// widget, so every click waits WW_CLICK_MS: a repeat of the last delivered cell that is followed within
// that time by any other widget's action is that echo, and is dropped.
var WW_CLICK_MS = 40, wwPending = null, wwLastCell = null;
function wwDropEcho() {
  if (wwPending && wwPending.cell === wwLastCell && Date.now() - wwPending.t < WW_CLICK_MS) {
    clearTimeout(wwPending.timer);
    wwPending = null;
  }
}
function wwDeliverClick(p) {
  if (wwPending !== p) return;
  wwPending = null;
  wwLastCell = p.cell;
  var b = 0 + (wwMods.alt ? 8 : 0) + (wwMods.ctrl ? 16 : 0), x = p.x + 1, y = p.y + 1;
  wwPush('\x1b[<' + b + ';' + x + ';' + y + 'M' + '\x1b[<' + b + ';' + x + ';' + y + 'm');
  if (wwMods.alt || wwMods.ctrl) { wwMods.alt = wwMods.ctrl = false; wwModLabel(); }
}
function wwClick(x, y) {
  wwStats.clicks++; wwStats.last = x + ',' + y;
  wwDropEcho();
  if (wwPending) { clearTimeout(wwPending.timer); wwDeliverClick(wwPending); }
  var p = { x: x, y: y, cell: x + ',' + y, t: Date.now() };
  p.timer = setTimeout(function () { wwDeliverClick(p); }, WW_CLICK_MS);
  wwPending = p;
}

function wwHeartbeat() {
  var M = WW.M;
  try { wwDoc.getField('log').value = 'fires=' + wwStats.fires + ' keys=' + wwStats.keys + ' clicks=' + wwStats.clicks + '@' + wwStats.last + ' frames=' + WW.frames +
    ' draw=' + WW.lastDrawMs + 'ms q=' + (M ? M.wwdosQueue.length + '/' + (M.wwdosPos | 0) : '-') + ' wake=' + (M && M.wwdosWake ? 1 : 0) + ' | ' + wwLogText.join(' | ').slice(-300); } catch (e) {}
}

// ---------- start ----------
function wwStart() {
  var C = WW.cols, R = WW.rows;
  WW.cur = new Uint32Array(C * R * 3);
  WW.shown = new Uint32Array(C * R * 3);
  for (var i = 0; i < WW.shown.length; i++) WW.shown[i] = 0xFFFFFFFF;
  var M = {
    wwdosQueue: [], wwdosResized: false, wwdosWake: null,
    wwdosWrite: function () {},
    wwdosCols: function () { return C; },
    wwdosRows: function () { return R; },
    wwdosGrid: function (cells, c, r, cx, cy) {
      if (c !== C || r !== R) return;
      if (!WW.firstMs) { WW.firstMs = Date.now() - WW.t0; wwLog('first frame ' + WW.firstMs + ' ms'); }
      WW.cur.set(cells);
      WW.caret = [cx, cy];
      WW.dirty = true;
      wwScheduleDraw();
    },
    preRun: [function () { M.ENV.WWDOS_SOUND_OUT = 'off'; var e = WWPDF.env || {}; for (var k in e) M.ENV[k] = e[k]; }],
    print: function (s) { wwLog(s); },
    printErr: function (s) { wwLog(s); },
    onAbort: function (w) { wwLog('abort: ' + w); }
  };
  WW.M = M; WW.t0 = Date.now();
  WW.pump = app.setInterval('__wwPump()', WWPDF.tickMs || 10);
  wwLog('starting WibWob-DOS...');
  try {
    WWDOS(M).then(function () { wwLog('running'); if (WWPDF.debug) WW.hb = app.setInterval('wwHeartbeat()', 1000); }, function (e) { wwLog('start failed: ' + e + (e && e.stack ? ' ' + e.stack : '')); });
  } catch (e) {
    wwLog('start threw: ' + e + (e && e.stack ? ' ' + e.stack : ''));
  }
}
