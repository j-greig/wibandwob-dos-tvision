// Stage: see stage.h for the script grammar.

#define Uses_TDeskTop
#define Uses_TBackground
#define Uses_TWindow
#define Uses_TView
#define Uses_TGroup
#define Uses_TText
#define Uses_TStringView
#include <tvision/tv.h>

#include "stage.h"
#include "windows/frame_animation_window.h"
#include "mirror_view.h"
#include "frame_file_player_view.h"
#include "picture.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include "ega_theme.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

// A window's colour picture (a PAINT file or a picture film): frame chosen by the stage clock (Stage::picWindows).
namespace {
struct PictureView : TView
{
    std::shared_ptr<pic::Picture> p;
    int frame = 0;
    PictureView(const TRect& r, std::shared_ptr<pic::Picture> pp) : TView(r), p(std::move(pp)) { growMode = gfGrowHiX | gfGrowHiY; }
    void setFrame(int k) { k = std::max(0, k); if (k != frame) { frame = k; drawView(); } }
    void draw() override
    {
        const auto& fr = p->frame(frame);
        TColorAttr back = getColor(1);
        for (int y = 0; y < size.y; ++y) {
            pic::Row b;
            b.moveChar(0, ' ', back, size.x);
            pic::drawRow(*p, fr, y, b.cells(), size.x, 0, p->pal.data());
            writeLine(0, y, size.x, 1, b);
        }
    }
};
}

// A stage window. Turbo Vision holds every window to at least 16x6 (minWinSize) whenever it is moved or resized; a
// framed stage window may be 3 rows (frame, one line, frame), so a two-row fold glides to the height it was given.
// Ghost windows keep Turbo Vision's limits.
namespace {
struct StageWindow : TFrameAnimationWindow
{
    StageWindow(const TRect& r, const char* title, const std::string& path, bool frameless, bool shadowless) :
        TWindowInit(frameless ? &TFrameAnimationWindow::initFrameless : &TFrameAnimationWindow::initFrame),
        TFrameAnimationWindow(r, title, path, frameless, shadowless)
    {
        // A colour picture replaces the text view the base class made for the file.
        if (auto p = pic::get(path)) {
            TView* old = nullptr;
            forEach([](TView* v, void* out) { if (dynamic_cast<FrameFilePlayerView*>(v) || dynamic_cast<TTextFileView*>(v)) *(TView**) out = v; }, &old);
            if (old) {
                TRect in = old->getBounds();
                remove(old);
                TObject::destroy(old);
                insert(new PictureView(in, p));
            }
        }
    }
    PictureView* picture()
    {
        TView* found = nullptr;
        forEach([](TView* v, void* out) { if (dynamic_cast<PictureView*>(v)) *(TView**) out = v; }, &found);
        return static_cast<PictureView*>(found);
    }
    void sizeLimits(TPoint& min, TPoint& max) override
    {
        TFrameAnimationWindow::sizeLimits(min, max);
        if (!isFrameless()) min.y = 3;
    }
};
}

static std::string trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// "head |title" -> head, title
static void splitBar(const std::string& s, std::string& head, std::string& title)
{
    size_t bar = s.find('|');
    head = trim(bar == std::string::npos ? s : s.substr(0, bar));
    title = bar == std::string::npos ? "" : s.substr(bar + 1);
    if (!title.empty() && title[0] == ' ') title.erase(0, 1);
}

extern bool ww_anim_paused;

static double wallNow()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The tick a command at script time t runs on: the first tick k with t <= k/hz + half a tick (the rule in tick()).
int Stage::tickOf(double t) const
{
    return std::max(0, int(std::ceil(t * clockHz - 0.5 - 1e-9)));
}

// The clock drawings run on. Inside a command (and a replay) it is the command's tick exactly, so a drawing's start
// is the same by seek and by play. Otherwise the shown tick plus the time since it, so a drawing faster than the
// clock keeps its own rate; held at the tick while paused; running on while the clock waits at a gate.
double Stage::animTime() const
{
    if (inRun || quiet) return double(now) / clockHz;
    double t = double(shown) / clockHz;
    if (gated) return t + (gateWall >= 0 ? wallNow() - gateWall : 0);
    if (held || !running || tickWall < 0) return t;
    return t + std::max(0.0, std::min(wallNow() - tickWall, 1.0 / clockHz - 1e-4));
}

void Stage::paused(bool on)
{
    if (held == on) return;
    held = on;
    settle();
}

void Stage::settle()
{
    host.stageFlush();      // the desktop's layers first: the mirrors copy what is on the screen
    for (auto& kv : wins) if (auto* w = win(kv.first)) ww::redrawDeep(w);
    host.stageSettle(true);
}

bool Stage::load(const char* path)
{
    std::ifstream f(bootPath(path));
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line.compare(0, 6, "clock ") == 0) { clockHz = std::max(1, atoi(line.c_str() + 6)); continue; }
        if (line.compare(0, 5, "size ") == 0) { sscanf(line.c_str() + 5, "%d %d", &laidOut.x, &laidOut.y); continue; }
        if (line[0] != '@') continue;
        Cmd c; char* end = nullptr;
        c.t = strtod(line.c_str() + 1, &end);
        c.line = trim(end ? std::string(end) : "");
#ifndef __EMSCRIPTEN__
        for (size_t at = 0; (at = c.line.find(" /boot/", at)) != std::string::npos; )
            { std::string p = bootPath(c.line.substr(at + 1, 6)); c.line.replace(at + 1, 6, p); at += 1 + p.size(); }
