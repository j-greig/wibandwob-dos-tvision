// WWDOS_MINIMAL: the smallest browser build of the WibWob-DOS desktop.
//
// Desktop (TWibWobBackground), menu bar with the kaomoji, status line, the primer
// viewer (TFrameAnimationWindow) and the four-window boot scene. Nothing else is
// linked: no generative views, games, paint, chat, terminal, IPC or primer shelf.
// The only files in the virtual file system are the four boot primers in /boot.
// Menu and status-line classes are trimmed copies of the ones in wwdos_app.cpp.

#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TKeys
#define Uses_TRect
#define Uses_TEvent
#define Uses_TDrawBuffer
#define Uses_TScreen
#define Uses_TFileDialog
#define Uses_TDialog
#define Uses_MsgBox
#define Uses_TText
#define Uses_TStringView
#define Uses_cmTile
#define Uses_cmCascade
#include <tvision/tv.h>
#include <tvision/internal/codepage.h>   // tvision::CpTranslator, for cellCodepoint

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#ifndef __EMSCRIPTEN__
#include <sys/wait.h>   // waitpid, for the clip player (Linux needs it named; macOS had it through another header)
#endif

#include "ega_theme.h"
#include "wibwob_background.h"
#include "frame_file_player_view.h"
#include "windows/frame_animation_window.h"
#include "stage.h"
#include "picture.h"
#include "mirror_view.h"

static std::string g_hint;   // status line text set by the stage (hint |text)
static bool g_timerSeen = false;   // a timer fired since the last idle (TMinApp::idle draws the clear sprites again)
static double g_bgDrawMs = 0;   // time spent drawing the desktop and its marks (wwdos_bg_draw_ms, for tools/paint_bench.py)
static int g_bgDraws = 0;
static double g_tickMs = 0;     // time spent in stage timer events: commands, file loads, glides (wwdos_tick_ms)
static int g_ticks = 0;
struct StageTickTimer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    ~StageTickTimer() { g_tickMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); ++g_ticks; }
};

// Under-glyphs (docs/stage.md § Marks): drawLayers lists every line-art mark cell that stands on a patterned
// desktop, with the pattern cell it covers. The WebGL page draws that pattern glyph under the mark's glyph, so the
// dots run on behind the line. The cell itself keeps its flat paper (the pattern's average colour), which is what
// a display with one glyph per cell shows: the ANSI page, the native build, a terminal.
// Seven words per cell: x | y << 16 (screen), the mark's code point, its ink, its paper (RGB), then the pattern's
// code point, ink and paper. The page draws the pattern only while the screen cell still shows that mark.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, wwdos_js_under, (const uint32_t* p, int n), {
    if (Module.wwdosUnder) Module.wwdosUnder(n > 0 ? HEAPU32.slice(p >> 2, (p >> 2) + n * 7) : null);
});
#else
static void wwdos_js_under(const uint32_t*, int) {}
#endif
static const int kUnderWords = 7;

// The first code point of a cell's text (a single byte is a code page 437 character, as the display reads it).
static uint32_t cellCodepoint(const TScreenCell& c)
{
    TStringView t = c.character.getText();
    if (t.empty() || t[0] == 0) return ' ';
    unsigned char b = t[0];
    if (t.size() == 1 && b >= 0x80) { const char* u = tvision::CpTranslator::toUtf8(b); t = TStringView(u, strnlen(u, 4)); b = t[0]; }
    size_t n = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;
    if (n > t.size()) n = t.size();
    uint32_t u = n == 1 ? b : n == 2 ? (b & 0x1F) : n == 3 ? (b & 0x0F) : (b & 0x07);
    for (size_t i = 1; i < n; ++i) u = (u << 6) | ((unsigned char) t[i] & 0x3F);
    return u;
}
// A colour as the page's grid holds it (the web display's rule: RGB as it is, BIOS through the EGA palette).
static uint32_t gridRGB(TColor c, bool fg)
{
    TColorRGB v = c.isRGB() ? c.asRGB() : c.isBIOS() ? ww::egaRGB(c.asBIOS()) : fg ? TColorRGB(0xC0, 0xC0, 0xC0) : TColorRGB(0, 0, 0);
    return ((uint32_t) v.getRed() << 16) | ((uint32_t) v.getGreen() << 8) | v.getBlue();
}

// The desktop background with weather: rain falls across the whole desktop under the windows
// (stage command "weather rain"), a flash lights it for two ticks ("weather flash").
class TWeatherBackground : public TWibWobBackground
{
public:
    using TWibWobBackground::TWibWobBackground;
    int weather = 0;    // 0 clear, 1 rain
    // Everything that moves here is counted in stage ticks (StageHost::stageNow), so a seek shows what playing shows.
    int now = 0;        // the stage tick on screen
    int rainAt = 0;     // the tick the rain began: a drop falls a row a tick
    int flashAt = -10;  // the tick of the last flash: the sky is lit for that tick
    bool lit() const { return now == flashAt; }
    // Marks: the desktop's layers (stage command "mark"), drawn under every window in z order, then by name.
    // Line art draws in one EGA ink; a PAINT file or picture film is a colour picture (picture.h, docs/stage.md).
    // start: the tick the file was placed; a file that animates on its own (frame -1) steps every 3 ticks from it.
    struct Ink { StageMark m; int start = 0; std::shared_ptr<pic::Picture> pic; std::vector<std::vector<std::string>> frames; };
    std::map<std::string, Ink> inks;
    std::vector<uint32_t> underList;   // the under-glyph list of the last drawLayers (kUnderWords per cell)
    bool underSent = false;        // the page holds a non-empty list
    bool dirty = false;     // a mark changed since the last full redraw (redraw() clears it; a draw for an exposed
                            // region does not, since it may cover only part of the desktop)
    void redraw() { dirty = false; drawView(); }

    // A mark whose file animates on its own (frame -1, more than one frame) needs a draw every tick.
    bool freeRunning() const
    {
        for (auto& kv : inks)
            if (kv.second.m.frame < 0 && (kv.second.pic ? kv.second.pic->frames() : (int) kv.second.frames.size()) > 1) return true;
        return false;
    }

    void setInk(const std::string& name, const StageMark& m)
    {
        dirty = true;
        if (m.path.empty()) { inks.erase(name); return; }
        auto it = inks.find(name);
        if (it != inks.end() && it->second.m.path == m.path) { it->second.m = m; return; }   // same file: a glide keeps its frames going
        Ink k; k.m = m; k.start = now;
        k.pic = pic::get(m.path);
        if (!k.pic) {
            std::ifstream f(m.path, std::ios::binary);
            std::string line; std::vector<std::string> cur;
            while (std::getline(f, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.compare(0, 4, "FPS=") == 0) continue;
                if (line == "----") { if (!cur.empty()) k.frames.push_back(cur); cur.clear(); continue; }
                cur.push_back(line);
            }
            if (!cur.empty()) k.frames.push_back(cur);
        }
        inks[name] = std::move(k);
    }

    // The frame a mark shows at the tick on screen: the stage's choice, or every 3 ticks from the tick it was placed.
    int frameOf(const Ink& k, int n) const { return k.m.frame >= 0 ? k.m.frame % n : (std::max(0, now - k.start) / 3) % n; }

    // The x-ray map (StageHost::stageMarkRect): where drawLayers puts the mark NAME and the frame it shows. A wrapped
    // mark covers the whole desktop along its wrap.
    bool markRect(const std::string& name, int& x, int& y, int& w, int& h, int& frame, int& frames) const
    {
        auto it = inks.find(name);
        if (it == inks.end()) return false;
        const Ink& k = it->second;
        x = k.m.x; y = k.m.y; w = h = 0;
        if (k.pic) {
            frames = k.pic->frames();
            if (frames == 0) return false;
            frame = frameOf(k, frames); w = k.pic->w; h = k.pic->h;
        } else {
            frames = (int) k.frames.size();
            if (frames == 0) return false;
            frame = frameOf(k, frames);
            for (auto& s : k.frames[frame]) w = std::max(w, (int) TText::width(TStringView(s.c_str(), s.size())));
            h = (int) k.frames[frame].size();
        }
        if (k.m.right) x = size.x - w - 1 + k.m.x;
        if (k.m.bottom) y = size.y - h + k.m.y;
        if (k.m.wrapX) { x = 0; w = size.x; }
        if (k.m.wrapY) { y = 0; h = size.y; }
        return w > 0 && h > 0;
    }

