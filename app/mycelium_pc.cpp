/*---------------------------------------------------------*/
/*                                                         */
/*   mycelium_pc.cpp                                       */
/*                                                         */
/*   Standalone TVision app:                               */
/*   Prof. Mycelium's Personal HyphaeComputer (1985).      */
/*                                                         */
/*   Loads a tuiforge render (default path:                */
/*     ../tuiforge/renders/mycelium-pc-1985/default)       */
/*   at 80x50 and overlays interactive state on top of     */
/*   the painted grid:                                     */
/*                                                         */
/*     - Calculator display cycles through numbers         */
/*     - Palette window rotates the rainbow                */
/*     - Sporensi (Reversi) board accepts new spores       */
/*     - Spore Catalog selection moves on click            */
/*     - Mycelium blinks his glasses-eyes                  */
/*     - Red LEDs on the monitor base flash at 1Hz         */
/*                                                         */
/*   Controls:                                             */
/*     c / click Calc      cycle number                    */
/*     p / click Palette   rotate rainbow                  */
/*     s / click Board     place next spore                */
/*     k / click Catalog   advance selection               */
/*     b / click Face      manual blink                    */
/*     a                   toggle full autoplay            */
/*     r                   reset to initial state          */
/*     Esc / Alt-X         quit                            */
/*                                                         */
/*   Usage:                                                */
/*     mycelium_pc [render-dir]                            */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TApplication
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TView
#define Uses_TRect
#define Uses_TPoint
#define Uses_TDeskTop
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TKeys
#define Uses_TDrawBuffer
#define Uses_TColorAttr
#define Uses_TPalette
#define Uses_TEvent
#define Uses_TGroup
#include <tvision/tv.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// ---------- grid loading (variable height) ----------

static const int GRID_W = 80;
static const int GRID_H = 50;

struct TuiCell {
    std::string glyph;   // one UTF-8 codepoint
    uchar fg = 0x07;
    uchar bg = 0x00;
};

struct TuiGrid {
    std::vector<TuiCell> cells;  // row-major, GRID_W * GRID_H
    bool loaded = false;
    std::string error;

    TuiGrid() : cells(GRID_W * GRID_H, TuiCell{" ", 0x07, 0x00}) {}

    const TuiCell &at(int r, int c) const { return cells[r * GRID_W + c]; }
    TuiCell &at(int r, int c) { return cells[r * GRID_W + c]; }
};