#endif
        if (!c.line.empty()) cmds.push_back(c);
    }
    std::stable_sort(cmds.begin(), cmds.end(), [](const Cmd& a, const Cmd& b) { return a.t < b.t; });
    for (auto& c : cmds) {
        if (c.line.compare(0, 7, "section") == 0) {
            sections.push_back(c.t);
            std::string head, title; splitBar(c.line.substr(7), head, title);
            secNames.push_back(title);
        } else if (!tempoSet && c.line.compare(0, 6, "tempo ") == 0) {
            double b = 0; int n = 4;
            if (sscanf(c.line.c_str() + 6, "%lf %d", &b, &n) >= 1 && b > 0) { tempoSet = true; bpm = b; beatsPerBar = std::max(1, n); tempoT0 = c.t; }
        }
    }
    if (cmds.empty()) return false;
    if (!tempoSet) tempoT0 = filmStart();
    TRect ext = host.stageDesk()->getExtent();
    layout(ext.b.x, ext.b.y);
    return true;
}

void Stage::layout(int W, int H)
{
    scaleX = scaleY = 1; offset = TPoint {0, 0}; deskSize = TPoint {W, H};
    if (laidOut.x > 0 && laidOut.y > 0) {
        if (laidOut.x > W) scaleX = double(W) / laidOut.x; else offset.x = (W - laidOut.x) / 2;
        if (laidOut.y > H) scaleY = double(H) / laidOut.y; else offset.y = (H - laidOut.y) / 2;
    }
}

// The page was resized or went fullscreen. Turbo Vision has already moved and resized every window
// in proportion, so the stage closes its windows and runs its script again up to the present moment
// under the new layout, with the clock set to each command's own time so glides, bubbles and shakes
// end when they would have.
void Stage::relayout()
{
    TRect ext = host.stageDesk()->getExtent();
    if (ext.b.x == deskSize.x && ext.b.y == deskSize.y) return;
    layout(ext.b.x, ext.b.y);
    replay(now, next);
    advance();
    host.stageFlush();
}

void Stage::replay(int target, size_t upto)
{
    std::vector<std::string> names;
    for (auto& kv : wins) names.push_back(kv.first);
    for (auto& kv : marks) host.stageMark(kv.first, StageMark());
    winPics.clear();
    quiet = true;
    now = 0; host.stageNow(0);
    soundSent.assign(cmds.size(), 0);
    for (auto& n : names) closeWin(n);
    wins.clear(); tweens.clear(); bubbles.clear(); shakeHome.clear(); shakeUntil = -1; marks.clear(); widgets.clear();
    host.stageWeather("off"); host.stageHint("");
    int lastK = -1;
    for (size_t i = 0; i < upto && i < cmds.size(); ++i) {
        const std::string& l = cmds[i].line;
        if (l.compare(0, 4, "loop") == 0) continue;
        // As tick() has it: a command at tick k runs on what the advance of tick k-1 left (a bubble sits where its
        // window stood a tick ago, a glide starts from there), and tick k's own advance comes after its commands.
        int k = tickOf(cmds[i].t);
        if (k != lastK) { if (k > 0) { now = k - 1; advance(); } lastK = k; }
        now = k;
        run(l);
    }
    now = target; next = upto; shown = target;
    quiet = false;
}

double Stage::length() const
{
    for (auto& c : cmds) if (c.line.compare(0, 4, "loop") == 0 || c.line.compare(0, 3, "end") == 0) return c.t;
    return cmds.empty() ? 0 : cmds.back().t;
}

// The frame at secs: commands up to secs (never past a loop) run quietly, glides stand where they would be.
void Stage::seek(double secs, bool keepClock)
{
    double t = std::max(0.0, std::min(secs, length()));
    int target = int(std::lround(t * clockHz));
    t = double(target) / clockHz;
    size_t upto = 0;
    while (upto < cmds.size() && cmds[upto].t <= t + 0.5 / clockHz && cmds[upto].line.compare(0, 4, "loop") != 0) ++upto;
    running = true; gated = false;
    ++seeks;
    auto w0 = std::chrono::steady_clock::now();
    replay(target, upto);
    // A command in the replay may have changed the desktop's size (bars off): lay out for the size it has now and
    // run the replay again, as the next tick would. Without this a paused page kept the layout of the old size.
    { TRect ext = host.stageDesk()->getExtent();
      if (ext.b.x != deskSize.x || ext.b.y != deskSize.y) { layout(ext.b.x, ext.b.y); replay(target, upto); } }
    advance();
    host.stageFlush();      // the desktop's layers first, then the windows and the mirrors that copy them
    settle();
    lastSeekMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - w0).count();
    if (!keepClock) rebase();
    // Sound follows the seek: stop what is playing, then start each clip the script had already begun, at the
    // point it would have reached ("NAME@SECONDS"; the page skips one that would be over).
    host.stageSound("!stop");
    { char at[32]; snprintf(at, sizeof at, "!at %.6f", t); host.stageSound(at); }   // the host's clock: script time t is now
    for (size_t i = 0; i < upto && i < cmds.size(); ++i) {
        const std::string& l = cmds[i].line;
        if (l.compare(0, 6, "sound ") != 0 || t - cmds[i].t > 60) continue;
        std::string name = trim(l.substr(6));
        if (name.empty() || name[0] == '~') continue;
        char at[32]; snprintf(at, sizeof at, "@%.3f", t - cmds[i].t);
        host.stageSound(name + at);
    }
    // The host has tied its clips to script time t (for a quantised jump, at the landing frame). Put the clock's
    // ticks where its audio clock says they belong: the replay above took real time (tens of ms on a long script),
    // and the timer's phase is otherwise wherever it was.
    double a = host.stageAudioTime();
    if (a >= 0 && std::fabs(a - t) < 0.5) {
        // nearest-below with a thousandth of a tick of slack: the host's clock answers in decimal seconds, and a
        // tick that is not a whole number of them (clock 24) once read as the tick before
        now = int(std::floor(a * clockHz + 1e-3));
        shown = now;
        host.stageRetime(double(now + 1) / clockHz - a);
        rebase();
    }
}

int Stage::bars() const
{
    double n = (length() - tempoT0) / barLen();
    return std::max(0, int(std::ceil(n - 1e-3)));
}