    // The desktop under the marks, one row: the pattern, or the rain sky.
    void skyRow(pic::Row& b, int y)
    {
        const bool flash = lit();
        const int phase = now - rainAt;
        if (!weather && !flash) { b.moveChar(0, getPattern(), baseAttr(), size.x); return; }
        const TColorRGB sky = flash ? TColorRGB(0xE8, 0xE8, 0xF4) : TColorRGB(0x06, 0x06, 0x0C);
        const TColorAttr ground(flash ? TColorRGB(0x60, 0x60, 0x80) : TColorRGB(0x1A, 0x1A, 0x2C), sky);
        const TColorAttr drop(flash ? TColorRGB(0x30, 0x30, 0x50) : TColorRGB(0x9A, 0xB4, 0xD0), sky);
        const TColorAttr fine(flash ? TColorRGB(0x50, 0x50, 0x70) : TColorRGB(0x4A, 0x5A, 0x78), sky);
        b.moveChar(0, getPattern(), ground, size.x);
        if (weather)
            for (int x = 0; x < size.x; ++x) {
                // Each column has its own drops; a drop at row r at tick t is at row r+1 at
                // tick t+1 (hash of y - phase), so it falls straight. The cell above a head
                // shows its trail.
                auto H = [](unsigned xx, unsigned yy) {
                    unsigned h = xx * 374761393u ^ yy * 668265263u;
                    h ^= h >> 13; h *= 1274126177u; h ^= h >> 16; return h % 37;
                };
                unsigned yy = (unsigned) (y - phase);
                unsigned here = H((unsigned) x, yy), below = H((unsigned) x, yy + 1);
                if (here == 0)       { b.putChar(x, '|');  b.putAttribute(x, drop); }
                else if (below == 0) { b.putChar(x, '\''); b.putAttribute(x, fine); }
                else if (here == 1)  { b.putChar(x, '.');  b.putAttribute(x, fine); }
            }
    }

    TColorAttr underAttr() const
    {
        const bool flash = lit();
        if (!weather && !flash) return baseAttr();
        const TColorRGB sky = flash ? TColorRGB(0xE8, 0xE8, 0xF4) : TColorRGB(0x06, 0x06, 0x0C);
        return TColorAttr(flash ? TColorRGB(0x60, 0x60, 0x80) : TColorRGB(0x1A, 0x1A, 0x2C), sky);
    }

    // Every row composed in one buffer (sky, then each mark in order) and written once.
    void drawLayers()
    {
        // Paper for an inked line-art cell: the average colour of the desktop pattern, so the cell does not show as a block.
        const TColorAttr under = underAttr();
        const unsigned char pc = (unsigned char) getPattern();
        const double mix = pc == 0xB0 ? 0.25 : pc == 0xB1 ? 0.5 : pc == 0xB2 ? 0.75 : 0.0;
        auto rgb = [](TColor c, bool fg) {
            if (c.isRGB()) return c.asRGB();
            if (c.isBIOS()) return ww::egaRGB(c.asBIOS());
            return fg ? TColorRGB(0xAA, 0xAA, 0xAA) : TColorRGB(0, 0, 0);
        };
        TColorRGB f = rgb(under.getForeground(), true), g = rgb(under.getBackground(), false);
        auto ch = [&](int a, int b) { return (uint8_t) std::lround(a * mix + b * (1 - mix)); };
        const TColorRGB paper(ch(f.getRed(), g.getRed()), ch(f.getGreen(), g.getGreen()), ch(f.getBlue(), g.getBlue()));

        struct L { const Ink* k; const std::vector<pic::Cell>* fr; const std::vector<std::string>* art; std::vector<TColorRGB> pal; int x0, y0, w, h; };
        std::vector<std::pair<std::pair<int, std::string>, const Ink*>> order;
        for (auto& kv : inks) order.push_back({{kv.second.m.z, kv.first}, &kv.second});
        std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<L> ls;
        for (auto& o : order) {
            const Ink& k = *o.second;
            L l {&k, nullptr, nullptr, {}, k.m.x, k.m.y, 0, 0};
            if (k.pic) {
                int n = k.pic->frames();
                if (n == 0) continue;
                l.fr = &k.pic->frame(frameOf(k, n));
                l.w = k.pic->w; l.h = k.pic->h;
                l.pal = k.pic->pal;
                if (k.m.fx.any()) for (auto& c : l.pal) c = pic::apply(k.m.fx, c);
            } else {
                if (k.frames.empty()) continue;
                l.art = &k.frames[frameOf(k, (int) k.frames.size())];
                for (auto& s : *l.art) l.w = std::max(l.w, (int) TText::width(TStringView(s.c_str(), s.size())));
                l.h = (int) l.art->size();
            }
            // right / bottom: against the desktop's real right / bottom edge (line art and pictures alike)
            if (k.m.right) l.x0 = size.x - l.w - 1 + k.m.x;   // x, y: the drift from that edge
            if (k.m.bottom) l.y0 = size.y - l.h + k.m.y;
            if (l.w <= 0 || l.h <= 0) continue;
            ls.push_back(std::move(l));
        }
        // Under-glyphs: per column, what the layers left there (0 the desktop, 1 a line-art glyph, 2 a picture cell).
        const TPoint org = makeGlobal(TPoint {0, 0});
        underList.clear();
        std::vector<uint8_t> kind(size.x);
        std::vector<uint32_t> mark(size.x), markInk(size.x);
        std::vector<TScreenCell> sky(size.x), before;
        const uint32_t paperRGB = ((uint32_t) paper.getRed() << 16) | ((uint32_t) paper.getGreen() << 8) | paper.getBlue();
        TScreenCell patCell {};
        patCell.character.initWithChar(getPattern());
        const uint32_t patCp = cellCodepoint(patCell);   // only a cell showing the desktop's own pattern counts (not a raindrop)
        // Only a shade pattern (░ ▒ ▓) gets under-glyphs: its line-art cells have the blended paper that reads as a
        // patch. On a plain desktop, or a pattern of another character (noise's %), the paper is the pattern's own.
        const bool patterned = patCp >= 0x2591 && patCp <= 0x2593;
        for (int y = 0; y < size.y; ++y) {
            pic::Row b;
            skyRow(b, y);
            std::copy(b.cells(), b.cells() + size.x, sky.begin());
            std::fill(kind.begin(), kind.end(), 0);
            for (auto& l : ls) {
                int r = y - l.y0;
                if (l.k->m.wrapY) r = ((r % l.h) + l.h) % l.h;
                if (r < 0 || r >= l.h) continue;
                int xa = l.x0;
                if (l.k->m.wrapX) xa = ((l.x0 % l.w) + l.w) % l.w - l.w;
                for (int x0 = xa; x0 < size.x; x0 += l.w) {
                    if (l.fr) {
                        const int a = std::max(0, x0), e = std::min(size.x, x0 + l.w);
                        if (a < e) before.assign(b.cells() + a, b.cells() + e);
                        pic::drawRow(*l.k->pic, *l.fr, r, b.cells(), size.x, x0, l.pal.data());
                        for (int x = a; x < e; ++x) if (b.cells()[x] != before[x - a]) kind[x] = 2;
                    }
                    else {
                        const TColorAttr ink(ww::egaRGB(l.k->m.ink), paper);
                        const std::string& s = (*l.art)[r];
                        int col = 0; size_t i = 0;
                        while (i < s.size()) {
                            size_t n = 1; unsigned char c = s[i];
                            if (c >= 0xF0) n = 4; else if (c >= 0xE0) n = 3; else if (c >= 0xC0) n = 2;
                            std::string gl = s.substr(i, n); i += n;
                            int cw = std::max(1, (int) TText::width(TStringView(gl.c_str(), gl.size())));
                            int X = x0 + col;
                            if (gl != " " && X >= 0 && X + cw <= size.x) {
                                b.moveStr(X, gl.c_str(), ink);
                                // a wide glyph, or a line over a picture cell, gets no under-glyph
                                for (int j = 0; j < cw; ++j) kind[X + j] = cw == 1 && kind[X + j] != 2 ? 1 : 2;
                                if (cw == 1) { mark[X] = cellCodepoint(b.cells()[X]); markInk[X] = gridRGB(ink.getForeground(), true); }
                            }
                            col += cw;
                        }
                    }
                    if (!l.k->m.wrapX) break;
                }
            }
            for (int x = 0; x < size.x; ++x) {
                if (kind[x] != 1 || !patterned) continue;
                const uint32_t u = cellCodepoint(sky[x]);
                if (u != patCp) continue;
                const TColorAttr sa = sky[x].attribute;
                underList.insert(underList.end(), {(uint32_t) (org.x + x) | ((uint32_t) (org.y + y) << 16), mark[x], markInk[x], paperRGB,
                                           u, gridRGB(sa.getForeground(), true), gridRGB(sa.getBackground(), false)});
            }
            writeLine(0, y, size.x, 1, b);
        }
        if (!underList.empty() || underSent) wwdos_js_under(underList.data(), (int) (underList.size() / kUnderWords));
        underSent = !underList.empty();
    }

