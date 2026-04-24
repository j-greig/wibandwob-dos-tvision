/*---------------------------------------------------------*/
/*                                                         */
/*   tuiforge_viewer.cpp                                   */
/*                                                         */
/*   Standalone TVision viewer for tuiforge renders.       */
/*   Loads a render directory containing:                  */
/*     tui.txt  - UTF-8 text, one row per line             */
/*     tui.fg   - one hex digit per cell (CGA 0-f)         */
/*     tui.bg   - one hex digit per cell (CGA 0-f)         */
/*   and draws it at native 80x25 inside a TWindow.        */
/*                                                         */
/*   Usage:                                                */
/*     tuiforge_viewer <dir> [<dir> ...]                   */
/*                                                         */
/*   Each <dir> is either a single render dir (contains    */
/*   tui.txt+tui.fg+tui.bg) or a tree to auto-scan for     */
/*   renders (one per scene, classified > default).        */
/*                                                         */
/*   Menu:                                                 */
/*     PgDn / PgUp    - next / prev scene                  */
/*     F4             - 1-up view                          */
/*     F5             - toggle 1s autoplay                 */
/*     F6             - 2x2 grid of four scenes            */
/*     Alt-X          - exit                               */
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
#define Uses_TButton
#define Uses_TListBox
#define Uses_TScrollBar
#define Uses_TCollection
#define Uses_TStringCollection
#define Uses_TDialog
#define Uses_TObject
#include <tvision/tv.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

// ---------- grid loading ----------

static const int GRID_W = 80;
static const int GRID_H = 25;

struct TuiCell {
    std::string glyph;   // one UTF-8 codepoint, or " "
    uchar fg = 0x07;
    uchar bg = 0x00;
};

struct Hotspot {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // inclusive
    std::string action;  // "goto", "back", "quit"
    std::string target;  // scene name (for goto)
    std::string label;

    bool contains(int col, int row) const {
        return col >= x0 && col <= x1 && row >= y0 && row <= y1;
    }
};

// Declarative widget spec parsed from tui.widgets. Rendered as
// real TVision widgets layered on top of the background grid.
enum class WidgetKind { Unknown, ListBox, Button };

struct WidgetSpec {
    WidgetKind kind = WidgetKind::Unknown;
    int row = 0, col = 0, cols = 0, rows = 0;
    int selected = 0;               // for listbox
    std::string title;              // listbox title / button label
    std::string action;             // button action: ok | back | noop
    std::vector<std::string> items; // listbox items
};

struct TuiGrid {
    std::vector<TuiCell> cells;  // row-major, GRID_W * GRID_H
    std::vector<Hotspot> hotspots;
    std::vector<WidgetSpec> widgets;
    bool loaded = false;
    std::string error;

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
    out.cells.assign(GRID_W * GRID_H, TuiCell{" ", 0x07, 0x00});

    std::string txt = slurp(dir + "/tui.txt");
    std::string fg  = slurp(dir + "/tui.fg");
    std::string bg  = slurp(dir + "/tui.bg");
    if (txt.empty() || fg.empty() || bg.empty()) {
        out.error = "missing tui.txt / tui.fg / tui.bg in " + dir;
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
        int row = 0;
        size_t i = 0;
        while (i < src.size() && row < GRID_H) {
            int col = 0;
            while (i < src.size() && src[i] != '\n' && col < GRID_W) {
                if (src[i] == '\r') { ++i; continue; }
                int v = hexval(src[i]);
                if (v < 0) v = 0;
                if (is_bg) g.at(row, col).bg = (uchar)v;
                else       g.at(row, col).fg = (uchar)v;
                ++i; ++col;
            }
            while (i < src.size() && src[i] != '\n') ++i;
            if (i < src.size() && src[i] == '\n') ++i;
            ++row;
        }
    };
    parse_attr(fg, out, false);
    parse_attr(bg, out, true);