// A jump to script time target. BAR lands on the next bar line, BEAT on the next beat and at the same beat of the
// target bar, OFF at once. The landing runs in tick(); the host gets "!jump LAND TARGET" up to its sound lead ahead,
// so it can stop the old clips and start the new ones on the exact audio frame.
void Stage::queueJump(double target, bool atOnce)
{
    double t = time();
    jShow = target; jTarget = target; jLand = t; jSent = false; jq = true; jCut = atOnce || quant == QOFF;
    if (jCut) return;
    double unit = quant == QBAR ? barLen() : beatLen();
    jLand = tempoT0 + (std::floor((t - tempoT0) / unit + 1e-6) + 1) * unit;
    if (quant == QBEAT) {
        double in = std::fmod(jLand - tempoT0, barLen());
        if (in < 0) in += barLen();
        if (in > barLen() - 1e-6) in = 0;
        jTarget += in;
    }
}

double Stage::sectionTimeFrom(double t, int dir) const
{
    double found = -1;
    for (double s : sections) {
        if (dir > 0 && s > t + 0.2) return s;
        if (dir < 0 && s < t - 1.0) found = s;
    }
    return found;
}

double Stage::sectionTime(int dir) const
{
    double t = time(), found = -1;
    for (double s : sections) {
        if (dir > 0 && s > t + 0.2) return s;
        if (dir < 0 && s < t - 1.0) found = s;
    }
    return found;
}

int Stage::due()
{
    double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (wallLast < 0 || t - wallLast > 1.0) { wallLast = t; wallAcc = 0; return 1; }
    wallAcc += (t - wallLast) * clockHz; wallLast = t;
    int n = int(wallAcc + 0.5);            // nearest: a timer a hair early still runs its tick
    wallAcc -= n;
    return std::min(n, std::max(2, clockHz / 2));
}

// A window's place against the real desktop: X right|centre, Y bottom|centre; numbers leave it as placed.
void Stage::anchor(TRect& r, const std::string& xs, const std::string& ys)
{
    TRect ext = host.stageDesk()->getExtent();
    int w = r.b.x - r.a.x, hh = r.b.y - r.a.y;
    if (xs == "right") r.a.x = ext.b.x - w; else if (xs == "centre") r.a.x = (ext.b.x - w) / 2;
    if (ys == "bottom") r.a.y = ext.b.y - hh; else if (ys == "centre") r.a.y = (ext.b.y - hh) / 2;
    r.b.x = r.a.x + w; r.b.y = r.a.y + hh;
}

// Literal lines ('/' between them) as the window's contents, through a file under /boot, as bubbles do.
void Stage::setText(const std::string& name, const std::string& text)
{
    TFrameAnimationWindow* w = win(name);
    if (!w) return;
    std::string body;
    std::stringstream ss(text); std::string part;
    while (std::getline(ss, part, '/')) body += part + "\n";
    std::string path = bootPath("/boot/_text_" + name + ".txt");
    if (FILE* f = fopen(path.c_str(), "w")) { fputs(body.c_str(), f); fputs("----\n", f); fputs(body.c_str(), f); fclose(f); }
    swapWin(name, path);
}

void Stage::drawWidget(const std::string& name, Widget& g)
{
    double p = g.dur <= 0 ? 1.0 : std::min(1.0, std::max(0.0, double(now - g.start) / g.dur));
    int v = int(std::lround(g.from + (g.to - g.from) * p));
    if (v == g.shown) return;
    g.shown = v;
    char nb[16]; snprintf(nb, sizeof nb, "%d", v);
    std::string num = nb, out;
    if (g.bar) {
        int cols = g.width;
        if (cols <= 0) {
            TFrameAnimationWindow* w = win(name);
            int inner = w ? w->size.x - (w->isFrameless() ? 0 : 2) : 20;
            cols = inner - (g.text.empty() ? 0 : (int) TText::width(TStringView(g.text.c_str(), g.text.size())) + 1) - 5;
        }
        cols = std::max(1, cols);
        int fill = int(std::lround(cols * std::max(0, std::min(100, v)) / 100.0));
        if (!g.text.empty()) out = g.text + " ";
        for (int i = 0; i < cols; i++) out += i < fill ? "\xe2\x96\x88" : "\xe2\x96\x91";   // full block, light shade
        out += std::string(num.size() < 3 ? 4 - num.size() : 1, ' ') + num + "%";
    } else {
        out = g.text;
        size_t at = out.find("%d");
        if (at == std::string::npos) out += num; else out.replace(at, 2, num);
    }
    setText(name, out);
}

// Glides, shake and bubbles at the present tick.
void Stage::advance()
{
    host.stageNow(now);
    for (auto& tw : tweens) {
        TFrameAnimationWindow* w = win(tw.name);
        if (!w) continue;
        double p = std::min(1.0, double(now - tw.start) / tw.dur);
        if (tw.ease) p = p < 0.5 ? 4 * p * p * p : 1 - std::pow(-2 * p + 2, 3) / 2;
        auto lerp = [&](int a, int b) { return int(std::lround(a + (b - a) * p)); };
        TRect r(lerp(tw.from.a.x, tw.to.a.x), lerp(tw.from.a.y, tw.to.a.y),
                lerp(tw.from.b.x, tw.to.b.x), lerp(tw.from.b.y, tw.to.b.y));
        w->locate(r);
    }
    tweens.erase(std::remove_if(tweens.begin(), tweens.end(),
        [&](const Tween& tw) { return now - tw.start >= tw.dur || !win(tw.name); }), tweens.end());
    if (shakeUntil >= 0) {
        if (now >= shakeUntil) { restoreShake(); shakeUntil = -1; }
        else for (auto& kv : shakeHome)
            if (TFrameAnimationWindow* w = win(kv.first)) {
                // jitter from the seed, the name and the tick: the same by seek, by play and on every run
                unsigned h = seed * 2654435761u + unsigned(now) * 374761393u;
                for (unsigned char c : kv.first) h = (h ^ c) * 16777619u;
                h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
                TRect r = kv.second; int dx = int(h % 3) - 1, dy = int((h / 3) % 3) - 1;
                r.move(dx, dy); w->locate(r);
            }
    }
    for (size_t i = 0; i < bubbles.size();) {
        if (now >= bubbles[i].closeAt) { closeWin(bubbles[i].name); bubbles.erase(bubbles.begin() + i); }
        else ++i;
    }
    for (auto it = widgets.begin(); it != widgets.end();) {
        if (!win(it->first)) { it = widgets.erase(it); continue; }
        drawWidget(it->first, it->second);
        if (now - it->second.start >= it->second.dur) it = widgets.erase(it); else ++it;
    }
    for (auto& kv : marks) {
        Mark& m = kv.second;
        if (m.dur > 0) {
            double p = std::min(1.0, double(now - m.start) / m.dur);
            m.x = int(std::lround(m.fx + (m.tx - m.fx) * p)); m.y = int(std::lround(m.fy + (m.ty - m.fy) * p));
            if (p >= 1.0) m.dur = 0;
        }
        sendMark(kv.first, m);
    }
    picWindows();
}

