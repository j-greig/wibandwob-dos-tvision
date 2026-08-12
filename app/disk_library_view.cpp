/*-----------------------------------------------------------*/
/*   disk_library_view.cpp — SYMBIENT SHAREWARE LIBRARY      */
/*   Floppy launcher. Each disk boots a registry command.    */
/*-----------------------------------------------------------*/

#define Uses_TWindow
#define Uses_TView
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TEvent
#define Uses_TText
#include <tvision/tv.h>

#include "disk_library_view.h"
#include "theme_manager.h"
#include "command_registry.h"
#include "figlet_utils.h"

// ── helpers ────────────────────────────────────────────────

static TColorAttr cga(int fg, int bg) { return ThemeManager::attrIdx(fg, bg); }
static int floorBgIdx() { return ThemeManager::bgIndex(SkinRole::Floor); }

static int displayWidth(const std::string& s) {
    return (int)TText::width(TStringView(s.data(), s.size()));
}

// ── TDiskLibraryView ───────────────────────────────────────

TDiskLibraryView::TDiskLibraryView(const TRect& bounds, TScrollBar* aScrollBar)
    : TView(bounds), vScrollBar(aScrollBar)
{
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable | ofFirstClick;
    eventMask |= evMouseDown | evKeyDown;

    // Figma parity: white outline WIBWOB wordmark over the library floor,
    // with a film-sprocket strip beneath. figlet at runtime; graceful text
    // fallback when the binary is missing.
    logo_ = figlet::renderLines("WIBWOB", "smslant");
    while (!logo_.empty() && logo_.back().find_first_not_of(' ') == std::string::npos)
        logo_.pop_back();
    if (logo_.empty()) logo_.push_back("::  W I B W O B  ::");
    headerRows_ = (int)logo_.size() + 2;   // logo + sprockets + breathing row
}

int TDiskLibraryView::cols() const {
    int c = (size.x - 1) / CELL_W;
    return c < 1 ? 1 : c;
}

int TDiskLibraryView::rowsTotal() const {
    int c = cols();
    return ((int)disks.size() + c - 1) / c;
}

int TDiskLibraryView::visibleRows() const {
    int v = (size.y - headerRows_) / CELL_H;
    return v < 1 ? 1 : v;
}

void TDiskLibraryView::ensureFocusVisible() {
    int row = focused / cols();
    if (row < scrollOffset) scrollOffset = row;
    if (row >= scrollOffset + visibleRows())
        scrollOffset = row - visibleRows() + 1;
    adjustScrollBar();
}

void TDiskLibraryView::adjustScrollBar() {
    if (!vScrollBar) return;
    int maxScroll = rowsTotal() - visibleRows();
    if (maxScroll < 0) maxScroll = 0;
    vScrollBar->setParams(scrollOffset, 0, maxScroll, visibleRows() - 1, 1);
}