    void draw() override
    {
        auto t0 = std::chrono::steady_clock::now();
        if (inks.empty() && underSent) { underList.clear(); wwdos_js_under(nullptr, 0); underSent = false; }
        if (!inks.empty()) drawLayers();
        else if (!weather && !lit()) TWibWobBackground::draw();
        else for (int y = 0; y < size.y; ++y) { pic::Row b; skyRow(b, y); writeLine(0, y, size.x, 1, b); }
        g_bgDrawMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        ++g_bgDraws;
    }
};

static const ushort cmOpenPrimer = 109;
static const ushort cmAbout = 200;

static const ushort cmSendToBack = 202;  // cmCloseAll comes from tvision
static const ushort cmToggleBars = 210;  // viewer keys, same in every page: F4 bars, F9 pause, F8 step
static const ushort cmPause = 211;
static const ushort cmStep = 212;
static const ushort cmNextSection = 213;
static const ushort cmPrevSection = 214;
static const ushort cmFullscreen = 215;
static const ushort cmKeys = 216;
static const ushort cmMute = 217;
static bool g_scrub = false;             // stage command "scrubber on": a timeline in the status line
static bool g_scrubWide = false;         // "scrubber wide": the two-row timeline on the desktop's bottom rows
static int g_stepReq = 0;                // wwdos_stage_step(N): ticks to run while paused (served at once from idle)
static int g_served = 0;                 // seek and step requests served from idle, screen flushed: tools wait on it
static double g_jumpReq = -1;            // wwdosStage.jump(BAR): a bar to queue at the next stage timer event
static bool g_muted = false;             // M, File > Mute, or the sound icon at the bottom right of the status line

extern bool ww_anim_paused;      // frame_file_player_view.cpp
extern double (*ww_anim_clock)();
extern unsigned ww_anim_step;

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Page options (URL parameters, or tokenData baked into an NFT): Module.wwdosOpts = {bars: 0|1, paused: 0|1}.
EM_JS(int, wwdos_js_opt, (const char *name), {
    var o = Module.wwdosOpts, k = UTF8ToString(name);
    return o && o[k] !== undefined ? (o[k] | 0) : -1;
});
// Sound: the page plays a clip or a synthesised effect by name (web/shell-webgl.html, Module.wwdosSound).
EM_JS(void, wwdos_js_sound, (const char *name), {
    if (Module.wwdosSound) Module.wwdosSound(UTF8ToString(name));
});
// Command trace for tests and tools: window.wwdosTrace = [{t: script seconds, ms: performance.now(), cmd, warn?}].
EM_JS(void, wwdos_js_trace, (double t, const char *line, const char *warn), {
    var a = window.wwdosTrace || (window.wwdosTrace = []), e = {t: t, ms: performance.now(), cmd: UTF8ToString(line)};
    if (warn) e.warn = UTF8ToString(warn);
    if (a.push(e) > 5000) a.shift();
});
// The page's audio clock as script time, or -1 with no sound running (Module.wwdosAudioTime, web/shell-webgl.html).
EM_JS(double, wwdos_js_audio_time, (), { return Module.wwdosAudioTime ? Module.wwdosAudioTime() : -1; });
// Section names and bars for wwdosStage.sectionsList(), published once when the stage loads.
EM_JS(void, wwdos_js_sections, (const char *json), { window.wwdosSections = JSON.parse(UTF8ToString(json)); });
// Stage gate: 1 lets the clock run on (sound on already, no sounds, ?t= or ?autostart=1; web/shell-webgl.html).
EM_JS(int, wwdos_js_gate_pass, (), { return Module.wwdosGatePass ? Module.wwdosGatePass() : 1; });
// "xray on|off|toggle|inspect" from a live line: the page's x-ray view (web/shell-webgl.html, Module.wwdosXray).
EM_JS(void, wwdos_js_xray, (const char *mode), { if (Module.wwdosXray) Module.wwdosXray(UTF8ToString(mode)); });
#else
// Native build (wwdos-stage, tools/stage_term.py): command-line options stand in for the page.
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <unistd.h>
extern char **environ;
static std::string g_bootDir = "/boot";        // --boot DIR
static int g_optBars = -1, g_optPaused = -1;   // --bars 0|1, --paused
static bool g_autostart = false;               // --autostart or --t: the gate passes at once
static FILE* g_traceFile = nullptr;            // --trace FILE: one line per stage command
static std::vector<pid_t> g_players;
std::string bootPath(const std::string& p)
{
    return p.compare(0, 6, "/boot/") == 0 ? g_bootDir + p.substr(5) : p;
}
static int wwdos_js_opt(const char *name)
{
    if (!strcmp(name, "bars")) return g_optBars;
    if (!strcmp(name, "paused")) return g_optPaused;
    return -1;
}
// Clips are DIR/sounds/NAME.* (tools/stage_term.py converts them to m4a); afplay plays one without blocking.
// Window effects (~open and the rest) are synthesised by the page only and stay silent here.
static void stopPlayers();
static void reapPlayers()
{
    for (size_t i = 0; i < g_players.size(); )
        if (waitpid(g_players[i], nullptr, WNOHANG) == g_players[i]) g_players.erase(g_players.begin() + i); else ++i;
}
// Page commands natively: pause and resume stop and continue the running players (SIGSTOP/SIGCONT), stop ends them
// (a seek sends it before it restarts clips as "NAME@SECONDS"). Effects (~open and the rest) stay silent.
static void wwdos_js_sound(const char *name)
{
    reapPlayers();
    if (name[0] == '~') return;
    if (name[0] == '!') {
        if (!strcmp(name, "!pause")) for (pid_t p : g_players) kill(p, SIGSTOP);
        else if (!strcmp(name, "!resume")) for (pid_t p : g_players) kill(p, SIGCONT);
        else if (!strcmp(name, "!stop")) stopPlayers();
        else if (!strcmp(name, "!mute")) stopPlayers();
        return;
    }
    if (g_muted) return;
    std::string clip = name, from;
    size_t hash = clip.find('#');                   // "NAME#SECONDS": the exact script time; no scheduler here, play now
    if (hash != std::string::npos) clip = clip.substr(0, hash);
    size_t at = clip.find('@');                     // "NAME@SECONDS": start that far into the clip (after a seek)
    if (at != std::string::npos) { from = clip.substr(at + 1); clip = clip.substr(0, at); }
    static std::map<std::string, double> durs;      // sounds/durations.txt, written by tools/stage_term.py
    if (durs.empty()) {
        std::ifstream df(g_bootDir + "/sounds/durations.txt");
        std::string n; double s;
        while (df >> n >> s) durs[n] = s;
        durs[""] = 0;
    }
    if (!from.empty() && durs.count(clip) && atof(from.c_str()) >= durs[clip] - 0.05) return;   // that clip has ended
    static const char* exts[] = {".m4a", ".mp3", ".wav", ".aiff"};
    for (const char* e : exts) {
        std::string f = g_bootDir + "/sounds/" + clip + e;
        struct stat st;
        if (stat(f.c_str(), &st) != 0) continue;
        // afplay cannot start mid-file; a clip that starts part-way plays through ffplay
        bool offset = !from.empty() && atof(from.c_str()) > 0.05;
        const char* player = offset ? "ffplay" : (getenv("WWDOS_PLAYER") ? getenv("WWDOS_PLAYER") : "afplay");
        std::vector<char*> argv;
        argv.push_back((char*) player);
        if (offset) for (const char* a : {"-nodisp", "-autoexit", "-loglevel", "quiet", "-ss"}) argv.push_back((char*) a);
        if (offset) argv.push_back((char*) from.c_str());
        argv.push_back((char*) f.c_str());
        argv.push_back(nullptr);
        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);   // keep the player off the screen
        posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
        pid_t pid;
        if (posix_spawnp(&pid, player, &fa, nullptr, argv.data(), environ) == 0) g_players.push_back(pid);
        posix_spawn_file_actions_destroy(&fa);
        return;
    }
}
static void stopPlayers() { for (pid_t p : g_players) { kill(p, SIGCONT); kill(p, SIGTERM); } g_players.clear(); }
static void wwdos_js_trace(double t, const char *line, const char *warn)
{
    if (!g_traceFile) return;
    std::string s = line;   // paths back to the /boot names the stage source uses
    for (size_t at = 0; (at = s.find(g_bootDir + "/", at)) != std::string::npos; ) s.replace(at, g_bootDir.size(), "/boot");
    fprintf(g_traceFile, "%.3f\t%s%s%s\n", t, s.c_str(), warn ? "\twarn: " : "", warn ? warn : "");
    fflush(g_traceFile);
}
static int wwdos_js_gate_pass() { return g_autostart; }
static void wwdos_js_xray(const char*) {}   // the terminal build has no x-ray view (tools/stage.py xray reads the page)
static void wwdos_js_sections(const char*) {}
static double wwdos_js_audio_time() { return -1; }
#endif

