/*-----------------------------------------------------------*/
/*   tuiforge_view.cpp — TUIFORGE.DSK render viewer          */
/*   tui.txt = UTF-8 glyph grid (grid-true, one cell each)   */
/*   tui.fg / tui.bg = one hex CGA digit per cell            */
/*   Rendered in authentic CGA RGB: the art keeps its own    */
/*   colours no matter which skin is on the monitor.         */
/*-----------------------------------------------------------*/

#define Uses_TWindow
#define Uses_TView
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TEvent
#define Uses_TRect
#define Uses_TFrame
#define Uses_TText
#include <tvision/tv.h>

#include "tuiforge_view.h"
#include "theme_manager.h"
#include "command_registry.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>

// dirent/stat instead of <filesystem>: the deployment target predates
// std::filesystem on macOS libc++ (same reason api_windows.cpp uses dirent).
#include <dirent.h>
#include <sys/stat.h>

/*------------------------  loading  ------------------------*/

static int utf8Len(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x6) return 2;
    if ((lead >> 4) == 0xE) return 3;
    if ((lead >> 3) == 0x1E) return 4;
    return 1;   // invalid byte: treat as one cell so we never stall
}

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    if (!in) return lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

TuiforgeGrid loadTuiforgeGrid(const std::string& dir) {
    TuiforgeGrid g;
    g.dir = dir;

    auto txt = readLines(dir + "/tui.txt");
    if (txt.empty()) {
        g.error = "no tui.txt in " + dir;
        return g;
    }
    auto fgL = readLines(dir + "/tui.fg");   // may be empty: degrade, don't die
    auto bgL = readLines(dir + "/tui.bg");

    for (size_t r = 0; r < txt.size(); ++r) {
        std::vector<TuiforgeCell> row;
        const std::string& line = txt[r];
        const std::string* fgRow = r < fgL.size() ? &fgL[r] : nullptr;
        const std::string* bgRow = r < bgL.size() ? &bgL[r] : nullptr;
        size_t i = 0, col = 0;
        while (i < line.size()) {
            TuiforgeCell c;
            int n = utf8Len((unsigned char)line[i]);
            c.glyph = line.substr(i, (size_t)n);
            i += (size_t)n;
            if (fgRow && col < fgRow->size()) {
                int v = hexDigit((*fgRow)[col]);
                if (v >= 0) c.fg = (uint8_t)v;
            }
            if (bgRow && col < bgRow->size()) {
                int v = hexDigit((*bgRow)[col]);
                if (v >= 0) c.bg = (uint8_t)v;
            }
            row.push_back(std::move(c));
            ++col;
        }
        g.width = std::max(g.width, (int)row.size());
        g.rows.push_back(std::move(row));
    }
    return g;
}

const std::string& tuiforgeRoot() {
    static std::string root = [] {
        const char* home = std::getenv("HOME");
        return std::string(home ? home : "") + "/Repos/tuiforge/renders";
    }();
    return root;
}

static bool isFile(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
static bool isDir(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
static bool hasTxt(const std::string& dir) {
    return isFile(dir + "/tui.txt");
}

std::string resolveTuiforgeDir(const std::string& nameOrPath) {
    if (nameOrPath.empty()) return "";
    std::string base;
    if (nameOrPath[0] == '/' || nameOrPath[0] == '~') {
        base = nameOrPath;
        if (base[0] == '~') {
            const char* home = std::getenv("HOME");
            base = std::string(home ? home : "") + base.substr(1);
        }
    } else {
        base = tuiforgeRoot() + "/" + nameOrPath;
    }
    if (hasTxt(base)) return base;
    if (hasTxt(base + "/default")) return base + "/default";
    return "";
}

static std::vector<std::string> subdirs(const std::string& dir) {
    std::vector<std::string> out;
    DIR* d = ::opendir(dir.c_str());
    if (!d) return out;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name.empty() || name[0] == '.' || name[0] == '_') continue;
        if (isDir(dir + "/" + name)) out.push_back(name);
    }
    ::closedir(d);
    return out;
}