// Draw one 22x11 floppy. Geometry echoes the Figma `floppy-disk-3.5`
// component: coloured shell, big label sticker (art + title), black
// write-protect hole top-right, white shutter with slot at the bottom.
void TDiskLibraryView::drawDisk(const DiskDef& d, int x0, int y0, bool selected)
{
    // fg/bg pairs on half-block glyphs give the sub-cell shapes: the top
    // edge is a `▄` row (body fg on library-floor bg), the write-protect
    // hole is a black `▄` continuing into a full black cell, the sticker
    // stays clear of the notch column. Floor is the library blue (1).
    TColorAttr body  = cga(15, d.bodyIdx);
    TColorAttr edge  = cga(d.bodyIdx, floorBgIdx());  // ▄ body-on-floor
    TColorAttr notch = cga(0, floorBgIdx());          // ▄ black-on-floor
    TColorAttr label = cga(d.labelFg, d.labelBg);
    TColorAttr hole  = cga(15, 0);
    TColorAttr shut  = cga(0, 15);
    TColorAttr arrow = cga(15, d.bodyIdx);

    const int stickerW = DISK_W - 4;             // cols 2..DISK_W-3, symmetric
    static const char* kLowerHalf = "\xE2\x96\x84";  // ▄

    for (int row = 0; row < DISK_H; ++row) {
        int y = y0 + row;
        if (y < 0 || y >= size.y) continue;
        TDrawBuffer b;

        if (row == 0) {
            // half-block top edge; black ▄ where the write-protect hole cuts in
            for (int x = 0; x < DISK_W; ++x)
                b.moveStr(x, kLowerHalf, edge);
            b.moveStr(DISK_W - 3, kLowerHalf, notch);
        } else {
            // base: full-width body shell
            b.moveChar(0, ' ', body, DISK_W);

            if (row >= 1 && row <= 6) {
                // label sticker rows (cols 2..DISK_W-3)
                b.moveChar(2, ' ', label, stickerW);
                int artRow = row - 1;
                if (artRow < (int)d.art.size()) {
                    const std::string& a = d.art[artRow];
                    int w = displayWidth(a);
                    int ax = 2 + (stickerW - w) / 2;
                    if (ax < 2) ax = 2;
                    b.moveStr(ax, TStringView(a.data(), a.size()), label);
                } else if (row == 6 || (d.art.empty() && row == 3)) {
                    int w = displayWidth(d.title);
                    int tx = 2 + (stickerW - w) / 2;
                    if (tx < 2) tx = 2;
                    b.moveStr(tx, TStringView(d.title.data(), d.title.size()), label);
                }
                if (row == 1)
                    b.moveChar(DISK_W - 3, ' ', hole, 1);  // notch over sticker
            } else if (row >= 8) {
                // shutter assembly (Figma anatomy, rows 8..10, body-gap row 7
                // above): thin half-cell outline line, white plate with a
                // body-coloured slot, white strip at the disk's right edge
                b.moveStr(4, "\xE2\x96\x90", cga(15, d.bodyIdx));  // ▐ thin line
                b.moveChar(6, ' ', shut, 11);           // plate, cols 6..16
                b.moveChar(7, ' ', body, 2);            // slot, full plate height
                b.moveChar(18, ' ', shut, 3);           // right strip to margin
                if (row == 9)
                    b.moveStr(DISK_W - 1, "\xE2\x86\x93", arrow);  // ↓ cue
            }
            // clipped bottom-left corner — subtle, one ▀ cell: upper half
            // stays body, lower half falls away to the floor
            if (row == DISK_H - 1)
                b.moveStr(0, "\xE2\x96\x80", cga(d.bodyIdx, floorBgIdx()));
        }
        writeLine(x0, y, DISK_W, 1, b);
    }

    if (selected && y0 + DISK_H < size.y) {
        // constrained selection cue: white underline bar in the padding row
        // beneath the disk (▀ so it hugs the disk's bottom edge)
        TDrawBuffer bar;
        for (int x = 0; x < DISK_W; ++x)
            bar.moveStr(x, "\xE2\x96\x80", ThemeManager::attr(SkinRole::FloorInk));
        writeLine(x0, y0 + DISK_H, DISK_W, 1, bar);
    }
}

void TDiskLibraryView::draw()
{
    // Library floor: CGA blue like the ref's window interior
    TColorAttr floor = ThemeManager::attr(SkinRole::Floor);
    for (int y = 0; y < size.y; ++y) {
        TDrawBuffer b;
        b.moveChar(0, ' ', floor, size.x);
        writeLine(0, y, size.x, 1, b);
    }

    // Fixed header: white WIBWOB wordmark, centred, then the sprocket strip
    TColorAttr ink = ThemeManager::attr(SkinRole::FloorInk);
    for (int i = 0; i < (int)logo_.size() && i < size.y; ++i) {
        const std::string& l = logo_[i];
        int w = displayWidth(l);
        int lx = (size.x - w) / 2;
        if (lx < 0) lx = 0;
        TDrawBuffer b;
        b.moveChar(0, ' ', floor, size.x);
        b.moveStr(lx, TStringView(l.data(), l.size()), ink);
        writeLine(0, i, size.x, 1, b);
    }
    if ((int)logo_.size() < size.y) {
        // film sprockets: ▪ every other column, edge to edge
        TDrawBuffer b;
        b.moveChar(0, ' ', floor, size.x);
        for (int x = 1; x < size.x - 1; x += 2)
            b.moveStr(x, "\xE2\x96\xAA", ink);
        writeLine(0, (int)logo_.size(), size.x, 1, b);
    }

    int c = cols();
    for (int i = 0; i < (int)disks.size(); ++i) {
        int row = i / c - scrollOffset;
        int col = i % c;
        if (row < 0) continue;
        int x0 = 1 + col * CELL_W + 1;
        int y0 = headerRows_ + row * CELL_H;
        if (y0 >= size.y) continue;
        drawDisk(disks[i], x0, y0, i == focused && (state & sfFocused));
    }
}

void TDiskLibraryView::bootFocused()
{
    if (focused < 0 || focused >= (int)disks.size()) return;
    const DiskDef& d = disks[focused];
    wwdos_exec_command(d.command, d.args);
}