// Seek from the page (window.wwdosStage, ?t=): the request waits for the next stage timer event, so the
// app never changes its windows from inside a JavaScript call.
static Stage* g_stage = nullptr;
static double g_seekTo = -1;
static Stage* g_liveStage = nullptr;          // the app's stage, loaded or not: wwdosStage.run works on any page
static std::vector<std::string> g_liveQueue;  // checked live lines waiting for TMinApp::idle
static std::string g_liveErr;
static bool g_liveRedrawStatus = false;
#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif
extern "C" {
EMSCRIPTEN_KEEPALIVE double wwdos_stage_time() { return g_stage ? g_stage->time() : 0; }
// The tick on screen, in seconds: one tick behind wwdos_stage_time() while playing, equal to it after a seek.
EMSCRIPTEN_KEEPALIVE double wwdos_stage_shown() { return g_stage ? g_stage->shownTime() : 0; }
EMSCRIPTEN_KEEPALIVE double wwdos_stage_length() { return g_stage ? g_stage->length() : 0; }
EMSCRIPTEN_KEEPALIVE void wwdos_stage_seek(double secs) { g_seekTo = std::max(0.0, secs); }
// Tools (tests/stage/test_seek_play.py): run N ticks at the next stage timer event while paused, as playing would.
EMSCRIPTEN_KEEPALIVE void wwdos_stage_step(int n) { g_stepReq += std::max(0, n); }
// How many seek or step requests have been served on a paused page with the screen flushed after each. A tool asks,
// wakes the app, and reads the grid as soon as this number moves, without waiting for a stage timer event.
EMSCRIPTEN_KEEPALIVE int wwdos_stage_served() { return g_served; }
// A jump from the scrubber, PgDn/PgUp or the page: queued under the launch quantise; at once while paused or at a gate.
static void jumpTo(double t)
{
    if (!g_stage) return;
    if (ww_anim_paused || g_stage->waiting()) { g_stage->cancelJump(); g_seekTo = t; return; }
    g_stage->queueJump(t, false);
}
EMSCRIPTEN_KEEPALIVE int wwdos_stage_waiting() { return g_stage && g_stage->waiting(); }
EMSCRIPTEN_KEEPALIVE void wwdos_stage_go() { if (g_stage) g_stage->go(); }
// Playing a film live (docs/stage.md): bars from bar 1 of the film, a queued jump, the launch quantise.
EMSCRIPTEN_KEEPALIVE int wwdos_stage_bars() { return g_stage ? g_stage->bars() : 0; }
EMSCRIPTEN_KEEPALIVE double wwdos_stage_bar() { return g_stage ? g_stage->barAt(g_stage->time()) : 0; }
EMSCRIPTEN_KEEPALIVE void wwdos_stage_jump(double bar) { g_jumpReq = std::max(1.0, bar); }
EMSCRIPTEN_KEEPALIVE int wwdos_stage_quantise(int mode) { if (g_stage && mode >= 0 && mode <= 2) g_stage->quant = mode; return g_stage ? g_stage->quant : -1; }
EMSCRIPTEN_KEEPALIVE double wwdos_stage_queued() { return g_stage && g_stage->jumpQueued() ? g_stage->barAt(g_stage->jumpShown()) : -1; }
EMSCRIPTEN_KEEPALIVE double wwdos_stage_seek_ms() { return g_stage ? g_stage->lastSeekMs : 0; }
// Desktop draw cost for tools/paint_bench.py: total milliseconds and number of draws since the page started.
EMSCRIPTEN_KEEPALIVE double wwdos_bg_draw_ms() { return g_bgDrawMs; }
EMSCRIPTEN_KEEPALIVE int wwdos_bg_draws() { return g_bgDraws; }
EMSCRIPTEN_KEEPALIVE double wwdos_tick_ms() { return g_tickMs; }
EMSCRIPTEN_KEEPALIVE int wwdos_ticks() { return g_ticks; }
EMSCRIPTEN_KEEPALIVE double wwdos_stage_land() { return g_stage && g_stage->jumpQueued() ? g_stage->jumpLand() : -1; }
// wwdosStage.run(LINE): one stage command now. The line is checked here and applied at the app's next idle call
// (TMinApp::idle), so windows never change from inside a JavaScript call. Returns "" or why the line cannot run.
EMSCRIPTEN_KEEPALIVE const char* wwdos_stage_run(const char* line)
{
    static std::string ret;
    if (!g_liveStage) { ret = "the app is not up yet"; return ret.c_str(); }
    std::set<std::string> pending;
    for (auto& l : g_liveQueue) {
        std::istringstream in(l); std::string c, n; in >> c >> n;
        if (c == "open") pending.insert(n);
    }
    ret = g_liveStage->checkLive(line ? line : "", pending);
    if (ret.empty()) g_liveQueue.push_back(line);
    return ret.c_str();
}
// wwdosStage.windows(): the stage's windows, front first, as JSON (Stage::windowsJson).
EMSCRIPTEN_KEEPALIVE const char* wwdos_stage_windows()
{
    static std::string ret;
    ret = g_liveStage ? g_liveStage->windowsJson() : "[]";
    return ret.c_str();
}
// The x-ray map (Stage::objectsJson): every window and mark with its kind, rect and frame.
EMSCRIPTEN_KEEPALIVE const char* wwdos_stage_objects()
{
    static std::string ret;
    ret = g_liveStage ? g_liveStage->objectsJson() : "{}";
    return ret.c_str();
}
// Why the last applied live command did nothing ("" when it worked).
EMSCRIPTEN_KEEPALIVE const char* wwdos_stage_live_error() { return g_liveErr.c_str(); }
// on 1 pauses, 0 plays, -1 asks; returns 1 when paused. Same effect as F9.
EMSCRIPTEN_KEEPALIVE int wwdos_stage_pause(int on)
{
    if (on >= 0 && bool(on) != ww_anim_paused) { ww_anim_paused = on; wwdos_js_sound(on ? "!pause" : "!resume"); g_liveRedrawStatus = true; }
    return ww_anim_paused ? 1 : 0;
}
}

#ifdef __EMSCRIPTEN__
static const char* kPrimerGlob = "/boot/*.txt";
#else
static const char* kPrimerGlob = "*.txt";   // main() changes into the boot directory
#endif

class TMinMenuBar : public TMenuBar
{
public:
    TMinMenuBar(const TRect& bounds, TSubMenu& aMenu) : TMenuBar(bounds, aMenu) {}

    TColorAttr mapColor(uchar index) noexcept override
    {
        if (ww::isEga()) return TMenuBar::mapColor(index);
        switch (index) {
            case 1: case 3: case 4: case 6:
                return TColorAttr(TColorRGB(0, 0, 0), TColorRGB(255, 255, 255));
            default:
                return TMenuBar::mapColor(index);
        }
    }

    void draw() override
    {
        TMenuBar::draw();
        const char* kaomoji = "つ◕‿◕‿◕༽つ";
        const int w = 12;
        int x = size.x - w;
        if (x > 1) {
            TDrawBuffer b;
            TAttrPair c = getColor(0x0301);
            b.moveChar(0, ' ', c, w);
            b.moveStr(0, kaomoji, c);
            writeBuf(x, 0, w, 1, b);
        }
    }
};

// The page ships /boot/stage.txt (written before main runs), so menus can be chosen before the stage loads.
static bool isStagePage()
{
    return std::ifstream(bootPath("/boot/stage.txt")).good();
}

class TMinStatusLine : public TStatusLine
{
public:
    TMinStatusLine(const TRect& bounds, TStatusDef& defs) : TStatusLine(bounds, defs) {}

    const char* hint(ushort) noexcept override { return g_hint.c_str(); }

