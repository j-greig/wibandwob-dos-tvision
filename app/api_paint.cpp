/*---------------------------------------------------------*/
/*                                                         */
/*   api_paint.cpp - paint-canvas api_* bridge functions    */
/*   Moved verbatim from wwdos_app.cpp                      */
/*   (monolith split stage 6b).                              */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TWindow
#define Uses_TView
#define Uses_TRect
#define Uses_TDeskTop
#include <tvision/tv.h>

#include <fstream>
#include <string>
#include <algorithm>

#include "api_paint.h"
#include "api_windows.h"
#include "wwdos_app.h"
#include "paint/paint_window.h"
#include "paint/paint_canvas.h"
#include "paint/paint_wwp_codec.h"
#include "generative_lab_view.h"
#include "figlet_utils.h"

void api_spawn_paint(TWwdosApp& app, const TRect* bounds) {
    TRect r;
    if (bounds) {
        r = *bounds;
    } else {
        // Use getBounds() on the actual desktop view (safer from IPC thread than getExtent()).
        TRect d = app.deskTop->getBounds();
        int dw = d.b.x - d.a.x;
        int dh = d.b.y - d.a.y;
        // Fall back to safe defaults if desktop reports invalid size.
        if (dw <= 0) dw = 80;
        if (dh <= 0) dh = 24;
        int w = std::min((int)(dw * 0.9), dw);
        int h = std::min((int)(dh * 0.9), dh);
        int left = d.a.x + (dw - w) / 2;
        int top  = d.a.y + (dh - h) / 2;
        r = TRect(left, top, left + w, top + h);
    }
    TWindow* pw = createPaintWindow(r);
    app.deskTop->insert(pw);
    app.registerWindow(pw);
}

void api_spawn_paint_with_file(TWwdosApp& app, const std::string& path) {
    api_spawn_paint(app, nullptr);
    // Find the just-created paint window (last inserted)
    TView* v = app.deskTop->last;
    if (!v) return;
    // Walk to find the newest paint window
    TView* p = v;
    do {
        p = p->next;
        auto* pw = dynamic_cast<TPaintWindow*>(p);
        if (pw && pw->getCanvas()) {
            // Load file into it
            std::ifstream in(path);
            if (!in) return;
            std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

            auto* canvas = pw->getCanvas();
            if (!loadWwpFromString(data, canvas)) return;
            pw->setFilePath(path);
            canvas->drawView();
            return;
        }
    } while (p != v);
}

TPaintCanvasView* api_find_paint_canvas(TWwdosApp& app, const std::string& id) {
    TWindow* w = app.findWindowById(id);
    if (!w) return nullptr;
    auto *pw = dynamic_cast<TPaintWindow*>(w);
    if (!pw) return nullptr;
    return pw->getCanvas();
}

TGenerativeLabView* api_find_gen_lab_view(TWwdosApp& app, const std::string& id) {
    TWindow* w = app.findWindowById(id);
    if (!w) return nullptr;
    auto *gw = dynamic_cast<TGenerativeLabWindow*>(w);
    if (!gw) return nullptr;
    return gw->getView();
}

static bool clampXY(TPaintCanvasView* c, int& x, int& y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= c->getCols()) x = c->getCols() - 1;
    if (y >= c->getRows()) y = c->getRows() - 1;
    return c->getCols() > 0 && c->getRows() > 0;
}

std::string api_paint_cell(TWwdosApp& app, const std::string& id, int x, int y, uint8_t fg, uint8_t bg) {
    auto *canvas = api_find_paint_canvas(app, id);
    if (!canvas) return "err paint window not found";
    if (!clampXY(canvas, x, y)) return "err canvas has zero size";
    canvas->putCell(x, y, fg, bg);
    return "ok";
}

std::string api_paint_text(TWwdosApp& app, const std::string& id, int x, int y, const std::string& text, uint8_t fg, uint8_t bg) {
    auto *canvas = api_find_paint_canvas(app, id);
    if (!canvas) return "err paint window not found";
    if (!clampXY(canvas, x, y)) return "err canvas has zero size";
    canvas->putText(x, y, text, fg, bg);
    return "ok";
}