    // Optional: tui.hotspots alongside.
    std::ifstream hs(dir + "/tui.hotspots");
    if (hs) {
        std::string line;
        auto isws = [](char c){ return c==' '||c=='\t'||c=='\r'; };
        while (std::getline(hs, line)) {
            size_t hash = line.find('#');
            if (hash != std::string::npos) line.resize(hash);
            while (!line.empty() && isws(line.front())) line.erase(0, 1);
            while (!line.empty() && isws(line.back()))  line.pop_back();
            if (line.empty()) continue;
            std::istringstream iss(line);
            Hotspot h;
            if (!(iss >> h.x0 >> h.y0 >> h.x1 >> h.y1 >> h.action)) continue;
            if (h.action == "goto") {
                if (!(iss >> h.target)) continue;
            }
            std::string rest;
            std::getline(iss, rest);
            while (!rest.empty() && isws(rest.front())) rest.erase(0, 1);
            h.label = rest;
            out.hotspots.push_back(h);
        }
    }

    // Optional: tui.widgets (real TVision widget overlay spec).
    std::ifstream ws(dir + "/tui.widgets");
    if (ws) {
        std::string line;
        auto isws = [](char c){ return c==' '||c=='\t'||c=='\r'; };
        auto ltrim = [&isws](std::string &s){
            while (!s.empty() && isws(s.front())) s.erase(0, 1);
        };
        auto rtrim = [&isws](std::string &s){
            while (!s.empty() && isws(s.back())) s.pop_back();
        };
        WidgetSpec *pending = nullptr;  // open listbox accepting ITEMs
        while (std::getline(ws, line)) {
            size_t hash = line.find('#');
            if (hash != std::string::npos) line.resize(hash);
            rtrim(line);
            if (line.empty()) { pending = nullptr; continue; }
            std::string trimmed = line;
            ltrim(trimmed);
            std::istringstream iss(trimmed);
            std::string type;
            iss >> type;
            if (type == "LISTBOX") {
                WidgetSpec w;
                w.kind = WidgetKind::ListBox;
                if (!(iss >> w.row >> w.col >> w.cols >> w.rows >> w.selected)) continue;
                std::string rest;
                std::getline(iss, rest);
                ltrim(rest);
                w.title = rest;
                out.widgets.push_back(w);
                pending = &out.widgets.back();
            } else if (type == "BUTTON") {
                WidgetSpec w;
                w.kind = WidgetKind::Button;
                if (!(iss >> w.row >> w.col >> w.cols >> w.rows >> w.action)) continue;
                std::string rest;
                std::getline(iss, rest);
                ltrim(rest);
                w.title = rest;
                out.widgets.push_back(w);
                pending = nullptr;
            } else if (type == "ITEM" && pending) {
                std::string rest;
                std::getline(iss, rest);
                ltrim(rest);
                pending->items.push_back(rest);
            }
        }
    }

    out.loaded = true;
    return true;
}

// ---------- filesystem helpers ----------