    // A sound icon at the right end of the status line: click it to mute or unmute.
    static const char* soundLabel() { return g_muted ? " ♪ ~off~ " : " ♪ on  "; }   // "off" in the hotkey red
    // The scrubber (stage command "scrubber on"), left of the sound icon:
    //   [►] 1━━2━━━━●3────4──5 0:31/1:17
    // a play/pause button, a line from the film's first section to its end with a head at the present and a number
    // where each section starts, and the time. A click on the button pauses or plays; a click on the line jumps to
    // the start of the section under it (or to that moment, when the film has no sections).
    enum { SCRUB_W = 20, SCRUB_ALL = 5 + SCRUB_W + 1 + 9 };
    int scrubX() const { return size.x - cstrlen(soundLabel()) - SCRUB_ALL - 1; }
    bool scrubOn() const { return g_scrub && g_stage && scrubX() > 30 && g_stage->length() > g_stage->filmStart(); }
    int scrubFill() const
    {
        double t0 = g_stage->filmStart(), len = g_stage->length() - t0;
        return std::max(0, std::min<int>(SCRUB_W - 1, int((g_stage->shownTime() - t0) / len * SCRUB_W)));
    }
    int scrubKey() const { return scrubFill() * 4000 + int(std::max(0.0, g_stage->shownTime() - g_stage->filmStart())) + (ww_anim_paused ? 2000000 : 0); }
    void draw() override
    {
        TStatusLine::draw();
        if (!isStagePage()) return;
        TDrawBuffer b;
        int w = cstrlen(soundLabel());
        b.moveCStr(0, soundLabel(), getColor(0x0201));
        writeLine(size.x - w, 0, w, 1, b);
        if (!scrubOn()) return;
        double t0 = g_stage->filmStart(), len = g_stage->length() - t0;
        int head = scrubFill();
        std::string bar = ww_anim_paused ? " [\xe2\x96\xba] " : " [\xe2\x95\x91] ";   // ► when paused (press to play), ║ while playing
        const auto& secs = g_stage->sectionTimes();
        for (int i = 0; i < SCRUB_W; ++i) {
            int mark = -1;
            for (size_t k = 0; k < secs.size(); ++k)
                if (int((secs[k] - t0) / len * SCRUB_W) == i) mark = int(k);
            if (i == head) bar += "\xe2\x97\x8f";                                   // ● the present
            else if (mark >= 0) bar += char(mark < 9 ? '1' + mark : 'a' + mark - 9);  // a section starts here
            else bar += i < head ? "\xe2\x94\x81" : "\xe2\x94\x80";               // ━ played, ─ to come
        }
        int now = int(std::max(0.0, g_stage->shownTime() - t0)), all = int(len);
        char tm[24]; snprintf(tm, sizeof tm, " %d:%02d/%d:%02d", now / 60, now % 60, all / 60, all % 60);
        bar += tm;
        TDrawBuffer s;
        s.moveChar(0, ' ', getColor(0x0201), SCRUB_ALL);
        s.moveStr(0, bar.c_str(), getColor(0x0201));
        writeLine(scrubX(), 0, SCRUB_ALL, 1, s);
    }
    void handleEvent(TEvent& event) override
    {
        if (event.what == evMouseDown && isStagePage()) {
            TPoint p = makeLocal(event.mouse.where);
            if (scrubOn() && p.x >= scrubX() && p.x < scrubX() + 4) {          // the play/pause button
                TEvent e; e.what = evCommand; e.message.command = cmPause; e.message.infoPtr = nullptr;
                putEvent(e);
                clearEvent(event);
                return;
            }
            if (scrubOn() && p.x >= scrubX() + 5 && p.x < scrubX() + 5 + SCRUB_W) {
                double t0 = g_stage->filmStart(), len = g_stage->length() - t0;
                double t = t0 + (p.x - scrubX() - 5 + 0.5) / SCRUB_W * len, to = t;
                const auto& secs = g_stage->sectionTimes();
                if (!secs.empty()) { to = secs.front(); for (double s : secs) if (s <= t) to = s; }
                g_seekTo = to;
                clearEvent(event);
                return;
            }
            if (p.x >= size.x - cstrlen(soundLabel())) {
                TEvent e; e.what = evCommand; e.message.command = cmMute; e.message.infoPtr = nullptr;
                putEvent(e);
                clearEvent(event);
                return;
            }
        }
        TStatusLine::handleEvent(event);
    }

    TColorAttr mapColor(uchar index) noexcept override
    {
        if (ww::isEga()) return TStatusLine::mapColor(index);
        switch (index) {
            case 1: case 2: case 3: case 4:
                return TColorAttr(TColorRGB(0, 0, 0), TColorRGB(255, 255, 255));
            default:
                return TStatusLine::mapColor(index);
        }
    }
};


// The wide scrubber (stage command "scrubber wide"): two rows across the desktop's bottom edge, in front of the
// desktop so no stage window covers it. Upper row: section names at their first bar, the playing section reversed.
// Lower row: one cell per bar (N bars a cell when they do not fit), played cells solid, the head, section starts.
// The right end shows the launch quantise (Q:BAR, Q:BEAT, Q:OFF), the queued bar, and bar/total (xN when N > 1).
// A click on a bar or a section name queues a jump (Stage::queueJump); a click on the quantise label cycles it.
class TWideScrub : public TView
{
public:
    enum { RIGHT = 12 };
    int blink = 0;                      // ticks, for the queued target's blink
    explicit TWideScrub(const TRect& r) : TView(r) { growMode = gfGrowLoY | gfGrowHiY | gfGrowHiX; eventMask |= evMouseDown; }

    int cellsW() const { return size.x - RIGHT; }
    int perCell() const { int n = g_stage->bars(), w = cellsW(); return w > 0 ? std::max(1, (n + w - 1) / w) : 1; }
    int barIndex(double t) const { return (int) std::floor(g_stage->barAt(t) - 1 + 1e-6); }   // 0-based bar at t
    int playingSection() const
    {
        const auto& s = g_stage->sectionTimes(); int k = -1;
        for (size_t i = 0; i < s.size(); ++i) if (g_stage->shownTime() + 1e-6 >= s[i]) k = int(i);
        return k;
    }
    static const char* quantName(int q) { return q == Stage::QBAR ? "BAR" : q == Stage::QBEAT ? "BEAT" : "OFF"; }
    // What the rows show; draw() runs only when this changes.
    std::string key() const
    {
        if (!g_stage || cellsW() < 8) return "";
        char b[160];
        int q = g_stage->jumpQueued() ? barIndex(g_stage->jumpShown()) : -9;
        snprintf(b, sizeof b, "%d %d %d %d %d %d %d %d", size.x, barIndex(g_stage->shownTime()), playingSection(), g_stage->quant,
                 q, g_stage->jumpQueued() ? (blink / 2) % 2 : 0, g_stage->bars(), (int) ww::isEga());
        return b;
    }
    void draw() override
    {
        const TColorAttr norm(0x07), hi(0x70), head(0x0F), sec(0x08), queued(0xE0);
        TDrawBuffer up, lo;
        up.moveChar(0, ' ', norm, size.x);
        lo.moveChar(0, ' ', norm, size.x);
        if (g_stage && cellsW() >= 8) {
            const int nb = g_stage->bars(), N = perCell(), used = std::min(cellsW(), (nb + N - 1) / N);
            const int cur = barIndex(g_stage->shownTime()), headCol = cur >= 0 && cur < nb ? cur / N : -1;
            const bool qOn = g_stage->jumpQueued(), lit = qOn && (blink / 2) % 2 == 0;
            const int qCol = qOn ? barIndex(g_stage->jumpShown()) / N : -1;
            const auto& secs = g_stage->sectionTimes();
            const auto& names = g_stage->sectionNames();
            std::vector<int> secCol;
            for (double s : secs) secCol.push_back(std::max(0, std::min(used - 1, barIndex(s + 1e-3) / N)));
            const int play = playingSection();
            for (size_t i = 0; i < secs.size(); ++i) {
                int a = secCol[i], e = i + 1 < secs.size() ? secCol[i + 1] : used;
                if (e <= a) continue;
                bool isQ = qOn && std::fabs(g_stage->jumpShown() - secs[i]) < 1e-3;
                TColorAttr at = isQ && lit ? queued : int(i) == play ? hi : norm;
                up.moveChar(a, ' ', at, e - a);
                std::string nm = i < names.size() ? names[i] : "";
                int room = std::max(1, e - a - 1);
                while ((int) TText::width(TStringView(nm.c_str(), nm.size())) > room && !nm.empty()) nm.pop_back();
                up.moveStr(a, nm.c_str(), at);
            }
            for (int c = 0; c < used; ++c) {
                bool start = std::find(secCol.begin(), secCol.end(), c) != secCol.end();
                const char* g; TColorAttr at = norm;
                if (c == headCol) { g = "\xe2\x96\xb6"; at = head; }                    // ▶ the bar playing
                else if (c < headCol) g = start ? "\xe2\x96\x8c" : "\xe2\x96\x88";   // ▌ █ played
                else { g = start ? "\xe2\x94\x82" : "\xc2\xb7"; at = start ? norm : sec; }   // │ · to come
                if (c == qCol && lit) { g = "\xe2\x97\x86"; at = queued; }              // ◆ the queued bar, blinking
                lo.moveStr(c, g, at);
            }
            char r1[32], r2[32];
            if (qOn) snprintf(r1, sizeof r1, " Q:%s >%d", quantName(g_stage->quant), barIndex(g_stage->jumpShown()) + 1);
            else snprintf(r1, sizeof r1, " Q:%s", quantName(g_stage->quant));
            if (cur >= 0) snprintf(r2, sizeof r2, N > 1 ? " %d/%d x%d" : " %d/%d", std::min(cur + 1, nb), nb, N);
            else snprintf(r2, sizeof r2, N > 1 ? " -/%d x%d" : " -/%d", nb, N);
            up.moveStr(size.x - RIGHT, r1, qOn ? head : norm);
            lo.moveStr(size.x - RIGHT, r2, norm);
        }
        writeLine(0, 0, size.x, 1, up);
        writeLine(0, 1, size.x, 1, lo);
    }
    void handleEvent(TEvent& event) override
    {
        TView::handleEvent(event);
        if (event.what != evMouseDown || !g_stage || cellsW() < 8) return;
        TPoint p = makeLocal(event.mouse.where);
        const int nb = g_stage->bars(), N = perCell(), used = (nb + N - 1) / N;
        if (p.x >= cellsW()) {
            if (p.y == 0) { g_stage->quant = (g_stage->quant + 2) % 3; drawView(); }   // BAR -> BEAT -> OFF -> BAR
        } else if (p.y == 1 && p.x < used) {
            jumpTo(g_stage->barStart(p.x * N + 1));
        } else if (p.y == 0) {
            const auto& secs = g_stage->sectionTimes();
            double to = -1;
            for (double s : secs) if (barIndex(s + 1e-3) / N <= p.x) to = s;
            if (to >= 0) jumpTo(to);
        }
        clearEvent(event);
    }
};