static std::string slurp(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static int utf8_len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static bool load_grid(const std::string &dir, TuiGrid &out) {
    std::string txt = slurp(dir + "/tui.txt");
    std::string fg  = slurp(dir + "/tui.fg");
    std::string bg  = slurp(dir + "/tui.bg");
    if (txt.empty() || fg.empty() || bg.empty()) {
        out.error = "missing tui.txt/tui.fg/tui.bg in " + dir;
        return false;
    }

    int row = 0;
    size_t i = 0;
    while (i < txt.size() && row < GRID_H) {
        int col = 0;
        while (i < txt.size() && txt[i] != '\n' && col < GRID_W) {
            if (txt[i] == '\r') { ++i; continue; }
            int clen = utf8_len((unsigned char)txt[i]);
            if (i + (size_t)clen > txt.size()) clen = 1;
            out.at(row, col).glyph.assign(&txt[i], clen);
            i += clen;
            ++col;
        }
        while (i < txt.size() && txt[i] != '\n') ++i;
        if (i < txt.size() && txt[i] == '\n') ++i;
        ++row;
    }

    auto parse_attr = [](const std::string &src, TuiGrid &g, bool is_bg) {
        int r = 0;
        size_t j = 0;
        while (j < src.size() && r < GRID_H) {
            int c = 0;
            while (j < src.size() && src[j] != '\n' && c < GRID_W) {
                if (src[j] == '\r') { ++j; continue; }
                int v = hexval(src[j]);
                if (v < 0) v = 0;
                if (is_bg) g.at(r, c).bg = (uchar)v;
                else       g.at(r, c).fg = (uchar)v;
                ++j; ++c;
            }
            while (j < src.size() && src[j] != '\n') ++j;
            if (j < src.size() && src[j] == '\n') ++j;
            ++r;
        }
    };
    parse_attr(fg, out, false);
    parse_attr(bg, out, true);

    out.loaded = true;
    return true;
}

// ---------- scene-specific hotspot regions ----------

struct Region {
    int x0, y0, x1, y1;  // inclusive
    bool contains(int col, int row) const {
        return col >= x0 && col <= x1 && row >= y0 && row <= y1;
    }
};

// Coordinates tuned to scenes/mycelium-pc-1985/scene.yaml
static const Region R_CALC     = { 57, 10, 65, 21 };
static const Region R_PALETTE  = { 66,  9, 73, 17 };
static const Region R_BOARD    = { 43, 17, 55, 24 };
static const Region R_CATALOG  = { 43, 10, 55, 15 };
static const Region R_FACE     = {  8, 12, 29, 16 };

// ---------- app state ----------

struct AppState {
    int calc_value = 9;
    int palette_shift = 0;
    int catalog_idx = 0;     // 0..5
    int spores_placed = 0;   // 0..10
    bool led_on = true;
    bool eyes_closed = false;
    bool autoplay = false;

    std::chrono::steady_clock::time_point last_led_toggle;

    // Natural blink scheduler: random interval between blinks, with
    // occasional clusters (human eyes blink in bursts, not metronomically).
    std::chrono::steady_clock::time_point next_blink_at;
    std::chrono::steady_clock::time_point blink_end;
    int blink_cluster_remaining = 0;  // follow-up blinks queued for this cluster

    // Ambient UI tick: random 12-20s intervals, picks one element to mutate
    // so the scene feels alive even when the user is idle.
    std::chrono::steady_clock::time_point next_ui_tick_at;

    // Autoplay (opt-in): cycles everything fast
    std::chrono::steady_clock::time_point last_auto_calc;
    std::chrono::steady_clock::time_point last_auto_palette;
    std::chrono::steady_clock::time_point last_auto_catalog;
    std::chrono::steady_clock::time_point last_auto_spore;

    // ----- constant micro-motion channels (load-screen liveness) -----

    // CRT scan line: row index that currently gets brightened (8..27)
    int scan_row = 8;
    std::chrono::steady_clock::time_point last_scan_advance;

    // Spore breathing: pulse phase flips every ~1.2s, swaps ●<->◉ / ○<->◎
    bool spore_pulse = false;
    std::chrono::steady_clock::time_point last_spore_pulse;

    // Taskbar active button rotates through 4 positions at random intervals
    int taskbar_active = 0;
    std::chrono::steady_clock::time_point next_taskbar_rotate;

    // Two extra floppy-activity LEDs with independent irregular rhythms
    bool floppy_a_on = false;
    bool floppy_b_on = false;
    std::chrono::steady_clock::time_point next_floppy_a;
    std::chrono::steady_clock::time_point next_floppy_b;

    // Calc key press flash: -1 = no key, else index into a key-position table
    int calc_key_flash = -1;
    std::chrono::steady_clock::time_point calc_key_flash_end;

    // Micro-glitch: rare single-cell colour inversion
    int glitch_row = -1, glitch_col = -1;
    std::chrono::steady_clock::time_point glitch_end;
    std::chrono::steady_clock::time_point next_glitch_at;
};

// Cycle of calculator display values
static const std::array<int, 8> CALC_SEQUENCE = {
    9, 42, 128, 1024, 1985, 640, 16, 256
};

// Palette colour indices (CGA 0-15) matching scene.yaml palette rows 10-17.
// These are the bg colours for the 8 palette rows in initial order.
static const std::array<uchar, 8> PALETTE_BASE = {
    0xC,  // bright_red   (row 10)
    0x4,  // red          (row 11)
    0xE,  // yellow       (row 12)
    0xA,  // bright_green (row 13)
    0xB,  // bright_cyan  (row 14)
    0x1,  // blue         (row 15)
    0xD,  // bright_magenta (row 16)
    0x5,  // magenta      (row 17)
};

// Calc keypad positions (row, col) of the 12 visible key glyphs.
// 3 keys per row at cols 58, 60, 62, 64 and 4 rows at 13, 15, 17, 19.
// When calc cycles, a random key briefly flashes as if pressed.
struct CalcKey { int row, col; };
static const std::array<CalcKey, 12> CALC_KEYS = {{
    {13, 58}, {13, 60}, {13, 62},   // 7 8 9
    {15, 58}, {15, 60}, {15, 62},   // 4 5 6
    {17, 58}, {17, 60}, {17, 62},   // 1 2 3
    {19, 58}, {19, 60}, {19, 62},   // 0 . =
}};

// Taskbar button bg-ranges (col0..col1 inclusive) on row 26.
// Buttons are  [Spore] [Calc] [Palet] [Board].
struct TaskbarBtn { int col0, col1; };
static const std::array<TaskbarBtn, 4> TASKBAR_BTNS = {{
    {44, 50},   // [Spore]
    {52, 57},   // [Calc]
    {59, 65},   // [Palet]
    {67, 73},   // [Board]
}};

// Extra disk-activity LED positions (row, col) on the monitor base.
// The originals are at (33, 46) and (33, 70); these new ones sit on
// the floppy-slot `▓` cells for a second colour channel.
static const int FLOPPY_A_ROW = 33, FLOPPY_A_COL = 49;
static const int FLOPPY_B_ROW = 33, FLOPPY_B_COL = 67;

// Sporensi move sequence: (row, col, glyph, fg) in play order.
// Board panel runs rows 17-24, cols 43-55. These positions sit on empty
// ░ / ▒ cells in the initial board pattern so new spores overwrite bg texture.
struct SporeMove {
    int row, col;
    const char *glyph;
    uchar fg;
};
static const std::array<SporeMove, 10> SPORE_SEQUENCE = {{
    {19, 47, "\xE2\x97\x8F", 0x0},  // ● black
    {19, 49, "\xE2\x97\x8B", 0xF},  // ○ white
    {19, 51, "\xE2\x97\x8F", 0x0},
    {21, 51, "\xE2\x97\x8B", 0xF},
    {22, 53, "\xE2\x97\x8F", 0x0},
    {22, 55, "\xE2\x97\x8B", 0xF},
    {23, 45, "\xE2\x97\x8F", 0x0},
    {23, 51, "\xE2\x97\x8B", 0xF},
    {18, 47, "\xE2\x97\x8B", 0xF},
    {18, 49, "\xE2\x97\x8F", 0x0},
}};

// ---------- overlay application ----------

// Apply all state-derived modifications to a mutable snapshot of the base grid.
// Returns a grid that reflects the current AppState.
static void apply_overlay(const TuiGrid &base, const AppState &s, TuiGrid &out) {
    out = base;  // copy

    // --- Calc display: right-align the current value in 7-char field at row 11, cols 58-64
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%7d", s.calc_value);
        int col = 58;
        for (int i = 0; i < 7 && col <= 64; ++i, ++col) {
            TuiCell &c = out.at(11, col);
            c.glyph = std::string(1, buf[i]);
            c.fg = 0x0;   // black
            c.bg = 0x7;   // light_grey
        }
    }

    // --- Palette rotation: reassign bg of rows 10-17, cols 66-73
    for (int i = 0; i < 8; ++i) {
        int row = 10 + i;
        uchar bg = PALETTE_BASE[(i + s.palette_shift) % 8];
        uchar fg = PALETTE_BASE[(i + s.palette_shift + 1) % 8];  // highlight with next
        for (int col = 66; col <= 73; ++col) {
            TuiCell &c = out.at(row, col);
            c.bg = bg;
            c.fg = fg;
        }
    }

    // --- Sporensi board: place spores according to spores_placed count
    for (int i = 0; i < s.spores_placed && i < (int)SPORE_SEQUENCE.size(); ++i) {
        const SporeMove &m = SPORE_SEQUENCE[i];
        TuiCell &c = out.at(m.row, m.col);
        c.glyph = m.glyph;
        c.fg = m.fg;
        // keep existing bg (green from the board panel)
    }

    // --- Catalog highlight: invert row at 10 + catalog_idx, cols 43-55
    {
        int row = 10 + s.catalog_idx;
        if (row >= 10 && row <= 15) {
            for (int col = 43; col <= 55; ++col) {
                TuiCell &c = out.at(row, col);
                uchar tmp = c.fg;
                c.fg = c.bg;
                c.bg = tmp;
            }
        }
    }

    // --- Mycelium blink: replace the ● pupils in the glasses with ─
    // Face panel row 14 has "░░█●●●█▓▓▓▓▓█●●●█░░░░░" at cols 8-29.
    // Pupils live at grid cols 11-13 (left eye) and 21-23 (right eye).
    if (s.eyes_closed) {
        const char *dash = "\xE2\x94\x80";  // ─ U+2500
        for (int col : {11, 12, 13, 21, 22, 23}) {
            TuiCell &c = out.at(14, col);
            c.glyph = dash;
        }
    }

    // --- LED flash: two LEDs at row 33, cols 46 and 70
    if (!s.led_on) {
        out.at(33, 46).glyph = " ";
        out.at(33, 70).glyph = " ";
    }

    // --- Extra floppy-activity LEDs (green, irregular)
    {
        TuiCell &a = out.at(FLOPPY_A_ROW, FLOPPY_A_COL);
        if (s.floppy_a_on) { a.glyph = "\xE2\x97\x8F"; a.fg = 0xA; /* bright_green */ }
        TuiCell &b = out.at(FLOPPY_B_ROW, FLOPPY_B_COL);
        if (s.floppy_b_on) { b.glyph = "\xE2\x97\x8F"; b.fg = 0xA; }
    }

    // --- Taskbar active-button highlight (bg flips blue -> bright_cyan)
    if (s.taskbar_active >= 0 && s.taskbar_active < (int)TASKBAR_BTNS.size()) {
        const TaskbarBtn &b = TASKBAR_BTNS[s.taskbar_active];
        for (int col = b.col0; col <= b.col1; ++col) {
            TuiCell &c = out.at(26, col);
            c.bg = 0xB;          // bright_cyan
            c.fg = 0x0;          // black text pops on the lit button
        }
    }

    // --- Calc key press flash: bg flips to bright_red for a single cell
    if (s.calc_key_flash >= 0 && s.calc_key_flash < (int)CALC_KEYS.size()) {
        const CalcKey &k = CALC_KEYS[s.calc_key_flash];
        TuiCell &c = out.at(k.row, k.col);
        c.bg = 0xC;              // bright_red
        c.fg = 0xF;              // bright_white digit on red
    }

    // --- Board spore breathing: existing spores pulse ● <-> ◉ / ○ <-> ◎
    // Cells to inspect: any cell inside R_BOARD with a matching base glyph.
    if (s.spore_pulse) {
        for (int row = R_BOARD.y0; row <= R_BOARD.y1; ++row) {
            for (int col = R_BOARD.x0; col <= R_BOARD.x1; ++col) {
                TuiCell &c = out.at(row, col);
                if (c.glyph == "\xE2\x97\x8F") c.glyph = "\xE2\x97\x89"; // ● -> ◉
                else if (c.glyph == "\xE2\x97\x8B") c.glyph = "\xE2\x97\x8E"; // ○ -> ◎
            }
        }
    }

    // --- Micro-glitch: one cell inverts fg/bg for ~100ms
    if (s.glitch_row >= 0 && s.glitch_col >= 0 &&
        s.glitch_row < GRID_H && s.glitch_col < GRID_W) {
        TuiCell &c = out.at(s.glitch_row, s.glitch_col);
        uchar tmp = c.fg; c.fg = c.bg; c.bg = tmp;
    }

    // --- CRT scan line: brighten fg on the currently-swept row
    // Only affects cells inside the screen area (rows 8..27, cols 43..73).
    if (s.scan_row >= 8 && s.scan_row <= 27) {
        for (int col = 43; col <= 73; ++col) {
            TuiCell &c = out.at(s.scan_row, col);
            c.fg |= 0x08;  // flip to bright variant
        }
    }

    // --- Live clock in the masthead. Cells in row 0 cols 27..34 hold
    // "NOV 1985"; overwrite with a ticking HH:MM:SS in matching attrs.
    {
        std::time_t t = std::time(nullptr);
        std::tm tm_local{};
        localtime_r(&t, &tm_local);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                      tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec);
        // 8 chars fit cols 27-34
        int col = 27;
        for (int i = 0; i < 8 && col <= 34; ++i, ++col) {
            TuiCell &c = out.at(0, col);
            c.glyph = std::string(1, buf[i]);
            // keep existing fg/bg (red on light_grey, matches masthead)
        }
    }
}

