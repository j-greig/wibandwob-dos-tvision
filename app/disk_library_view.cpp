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

// ── helpers ────────────────────────────────────────────────

static TColorAttr cga(int fg, int bg) {
    return TColorAttr(ThemeManager::cgaColor(fg), ThemeManager::cgaColor(bg));
}

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
    int v = size.y / CELL_H;
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
    TColorAttr body  = cga(15, d.bodyIdx);
    TColorAttr label = cga(d.labelFg, d.labelBg);
    TColorAttr hole  = cga(15, 0);
    TColorAttr shut  = cga(0, 15);
    TColorAttr slot  = cga(15, d.bodyIdx);

    for (int row = 0; row < DISK_H; ++row) {
        int y = y0 + row;
        if (y < 0 || y >= size.y) continue;
        TDrawBuffer b;
        // base: full-width body shell
        b.moveChar(0, ' ', body, DISK_W);

        if (row == 0) {
            // top edge: write-protect hole top-right
            b.moveChar(DISK_W - 2, ' ', hole, 1);
        } else if (row >= 1 && row <= 6) {
            // label sticker rows (cols 1..DISK_W-2)
            b.moveChar(1, ' ', label, DISK_W - 2);
            int artRow = row - 1;
            if (artRow < (int)d.art.size()) {
                const std::string& a = d.art[artRow];
                int w = displayWidth(a);
                int ax = 1 + (DISK_W - 2 - w) / 2;
                if (ax < 1) ax = 1;
                b.moveStr(ax, TStringView(a.data(), a.size()), label);
            } else if (row == 6 || (d.art.empty() && row == 3)) {
                int w = displayWidth(d.title);
                int tx = 1 + (DISK_W - 2 - w) / 2;
                if (tx < 1) tx = 1;
                b.moveStr(tx, TStringView(d.title.data(), d.title.size()), label);
            }
        } else if (row >= 8 && row <= 10) {
            // metal shutter: white block with a body-coloured slot
            b.moveChar(5, ' ', shut, 12);
            b.moveChar(7, ' ', slot, 3);
            if (selected && row == 9)
                b.moveStr(DISK_W - 2, "\xE2\x86\x93", body);  // ↓ insert cue
        }
        writeLine(x0, y, DISK_W, 1, b);
    }

    if (selected) {
        // selection ring in the padding around the disk
        TColorAttr ring = cga(15, 0);
        TDrawBuffer top;
        top.moveChar(0, ' ', ring, DISK_W + 2);
        writeLine(x0 - 1, y0 - 1, DISK_W + 2, 1, top);
        writeLine(x0 - 1, y0 + DISK_H, DISK_W + 2, 1, top);
        for (int y = y0; y < y0 + DISK_H; ++y) {
            TDrawBuffer side;
            side.moveChar(0, ' ', ring, 1);
            writeLine(x0 - 1, y, 1, 1, side);
            writeLine(x0 + DISK_W, y, 1, 1, side);
        }
    }
}

void TDiskLibraryView::draw()
{
    // Library floor: CGA blue like the ref's window interior
    TColorAttr floor = cga(9, 1);
    for (int y = 0; y < size.y; ++y) {
        TDrawBuffer b;
        b.moveChar(0, ' ', floor, size.x);
        writeLine(0, y, size.x, 1, b);
    }

    int c = cols();
    for (int i = 0; i < (int)disks.size(); ++i) {
        int row = i / c - scrollOffset;
        int col = i % c;
        if (row < 0 || row * CELL_H + DISK_H > size.y + CELL_H) continue;
        int x0 = 1 + col * CELL_W + 1;
        int y0 = row * CELL_H + 1;
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
        int col = (p.x - 1) / CELL_W;
        int row = scrollOffset + p.y / CELL_H;
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

// The library catalogue. Art is hand-set per disk — label stickers are tiny
// canvases, treat them like it. Colours: CGA idx (bodies love 5/2/4/3/6).
void TDiskLibraryWindow::populateDisks()
{
    auto& d = grid->disks;
    d.clear();

    d.push_back({"WIBWOB-DOS v1.05",
        {"", "/(\xE2\x97\x95\xE2\x80\xBF\xE2\x97\x95)\\", ""},
        2, 10, 0, "open_wibwob", {}});

    d.push_back({"<<PAWS OFF!>>",
        {"SCRAMBLE'S", "", "=^..^="},
        5, 0, 13, "open_scramble", {}});

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
}

TWindow* createDiskLibraryWindow(const TRect& bounds)
{
    return new TDiskLibraryWindow(bounds);
}

bool isDiskLibraryWindow(TWindow* w)
{
    return dynamic_cast<TDiskLibraryWindow*>(w) != nullptr;
}
