/*---------------------------------------------------------*/
/*                                                         */
/*   paint_window.cpp - Paint canvas TWindow wrapper       */
/*                                                         */
/*   Now includes an in-window TMenuBar (File/Edit/Mode).  */
/*                                                         */
/*---------------------------------------------------------*/

#include "paint_window.h"
#include "paint_canvas.h"
#include "paint_tools.h"
#include "paint_palette.h"
#include "paint_status.h"

#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TKeys
#define Uses_TProgram
#define Uses_MsgBox
#define Uses_TClipboard
#include <tvision/tv.h>

#include <algorithm>

// Paint-window-local command constants
static const ushort cmPaintClose      = 500;
static const ushort cmPaintClear      = 501;
static const ushort cmPaintCopyText   = 502;
static const ushort cmPaintModeFull   = 510;
static const ushort cmPaintModeHalfY  = 511;
static const ushort cmPaintModeHalfX  = 512;
static const ushort cmPaintModeQuarter = 513;
static const ushort cmPaintModeText   = 514;

TPaintWindow::TPaintWindow(const TRect &bounds)
    : TWindow(bounds, "Paint", wnNoNumber), TWindowInit(&TPaintWindow::initFrame)
{
    options |= ofTileable;
    buildLayout();
}

void TPaintWindow::buildLayout()
{
    // Remove all existing children except frame (child index 0)
    // We rebuild on resize via changeBounds
    TView *v = last;
    if (v) {
        // Collect views to remove (skip frame)
        std::vector<TView*> toRemove;
        TView *p = last;
        do {
            p = p->next;
            if (p != frame) toRemove.push_back(p);
        } while (p != last);
        for (auto *r : toRemove) {
            remove(r);
            destroy(r);
        }
    }

    canvas = nullptr;
    menuBar = nullptr;

    TRect client = getExtent();
    client.grow(-1, -1);

    // Menu bar: top row of client area
    TRect menuRect(client.a.x, client.a.y, client.b.x, client.a.y + 1);
    menuBar = new TMenuBar(menuRect,
        *new TSubMenu("~F~ile", kbAltF) +
            *new TMenuItem("~C~lose", cmPaintClose, kbNoKey) +
        *new TSubMenu("~E~dit", kbAltE) +
            *new TMenuItem("C~l~ear Canvas", cmPaintClear, kbNoKey) +
            *new TMenuItem("~C~opy as Text", cmPaintCopyText, kbCtrlIns) +
        *new TSubMenu("~M~ode", kbAltM) +
            *new TMenuItem("~F~ull Block", cmPaintModeFull, kbNoKey) +
            *new TMenuItem("Half ~Y~", cmPaintModeHalfY, kbNoKey) +
            *new TMenuItem("Half ~X~", cmPaintModeHalfX, kbNoKey) +
            *new TMenuItem("~Q~uarter", cmPaintModeQuarter, kbNoKey) +
            *new TMenuItem("~T~ext", cmPaintModeText, kbNoKey)
    );
    menuBar->growMode = gfGrowHiX;
    insert(menuBar);

    // Remaining client area below menu bar
    TRect body(client.a.x, client.a.y + 1, client.b.x, client.b.y);

    // Layout: tools (left, w=16) | canvas (center) | palette (right, w=20) | status (bottom, h=1)
    int toolsW = 16;
    int palW = 20;

    TRect toolsRect(body.a.x, body.a.y,
                     std::min(body.a.x + toolsW, body.b.x), body.b.y - 1);

    TRect palRect(std::max(body.b.x - palW, toolsRect.b.x), body.a.y,
                   body.b.x, body.b.y - 1);

    // Canvas occupies center
    TRect canvasRect(toolsRect.b.x, body.a.y, palRect.a.x, body.b.y - 1);
    int cw = canvasRect.b.x - canvasRect.a.x;
    int ch = canvasRect.b.y - canvasRect.a.y;
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;
    canvas = new TPaintCanvasView(canvasRect, cw, ch, &ctx);
    insert(canvas);

    // Tool panel
    auto *tools = new TPaintToolPanel(toolsRect, &ctx, canvas);
    tools->growMode = gfGrowHiY;
    insert(tools);

    // Palette
    auto *pal = new TPaintPaletteView(palRect, canvas);
    pal->growMode = gfGrowHiX | gfGrowHiY;
    insert(pal);

    // Status bar
    auto *status = new TPaintStatusView(
        TRect(body.a.x, body.b.y - 1, body.b.x, body.b.y), canvas);
    status->growMode = gfGrowAll;
    canvas->setStatusView(status);
    insert(status);

    canvas->select();
}

void TPaintWindow::changeBounds(const TRect &bounds)
{
    TWindow::changeBounds(bounds);
    // Rebuild layout to reflow menu bar + panels to new size
    buildLayout();
}

void TPaintWindow::handleEvent(TEvent &event)
{
    TWindow::handleEvent(event);

    if (event.what == evCommand) {
        switch (event.message.command) {
            case cmPaintClose:
                close();
                clearEvent(event);
                break;
            case cmPaintClear:
                if (canvas) {
                    if (messageBox("Clear entire canvas?",
                                   mfConfirmation | mfYesButton | mfNoButton) == cmYes) {
                        canvas->clear();
                        canvas->drawView();
                    }
                }
                clearEvent(event);
                break;
            case cmPaintCopyText:
                if (canvas) {
                    std::string text = canvas->exportText();
                    // Put on system clipboard via TV
                    if (!text.empty()) {
                        TClipboard::setText(TStringView(text.data(), text.size()));
                        messageBox("Canvas copied to clipboard as text.",
                                   mfInformation | mfOKButton);
                    }
                }
                clearEvent(event);
                break;
            case cmPaintModeFull:
                if (canvas) { canvas->setPixelMode(PixelMode::Full); canvas->drawView(); }
                clearEvent(event);
                break;
            case cmPaintModeHalfY:
                if (canvas) { canvas->setPixelMode(PixelMode::HalfY); canvas->drawView(); }
                clearEvent(event);
                break;
            case cmPaintModeHalfX:
                if (canvas) { canvas->setPixelMode(PixelMode::HalfX); canvas->drawView(); }
                clearEvent(event);
                break;
            case cmPaintModeQuarter:
                if (canvas) { canvas->setPixelMode(PixelMode::Quarter); canvas->drawView(); }
                clearEvent(event);
                break;
            case cmPaintModeText:
                if (canvas) { canvas->setPixelMode(PixelMode::Text); canvas->drawView(); }
                clearEvent(event);
                break;
        }
    }
}

TWindow* createPaintWindow(const TRect &bounds)
{
    return new TPaintWindow(bounds);
}