static bool is_regular(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
static bool is_directory(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
static bool is_render_dir(const std::string &p) {
    return is_regular(p + "/tui.txt")
        && is_regular(p + "/tui.fg")
        && is_regular(p + "/tui.bg");
}

static std::vector<std::string> list_subdirs(const std::string &root) {
    std::vector<std::string> out;
    DIR *dp = opendir(root.c_str());
    if (!dp) return out;
    while (struct dirent *e = readdir(dp)) {
        std::string n = e->d_name;
        if (n.empty() || n[0] == '.') continue;
        std::string p = root + "/" + n;
        if (is_directory(p)) out.push_back(p);
    }
    closedir(dp);
    std::sort(out.begin(), out.end());
    return out;
}

// Walk `root` recursively; collect every directory that is itself a render dir.
static void walk_renders(const std::string &root, std::vector<std::string> &out, int depth = 0) {
    if (depth > 6) return;  // sanity
    if (is_render_dir(root)) { out.push_back(root); return; }
    for (const auto &sub : list_subdirs(root)) walk_renders(sub, out, depth + 1);
}

// Pick the canonical "best" render for a scene dir (renders/<scene>).
static std::string pick_scene_render(const std::string &scene_dir) {
    const char *priority[] = {
        "/original/classified",
        "/default/classified",
        "/default",
    };
    for (const char *p : priority) {
        std::string cand = scene_dir + p;
        if (is_render_dir(cand)) return cand;
    }
    std::vector<std::string> found;
    walk_renders(scene_dir, found);
    std::sort(found.begin(), found.end());
    return found.empty() ? std::string{} : found.front();
}

// Discover one render per scene inside a renders/ root.
static std::vector<std::string> discover_scenes(const std::string &renders_root) {
    std::vector<std::string> out;
    for (const auto &sd : list_subdirs(renders_root)) {
        std::string pick = pick_scene_render(sd);
        if (!pick.empty()) out.push_back(pick);
    }
    return out;
}

static std::string scene_label(const std::string &dir) {
    size_t pos = dir.find("/renders/");
    if (pos != std::string::npos) {
        std::string rest = dir.substr(pos + 9);
        size_t next = rest.find('/');
        return next == std::string::npos ? rest : rest.substr(0, next);
    }
    size_t slash = dir.find_last_of('/');
    return slash == std::string::npos ? dir : dir.substr(slash + 1);
}

// ---------- view & window ----------

// Insertion-ordered collection of C-string items (TStringCollection sorts,
// we want yaml order preserved). Streaming stubs keep TCollection happy.
class TOrderedStrCol : public TCollection {
public:
    TOrderedStrCol(ccIndex aLimit, ccIndex aDelta) : TCollection(aLimit, aDelta) {}
    void freeItem(void *item) override { delete[] (char *)item; }
    void *readItem(ipstream &) override { return nullptr; }
    void writeItem(void *, opstream &) override {}
};


// Commands shared between views, windows, and the app. Declared here so
// TuiForgeWindow::spawnWidgets can reference cmNavBack for buttons.
const int cmNextScene      = 1001;
const int cmPrevScene      = 1002;
const int cmToggleAutoplay = 1003;
const int cmLayoutSingle   = 1004;
const int cmLayoutGrid4    = 1005;
const int cmNavBack        = 1006;
const int cmShowHotspots   = 1007;

class TuiForgeApp;  // forward

class TuiForgeView : public TView {
public:
    TuiForgeView(const TRect &bounds, TuiForgeApp *appPtr, int paneIdx)
        : TView(bounds), app(appPtr), paneIndex(paneIdx)
    {
        options |= ofFramed;
        growMode = 0;
        eventMask = evMouseDown | evMouseMove | evMouseAuto;
    }

    void setGrid(const TuiGrid &g) {
        grid = g;
        hovered = -1;
        drawView();
    }

    const TuiGrid &getGrid() const { return grid; }

    void draw() override;  // body below TuiForgeApp (needs app->showAllHotspots())

    void handleEvent(TEvent &event) override;

    TPalette &getPalette() const override {
        static const char pal[] = "\x07";
        static TPalette p(pal, sizeof(pal) - 1);
        return p;
    }

private:
    TuiForgeApp *app = nullptr;
    int paneIndex = 0;
    int hovered = -1;
    TuiGrid grid;

    int findHotspot(int col, int row) const {
        for (int i = 0; i < (int)grid.hotspots.size(); ++i)
            if (grid.hotspots[i].contains(col, row)) return i;
        return -1;
    }
};

class TuiForgeWindow : public TWindow {
public:
    TuiForgeWindow(const TRect &bounds, const char *initTitle,
                   TuiForgeApp *app, int paneIdx)
        : TWindowInit(&TuiForgeWindow::initFrame),
          TWindow(bounds, initTitle, wnNoNumber)
    {
        flags = wfMove | wfZoom;
        options |= ofTileable;
        TRect v = getExtent();
        v.grow(-1, -1);
        view = new TuiForgeView(v, app, paneIdx);
        insert(view);
    }

    void setScene(const TuiGrid &g, const std::string &newTitle) {
        titleStr = newTitle;
        delete[] (char *)title;
        title = newStr(titleStr.c_str());
        clearWidgets();
        view->setGrid(g);
        spawnWidgets(g);
        // Give focus to the first selectable widget we spawned (listbox or
        // button). Without this, Tab has nothing to cycle from until the
        // user clicks a widget.
        for (auto *w : widgetViews) {
            if (w && (w->options & ofSelectable)) {
                w->select();
                break;
            }
        }
        frame->drawView();
    }

    const TuiGrid &grid() const { return view->getGrid(); }

    void redrawContent() { if (view) view->drawView(); }

    // TWindow's default palette has only 8 entries. TListBox's palette
    // string "\x1A\x1A\x1B\x1C\x1D" indexes beyond 8, so lookups fall out
    // of range and errorAttr gets used (every listbox row the same bright
    // red). Use TDialog's 32-entry palette, which covers those indices.
    TPalette &getPalette() const override {
        static TPalette dlg(cpGrayDialog, sizeof(cpGrayDialog) - 1);
        return dlg;
    }

private:
    TuiForgeView *view = nullptr;
    std::string titleStr;
    std::vector<TView *> widgetViews;  // listboxes, buttons, scrollbars we added

    void clearWidgets() {
        for (auto *w : widgetViews) {
            if (!w) continue;
            remove(w);
            TObject::destroy(w);
        }
        widgetViews.clear();
    }

    void spawnWidgets(const TuiGrid &g) {
        // Widget coords are in the 80x25 grid. The view sits at (1,1) inside
        // the window frame, so grid (col, row) -> window (col+1, row+1).
        const int OX = 1, OY = 1;
        // Capture current active state so we can propagate it to new widgets.
        // When widgets are inserted after the window is already active, they
        // don't get sfActive automatically, which breaks TListBox's focused-
        // item colour pick (tlstview.cpp:86 needs sfSelected AND sfActive).
        bool windowIsActive = (state & sfActive) != 0;
        for (const auto &w : g.widgets) {
            if (w.kind == WidgetKind::ListBox) {
                // Reserve the rightmost column inside the listbox for a scrollbar.
                int x0 = w.col + OX;
                int y0 = w.row + OY;
                int x1 = x0 + w.cols;
                int y1 = y0 + w.rows;
                int sbx = x1 - 1;

                TScrollBar *sb = new TScrollBar(TRect(sbx, y0, sbx + 1, y1));
                insert(sb);
                widgetViews.push_back(sb);

                TListBox *lb = new TListBox(TRect(x0, y0, sbx, y1), 1, sb);
                TOrderedStrCol *items = new TOrderedStrCol(
                    (ccIndex)std::max<size_t>(8, w.items.size()), 8);
                for (const auto &s : w.items)
                    items->insert(newStr(s.c_str()));
                lb->newList(items);
                if (w.selected >= 0 && w.selected < (int)w.items.size())
                    lb->focusItem(w.selected);
                insert(lb);
                widgetViews.push_back(lb);
            } else if (w.kind == WidgetKind::Button) {
                int x0 = w.col + OX;
                int y0 = w.row + OY;
                int x1 = x0 + w.cols;
                int y1 = y0 + w.rows;
                // All actions route through cmNavBack for PoC (OK/Cancel/etc
                // all dismiss this scene and pop the nav stack).
                ushort cmd = cmNavBack;
                ushort flags = bfNormal;
                if (w.action == "ok") flags = bfDefault;
                TButton *b = new TButton(TRect(x0, y0, x1, y1),
                                         w.title.c_str(), cmd, flags);
                insert(b);
                widgetViews.push_back(b);
            }
        }
        // Propagate sfActive to every inserted widget. Needed because insert()
        // after the window is active doesn't cascade sfActive automatically.
        if (windowIsActive) {
            for (auto *v : widgetViews) {
                if (v) v->setState(sfActive, True);
            }
        }
    }
};

// ---------- app ----------

enum { LAYOUT_SINGLE = 1, LAYOUT_GRID4 = 4 };

class TuiForgeApp : public TApplication {
public:
    TuiForgeApp(std::vector<std::string> dirs)
        : TProgInit(&TuiForgeApp::initStatusLine,
                    &TuiForgeApp::initMenuBar,
                    &TuiForgeApp::initDeskTop),
          renderDirs(std::move(dirs))
    {
        for (int i = 0; i < (int)renderDirs.size(); ++i)
            sceneByName[scene_label(renderDirs[i])] = i;
        rebuildWindows();
    }

    // Click dispatch from a view. `paneIdx` is which pane was clicked
    // (0 in single mode, 0-3 in grid mode). (col, row) are view-local
    // 80x25 grid coordinates.
    void handleClick(int paneIdx, int col, int row) {
        if (renderDirs.empty()) return;
        int n = (int)renderDirs.size();
        // In grid mode, a click on any pane zooms into 1-up at that scene.
        if (layout == LAYOUT_GRID4) {
            cursor = (cursor + paneIdx) % n;
            layout = LAYOUT_SINGLE;
            rebuildWindows();
            return;
        }
        // Single mode: look up a matching hotspot on the active scene.
        const TuiGrid &g = windows.empty() ? TuiGrid{} : windows[0]->grid();
        for (const auto &h : g.hotspots) {
            if (!h.contains(col, row)) continue;
            if (h.action == "goto" && !h.target.empty()) {
                auto it = sceneByName.find(h.target);
                if (it == sceneByName.end()) return;  // unknown scene
                navStack.push_back(cursor);
                cursor = it->second;
                refreshAllScenes();
                return;
            }
            if (h.action == "back") { popNav(); return; }
            if (h.action == "quit") { message(this, evCommand, cmQuit, nullptr); return; }
        }
    }

    void popNav() {
        if (navStack.empty()) return;
        cursor = navStack.back();
        navStack.pop_back();
        refreshAllScenes();
    }

    static TMenuBar *initMenuBar(TRect r) {
        r.b.y = r.a.y + 1;
        return new TMenuBar(r,
            *new TSubMenu("~F~ile", kbAltF) +
                *new TMenuItem("~N~ext scene",        cmNextScene,       kbPgDn, hcNoContext, "PgDn") +
                *new TMenuItem("~P~rev scene",        cmPrevScene,       kbPgUp, hcNoContext, "PgUp") +
                *new TMenuItem("~B~ack",              cmNavBack,         kbEsc,  hcNoContext, "Esc") +
                newLine() +
                *new TMenuItem("E~x~it",              cmQuit,            kbAltX, hcNoContext, "Alt-X") +
            *new TSubMenu("~V~iew", kbAltV) +
                *new TMenuItem("~1~-up view",         cmLayoutSingle,    kbF4,   hcNoContext, "F4") +
                *new TMenuItem("~4~-up grid",         cmLayoutGrid4,     kbF6,   hcNoContext, "F6") +
                newLine() +
                *new TMenuItem("Toggle ~A~utoplay",   cmToggleAutoplay,  kbF5,   hcNoContext, "F5") +
                *new TMenuItem("Show ~H~otspots",     cmShowHotspots,    kbF7,   hcNoContext, "F7"));
    }

    static TStatusLine *initStatusLine(TRect r) {
        r.a.y = r.b.y - 1;
        return new TStatusLine(r,
            *new TStatusDef(0, 0xFFFF) +
                *new TStatusItem("~Alt-X~ Exit",  kbAltX, cmQuit) +
                *new TStatusItem("~Esc~ Back",    kbEsc,  cmNavBack) +
                *new TStatusItem("~PgDn~ Next",   kbPgDn, cmNextScene) +
                *new TStatusItem("~F4~ 1up",      kbF4,   cmLayoutSingle) +
                *new TStatusItem("~F5~ Play",     kbF5,   cmToggleAutoplay) +
                *new TStatusItem("~F6~ 4up",      kbF6,   cmLayoutGrid4) +
                *new TStatusItem("~F7~ Spots",    kbF7,   cmShowHotspots) +
                *new TStatusItem("~F10~ Menu",    kbF10,  cmMenu));
    }

    bool showAllHotspots() const { return showAll; }

    void redrawAll() {
        for (auto *w : windows) if (w) w->redrawContent();
    }

    void handleEvent(TEvent &event) override {
        TApplication::handleEvent(event);
        if (event.what == evCommand) {
            switch (event.message.command) {
                case cmNextScene:
                    advance(+1);
                    clearEvent(event);
                    break;
                case cmPrevScene:
                    advance(-1);
                    clearEvent(event);
                    break;
                case cmNavBack:
                    popNav();
                    clearEvent(event);
                    break;
                case cmToggleAutoplay:
                    autoplay = !autoplay;
                    autoplayAnchor = std::chrono::steady_clock::now();
                    refreshAllTitles();
                    clearEvent(event);
                    break;
                case cmLayoutSingle:
                    layout = LAYOUT_SINGLE;
                    rebuildWindows();
                    clearEvent(event);
                    break;
                case cmLayoutGrid4:
                    layout = LAYOUT_GRID4;
                    rebuildWindows();
                    clearEvent(event);
                    break;
                case cmShowHotspots:
                    showAll = !showAll;
                    redrawAll();
                    clearEvent(event);
                    break;
            }
        }
    }

    void idle() override {
        TApplication::idle();
        if (!autoplay || renderDirs.size() <= 1) return;
        auto now = std::chrono::steady_clock::now();
        if (now - autoplayAnchor >= std::chrono::seconds(1)) {
            autoplayAnchor = now;
            advance(+1);
        }
    }

private:
    std::vector<std::string> renderDirs;
    std::map<std::string, int> sceneByName;
    std::vector<int> navStack;
    int cursor = 0;
    int layout = LAYOUT_SINGLE;
    bool autoplay = false;
    bool showAll = false;
    std::chrono::steady_clock::time_point autoplayAnchor{};
    std::vector<TuiForgeWindow *> windows;  // currently inserted, in pane order

    void advance(int delta) {
        if (renderDirs.empty()) return;
        int n = (int)renderDirs.size();
        cursor = ((cursor + delta) % n + n) % n;
        refreshAllScenes();
    }

    // Tear down existing TuiForgeWindows and spawn according to `layout`.
    void rebuildWindows() {
        for (auto *w : windows) {
            if (w) {
                deskTop->remove(w);
                destroy(w);
            }
        }
        windows.clear();

        TRect dr = deskTop->getExtent();
        const int W = GRID_W + 2;   // 82
        const int H = GRID_H + 2;   // 27
        int dw = dr.b.x - dr.a.x;
        int dh = dr.b.y - dr.a.y;

        if (layout == LAYOUT_SINGLE) {
            int x = dr.a.x + (dw - W) / 2;
            int y = dr.a.y + (dh - H) / 2;
            if (x < dr.a.x) x = dr.a.x;
            if (y < dr.a.y) y = dr.a.y;
            spawnPane(TRect(x, y, x + W, y + H));
        } else if (layout == LAYOUT_GRID4) {
            // 2x2 grid, 2-char gap between panes.
            const int GAP = 2;
            int totalW = W * 2 + GAP;
            int totalH = H * 2 + GAP;
            int x0 = dr.a.x + (dw - totalW) / 2;
            int y0 = dr.a.y + (dh - totalH) / 2;
            if (x0 < dr.a.x) x0 = dr.a.x;
            if (y0 < dr.a.y) y0 = dr.a.y;
            int xL = x0,            xR = x0 + W + GAP;
            int yT = y0,            yB = y0 + H + GAP;
            spawnPane(TRect(xL, yT, xL + W, yT + H));
            spawnPane(TRect(xR, yT, xR + W, yT + H));
            spawnPane(TRect(xL, yB, xL + W, yB + H));
            spawnPane(TRect(xR, yB, xR + W, yB + H));
        }

        refreshAllScenes();
    }

    void spawnPane(const TRect &r) {
        int idx = (int)windows.size();
        TuiForgeWindow *w = new TuiForgeWindow(r, "(loading)", this, idx);
        deskTop->insert(w);
        windows.push_back(w);
    }

    // Push the correct scene into each existing pane.
    void refreshAllScenes() {
        if (renderDirs.empty()) return;
        int n = (int)renderDirs.size();
        for (size_t i = 0; i < windows.size(); ++i) {
            int idx = (cursor + (int)i) % n;
            const std::string &dir = renderDirs[idx];
            TuiGrid g;
            std::string label = scene_label(dir);
            if (!load_grid(dir, g)) {
                label += "  [load failed]";
            }
            label += titleSuffix(idx);
            windows[i]->setScene(g, label);
        }
    }

    // Rewrite all titles without reloading grids (cheap; for autoplay flag flip).
    void refreshAllTitles() {
        if (windows.empty()) return;
        // Simplest: just refresh scenes. Cost is ~1-4 grid loads.
        refreshAllScenes();
    }

    std::string titleSuffix(int idx) {
        char buf[64];
        if (renderDirs.size() > 1) {
            std::snprintf(buf, sizeof(buf), "  [%d/%d]%s",
                          idx + 1, (int)renderDirs.size(),
                          autoplay ? " PLAY" : "");
        } else {
            std::snprintf(buf, sizeof(buf), "%s",
                          autoplay ? "  PLAY" : "");
        }
        return buf;
    }
};

// Defined after TuiForgeApp so we can call into it.
void TuiForgeView::draw() {
    bool showAllMode = app && app->showAllHotspots();
    TDrawBuffer b;
    for (int y = 0; y < size.y; ++y) {
        b.moveChar(0, ' ', TColorAttr{0x07}, size.x);
        if (grid.loaded && y < GRID_H) {
            for (int x = 0; x < size.x && x < GRID_W; ++x) {
                const TuiCell &c = grid.at(y, x);
                // Check if this cell is in any active hotspot.
                bool invert = false;
                for (int i = 0; i < (int)grid.hotspots.size(); ++i) {
                    bool active = showAllMode || i == hovered;
                    if (active && grid.hotspots[i].contains(x, y)) { invert = true; break; }
                }
                uchar byte = invert
                    ? (uchar)(((c.fg & 0x0F) << 4) | (c.bg & 0x0F))
                    : (uchar)(((c.bg & 0x0F) << 4) | (c.fg & 0x0F));
                b.moveStr((ushort)x,
                          TStringView{c.glyph.data(), c.glyph.size()},
                          TColorAttr{byte});
            }
        }
        writeLine(0, y, size.x, 1, b);
    }
}

void TuiForgeView::handleEvent(TEvent &event) {
    TView::handleEvent(event);
    if (event.what == evMouseDown && app) {
        TPoint local = makeLocal(event.mouse.where);
        app->handleClick(paneIndex, local.x, local.y);
        clearEvent(event);
    } else if (event.what == evMouseMove || event.what == evMouseAuto) {
        TPoint local = makeLocal(event.mouse.where);
        int nh = findHotspot(local.x, local.y);
        if (nh != hovered) {
            hovered = nh;
            drawView();
        }
        clearEvent(event);
    }
}

// ---------- main ----------

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr,
            "usage: tuiforge_viewer <dir> [<dir> ...]\n"
            "\n"
            "  Each <dir> is either a single render dir (contains\n"
            "  tui.txt/tui.fg/tui.bg) or a tree to scan (one render\n"
            "  per scene, preferring classified over default).\n"
            "\n"
            "  PgDn/PgUp cycle. F4/F6 switch 1-up/4-up. F5 toggles\n"
            "  1-second autoplay.\n");
        return 2;
    }

    std::vector<std::string> dirs;
    for (int i = 1; i < argc; ++i) {
        std::string d = argv[i];
        while (d.size() > 1 && d.back() == '/') d.pop_back();

        if (is_render_dir(d)) {
            dirs.push_back(d);
        } else if (is_directory(d)) {
            // Try "scene root" first (one level under renders/).
            std::string pick = pick_scene_render(d);
            if (!pick.empty() && pick.find(d) == 0) {
                // pick is inside d and is one render; but if d has *multiple*
                // scene subdirs, we want all of them. Heuristic: if d's
                // immediate children look like scene roots, scan.
                std::vector<std::string> sub = list_subdirs(d);
                bool looksLikeRendersRoot = false;
                for (const auto &s : sub) {
                    if (!pick_scene_render(s).empty()) {
                        looksLikeRendersRoot = true;
                        break;
                    }
                }
                if (looksLikeRendersRoot) {
                    for (const auto &scene : discover_scenes(d)) dirs.push_back(scene);
                } else {
                    dirs.push_back(pick);
                }
            } else {
                std::vector<std::string> found;
                walk_renders(d, found);
                std::sort(found.begin(), found.end());
                for (const auto &f : found) dirs.push_back(f);
            }
        } else {
            std::fprintf(stderr, "error: not a directory: %s\n", d.c_str());
            return 1;
        }
    }

    if (dirs.empty()) {
        std::fprintf(stderr, "error: no renders found in given paths\n");
        return 1;
    }

    // Dedupe preserving order.
    {
        std::vector<std::string> out;
        out.reserve(dirs.size());
        for (const auto &d : dirs) {
            if (std::find(out.begin(), out.end(), d) == out.end()) out.push_back(d);
        }
        dirs.swap(out);
    }

    std::fprintf(stderr, "tuiforge_viewer: loaded %zu render(s)\n", dirs.size());

    TuiForgeApp app(dirs);
    app.run();
    return 0;
}