// ---------- view ----------

class MyceliumApp;  // fwd

class MyceliumView : public TView {
public:
    MyceliumView(const TRect &bounds, MyceliumApp *a)
        : TView(bounds), app(a)
    {
        options |= ofFramed;
        growMode = 0;
        eventMask = evMouseDown | evKeyDown;
    }

    void setBase(const TuiGrid &g) { base = g; drawView(); }

    void draw() override;

    void handleEvent(TEvent &event) override;

    TPalette &getPalette() const override {
        static const char pal[] = "\x07";
        static TPalette p(pal, sizeof(pal) - 1);
        return p;
    }

    const TuiGrid &baseGrid() const { return base; }

private:
    MyceliumApp *app = nullptr;
    TuiGrid base;
};

class MyceliumWindow : public TWindow {
public:
    MyceliumWindow(const TRect &bounds, MyceliumApp *a)
        : TWindowInit(&MyceliumWindow::initFrame),
          TWindow(bounds, "Prof. Mycelium -- HyphaeCom-1", wnNoNumber)
    {
        flags = wfMove | wfZoom;
        options |= ofTileable;
        TRect v = getExtent();
        v.grow(-1, -1);
        view = new MyceliumView(v, a);
        insert(view);
    }

    void setGrid(const TuiGrid &g) { view->setBase(g); }
    MyceliumView *getView() { return view; }

private:
    MyceliumView *view = nullptr;
};