// Frames in a file of line-art frames (as the desktop counts them): blocks between "----" lines.
static int lineArtFrames(const std::string& path)
{
    static std::map<std::string, int> seen;
    auto it = seen.find(path);
    if (it != seen.end()) return it->second;
    std::ifstream f(path, std::ios::binary);
    std::string line; int n = 0; bool cur = false;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.compare(0, 4, "FPS=") == 0) continue;
        if (line == "----") { if (cur) ++n; cur = false; continue; }
        cur = true;
    }
    if (cur) ++n;
    return seen[path] = n;
}

// The frame a file shows at the present tick (stage.h, mark: frame, fps, loop, once); -1: it animates on its own.
int Stage::frameAt(const std::string& path, int frame0, double fps, int loopMode, int start) const
{
    auto p = pic::get(path);
    bool film = p && p->film;
    int n = p ? p->frames() : lineArtFrames(path);
    if (fps < 0 && !film && frame0 < 0) return -1;
    double rate = fps >= 0 ? fps : (film && frame0 < 0 ? p->fps : 0);
    int k = std::max(0, frame0);
    if (rate > 0) k += int(std::floor(double(std::max(0, now - start)) * rate / clockHz + 1e-9));
    if (n <= 0) return 0;
    bool loop = loopMode == 1 || (loopMode == 0 && !film);
    return loop ? k % n : std::min(k, n - 1);
}

// Drift: whole cells, the accumulated position truncated toward zero, so a cell is crossed when it is reached.
void Stage::driftOffset(const Mark& m, int& dx, int& dy) const
{
    double s = double(std::max(0, now - m.dstart)) / clockHz;
    auto cells = [](double v) { return int(std::trunc(v + (v >= 0 ? 1e-9 : -1e-9))); };
    dx = m.dx0 + cells(m.vx * s); dy = m.dy0 + cells(m.vy * s);
}

StageMark Stage::markView(const Mark& m) const
{
    StageMark v;
    int dx, dy; driftOffset(m, dx, dy);
    // against the right / bottom edge the host places the mark; x / y then carry only the drift
    v.path = m.path; v.x = (m.right ? 0 : m.x) + dx; v.y = (m.bottom ? 0 : m.y) + dy; v.right = m.right; v.bottom = m.bottom; v.ink = m.ink;
    v.z = m.z; v.wrapX = m.wrapX; v.wrapY = m.wrapY;
    v.frame = frameAt(m.path, m.frame0, m.fps, m.loopMode, m.fstart);
    return v;
}

void Stage::sendMark(const std::string& name, Mark& m, bool force)
{
    StageMark v = markView(m);
    if (!force && m.hasSent && v == m.sent) return;
    host.stageMark(name, v);
    m.sent = v; m.hasSent = true;
}

// frame / fps / loop / once options (mark, open, swap); a /boot path is taken too.
void Stage::markOpts(std::istringstream& h, int& frame0, double& fps, int& loopMode, std::string& path, bool& any)
{
    std::string tok;
    while (h >> tok) {
        if (tok == "frame") { h >> frame0; any = true; }
        else if (tok == "fps") { h >> fps; any = true; }
        else if (tok == "loop") loopMode = 1;
        else if (tok == "once") loopMode = 2;
        else if (!tok.empty() && tok[0] == '/') path = tok;
    }
}

// Picture windows: each shows the frame its clock gives now.
void Stage::picWindows()
{
    for (auto it = winPics.begin(); it != winPics.end();) {
        auto* w = dynamic_cast<StageWindow*>(win(it->first));
        PictureView* v = w ? w->picture() : nullptr;
        if (!v) { it = winPics.erase(it); continue; }
        PicClock& c = it->second;
        int k = frameAt(w->getFilePath(), c.frame0, c.fps, c.loopMode, c.start);
        if (k != c.shown) { v->setFrame(k); c.shown = k; }
        ++it;
    }
    if (!quiet) { host.stageFlush(); host.stageSettle(false); }   // the desktop's layers, then the mirrors that copy the screen (one pass a tick)
}

// A size the script gives in cells shrinks with the layout on a small desktop; fitted sizes do not.
TRect Stage::placeSized(int x, int y, int w, int h) const
{
    return place(x, y, std::max(6, int(w * scaleX)), std::max(3, int(h * scaleY)));
}

TRect Stage::place(int x, int y, int w, int h) const
{
    int X = int(x * scaleX) + offset.x, Y = int(y * scaleY) + offset.y;
    return TRect(X, Y, X + w, Y + h);
}