class TMinApp : public TApplication, public StageHost
{
public:
    TMinApp();
    void handleEvent(TEvent& event) override;
    void idle() override
    {
        TApplication::idle();
        // A timer may have drawn a view under a clear sprite (a drawing's own frame); draw the sprites again before
        // the screen is flushed.
        if (g_timerSeen) { g_timerSeen = false; stage.spriteSettle(); }
        serveTools();
        if (g_liveRedrawStatus) { g_liveRedrawStatus = false; if (statusLine) statusLine->drawView(); }
        if (g_liveQueue.empty()) return;
        std::vector<std::string> lines; lines.swap(g_liveQueue);
        for (auto& l : lines) g_liveErr = stage.runLive(l);
    }

    // A seek or a step asked for on a paused page is served here, in the same idle call that follows the page's
    // wake, and the screen is flushed, so a tool reads the result a few milliseconds later instead of waiting for
    // the next stage timer event (up to a tick). A playing page is left alone: its requests go with its timer.
    void serveTools()
    {
        if (!g_stage || !ww_anim_paused || (g_seekTo < 0 && g_stepReq <= 0)) return;
        stage.paused(true);
        stage.resized();
        if (g_seekTo >= 0) { stage.seek(g_seekTo); g_seekTo = -1; if (statusLine) statusLine->drawView(); }
        if (g_stepReq > 0) { int n = g_stepReq; g_stepReq = 0; while (n-- > 0) stage.tick(); stage.settle(); }
        stage.rebase();
        refreshWide();
        TScreen::flushScreen();
        ++g_served;
    }

    // The theme's palette with the eight window-colour families (indices 136..199), so "colour" frames draw right.
    TPalette& getPalette() const override { return ww::appPalette(); }

    // StageHost
    TDeskTop* stageDesk() override { return deskTop; }
    void stageTile() override { tile(); }
    void stageCascade() override { cascade(); }
    void stageBars(bool show) override { setBars(show); }
    void stageHint(const std::string& text) override { g_hint = text; if (statusLine) statusLine->drawView(); }
    void stageBackground(const std::string& preset) override
    {
        if (auto* bg = dynamic_cast<TWibWobBackground*>(deskTop->background)) bg->setPreset(preset);
    }
    void stageWeather(const std::string& kind) override
    {
        auto* bg = dynamic_cast<TWeatherBackground*>(deskTop->background);
        if (!bg) return;
        if (kind == "rain") { bg->weather = 1; bg->rainAt = bg->now; }
        else if (kind == "flash") bg->flashAt = bg->now;
        else { bg->weather = 0; bg->flashAt = -10; }
        bg->dirty = true;                        // drawn at the end of the tick, seek or live command (stageFlush)
    }
    // The desktop moves on the stage tick only: rain, the tick of a flash (and the tick after it), and a mark that
    // animates on its own. It is drawn once a tick, at stageFlush, after the tick's commands and glides.
    void stageNow(int tick) override
    {
        auto* bg = dynamic_cast<TWeatherBackground*>(deskTop->background);
        if (!bg || tick == bg->now) return;
        bool was = bg->lit();
        bg->now = tick;
        if (bg->weather || was || bg->lit() || bg->freeRunning()) bg->dirty = true;
    }
    void stageSettle(bool full) override { stageMirrorSettle(deskTop, full); }
    void stageEveryTick() override
    {
        if (g_scrub && statusLine) {                 // the scrubber moves a cell at a time
            static int shown = -1;
            auto* sl = static_cast<TMinStatusLine*>(statusLine);
            int f = sl->scrubOn() ? sl->scrubKey() : -1;
            if (f != shown) { shown = f; statusLine->drawView(); }
        }
    }

    void stageSound(const std::string& name) override { wwdos_js_sound(name.c_str()); }
    void stageScrubber(int mode) override
    {
        g_scrub = mode == 1; g_scrubWide = mode == 2;
        if (wide) { placeWide(); if (g_scrubWide) wide->show(); else wide->hide(); }
        if (statusLine) statusLine->drawView();
    }
    // Every stage timer event (playing, paused or after a seek): redraw the wide scrubber when what it shows changed.
    void refreshWide()
    {
        if (!wide) return;
        placeWide();
        static std::string shown;
        ++wide->blink;
        std::string k = g_scrubWide ? wide->key() : "";
        if (k != shown) { shown = k; if (g_scrubWide) wide->drawView(); }
    }
    // The wide scrubber sits on the desktop's bottom two rows (bars shown or hidden).
    void placeWide()
    {
        TRect d = deskTop->getBounds(), r(d.a.x, d.b.y - 2, d.b.x, d.b.y);
        if (r != wide->getBounds()) wide->locate(r);
    }
#ifdef __EMSCRIPTEN__
    double stageSoundLead() override { return 0.3; }   // the page schedules clips on the audio clock
#endif
    void stageTrace(double t, const std::string& line, const char* warn) override { wwdos_js_trace(t, line.c_str(), warn); }
    bool stageGatePass() override { return wwdos_js_gate_pass(); }
    double stageAudioTime() override { return wwdos_js_audio_time(); }
    // Fire the stage timer next in secs, then every period: the tick after a quantised jump meets the audio.
    void stageRetime(double secs) override
    {
        if (!stageTimer) return;
        killTimer(stageTimer);
        stageTimer = setTimer((uint) std::max(1L, std::lround(secs * 1000)), stage.periodMs());
        retimed = true;
    }
    void stageTheme(const std::string& name) override
    {
        ww::Theme t;
        if (!ww::parseTheme(name, t)) return;
        ww::setTheme(t);
        if (auto* bg = dynamic_cast<TWibWobBackground*>(deskTop->background)) bg->setPreset(ww::isEga() ? "ega" : "default", false);
        redraw();
    }
    bool stageMarkRect(const std::string& name, int& x, int& y, int& w, int& h, int& frame, int& frames) override
    {
        auto* bg = dynamic_cast<TWeatherBackground*>(deskTop->background);
        return bg && bg->markRect(name, x, y, w, h, frame, frames);
    }
    void stageXray(const std::string& mode) override { wwdos_js_xray(mode.c_str()); }
    void stageMark(const std::string& name, const StageMark& m) override
    {
        if (auto* bg = dynamic_cast<TWeatherBackground*>(deskTop->background)) bg->setInk(name, m);
    }
    void stageFlush() override
    {
        auto* bg = dynamic_cast<TWeatherBackground*>(deskTop->background);
        if (bg && bg->dirty) bg->redraw();
    }

    static TMenuBar* initMenuBar(TRect r);
    static TStatusLine* initStatusLine(TRect r);
    static TDeskTop* initDeskTop(TRect r);

private:
    bool barsHidden = false;
    Stage stage {*this};
    TTimerId stageTimer {0};
    unsigned stageStepSeen = 0;
    TWideScrub* wide = nullptr;
    bool retimed = false;                 // stageRetime ran: the ticks owed in this timer event are dropped
    void queueSection(int dir)
    {
        double from = stage.jumpQueued() ? stage.jumpShown() : stage.time();
        double s = stage.sectionTimeFrom(from, dir);
        if (s >= 0) jumpTo(s);
    }
    void setBars(bool show);
    void openPrimer();
    void openPrimerAt(const std::string& path, const TRect& r, const char* title);
    TRect boundsFor(const std::string& path);
    void closeAll();
};

// A page can supply its own scene: it writes /boot/scene.txt (and the primers it names)
// before main runs. Format, one item per line:
//   size W H                      the desktop the scene was laid out for
//   x y w h /path/to/file.txt|title
// On a smaller desktop, positions scale down to fit; window sizes stay as given.
// The boot scene, in the same format; used when the page supplies no /boot/scene.txt.
// The standing WWW tower and, in front, the same tower walking on owl legs.
static const char* kDefaultScene =
    "size 120 42\n"
    "2 1 46 19 /boot/island-sun.txt|the island\n"
    "54 0 17 16 /boot/tower.txt|WWW\n"
    "76 2 18 11 /boot/pair-small.txt|Wib&Wob\n"
    "14 24 24 9 /boot/wibblers-happy.txt|the wibblers\n"
    "52 16 66 25 /boot/walking-tower.txt|the tower walks\n";