// ---------- app ----------

const ushort cmCalcCycle    = 2001;
const ushort cmPalRotate    = 2002;
const ushort cmBoardPlace   = 2003;
const ushort cmCatalogNext  = 2004;
const ushort cmMycBlink     = 2005;
const ushort cmReset        = 2006;
const ushort cmToggleAuto   = 2007;

class MyceliumApp : public TApplication {
public:
    MyceliumApp(std::string renderDir)
        : TProgInit(&MyceliumApp::initStatusLine,
                    &MyceliumApp::initMenuBar,
                    &MyceliumApp::initDeskTop),
          dir(std::move(renderDir)),
          rng(std::random_device{}())
    {
        auto now = std::chrono::steady_clock::now();
        state.last_led_toggle = now;
        state.blink_end       = now;
        state.next_blink_at   = now + randMs(3500, 8000);
        state.next_ui_tick_at = now + randMs(12000, 20000);
        state.last_auto_calc  = now;
        state.last_auto_palette = now;
        state.last_auto_catalog = now;
        state.last_auto_spore = now;

        // Seed new liveness channels
        state.last_scan_advance    = now;
        state.last_spore_pulse     = now;
        state.next_taskbar_rotate  = now + randMs(1500, 3000);
        state.next_floppy_a        = now + randMs(400, 1200);
        state.next_floppy_b        = now + randMs(700, 1800);
        state.calc_key_flash_end   = now;
        state.next_glitch_at       = now + randMs(20000, 40000);
        state.glitch_end           = now;

        TuiGrid g;
        if (!load_grid(dir, g)) {
            std::fprintf(stderr, "error: %s\n", g.error.c_str());
        }
        base = g;

        spawnWindow();
    }