// The file's first frame, plus the frame rows and columns unless ghost.
TRect Stage::fit(const std::string& path, bool ghost) const
{
    if (auto p = pic::get(path)) { int pad = ghost ? 0 : 2; return TRect(0, 0, std::max(3, p->w + pad), std::max(2, p->h + pad)); }
    int maxW = 0, h = 0;
    bool frames = false;
    std::ifstream f(path, std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "----") { frames = true; break; }
        if (line.compare(0, 4, "FPS=") == 0) continue;
        maxW = std::max(maxW, (int) TText::width(TStringView(line.c_str(), line.size())));
        h++;
    }
    int pad = ghost ? (frames ? 0 : 1) : 2;   // a one-frame file shows in a text view that keeps a scroll-bar column
    return TRect(0, 0, std::max(3, maxW + pad), std::max(2, h + pad));
}

TFrameAnimationWindow* Stage::win(const std::string& name)
{
    auto it = wins.find(name);
    if (it == wins.end()) return nullptr;
    if (host.stageDesk()->indexOf(it->second) == 0) { wins.erase(it); return nullptr; }  // closed by hand
    return it->second;
}

void Stage::setTitle(TFrameAnimationWindow* w, const std::string& title)
{
    delete[] (char*) w->title;
    w->title = newStr(title.c_str());
    if (w->frame) w->frame->drawView();
}

void Stage::openWin(const std::string& name, TRect r, bool ghost, const std::string& path, const std::string& title)
{
    if (TFrameAnimationWindow* w = win(name)) {
        if (w->getFilePath() != path) swapWin(name, path);
        w = win(name);
        w->locate(r);
        setTitle(w, title);
        w->select();
        return;
    }
    auto* w = new StageWindow(r, title.c_str(), path, ghost, ghost);
    host.stageDesk()->insert(w);
    wins[name] = w;
    if (!ghost) sfx("~open");
}

void Stage::closeWin(const std::string& name)
{
    if (TFrameAnimationWindow* w = win(name)) {
        if (!w->isFrameless()) sfx("~close");
        ww::forgetWindow(w);
        host.stageDesk()->remove(w);
        TObject::destroy(w);
    }
    wins.erase(name);
    winPics.erase(name);
}

// New contents in the same place, title and depth: a new window slips in behind the old one.
void Stage::swapWin(const std::string& name, const std::string& path)
{
    TFrameAnimationWindow* old = win(name);
    if (!old) return;
    std::string title = old->title ? old->title : "";
    auto* w = new StageWindow(old->getBounds(), title.c_str(), path, old->isFrameless(), old->isFrameless());
    ww::WinColour colour = ww::windowColour(old);        // the colour belongs to the name, not the window object
    if (colour.bg >= 0 || colour.ink >= 0) ww::setWindowColour(w, colour);
    ww::forgetWindow(old);
    host.stageDesk()->insertBefore(w, old);
    host.stageDesk()->remove(old);
    TObject::destroy(old);
    wins[name] = w;
}

void Stage::bubble(const std::string& name, const std::string& text, double secs)
{
    TFrameAnimationWindow* w = win(name);
    if (!w) return;
    std::vector<std::string> lines;
    std::stringstream ss(text); std::string part;
    while (std::getline(ss, part, '/')) lines.push_back(trim(part));
    int width = 0;
    for (auto& l : lines) width = std::max(width, (int) TText::width(TStringView(l.c_str(), l.size())));
    std::string frame;
    frame += "\xe2\x95\xad" + std::string(width + 2, '-') + "\xe2\x95\xae\n";   // ╭ ╮ with - : replaced below
    for (auto& l : lines) {
        int padN = width - (int) TText::width(TStringView(l.c_str(), l.size()));
        frame += "\xe2\x94\x82 " + l + std::string(padN, ' ') + " \xe2\x94\x82\n";
    }
    frame += "\xe2\x95\xb0" + std::string(width + 2, '-') + "\xe2\x95\xaf\n";
    // the box-drawing dash is three bytes; swap the ASCII placeholders now
    std::string out;
    for (char c : frame) out += (c == '-') ? std::string("\xe2\x94\x80") : std::string(1, c);
    out += "  \xe2\x95\xb2\n";   // ╲ the tail
    std::string path = bootPath("/boot/_say_" + std::to_string(++bubbleSerial) + ".txt");
    if (FILE* f = fopen(path.c_str(), "w")) { fputs(out.c_str(), f); fputs("----\n", f); fputs(out.c_str(), f); fclose(f); }
    TRect wb = w->getBounds(), ext = host.stageDesk()->getExtent();
    int bw = width + 4, bh = (int) lines.size() + 3;
    int x = wb.a.x + 2, y = wb.a.y - bh;
    if (y < 0) y = wb.b.y;
    if (y + bh > ext.b.y) y = std::max(0, ext.b.y - bh);
    if (x + bw > ext.b.x) x = std::max(0, ext.b.x - bw);
    std::string bname = "_say" + std::to_string(bubbleSerial);
    openWin(bname, TRect(x, y, x + bw, y + bh), true, path, "");
    bubbles.push_back({bname, now + int(secs * clockHz)});
    sfx("~say");
}

void Stage::restoreShake()
{
    for (auto& kv : shakeHome)
        if (TFrameAnimationWindow* w = win(kv.first)) { TRect r = kv.second; w->locate(r); }
    shakeHome.clear();
}