std::string api_paint_line(TWwdosApp& app, const std::string& id, int x0, int y0, int x1, int y1, bool erase) {
    auto *canvas = api_find_paint_canvas(app, id);
    if (!canvas) return "err paint window not found";
    if (!clampXY(canvas, x0, y0)) return "err canvas has zero size";
    clampXY(canvas, x1, y1);
    canvas->putLine(x0, y0, x1, y1, erase);
    return "ok";
}

std::string api_paint_rect(TWwdosApp& app, const std::string& id, int x0, int y0, int x1, int y1, bool erase) {
    auto *canvas = api_find_paint_canvas(app, id);
    if (!canvas) return "err paint window not found";
    if (!clampXY(canvas, x0, y0)) return "err canvas has zero size";
    clampXY(canvas, x1, y1);
    canvas->putRect(x0, y0, x1, y1, erase);
    return "ok";
}

std::string api_paint_clear(TWwdosApp& app, const std::string& id) {
    auto *canvas = api_find_paint_canvas(app, id);
    if (!canvas) return "err paint window not found";
    canvas->clear();
    canvas->drawView();
    return "ok";
}

std::string api_paint_export(TWwdosApp& app, const std::string& id) {
    auto *canvas = api_find_paint_canvas(app, id);
    if (!canvas) return "err paint window not found";
    return canvas->exportText();
}

std::string api_paint_save(TWwdosApp& app, const std::string& id, const std::string& path) {
    auto *canvas = api_find_paint_canvas(app, id);
    if (!canvas) return "err paint window not found";
    if (path.empty()) return "err path required";

    std::string json = buildWwpJson(canvas);
    if (!saveWwpFile(path, json))
        return "err cannot write file";
    return "ok saved " + path;
}

std::string api_paint_load(TWwdosApp& app, const std::string& id, const std::string& path) {
    auto *canvas = api_find_paint_canvas(app, id);
    if (!canvas) return "err paint window not found";
    if (path.empty()) return "err path required";

    std::ifstream in(path);
    if (!in) return "err cannot open " + path;
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    int fileCols = parseIntAfter(data, 0, "cols");
    int fileRows = parseIntAfter(data, 0, "rows");
    if (fileCols <= 0 || fileRows <= 0) return "err invalid wwp (missing cols/rows)";
    if (!loadWwpFromString(data, canvas))
        return "err invalid wwp (missing cells)";
    canvas->drawView();
    return "ok loaded " + path;
}

std::string api_paint_stamp_figlet(TWwdosApp& app, const std::string& id,
    const std::string& text, const std::string& font,
    int x, int y, uint8_t fg, uint8_t bg)
{
    auto *canvas = api_find_paint_canvas(app, id);
    if (!canvas) return "err paint window not found";
    if (text.empty()) return "err text required";

    std::string f = font.empty() ? "standard" : font;
    auto lines = figlet::renderLines(text, f, canvas->getCols());
    if (lines.empty()) return "err figlet render failed";

    int stamped = 0;
    for (int row = 0; row < (int)lines.size(); row++) {
        int cy = y + row;
        if (cy < 0 || cy >= canvas->getRows()) continue;
        const std::string& line = lines[row];
        for (int col = 0; col < (int)line.size(); col++) {
            char ch = line[col];
            if (ch == ' ' || ch == '\0') continue;
            int cx = x + col;
            if (cx < 0 || cx >= canvas->getCols()) continue;
            PaintCell& cell = canvas->cellAt(cx, cy);
            cell.textChar = ch;
            cell.textFg = fg;
            cell.textBg = bg;
            stamped++;
        }
    }
    canvas->drawView();
    return "ok stamped " + std::to_string(stamped) + " chars (" + std::to_string(lines.size()) + " lines)";
}