    // Uniform random duration in [lo, hi] milliseconds.
    std::chrono::milliseconds randMs(int lo, int hi) {
        std::uniform_int_distribution<int> d(lo, hi);
        return std::chrono::milliseconds(d(rng));
    }

    // Schedule the next blink, randomly, with occasional cluster follow-ups.
    // 25% chance this blink starts a cluster (1 or 2 rapid re-blinks after).
    void scheduleNextBlink(std::chrono::steady_clock::time_point from) {
        std::uniform_int_distribution<int> clusterRoll(1, 100);
        int roll = clusterRoll(rng);
        if (state.blink_cluster_remaining > 0) {
            // Cluster continuation: fast re-blink 150-400ms later
            state.next_blink_at = from + randMs(150, 400);
        } else {
            // Fresh blink 3.5-8s out
            state.next_blink_at = from + randMs(3500, 8000);
            if (roll <= 20) {
                state.blink_cluster_remaining = 1;       // double-blink
            } else if (roll <= 25) {
                state.blink_cluster_remaining = 2;       // rare triple
            } else {
                state.blink_cluster_remaining = 0;       // single
            }
        }
    }

    // Pick a random UI element to nudge, for ambient liveness.
    // Weighted so palette rotation happens most often (it's the most
    // visually satisfying change).
    void ambientUiTick() {
        std::uniform_int_distribution<int> pick(1, 100);
        int roll = pick(rng);
        if (roll <= 40)      { state.palette_shift = (state.palette_shift + 1) % 8; }
        else if (roll <= 60) { state.catalog_idx = (state.catalog_idx + 1) % 6; }
        else if (roll <= 75) {
            // Cycle calc to a fresh value from the sequence
            std::uniform_int_distribution<int> cd(0, (int)CALC_SEQUENCE.size() - 1);
            state.calc_value = CALC_SEQUENCE[cd(rng)];
        }
        else if (roll <= 88) {
            // Drop a spore (wrap if board is full)
            state.spores_placed++;
            if (state.spores_placed > (int)SPORE_SEQUENCE.size())
                state.spores_placed = 0;
        }
        else {
            // Rare: trigger a manual blink -- "Mycelium noticed you"
            triggerBlink();
        }
    }

    // Called by the view on mouse clicks (col, row are grid coords).
    void handleClick(int col, int row) {
        if (R_CALC.contains(col, row))        { cycleCalc(); return; }
        if (R_PALETTE.contains(col, row))     { rotatePalette(); return; }
        if (R_BOARD.contains(col, row))       { placeSpore(); return; }
        if (R_CATALOG.contains(col, row))     { advanceCatalog(); return; }
        if (R_FACE.contains(col, row))        { triggerBlink(); return; }
    }

    void cycleCalc() {
        static int idx = 0;
        idx = (idx + 1) % (int)CALC_SEQUENCE.size();
        state.calc_value = CALC_SEQUENCE[idx];
        // Flash a random keypad key for ~160ms (as if it was pressed)
        std::uniform_int_distribution<int> kd(0, (int)CALC_KEYS.size() - 1);
        state.calc_key_flash = kd(rng);
        state.calc_key_flash_end = std::chrono::steady_clock::now()
                                 + std::chrono::milliseconds(160);
        redraw();
    }