void Stage::run(const std::string& line)
{
    std::istringstream in(line);
    std::string cmd; in >> cmd;
    std::string rest; std::getline(in, rest); rest = trim(rest);
    std::string head, title; splitBar(rest, head, title);
    std::istringstream h(head);

    warn = nullptr;
    host.stageNow(now);
    struct InRun { bool& f; bool was; InRun(bool& f) : f(f), was(f) { f = true; } ~InRun() { f = was; } } inRunGuard(inRun);
    static const char* const needWin[] = {"move", "rect", "close", "raise", "lower", "title", "swap", "say", "colour", "text", "bar", "counter"};
    for (const char* k : needWin)
        if (cmd == k) { std::string nm; std::istringstream(head) >> nm; if (!win(nm)) warn = "no window"; break; }

    if (cmd == "open") {
        std::string name, tok, path, of, xs, ys; int w = 0, hh = 0, fam = -1; bool ghost = false, mirror = false, flip = false, block = false;
        PicClock pc;
        h >> name >> xs >> ys >> w >> hh;
        int x = atoi(xs.c_str()), y = atoi(ys.c_str());
        while (h >> tok) {
            if (tok == "ghost") ghost = true; else if (tok[0] == '/') path = tok;
            else if (tok == "frame") h >> pc.frame0; else if (tok == "fps") h >> pc.fps;
            else if (tok == "loop") pc.loopMode = 1; else if (tok == "once") pc.loopMode = 2;
            else if (tok == "mirror") mirror = true; else if (tok == "flip") flip = true;
            else if (tok == "block") block = true; else if (tok == "of") h >> of;
            else if (ww::colourIndex(tok) >= 0) fam = ww::colourIndex(tok);
        }
        if (path.empty() && !mirror) {                 // no file: an empty window of the size given
            if (w <= 0 || hh <= 0) { warn = "open needs a file, mirror, or W H"; return; }
            path = bootPath("/boot/_text_" + name + ".txt");
            if (FILE* f = fopen(path.c_str(), "w")) { fputs(" \n----\n \n", f); fclose(f); }
        }
        TRect r = (w > 0 && hh > 0) ? placeSized(x, y, w, hh) : place(x, y, w, hh);
        if (w <= 0 || hh <= 0) { TRect f = fit(path, ghost); r.b.x = r.a.x + f.b.x; r.b.y = r.a.y + f.b.y; }
        anchor(r, xs, ys);
        openWin(name, r, ghost, path, title);
        if (fam >= 0) if (auto* cw = win(name)) { ww::WinColour c; c.bg = fam; ww::setWindowColour(cw, c); ww::redrawDeep(cw); }
        if (mirror) if (auto* mw = win(name)) stageMirror(mw, host.stageDesk(), of.empty() ? nullptr : win(of), flip, block);
        if (win(name) && pic::get(path)) { pc.start = now; winPics[name] = pc; picWindows(); } else winPics.erase(name);
    } else if (cmd == "move" || cmd == "rect") {
        std::string name, tok, xs, ys; int w = 0, hh = 0; double secs = 0; bool ease = false;
        h >> name >> xs >> ys;
        int x = atoi(xs.c_str()), y = atoi(ys.c_str());
        if (cmd == "rect") h >> w >> hh;
        while (h >> tok) { if (tok == "in") h >> secs; else if (tok == "ease") ease = true; }
        TFrameAnimationWindow* wn = win(name);
        if (!wn) return;
        TRect from = wn->getBounds();
        TRect to;
        if (cmd == "rect" && (w <= 0 || hh <= 0)) {   // rect NAME X Y 0 0: back to the file's own size
            TRect f = fit(wn->getFilePath(), wn->isFrameless());
            to = place(x, y, f.b.x, f.b.y);
        } else if (w > 0 && hh > 0)
            to = placeSized(x, y, w, hh);
        else
            to = place(x, y, from.b.x - from.a.x, from.b.y - from.a.y);
        anchor(to, xs, ys);
        tweens.erase(std::remove_if(tweens.begin(), tweens.end(), [&](const Tween& t) { return t.name == name; }), tweens.end());
        if (secs <= 0) { wn->locate(to); return; }
        tweens.push_back({name, from, to, now, std::max(1, int(secs * clockHz)), ease});
    } else if (cmd == "close") {
        std::string name; h >> name; closeWin(name);
    } else if (cmd == "raise") {
        std::string name; h >> name; if (auto* w = win(name)) w->select();
    } else if (cmd == "lower") {
        std::string name; h >> name;
        if (auto* w = win(name)) { TDeskTop* d = host.stageDesk(); if (d->background) w->putInFrontOf(d->background); }
    } else if (cmd == "title") {
        std::string name; h >> name; if (auto* w = win(name)) setTitle(w, title);
    } else if (cmd == "swap") {
        std::string name, path; h >> name;
        PicClock pc; bool any = false;
        markOpts(h, pc.frame0, pc.fps, pc.loopMode, path, any);
        swapWin(name, path);
        if (win(name) && pic::get(path)) { pc.start = now; winPics[name] = pc; picWindows(); } else winPics.erase(name);
    } else if (cmd == "say") {
        std::string name; h >> name;
        double secs = 3; std::string tok;
        // "for S" may sit after the title text
        size_t f = title.rfind(" for ");
        if (f != std::string::npos) { secs = atof(title.c_str() + f + 5); title = title.substr(0, f); }
        bubble(name, title, secs);
    } else if (cmd == "hint") {
        host.stageHint(title);
    } else if (cmd == "bg") {
        std::string preset; h >> preset; host.stageBackground(preset);
    } else if (cmd == "weather") {
        std::string kind; h >> kind; host.stageWeather(kind);
        if (kind == "flash") sfx("~flash");
    } else if (cmd == "sound") {
        std::string name; h >> name;
        if (!name.empty() && name[0] != '~') { char at[32]; snprintf(at, sizeof at, "#%.4f", curT); name += at; }
        if (!name.empty()) sfx(name.c_str());
    } else if (cmd == "scrubber") {
        std::string v; h >> v; host.stageScrubber(v == "wide" ? 2 : v == "off" || v == "0" ? 0 : 1);
    } else if (cmd == "theme") {
        std::string name; h >> name; host.stageTheme(name);
    } else if (cmd == "colour") {
        std::string name, fam; int ink = -1; h >> name >> fam >> ink;
        if (auto* w = win(name)) {
            ww::WinColour c; c.bg = ww::colourIndex(fam); c.ink = ink;
            ww::setWindowColour(w, c);
            ww::redrawDeep(w);
        }
    } else if (cmd == "mark") {
        std::string name, xs, ys, tok, path; double secs = 0;
        h >> name >> xs;
        if (xs == "off") { marks.erase(name); host.stageMark(name, StageMark()); return; }
        h >> ys;
        int inkN = -1, z = 0, frame0 = -1, loopMode = 0; double fps = -1;
        bool zSet = false, wrapSet = false, wx = false, wy = false, frameSet = false, fpsSet = false;
        while (h >> tok) {
            if (tok == "in") h >> secs; else if (tok == "ink") h >> inkN; else if (tok[0] == '/') path = tok;
            else if (tok == "z") { h >> z; zSet = true; }
            else if (tok == "wrap") { std::string a; h >> a; wx = a.find('x') != std::string::npos; wy = a.find('y') != std::string::npos; wrapSet = true; }
            else if (tok == "frame") { h >> frame0; frameSet = true; }
            else if (tok == "fps") { h >> fps; fpsSet = true; }
            else if (tok == "loop") loopMode = 1; else if (tok == "once") loopMode = 2;
        }
        bool newFile = !path.empty();
        bool right = xs == "right", bottom = ys == "bottom";
        TRect p = place(right ? 0 : atoi(xs.c_str()), bottom ? 0 : atoi(ys.c_str()), 0, 0);
        if (ys == "top") p.a.y = 0;               // the desktop's real top row, not the laid-out stage's
        if (xs == "left") p.a.x = 0;              // likewise the real left column
        auto it = marks.find(name);
        Mark m;
        int dx = 0, dy = 0;
        if (it != marks.end()) { m = it->second; if (path.empty()) path = m.path; driftOffset(m, dx, dy); }
        else { m.x = p.a.x; m.y = p.a.y; }
        m.path = path; m.right = right; m.bottom = bottom; if (inkN >= 0) m.ink = inkN;
        if (zSet) m.z = z;
        if (wrapSet) { m.wrapX = wx; m.wrapY = wy; }
        if (frameSet) m.frame0 = frame0; else if (newFile) m.frame0 = -1;
        if (fpsSet) m.fps = fps;
        if (loopMode) m.loopMode = loopMode;
        if (newFile || frameSet || fpsSet) m.fstart = now;
        // the drift so far is folded into the place the mark starts from; it goes on from the new place
        m.fx = m.x + dx; m.fy = m.y + dy; m.dx0 = m.dy0 = 0; m.dstart = now;
        m.tx = p.a.x; m.ty = p.a.y; m.start = now; m.dur = std::max(0, int(secs * clockHz));
        m.x = m.fx; m.y = m.fy;
        if (m.dur == 0) { m.x = m.tx; m.y = m.ty; }
        marks[name] = m;
        sendMark(name, marks[name], true);
    } else if (cmd == "drift") {
        std::string name; double vx = 0, vy = 0; h >> name >> vx >> vy;
        auto it = marks.find(name);
        if (it == marks.end()) { warn = "no mark"; return; }
        Mark& m = it->second;
        int dx, dy; driftOffset(m, dx, dy);
        m.dx0 = dx; m.dy0 = dy; m.vx = vx; m.vy = vy; m.dstart = now;
        sendMark(name, m);
    } else if (cmd == "text") {
        std::string name; h >> name; widgets.erase(name); setText(name, title);
    } else if (cmd == "bar" || cmd == "counter") {
        std::string name, tok; Widget g {cmd == "bar", 0, 0, now, 0, 0, title, INT_MIN}; double secs = 0;
        h >> name >> g.from >> g.to;
        while (h >> tok) { if (tok == "over") h >> secs; else if (tok == "width") h >> g.width; }
        g.dur = int(secs * clockHz);
        if (!win(name)) return;
        drawWidget(name, g);
        if (g.dur > 0) widgets[name] = g; else widgets.erase(name);
    } else if (cmd == "shake") {
        double secs = 1; h >> secs;
        if (shakeHome.empty())
            for (auto& kv : wins) if (auto* w = win(kv.first)) shakeHome[kv.first] = w->getBounds();
        shakeUntil = now + std::max(1, int(secs * clockHz));
        sfx("~shake");
    } else if (cmd == "seed") {
        unsigned n = 0; h >> n; seed = n;
    } else if (cmd == "tile") {
        host.stageTile(); sfx("~tile");
    } else if (cmd == "cascade") {
        host.stageCascade(); sfx("~tile");
    } else if (cmd == "bars") {
        std::string v; h >> v; host.stageBars(v != "off" && v != "0");
    } else if (cmd == "loop") {
        // The desktop as seek(T) leaves it before the commands at T: the commands before T run again quietly from
        // an empty stage, so the pass that follows is the film from T, whatever the last pass left open.
        double t = 0; h >> t;
        size_t upto = 0;
        while (upto < cmds.size() && cmds[upto].t < t && cmds[upto].line.compare(0, 4, "loop") != 0) ++upto;
        int target = tickOf(t);
        bool wasQuiet = quiet;
        replay(target, upto);
        quiet = wasQuiet;
        while (next < cmds.size() && cmds[next].t < t) ++next;
        return;   // the tick that follows runs the commands at t
    } else if (cmd == "gate") {
        if (!quiet && !host.stageGatePass()) { gated = true; gateWall = wallNow(); }
    } else if (cmd == "end") {
        running = false;
    }
}