static bool loadScene(TMinApp* app, TDeskTop* desk, std::istream& f)
{
    int dw = 0, dh = 0;
    std::string line;
    struct Item { int x, y, w, h; std::string path, title; };
    std::vector<Item> items;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.compare(0, 5, "size ") == 0) { sscanf(line.c_str() + 5, "%d %d", &dw, &dh); continue; }
        Item it; int n = 0;
        if (sscanf(line.c_str(), "%d %d %d %d %n", &it.x, &it.y, &it.w, &it.h, &n) < 4) continue;
        std::string rest = line.substr(n);
        size_t bar = rest.find('|');
        it.path = rest.substr(0, bar);
        it.title = bar == std::string::npos ? "" : rest.substr(bar + 1);
        items.push_back(it);
    }
    if (items.empty()) return false;
    TRect ext = desk->getExtent();
    int W = ext.b.x, H = ext.b.y;
    for (auto& it : items) {
        int x = it.x, y = it.y;
        if (dw > W) x = x * W / dw;
        if (dh > H) y = y * H / dh;
        x = std::max(0, std::min(x, W - std::min(it.w, W)));
        y = std::max(0, std::min(y, H - std::min(it.h, H)));
        desk->insert(new TFrameAnimationWindow(TRect(x, y, x + it.w, y + it.h), it.title.c_str(), bootPath(it.path)));
    }
    (void)app;
    return true;
}

TMinApp::TMinApp() :
    TProgInit(&TMinApp::initStatusLine, &TMinApp::initMenuBar, &TMinApp::initDeskTop)
{
    if (wwdos_js_opt("bars") == 0) setBars(false);
    if (wwdos_js_opt("paused") == 1) ww_anim_paused = true;
    std::ifstream f(bootPath("/boot/scene.txt"));
    bool haveScene = f && loadScene(this, deskTop, f);
    // A stage script opens its own windows on a beat clock (stage.h); it may stand alone.
    g_liveStage = &stage;
    if (stage.load("/boot/stage.txt")) {
        g_stage = &stage;
        ww_anim_clock = [] { return g_stage ? g_stage->animTime() : 0.0; };   // drawings follow the stage clock
        stageTimer = setTimer(stage.periodMs(), stage.periodMs());
        TRect d = deskTop->getBounds();
        wide = new TWideScrub(TRect(d.a.x, d.b.y - 2, d.b.x, d.b.y));
        wide->hide();
        insert(wide);                                   // in front of the desktop: no stage window covers it
        std::string js = "[";
        for (size_t i = 0; i < stage.sectionTimes().size(); ++i) {
            std::string n;
            for (char ch : stage.sectionNames()[i]) { if (ch == '"' || ch == '\\') n += '\\'; n += ch; }
            char b[64]; snprintf(b, sizeof b, "%s{\"time\":%.4f,\"bar\":%.4f,\"name\":\"", i ? "," : "", stage.sectionTimes()[i],
                                 stage.barAt(stage.sectionTimes()[i]));
            js += b; js += n; js += "\"}";
        }
        js += "]";
        wwdos_js_sections(js.c_str());
    }
    else if (!haveScene) {
        std::istringstream def(kDefaultScene);
        loadScene(this, deskTop, def);
    }
}

// Menu bar and status line off: the desktop takes every row (F4 toggles; ?bars=0 starts that way).
void TMinApp::setBars(bool show)
{
    barsHidden = !show;
    if (show) { menuBar->show(); statusLine->show(); }
    else { menuBar->hide(); statusLine->hide(); }
    TRect r = getExtent();
    if (show) { r.a.y = 1; r.b.y--; }
    deskTop->locate(r);
    if (wide) placeWide();
    redraw();
}


TMenuBar* TMinApp::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;
    // A stage page (a film) has no Open Primer: the file system holds only the film's own files.
    TSubMenu& file = *new TSubMenu("~F~ile", kbAltF);
    file + *new TMenuItem("~P~lay / Pause", cmPause, kbF9, hcNoContext, "F9");
    if (isStagePage()) file + *new TMenuItem("~M~ute / Unmute", cmMute, kbNoKey, hcNoContext, "M");
    file + newLine();
    if (!isStagePage())
        file + *new TMenuItem("~O~pen Primer...", cmOpenPrimer, kbCtrlO, hcNoContext, "Ctrl-O") + newLine();
    file + *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X");
    return new TMinMenuBar(r,
        file +
        *new TSubMenu("~W~indow", kbAltW) +
            *new TMenuItem("~C~ascade", cmCascade, kbNoKey) +
            *new TMenuItem("Ti~l~e", cmTile, kbNoKey) +
            *new TMenuItem("Send to Bac~k~", cmSendToBack, kbNoKey) +
            newLine() +
            *new TMenuItem("~N~ext", cmNext, kbF6, hcNoContext, "F6") +
            *new TMenuItem("~P~revious", cmPrev, kbShiftF6, hcNoContext, "Shift-F6") +
            newLine() +
            *new TMenuItem("Clos~e~", cmClose, kbAltF3, hcNoContext, "Alt-F3") +
            *new TMenuItem("Close ~A~ll", cmCloseAll, kbNoKey) +
        *new TSubMenu("~V~iew", kbAltV) +
            *new TMenuItem("Hide ~B~ars", cmToggleBars, kbF4, hcNoContext, "F4") +
            *new TMenuItem("~S~tep", cmStep, kbF8, hcNoContext, "F8") +
            *new TMenuItem("~N~ext section", cmNextSection, kbPgDn, hcNoContext, "PgDn") +
            *new TMenuItem("Pre~v~ious section", cmPrevSection, kbPgUp, hcNoContext, "PgUp") +
            *new TMenuItem("~F~ullscreen", cmFullscreen, kbNoKey, hcNoContext, "F3") +
        *new TSubMenu("~H~elp", kbAltH) +
            *new TMenuItem("~K~eys...", cmKeys, kbNoKey) +
            *new TMenuItem("~A~bout", cmAbout, kbNoKey));
}

TStatusLine* TMinApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    TStatusDef& def = *new TStatusDef(0, 0xFFFF);
    def + *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit);
    if (!isStagePage()) def + *new TStatusItem("~Ctrl-O~ Open", kbCtrlO, cmOpenPrimer);
    return new TMinStatusLine(r,
        def +
            *new TStatusItem("~F6~ Next", kbF6, cmNext) +
            *new TStatusItem("~Alt-F3~ Close", kbAltF3, cmClose));
}

TDeskTop* TMinApp::initDeskTop(TRect r)
{
    r.a.y = 1;
    r.b.y--;
    TDeskTop* desktop = new TDeskTop(r);
    if (desktop->background) {
        TRect bgBounds = desktop->background->getBounds();
        desktop->remove(desktop->background);
        destroy(desktop->background);
        auto* bg = new TWeatherBackground(bgBounds, '\xB1', 7, 1);
        if (ww::isEga()) bg->setPreset("ega");
        bg->growMode = gfGrowHiX | gfGrowHiY;
        desktop->background = bg;
        desktop->insertBefore(bg, desktop->first());
    }
    return desktop;
}

void TMinApp::openPrimerAt(const std::string& path, const TRect& r, const char* title)
{
    deskTop->insert(new TFrameAnimationWindow(r, title, path));
}

// Size a window to the file's largest frame (or whole text), centred, capped to the desktop.
TRect TMinApp::boundsFor(const std::string& path)
{
    int maxW = 0, maxH = 0, curW = 0, curH = 0;
    std::ifstream f(path, std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "----") {
            maxW = std::max(maxW, curW); maxH = std::max(maxH, curH);
            curW = curH = 0;
            continue;
        }
        curW = std::max(curW, (int)TText::width(TStringView(line.c_str(), line.size())));
        curH++;
    }
    maxW = std::max(maxW, curW); maxH = std::max(maxH, curH);

    TRect ext = deskTop->getExtent();
    int w = std::max(20, std::min(maxW + 2, ext.b.x));
    int h = std::max(5, std::min(maxH + 2, ext.b.y - 2));
    int x = std::max(0, (ext.b.x - w) / 2);
    int y = std::max(0, (ext.b.y - h) / 2);
    return TRect(x, y, x + w, y + h);
}

void TMinApp::openPrimer()
{
    char fileName[MAXPATH];
    strcpy(fileName, kPrimerGlob);
    TFileDialog* d = new TFileDialog(kPrimerGlob, "Open Primer", "~N~ame", fdOpenButton, 100);
    if (executeDialog(d, fileName) != cmCancel) {
        std::string path = fileName;
        size_t slash = path.find_last_of('/');
        std::string title = slash == std::string::npos ? path : path.substr(slash + 1);
        openPrimerAt(path, boundsFor(path), title.c_str());
    }
}