    void rotatePalette() {
        state.palette_shift = (state.palette_shift + 1) % 8;
        redraw();
    }

    void placeSpore() {
        if (state.spores_placed < (int)SPORE_SEQUENCE.size())
            state.spores_placed++;
        else
            state.spores_placed = 0;
        redraw();
    }

    void advanceCatalog() {
        state.catalog_idx = (state.catalog_idx + 1) % 6;
        redraw();
    }

    void triggerBlink() {
        auto now = std::chrono::steady_clock::now();
        state.eyes_closed = true;
        state.blink_end = now + randMs(180, 280);  // 180-280ms for varied feel
        // Push the auto-blink schedule past this blink so manual clicks
        // don't cause immediate re-triggers on the next idle tick.
        if (state.next_blink_at < state.blink_end)
            state.next_blink_at = state.blink_end + randMs(2500, 6000);
        redraw();
    }

    void resetState() {
        AppState fresh;
        auto now = std::chrono::steady_clock::now();
        fresh.last_led_toggle = now;
        fresh.blink_end = now;
        fresh.next_blink_at = now + randMs(3500, 8000);
        fresh.next_ui_tick_at = now + randMs(12000, 20000);
        fresh.last_auto_calc = now;
        fresh.last_auto_palette = now;
        fresh.last_auto_catalog = now;
        fresh.last_auto_spore = now;
        fresh.last_scan_advance = now;
        fresh.last_spore_pulse = now;
        fresh.next_taskbar_rotate = now + randMs(1500, 3000);
        fresh.next_floppy_a = now + randMs(400, 1200);
        fresh.next_floppy_b = now + randMs(700, 1800);
        fresh.calc_key_flash_end = now;
        fresh.next_glitch_at = now + randMs(20000, 40000);
        fresh.glitch_end = now;
        state = fresh;
        redraw();
    }

    void toggleAutoplay() {
        state.autoplay = !state.autoplay;
        auto now = std::chrono::steady_clock::now();
        state.last_auto_calc = now;
        state.last_auto_palette = now;
        state.last_auto_catalog = now;
        state.last_auto_spore = now;
        redraw();
    }

    const AppState &getState() const { return state; }
    const TuiGrid &getBase() const { return base; }

    static TMenuBar *initMenuBar(TRect r) {
        r.b.y = r.a.y + 1;
        return new TMenuBar(r,
            *new TSubMenu("~F~ile", kbAltF) +
                *new TMenuItem("~R~eset",           cmReset,      kbAltR, hcNoContext, "Alt-R") +
                *new TMenuItem("Toggle ~A~utoplay", cmToggleAuto, kbAltA, hcNoContext, "Alt-A") +
                newLine() +
                *new TMenuItem("E~x~it",            cmQuit,       kbAltX, hcNoContext, "Alt-X") +
            *new TSubMenu("~P~oke", kbAltP) +
                *new TMenuItem("~C~alc cycle",      cmCalcCycle,   kbCtrlC) +
                *new TMenuItem("~P~alette rotate",  cmPalRotate,   kbCtrlP) +
                *new TMenuItem("~S~porensi move",   cmBoardPlace,  kbCtrlS) +
                *new TMenuItem("Catalog ~n~ext",    cmCatalogNext, kbCtrlN) +
                *new TMenuItem("~B~link Mycelium",  cmMycBlink,    kbCtrlB));
    }

    static TStatusLine *initStatusLine(TRect r) {
        r.a.y = r.b.y - 1;
        return new TStatusLine(r,
            *new TStatusDef(0, 0xFFFF) +
                *new TStatusItem("~Alt-X~ Exit",   kbAltX, cmQuit)   +
                *new TStatusItem("~C~alc",         0,      cmCalcCycle)   +
                *new TStatusItem("~P~alette",      0,      cmPalRotate)   +
                *new TStatusItem("~S~pore",        0,      cmBoardPlace)  +
                *new TStatusItem("Cata~l~og",      0,      cmCatalogNext) +
                *new TStatusItem("~B~link",        0,      cmMycBlink)    +
                *new TStatusItem("~A~uto",         0,      cmToggleAuto)  +
                *new TStatusItem("~R~eset",        0,      cmReset));
    }