void Stage::tick()
{
    relayout();                            // before the running check, so a stopped stage follows a resize too
    if (!running || gated) return;
    const double half = 0.5 / clockHz;
    const double lead = host.stageSoundLead();
    if (jq && double(now) / clockHz + half >= jLand) {
        // Land: the script time is the target plus how far this tick is past the landing line. The clock keeps its
        // phase, moved by the part of a tick that rounding the target took off, so script time and the host's audio
        // clock (which jumps at exactly LAND) stay together.
        jq = false;
        double s = jTarget + (double(now) / clockHz - jLand);
        if (jCut) {                        // OFF: a plain cut; the host ties its clips to the present
            if (lead > 0) { char b[64]; snprintf(b, sizeof b, "!cut %.4f", jTarget); host.stageSound(b); }
            seek(s);
        } else {
            if (!jSent && lead > 0) { char b[64]; snprintf(b, sizeof b, "!jump %.4f %.4f", jLand, jTarget); host.stageSound(b); }
            seek(s, true);
        }
    }
    shown = now; tickWall = wallNow();
    double t = double(now) / clockHz;
    size_t guard = 0;
    bool looped = false;
    // A command runs at the tick nearest its time (not the first tick after it), so nothing is late on average.
    while (next < cmds.size() && cmds[next].t <= t + half && guard++ < 1000) {
        size_t i = next++;
        curT = cmds[i].t;
        if (!(i < soundSent.size() && soundSent[i])) run(cmds[i].line);
        host.stageTrace(cmds[i].t, cmds[i].line, warn);
        if (gated) { advance(); return; }      // the clock holds at this tick until go()
        if (next <= i) {        // loop put the stage at its target: this tick shows that tick, and runs its commands
            if (looped) break;
            looped = true; shown = now; t = double(now) / clockHz;
        }
    }
    if (jq && !jCut && !jSent && lead > 0 && jLand - t <= lead) {
        char b[64]; snprintf(b, sizeof b, "!jump %.4f %.4f", jLand, jTarget); host.stageSound(b); jSent = true;
    }
    // Clips whose time falls between ticks: hand them over early with their exact time, so the host starts each
    // one sample-accurately and consecutive clips meet with no gap.
    if (lead > 0 && !quiet) {
        if (soundSent.size() != cmds.size()) soundSent.assign(cmds.size(), 0);
        for (size_t j = next; j < cmds.size() && cmds[j].t <= t + lead; ++j) {
            const std::string& l = cmds[j].line;
            if (l.compare(0, 4, "loop") == 0 || l.compare(0, 4, "gate") == 0 || l.compare(0, 3, "end") == 0) break;
            if (soundSent[j] || l.compare(0, 6, "sound ") != 0) continue;
            std::string name = trim(l.substr(6));
            if (name.empty() || name[0] == '~') continue;
            char at[32]; snprintf(at, sizeof at, "#%.4f", cmds[j].t);
            host.stageSound(name + at);
            soundSent[j] = 1;
        }
    }
    advance();
    host.stageEveryTick();
    ++now;
}