void TDiskLibraryView::handleEvent(TEvent& event)
{
    TView::handleEvent(event);

    if (event.what == evKeyDown) {
        int c = cols();
        int oldFocused = focused;
        bool handled = true;
        switch (event.keyDown.keyCode) {
            case kbLeft:  if (focused > 0) focused--; break;
            case kbRight: if (focused < (int)disks.size() - 1) focused++; break;
            case kbUp:    if (focused - c >= 0) focused -= c; break;
            case kbDown:  if (focused + c < (int)disks.size()) focused += c; break;
            case kbHome:  focused = 0; break;
            case kbEnd:   focused = (int)disks.size() - 1; break;
            case kbEnter:
                bootFocused();
                clearEvent(event);
                return;
            default: handled = false; break;
        }
        if (handled) {
            if (focused != oldFocused) { ensureFocusVisible(); drawView(); }
            clearEvent(event);
        }
    }

    if (event.what == evMouseDown) {
        TPoint p = makeLocal(event.mouse.where);
        if (p.y < headerRows_) { clearEvent(event); return; }
        int col = (p.x - 1) / CELL_W;
        int row = scrollOffset + (p.y - headerRows_) / CELL_H;
        int idx = row * cols() + col;
        if (col < cols() && idx >= 0 && idx < (int)disks.size()) {
            if (idx == focused && (event.mouse.eventFlags & meDoubleClick)) {
                bootFocused();
            } else {
                focused = idx;
                ensureFocusVisible();
                drawView();
            }
        }
        clearEvent(event);
    }
}

// ── TDiskLibraryWindow ─────────────────────────────────────

TDiskLibraryWindow::TDiskLibraryWindow(const TRect& bounds)
    : TWindowInit(&TDiskLibraryWindow::initFrame),
      TWindow(bounds, "SYMBIENT SHAREWARE LIBRARY", wnNoNumber)
{
    flags = wfMove | wfGrow | wfClose | wfZoom;
    growMode = gfGrowAll;

    TRect interior = getExtent();
    interior.grow(-1, -1);

    TRect sbRect(interior.b.x, interior.a.y, interior.b.x + 1, interior.b.y);
    TScrollBar* sb = new TScrollBar(sbRect);
    insert(sb);

    grid = new TDiskLibraryView(interior, sb);
    populateDisks();
    insert(grid);
    grid->select();
}

// ── optional catalogue file ────────────────────────────────
// `disk_library.cat` in the working directory (repo root) overrides the
// built-in table — hand-editable, no rebuild. Format, one disk per record:
//
//   disk WIBWOB-DOS v1.05          <- starts a record; rest of line = title
//   art /(o_o)\                    <- 0-4 art lines (UTF-8, centred)
//   body 2                         <- shell colour, CGA 0-15
//   label 10 0                     <- label fg bg
//   boot open_wibwob skin=dflat    <- registry command + optional key=val args
//
// Lines starting with # are comments. Unknown keys are ignored.
static bool loadDiskCatalogue(const char* path, std::vector<DiskDef>& out)
{
    FILE* f = fopen(path, "r");
    if (!f) return false;
    std::vector<DiskDef> disks;
    DiskDef cur;
    bool open = false;
    char line[512];
    auto flush = [&]() { if (open) disks.push_back(cur); cur = DiskDef(); open = false; };
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (s.empty() || s[0] == '#') continue;
        size_t sp = s.find(' ');
        std::string key = s.substr(0, sp);
        std::string val = (sp == std::string::npos) ? "" : s.substr(sp + 1);
        if (key == "disk") {
            flush();
            cur.title = val;
            cur.bodyIdx = 7; cur.labelBg = 15; cur.labelFg = 0;
            open = true;
        } else if (!open) {
            continue;
        } else if (key == "art") {
            if (cur.art.size() < 5) cur.art.push_back(val);
        } else if (key == "body") {
            cur.bodyIdx = atoi(val.c_str());
        } else if (key == "label") {
            sscanf(val.c_str(), "%d %d", &cur.labelFg, &cur.labelBg);
        } else if (key == "boot") {
            size_t p = val.find(' ');
            cur.command = val.substr(0, p);
            while (p != std::string::npos) {
                size_t q = val.find(' ', p + 1);
                std::string kv = val.substr(p + 1, q == std::string::npos ? q : q - p - 1);
                size_t eq = kv.find('=');
                if (eq != std::string::npos)
                    cur.args[kv.substr(0, eq)] = kv.substr(eq + 1);
                p = q;
            }
        }
    }
    flush();
    fclose(f);
    if (disks.empty()) return false;
    out = disks;
    return true;
}