std::vector<std::string> listTuiforgeRenders() {
    std::vector<std::string> out;
    const std::string& root = tuiforgeRoot();
    if (!isDir(root)) return out;
    for (const auto& name : subdirs(root)) {
        if (hasTxt(root + "/" + name + "/default")) {
            out.push_back(name);
        } else {
            // One level of nesting: kevart/cat3d/default
            for (const auto& sub : subdirs(root + "/" + name))
                if (hasTxt(root + "/" + name + "/" + sub + "/default"))
                    out.push_back(name + "/" + sub);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

/*------------------------  viewer  -------------------------*/

TTuiforgeView::TTuiforgeView(const TRect& bounds, TuiforgeGrid&& grid)
    : TView(bounds), grid_(std::move(grid))
{
    growMode = gfGrowHiX | gfGrowHiY;   // resize with window, never translate
    options |= ofSelectable;
    eventMask |= evKeyDown | evBroadcast;
}

TTuiforgeView::~TTuiforgeView()
{
    if (timerId_) killTimer(timerId_);
}

void TTuiforgeView::startChannel(int periodMs)
{
    playlist_ = listTuiforgeRenders();
    if (playlist_.empty()) {
        grid_.error = "no corpus at " + tuiforgeRoot();
        return;
    }
    // Shuffle once per power-on — a fixed LCG keeps startup deterministic
    // per corpus size while still feeling like channel-surfing.
    unsigned s = (unsigned)playlist_.size() * 2654435761u;
    for (size_t i = playlist_.size() - 1; i > 0; --i) {
        s = s * 1664525u + 1013904223u;
        std::swap(playlist_[i], playlist_[s % (i + 1)]);
    }
    channel_ = true;
    periodMs_ = periodMs;
    station_ = 0;
    tuneNext();
    timerId_ = setTimer((uint)periodMs_, periodMs_);
}

void TTuiforgeView::tuneNext()
{
    if (playlist_.empty()) return;
    const std::string& name = playlist_[station_ % playlist_.size()];
    ++station_;
    std::string dir = resolveTuiforgeDir(name);
    TuiforgeGrid g = loadTuiforgeGrid(dir.empty() ? name : dir);
    if (g.ok()) {
        grid_ = std::move(g);
        scrollX_ = scrollY_ = 0;
        // Centre landscapes vertically inside a portrait-sized set (and
        // vice versa horizontally) so every render sits framed, not cornered.
        if (grid_.height() < size.y) scrollY_ = -(size.y - grid_.height()) / 2;
        if (grid_.width < size.x)    scrollX_ = -(size.x - grid_.width) / 2;
        drawView();
    }
}

void TTuiforgeView::clampScroll() {
    scrollX_ = std::max(0, std::min(scrollX_, grid_.width - size.x));
    scrollY_ = std::max(0, std::min(scrollY_, grid_.height() - size.y));
}

void TTuiforgeView::changeBounds(const TRect& bounds) {
    TView::changeBounds(bounds);
    clampScroll();
}

void TTuiforgeView::draw() {
    TColorAttr voidAttr(TColorRGB(0x000000), TColorRGB(0x000000));
    for (int y = 0; y < size.y; ++y) {
        TDrawBuffer b;
        b.moveChar(0, ' ', voidAttr, size.x);
        int gy = y + scrollY_;
        if (gy >= 0 && gy < grid_.height()) {
            const auto& row = grid_.rows[(size_t)gy];
            for (int x = 0; x < size.x; ++x) {
                int gx = x + scrollX_;
                if (gx < 0 || gx >= (int)row.size()) continue;
                const TuiforgeCell& c = row[(size_t)gx];
                TColorAttr a(TColorRGB(ThemeManager::cgaRgb(c.fg)),
                             TColorRGB(ThemeManager::cgaRgb(c.bg)));
                b.moveStr((ushort)x, c.glyph, a);
            }
        }
        writeLine(0, (short)y, (short)size.x, 1, b);
    }
    if (!grid_.ok()) {
        TDrawBuffer b;
        TColorAttr err(TColorRGB(0xFF5555), TColorRGB(0x000000));
        std::string msg = " TUIFORGE: " +
            (grid_.error.empty() ? std::string("empty grid") : grid_.error) + " ";
        b.moveStr(0, msg.substr(0, (size_t)size.x), err);
        writeLine(0, 0, (short)std::min((int)msg.size(), (int)size.x), 1, b);
    }
    if (channel_ && size.y > 1) {
        // Channel banner, bottom-left: what's playing + transport state.
        std::string name = grid_.dir;
        size_t root = tuiforgeRoot().size();
        if (name.size() > root + 1) name = name.substr(root + 1);
        if (name.size() > 8 && name.compare(name.size() - 8, 8, "/default") == 0)
            name = name.substr(0, name.size() - 8);
        std::string banner = " \xE2\x96\xB6 " + name +
            (timerId_ ? " " : " [paused] ");   // ▶ name
        int cells = (int)TText::width(TStringView(banner));
        cells = std::min(cells, (int)size.x);
        TDrawBuffer b;
        TColorAttr on(TColorRGB(0x000000), TColorRGB(0x55FFFF));
        b.moveChar(0, ' ', on, cells);
        b.moveStr(0, banner, on);
        writeLine(0, (short)(size.y - 1), (short)cells, 1, b);
    }
}

void TTuiforgeView::handleEvent(TEvent& event) {
    TView::handleEvent(event);
    if (channel_ && event.what == evBroadcast &&
        event.message.command == cmTimerExpired &&
        event.message.infoPtr == timerId_ && timerId_ != 0) {
        tuneNext();
        clearEvent(event);
        return;
    }
    if (channel_ && event.what == evKeyDown) {
        char c = (char)event.keyDown.charScan.charCode;
        if (c == ' ') {                       // pause / resume
            if (timerId_) { killTimer(timerId_); timerId_ = 0; }
            else timerId_ = setTimer((uint)periodMs_, periodMs_);
            drawView(); clearEvent(event); return;
        }
        if (c == 'n' || c == 'N') {           // skip to next station
            tuneNext(); clearEvent(event); return;
        }
    }
    if (event.what != evKeyDown) return;
    int px = scrollX_, py = scrollY_;
    switch (event.keyDown.keyCode) {
        case kbUp:    --scrollY_; break;
        case kbDown:  ++scrollY_; break;
        case kbLeft:  --scrollX_; break;
        case kbRight: ++scrollX_; break;
        case kbPgUp:  scrollY_ -= size.y; break;
        case kbPgDn:  scrollY_ += size.y; break;
        case kbHome:  scrollX_ = scrollY_ = 0; break;
        case kbEnd:   scrollY_ = grid_.height(); break;   // clamped below
        default: return;
    }
    clampScroll();
    if (px != scrollX_ || py != scrollY_) drawView();
    clearEvent(event);
}

TTuiforgeWindow::TTuiforgeWindow(const TRect& bounds, const std::string& title,
                                 TuiforgeGrid&& grid, bool channel)
    : TWindowInit(&TTuiforgeWindow::initFrame),
      TWindow(bounds, title.c_str(), wnNoNumber),
      renderDir_(channel ? "tv" : grid.dir)
{
    TRect r = getExtent();
    r.grow(-1, -1);
    auto* v = new TTuiforgeView(r, std::move(grid));
    insert(v);
    channelPending_ = channel;
    // NOTE: startChannel() cannot run here — TView::setTimer walks the
    // owner chain to TProgram and the window isn't on the desktop yet, so
    // it silently returns 0 (the "[paused] at power-on" bug). setState
    // powers on once the desktop exposes us.
}

void TTuiforgeWindow::setState(ushort aState, Boolean enable)
{
    TWindow::setState(aState, enable);
    if (channelPending_ && (aState & sfExposed) && enable) {
        channelPending_ = false;
        if (auto* v = dynamic_cast<TTuiforgeView*>(first()))
            v->startChannel();
    }
}

/*------------------------  picker  -------------------------*/

TTuiforgePickerView::TTuiforgePickerView(const TRect& bounds, TScrollBar* aScrollBar)
    : TView(bounds), vScroll_(aScrollBar)
{
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable | ofFirstClick;
    eventMask |= evKeyDown | evMouseDown | evBroadcast;
    names_ = listTuiforgeRenders();
    syncScroll();
}

void TTuiforgePickerView::syncScroll() {
    if (!vScroll_) return;
    vScroll_->setParams(focused_, 0,
                        std::max(0, (int)names_.size() - 1),
                        std::max(1, size.y - 1), 1);
}

void TTuiforgePickerView::draw() {
    TColorAttr paper  = ThemeManager::attr(SkinRole::Paper);
    // Inverse selection (canon rule: selection is inversion, never a colour)
    TColorAttr sel    = ThemeManager::attrIdx(
        ThemeManager::bgIndex(SkinRole::Paper),
        ThemeManager::fgIndex(SkinRole::Paper));
    // Keep focused row visible
    if (focused_ < top_) top_ = focused_;
    if (focused_ >= top_ + size.y) top_ = focused_ - size.y + 1;

    for (int y = 0; y < size.y; ++y) {
        TDrawBuffer b;
        int idx = top_ + y;
        bool isFocused = idx == focused_;
        TColorAttr a = isFocused ? sel : paper;
        b.moveChar(0, ' ', a, size.x);
        if (idx >= 0 && idx < (int)names_.size()) {
            std::string line = (isFocused ? " > " : "   ") + names_[idx];
            b.moveStr(1, line.substr(0, (size_t)(size.x - 2)), a);
        }
        writeLine(0, (short)y, (short)size.x, 1, b);
    }
    syncScroll();
}

void TTuiforgePickerView::openFocused() {
    if (focused_ < 0 || focused_ >= (int)names_.size()) return;
    std::map<std::string, std::string> kv{{"path", names_[(size_t)focused_]}};
    wwdos_exec_command("open_tuiforge", kv);
}

void TTuiforgePickerView::handleEvent(TEvent& event) {
    TView::handleEvent(event);
    if (event.what == evKeyDown) {
        int prev = focused_;
        switch (event.keyDown.keyCode) {
            case kbUp:    --focused_; break;
            case kbDown:  ++focused_; break;
            case kbPgUp:  focused_ -= size.y; break;
            case kbPgDn:  focused_ += size.y; break;
            case kbHome:  focused_ = 0; break;
            case kbEnd:   focused_ = (int)names_.size() - 1; break;
            case kbEnter: openFocused(); clearEvent(event); return;
            default: return;
        }
        focused_ = std::max(0, std::min(focused_, (int)names_.size() - 1));
        if (focused_ != prev) drawView();
        clearEvent(event);
    } else if (event.what == evMouseDown) {
        TPoint p = makeLocal(event.mouse.where);
        int idx = top_ + p.y;
        if (idx >= 0 && idx < (int)names_.size()) {
            bool dbl = event.mouse.eventFlags & meDoubleClick;
            focused_ = idx;
            drawView();
            if (dbl) openFocused();
        }
        clearEvent(event);
    } else if (event.what == evBroadcast &&
               event.message.command == cmScrollBarChanged &&
               event.message.infoPtr == vScroll_ && vScroll_) {
        int v = vScroll_->value;
        if (v != focused_) {
            focused_ = std::max(0, std::min(v, (int)names_.size() - 1));
            drawView();
        }
        clearEvent(event);
    }
}

TTuiforgePickerWindow::TTuiforgePickerWindow(const TRect& bounds)
    : TWindowInit(&TTuiforgePickerWindow::initFrame),
      TWindow(bounds, "TUIFORGE.DSK", wnNoNumber)
{
    TScrollBar* sb = standardScrollBar(sbVertical | sbHandleKeyboard);
    TRect r = getExtent();
    r.grow(-1, -1);
    insert(new TTuiforgePickerView(r, sb));
}