// Live commands (wwdosStage.run): the same run() a script line goes through, without the time field.
static std::string liveLine(const std::string& line)
{
    std::string l = trim(line);
    if (!l.empty() && l[0] == '@') { size_t sp = l.find(' '); l = sp == std::string::npos ? "" : trim(l.substr(sp)); }
    return l;
}

std::string Stage::checkLive(const std::string& raw, const std::set<std::string>& pending) const
{
    std::string line = liveLine(raw);
    if (line.empty() || line[0] == '#') return "empty line";
    std::istringstream in(line);
    std::string cmd; in >> cmd;
    static const char* const notLive[] = {"loop", "end", "gate", "tempo", "section", "clock", "size"};
    for (const char* k : notLive) if (cmd == k) return cmd + " does not run live";
    static const char* const known[] = {"open", "move", "rect", "close", "raise", "lower", "title", "swap", "say", "hint", "bg",
        "weather", "sound", "scrubber", "theme", "colour", "mark", "drift", "text", "bar", "counter", "shake", "seed", "tile", "cascade", "bars"};
    bool ok = false;
    for (const char* k : known) if (cmd == k) ok = true;
    if (!ok) return "unknown command: " + cmd;
    std::string rest; std::getline(in, rest);
    std::string head, title; splitBar(trim(rest), head, title);
    std::istringstream h(head);
    std::string name; h >> name;
    if (cmd == "open") {
        std::string xs, ys; int w, hh;
        if (!(h >> xs >> ys >> w >> hh)) return "open needs NAME X Y W H";
        return "";
    }
    static const char* const needWin[] = {"move", "rect", "close", "raise", "lower", "title", "swap", "say", "colour", "text", "bar", "counter"};
    for (const char* k : needWin)
        if (cmd == k) {
            if (name.empty()) return cmd + " needs a window name";
            auto it = wins.find(name);
            if ((it == wins.end() || host.stageDesk()->indexOf(it->second) == 0) && !pending.count(name)) return "no window: " + name;
        }
    return "";
}

static std::string jsonStr(const std::string& s)
{
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += char(c); }
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += char(c);
    }
    return o + "\"";
}

std::string Stage::windowsJson()
{
    // front to back: the desktop's views in z order (first() is the front one), keeping those the stage owns
    std::map<TView*, std::string> byView;
    for (auto& kv : wins) if (win(kv.first)) byView[kv.second] = kv.first;
    std::string out = "[";
    TDeskTop* d = host.stageDesk();
    TView* v = d->first();
    if (v) do {
        auto it = byView.find(v);
        if (it != byView.end()) {
            auto* w = static_cast<TFrameAnimationWindow*>(v);
            TRect r = w->getBounds();
            char b[96]; snprintf(b, sizeof b, ",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"ghost\":%s", r.a.x, r.a.y, r.b.x - r.a.x, r.b.y - r.a.y, w->isFrameless() ? "true" : "false");
            if (out.size() > 1) out += ",";
            out += "{\"name\":" + jsonStr(it->second) + b + ",\"title\":" + jsonStr(w->title ? w->title : "") + ",\"file\":" + jsonStr(w->getFilePath()) + "}";
        }
        v = v->next;
    } while (v != d->first());
    return out + "]";
}

std::string Stage::runLive(const std::string& raw)
{
    std::string line = liveLine(raw);
    curT = time();
    run(line);
    host.stageFlush();
    host.stageTrace(curT, line, warn);
    return warn ? warn : "";
}