// The library catalogue. Art is hand-set per disk — label stickers are tiny
// canvases, treat them like it. Colours: CGA idx (bodies love 5/2/4/3/6).
void TDiskLibraryWindow::populateDisks()
{
    auto& d = grid->disks;
    d.clear();

    // Hand-editable catalogue wins when present (see loadDiskCatalogue docs)
    if (loadDiskCatalogue("disk_library.cat", d)) return;

    d.push_back({"WIBWOB-DOS v1.05",
        {"", "/(\xE2\x97\x95\xE2\x80\xBF\xE2\x97\x95)\\", ""},
        2, 10, 0, "open_wibwob", {}});

    d.push_back({"",
        {"SCRAMBLE'S", "<<PAWS OFF!>>", "/\\_/\\", "( o.o )", "/| |\\"},
        13, 0, 15, "open_scramble", {}});

    d.push_back({"WIBBLE.WOBBLE",
        {"", "\xE3\x81\xA4\xE2\x97\x95\xE2\x80\xBF\xE2\x97\x95\xE3\x81\xA4", ""},
        4, 15, 1, "open_verse", {}});

    d.push_back({"LIFE.EXE",
        {"", ". \xE2\x96\xA0 .", "\xE2\x96\xA0 \xE2\x96\xA0 \xE2\x96\xA0"},
        3, 0, 10, "open_life", {}});

    d.push_back({"MYCELIUM.SYS",
        {"", ".~.~.", "~'~'~"},
        6, 8, 15, "open_mycelium", {}});

    d.push_back({"PAINT.EXE",
        {"", "\xE2\x99\xA5 \xE2\x96\x91\xE2\x96\x92\xE2\x96\x93", ""},
        13, 15, 4, "new_paint_canvas", {}});

    d.push_back({"GALLERY.LIB",
        {"", "[\xE2\x96\xA3][\xE2\x96\xA3]", ""},
        1, 7, 0, "open_gallery", {}});

    d.push_back({"BACKROOMS.TV",
        {"", "\xE2\x96\x9B\xE2\x96\x9C \xE2\x96\x9B\xE2\x96\x9C", "\xE2\x96\x9F\xE2\x96\x99 \xE2\x96\x9F\xE2\x96\x99"},
        14, 0, 14, "open_backrooms_tv", {}});

    d.push_back({"Ooo!",
        {"", " _ ", "(\xE2\x97\x8B\xE2\x97\x8B)", " \xE2\x80\xBE "},
        4, 15, 0, "open_monster_portal", {}});

    d.push_back({"CUBE.EXE",
        {"a┌───┐b", " │   │", "c└───┘d"},
        7, 15, 0, "open_cube", {}});

    d.push_back({"Nov/Dec 1992",
        {"\xE2\x8A\x9E \xE2\x8A\x9E", "", "WIT Lab Notes"},
        8, 7, 4, "open_text_editor", {}});

    d.push_back({"QUADRA.EXE",
        {"", "\xE2\x96\x9B\xE2\x96\x9C", "\xE2\x96\x9B\xE2\x96\x9C\xE2\x96\x9B\xE2\x96\x9C"},
        0, 8, 14, "open_quadra", {}});

    d.push_back({"CIS.HOB maze",
        {"C┐ ┌I┐S", "└H┘O└B", "W┌─┘A─┐"},
        8, 0, 11, "open_rogue", {}});

    d.push_back({"WWW",
        {"", "|WWW|", ""},
        5, 13, 15, "open_browser", {}});

    d.push_back({"SHADER.SYS",
        {"\xE2\x97\xA2\xE2\x97\xA4 iso", ".:=+*#%@", "N cycles"},
        8, 0, 15, "open_shader", {{"shader", "isotower"}}});

    // ── Skin disks: Symbient *Not* Software, disks 1-4 of 8 ──
    d.push_back({"DFLAT SKIN 1/8",
        {"Symbient", "*Not*", "Software"},
        7, 15, 4, "set_skin", {{"skin", "dflat"}}});

    d.push_back({"TURBO SKIN 2/8",
        {"Symbient", "*Not*", "Software"},
        1, 15, 4, "set_skin", {{"skin", "turbo"}}});

    d.push_back({"TERRA SKIN 3/8",
        {"Symbient", "*Not*", "Software"},
        2, 15, 4, "set_skin", {{"skin", "terra"}}});

    d.push_back({"PIPELINE SKIN 4/8",
        {"Symbient", "*Not*", "Software"},
        0, 15, 4, "set_skin", {{"skin", "pipeline"}}});

    d.push_back({"PHOSPHOR SKIN 5/8",
        {"Symbient", "*Not*", "Software"},
        2, 0, 10, "set_skin", {{"skin", "phosphor"}}});

    d.push_back({"HERCULES SKIN 6/8",
        {"Symbient", "*Not*", "Software"},
        6, 0, 14, "set_skin", {{"skin", "hercules"}}});

    d.push_back({"PAPER SKIN 7/8",
        {"Symbient", "*Not*", "Software"},
        7, 15, 0, "set_skin", {{"skin", "paper"}}});

    d.push_back({"MIDNIGHT SKIN 8/8",
        {"Symbient", "*Not*", "Software"},
        8, 0, 11, "set_skin", {{"skin", "midnight"}}});
}

TWindow* createDiskLibraryWindow(const TRect& bounds)
{
    return new TDiskLibraryWindow(bounds);
}

bool isDiskLibraryWindow(TWindow* w)
{
    return dynamic_cast<TDiskLibraryWindow*>(w) != nullptr;
}