void TMinApp::closeAll()
{
    // Collect first: closing a window unlinks it from the list being walked.
    std::vector<TView*> wins;
    deskTop->forEach([](TView* v, void* out) {
        if (dynamic_cast<TWindow*>(v)) static_cast<std::vector<TView*>*>(out)->push_back(v);
    }, &wins);
    for (TView* v : wins) message(v, evCommand, cmClose, nullptr);
}

void TMinApp::handleEvent(TEvent& event)
{
#ifdef __EMSCRIPTEN__
    // Exit in a browser: ending Turbo Vision's loop would freeze the page, so the page exits instead
    // (back to the films player when framed, otherwise a DOS prompt; web/shell-webgl.html "!exit").
    if (event.what == evCommand && event.message.command == cmQuit) {
        ww_anim_paused = true;
        wwdos_js_sound("!exit");
        clearEvent(event);
        return;
    }
#endif
    if (event.what == evBroadcast && event.message.command == cmTimerExpired) g_timerSeen = true;
    if (event.what == evBroadcast && event.message.command == cmTimerExpired && stageTimer && event.message.infoPtr == stageTimer) {
        StageTickTimer tt;
        serveTools();                            // a paused page's seek or step, if the timer got here before idle did
        stage.paused(ww_anim_paused);            // drawings hold at the shown tick while paused
        if (ww_anim_paused) stage.resized();     // a paused stage follows a resize too (tick() does it while playing)
        if (g_seekTo >= 0) { stage.seek(g_seekTo); g_seekTo = -1; if (statusLine) statusLine->drawView(); }
        if (g_jumpReq >= 0) { jumpTo(stage.barStart(g_jumpReq)); g_jumpReq = -1; }
        if (ww_anim_paused) {                    // F8 steps one tick; the clock starts again on resume
            if (ww_anim_step != stageStepSeen) { stage.tick(); stage.settle(); }
            if (g_stepReq > 0) { int n = g_stepReq; g_stepReq = 0; while (n-- > 0) stage.tick(); stage.settle(); }
            stage.rebase();
        } else
        {
            retimed = false;
            const int seeks0 = stage.seeks;
            for (int n = stage.due(); n > 0 && !retimed && stage.seeks == seeks0; --n) stage.tick();
        }
        stageStepSeen = ww_anim_step;
        refreshWide();
        clearEvent(event);
        return;
    }
#ifndef __EMSCRIPTEN__
    // At a gate the first key or click starts the clock, as the page's first click does.
    if ((event.what == evKeyDown || event.what == evMouseDown) && g_stage && g_stage->waiting()) {
        g_stage->go();
        clearEvent(event);
        return;
    }
#endif
    // F7 back 2 s, F5 forward 2 s: applied at the next stage timer event, paused or not.
    if (event.what == evKeyDown && g_stage && (event.keyDown.keyCode == kbF7 || event.keyDown.keyCode == kbF5)) {
        double from = g_seekTo >= 0 ? g_seekTo : stage.time();
        g_seekTo = std::max(0.0, from + (event.keyDown.keyCode == kbF5 ? 2 : -2));
        clearEvent(event);
        return;
    }
    // Viewer keys work with the menu bar hidden too.
    // Q cycles the launch quantise of queued jumps: BAR, BEAT, OFF.
    if (event.what == evKeyDown && g_stage && (event.keyDown.charScan.charCode == 'q' || event.keyDown.charScan.charCode == 'Q')) {
        stage.quant = (stage.quant + 2) % 3;
        if (wide && g_scrubWide) wide->drawView();
        clearEvent(event);
        return;
    }
    if (event.what == evKeyDown && g_stage && (event.keyDown.charScan.charCode == 'm' || event.keyDown.charScan.charCode == 'M')) {
        event.what = evCommand; event.message.command = cmMute; event.message.infoPtr = nullptr;
    }
    if (event.what == evKeyDown) {
        ushort cmd = 0;
        switch (event.keyDown.keyCode) {
            case kbF4: cmd = cmToggleBars; break;
            case kbF9: cmd = cmPause; break;
            case kbF8: cmd = cmStep; break;
            case kbPgDn: cmd = cmNextSection; break;
            case kbPgUp: cmd = cmPrevSection; break;
        }
        if (cmd) { event.what = evCommand; event.message.command = cmd; event.message.infoPtr = nullptr; }
    }
    TApplication::handleEvent(event);
    if (event.what != evCommand) return;
    switch (event.message.command) {
        case cmOpenPrimer: openPrimer(); break;
        case cmCascade: cascade(); break;
        case cmTile: tile(); break;
        case cmCloseAll: closeAll(); break;
        case cmToggleBars: setBars(barsHidden); break;
        // Pause holds the sound too: the page suspends its audio and resumes it from the same point.
        case cmPause: ww_anim_paused = !ww_anim_paused; wwdos_js_sound(ww_anim_paused ? "!pause" : "!resume");
            if (statusLine) statusLine->drawView(); break;
        case cmStep: ww_anim_paused = true; ++ww_anim_step; wwdos_js_sound("!pause"); break;
        case cmFullscreen: wwdos_js_sound("!fullscreen"); break;
        case cmMute:
            g_muted = !g_muted;
            wwdos_js_sound(g_muted ? "!mute" : "!unmute");
#ifndef __EMSCRIPTEN__
            if (!g_muted && g_stage) g_seekTo = stage.time();   // native: restart the clip under the clock
#endif
            if (statusLine) statusLine->drawView();
            break;
        case cmKeys:
            {
            TRect d = deskTop->getExtent();
            TRect r(0, 0, 58, 16);
            r.move(std::max(0, (d.b.x - 58) / 2), std::max(0, (d.b.y - 16) / 2));
            messageBoxRect(r, mfInformation | mfOKButton,
                "Option+Enter, fn+F3   fullscreen\n"
                "Tab                   fullscreen (start screen)\n"
                "fn+F9                 play / pause\n"
                "M                     mute / unmute (or the ♪ icon)\n"
                "fn+F8                 step while paused\n"
                "PgDn / PgUp           next / previous section\n"
                "Q                     jump on: bar / beat / at once\n"
                "fn+F5 / fn+F7         forward / back 2 seconds\n"
                "fn+F4                 hide / show the bars\n"
                "F10                   open the menu");
            }
            break;
        case cmNextSection: case cmPrevSection:
            queueSection(event.message.command == cmNextSection ? 1 : -1);
            break;
        case cmSendToBack:
            if (deskTop->current && deskTop->background)
                deskTop->current->putInFrontOf((TView*)deskTop->background);
            break;
        case cmAbout:
            messageBox("\x03WibWob-DOS\n\x03つ◕‿◕‿◕༽つ",
                       mfInformation | mfOKButton);
            break;
        default:
            return;
    }
    clearEvent(event);
}

#ifdef __EMSCRIPTEN__
int main()
{
    // A page opened paused is a tool's page (frame, export, regress, timecheck): Turbo Vision holds screen flushes to
    // 60 a second and defers one that comes sooner, so a tool could read the grid before the picture it asked for
    // was flushed. TVISION_MAX_FPS=0 is no limit: every flush happens when it is called. Other pages keep the limit.
    if (wwdos_js_opt("paused") == 1) setenv("TVISION_MAX_FPS", "0", 1);
    ww::initThemeFromEnv();
    TMinApp app;
    app.run();
    app.shutDown();
    return 0;
}
#else
// wwdos-stage --boot DIR [--t S] [--autostart] [--paused] [--bars 0|1] [--trace FILE]
// DIR holds what the page writes to /boot: stage.txt and/or scene.txt, the primers, sounds/NAME.m4a.
int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        bool more = i + 1 < argc;
        if (a == "--boot" && more) g_bootDir = argv[++i];
        else if (a == "--t" && more) { g_seekTo = std::max(0.0, atof(argv[++i])); g_autostart = true; }
        else if (a == "--autostart") g_autostart = true;
        else if (a == "--paused") g_optPaused = 1;
        else if (a == "--bars" && more) g_optBars = atoi(argv[++i]);
        else if (a == "--trace" && more) g_traceFile = fopen(argv[++i], "w");
        else {
            fprintf(stderr, "usage: %s --boot DIR [--t S] [--autostart] [--paused] [--bars 0|1] [--trace FILE]\n", argv[0]);
            return 2;
        }
    }
    char real[PATH_MAX];
    if (!realpath(g_bootDir.c_str(), real) || chdir(real) != 0) { fprintf(stderr, "wwdos-stage: no boot directory %s\n", g_bootDir.c_str()); return 2; }
    g_bootDir = real;
    signal(SIGCHLD, SIG_IGN);   // finished players are reaped by the system
    ww::initThemeFromEnv();
    {
        TMinApp app;
        app.run();
        app.shutDown();
    }
    stopPlayers();
    return 0;
}
#endif