    void handleEvent(TEvent &event) override {
        TApplication::handleEvent(event);

        if (event.what == evKeyDown) {
            switch (event.keyDown.charScan.charCode) {
                case 'c': case 'C': cycleCalc(); clearEvent(event); break;
                case 'p': case 'P': rotatePalette(); clearEvent(event); break;
                case 's': case 'S': placeSpore(); clearEvent(event); break;
                case 'k': case 'K':
                case 'l': case 'L': advanceCatalog(); clearEvent(event); break;
                case 'b': case 'B': triggerBlink(); clearEvent(event); break;
                case 'r': case 'R': resetState(); clearEvent(event); break;
                case 'a': case 'A': toggleAutoplay(); clearEvent(event); break;
            }
        }

        if (event.what == evCommand) {
            switch (event.message.command) {
                case cmCalcCycle:    cycleCalc();       clearEvent(event); break;
                case cmPalRotate:    rotatePalette();   clearEvent(event); break;
                case cmBoardPlace:   placeSpore();      clearEvent(event); break;
                case cmCatalogNext:  advanceCatalog();  clearEvent(event); break;
                case cmMycBlink:     triggerBlink();    clearEvent(event); break;
                case cmReset:        resetState();      clearEvent(event); break;
                case cmToggleAuto:   toggleAutoplay();  clearEvent(event); break;
            }
        }
    }

    void idle() override {
        TApplication::idle();
        auto now = std::chrono::steady_clock::now();

        bool dirty = false;

        // LEDs toggle at 1Hz
        if (now - state.last_led_toggle >= std::chrono::milliseconds(1000)) {
            state.led_on = !state.led_on;
            state.last_led_toggle = now;
            dirty = true;
        }

        // Natural blink: random interval 3.5-8s base, with 25% chance of
        // a cluster (1-2 follow-up blinks 150-400ms apart).
        if (!state.eyes_closed && now >= state.next_blink_at) {
            state.eyes_closed = true;
            state.blink_end = now + randMs(180, 280);
            dirty = true;
        }
        // Blink expiry -- schedule next blink (cluster or fresh)
        if (state.eyes_closed && now >= state.blink_end) {
            state.eyes_closed = false;
            if (state.blink_cluster_remaining > 0) {
                state.blink_cluster_remaining--;
            }
            scheduleNextBlink(now);
            dirty = true;
        }

        // Ambient UI tick every 12-20s: one random element changes.
        // Skipped during autoplay (autoplay drives its own cadence).
        if (!state.autoplay && now >= state.next_ui_tick_at) {
            ambientUiTick();
            state.next_ui_tick_at = now + randMs(12000, 20000);
            dirty = true;
        }

        // CRT scan line sweeps down, wraps at bottom.  ~150ms/row -> 3s sweep.
        if (now - state.last_scan_advance >= std::chrono::milliseconds(150)) {
            state.last_scan_advance = now;
            state.scan_row++;
            if (state.scan_row > 27) state.scan_row = 8;
            dirty = true;
        }

        // Board spore breathing: toggle phase every ~1.2s
        if (now - state.last_spore_pulse >= std::chrono::milliseconds(1200)) {
            state.last_spore_pulse = now;
            state.spore_pulse = !state.spore_pulse;
            dirty = true;
        }

        // Clock ticks once per second -> force a redraw so HH:MM:SS updates
        {
            static std::chrono::steady_clock::time_point last_clock_tick =
                std::chrono::steady_clock::now();
            if (now - last_clock_tick >= std::chrono::milliseconds(1000)) {
                last_clock_tick = now;
                dirty = true;
            }
        }

        // Taskbar active-button rotates every 1.5-3s random
        if (now >= state.next_taskbar_rotate) {
            state.taskbar_active = (state.taskbar_active + 1) % 4;
            state.next_taskbar_rotate = now + randMs(1500, 3000);
            dirty = true;
        }

        // Floppy LED A: irregular read pulses, 400-1200ms when on, 600-2000ms off
        if (now >= state.next_floppy_a) {
            state.floppy_a_on = !state.floppy_a_on;
            state.next_floppy_a = now + (state.floppy_a_on
                ? randMs(200, 800)     // on for 200-800ms
                : randMs(700, 2400));  // off for 700-2400ms
            dirty = true;
        }
        // Floppy LED B: different rhythm so they never sync
        if (now >= state.next_floppy_b) {
            state.floppy_b_on = !state.floppy_b_on;
            state.next_floppy_b = now + (state.floppy_b_on
                ? randMs(150, 600)
                : randMs(900, 3000));
            dirty = true;
        }

        // Calc key flash expiry
        if (state.calc_key_flash >= 0 && now >= state.calc_key_flash_end) {
            state.calc_key_flash = -1;
            dirty = true;
        }

        // Micro-glitch: random single cell inverts for ~100ms, every 20-40s
        if (now >= state.next_glitch_at && state.glitch_row < 0) {
            std::uniform_int_distribution<int> rd(1, GRID_H - 4);   // skip masthead + caption
            std::uniform_int_distribution<int> cd(0, GRID_W - 1);
            state.glitch_row = rd(rng);
            state.glitch_col = cd(rng);
            state.glitch_end = now + std::chrono::milliseconds(100);
            state.next_glitch_at = now + randMs(20000, 40000);
            dirty = true;
        }
        if (state.glitch_row >= 0 && now >= state.glitch_end) {
            state.glitch_row = -1;
            state.glitch_col = -1;
            dirty = true;
        }

        // Autoplay: cycle everything at offset intervals
        if (state.autoplay) {
            if (now - state.last_auto_calc >= std::chrono::milliseconds(1500)) {
                state.last_auto_calc = now;
                static int i = 0; i = (i + 1) % (int)CALC_SEQUENCE.size();
                state.calc_value = CALC_SEQUENCE[i];
                dirty = true;
            }
            if (now - state.last_auto_palette >= std::chrono::milliseconds(500)) {
                state.last_auto_palette = now;
                state.palette_shift = (state.palette_shift + 1) % 8;
                dirty = true;
            }
            if (now - state.last_auto_catalog >= std::chrono::milliseconds(900)) {
                state.last_auto_catalog = now;
                state.catalog_idx = (state.catalog_idx + 1) % 6;
                dirty = true;
            }
            if (now - state.last_auto_spore >= std::chrono::milliseconds(2200)) {
                state.last_auto_spore = now;
                state.spores_placed++;
                if (state.spores_placed > (int)SPORE_SEQUENCE.size())
                    state.spores_placed = 0;
                dirty = true;
            }
        }

        if (dirty) redraw();
    }

    void redraw() {
        if (window) window->getView()->drawView();
    }

private:
    std::string dir;
    TuiGrid base;
    AppState state;
    MyceliumWindow *window = nullptr;
    std::mt19937 rng;

    void spawnWindow() {
        TRect dr = deskTop->getExtent();
        const int W = GRID_W + 2;
        const int H = GRID_H + 2;
        int dw = dr.b.x - dr.a.x;
        int dh = dr.b.y - dr.a.y;
        int x = dr.a.x + (dw - W) / 2;
        int y = dr.a.y + (dh - H) / 2;
        if (x < dr.a.x) x = dr.a.x;
        if (y < dr.a.y) y = dr.a.y;
        TRect wr(x, y, x + W, y + H);

        window = new MyceliumWindow(wr, this);
        deskTop->insert(window);
        window->setGrid(base);
    }
};

// --- view draw/event bodies (defined after MyceliumApp so they can see it) ---

void MyceliumView::draw() {
    TuiGrid overlay;
    if (app) {
        apply_overlay(base, app->getState(), overlay);
    } else {
        overlay = base;
    }

    TDrawBuffer b;
    for (int y = 0; y < size.y; ++y) {
        b.moveChar(0, ' ', TColorAttr{0x07}, size.x);
        if (y < GRID_H) {
            for (int x = 0; x < size.x && x < GRID_W; ++x) {
                const TuiCell &c = overlay.at(y, x);
                uchar byte = (uchar)(((c.bg & 0x0F) << 4) | (c.fg & 0x0F));
                b.moveStr((ushort)x,
                          TStringView{c.glyph.data(), c.glyph.size()},
                          TColorAttr{byte});
            }
        }
        writeLine(0, y, size.x, 1, b);
    }
}

void MyceliumView::handleEvent(TEvent &event) {
    TView::handleEvent(event);
    if (event.what == evMouseDown && app) {
        TPoint local = makeLocal(event.mouse.where);
        app->handleClick(local.x, local.y);
        clearEvent(event);
    }
}

// ---------- main ----------

int main(int argc, char **argv) {
    std::string dir;
    if (argc >= 2) {
        dir = argv[1];
    } else {
        // Default: assume sibling tuiforge repo
        const char *home = std::getenv("HOME");
        if (home) {
            dir = std::string(home) +
                  "/Repos/tuiforge/renders/mycelium-pc-1985/default";
        }
    }

    if (dir.empty()) {
        std::fprintf(stderr,
            "usage: mycelium_pc [render-dir]\n"
            "  render-dir must contain tui.txt / tui.fg / tui.bg\n"
            "  (default: $HOME/Repos/tuiforge/renders/mycelium-pc-1985/default)\n");
        return 2;
    }

    // Strip trailing slash
    while (dir.size() > 1 && dir.back() == '/') dir.pop_back();

    std::fprintf(stderr, "mycelium_pc: loading %s\n", dir.c_str());
    MyceliumApp app(dir);
    app.run();
    return 0;
}
