/*---------------------------------------------------------*/
/*                                                         */
/*   api_figlet.cpp - figlet-text api_* bridge functions    */
/*   Moved verbatim from wwdos_app.cpp                      */
/*   (monolith split stage 6c).                              */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TView
#define Uses_TWindow
#define Uses_TRect
#define Uses_TDeskTop
#define Uses_TColorAttr
#include <tvision/tv.h>

#include <string>
#include <sstream>
#include <cstdlib>
#include <algorithm>

#include "api_figlet.h"
#include "wwdos_app.h"
#include "figlet_text_view.h"
#include "figlet_utils.h"

void api_spawn_figlet_text(TWwdosApp& app, const TRect* bounds,
    const std::string& text, const std::string& font,
    bool frameless, bool shadowless) {
    TRect desk = app.deskTop->getExtent();
    TRect r;
    if (bounds) {
        r = *bounds;
    } else {
        // Auto-size: render the figlet text at unlimited width, measure it.
        std::string rendered = figlet::render(text, font, 0);
        int maxW = 0, lines = 0;
        size_t pos = 0;
        while (pos < rendered.size()) {
            size_t nl = rendered.find('\n', pos);
            if (nl == std::string::npos) nl = rendered.size();
            int len = static_cast<int>(nl - pos);
            if (len > maxW) maxW = len;
            ++lines;
            pos = nl + 1;
        }
        // Detect figlet wrapping: if total lines > font char height, the text
        // wrapped into multiple rows. Use only the first row's height.
        int fh = figlet::fontHeight(font);
        if (fh > 0 && lines > fh) {
            lines = fh;  // show one row — window will be sized to fit it
        }
        // Chrome: 2 for window borders + 2 padding so figlet doesn't re-wrap
        int w = std::min(maxW + 6, (int)desk.b.x - 2);
        int h = std::min(lines + 3, (int)desk.b.y - 2);  // +2 border +1 breathing room
        // Centre on desktop
        int x = (desk.b.x - w) / 2;
        int y = (desk.b.y - h) / 2;
        r = TRect(x, y, x + w, y + h);
    }
    TFigletTextWindow* w = new TFigletTextWindow(r, text, font, frameless, shadowless);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_figlet_text_at(TWwdosApp& app,
    const std::string& text, const std::string& font, int x, int y,
    bool frameless, bool shadowless) {
    TRect desk = app.deskTop->getExtent();
    // Auto-size: render at unlimited width, measure
    std::string rendered = figlet::render(text, font, 0);
    int maxW = 0, lines = 0;
    size_t pos = 0;
    while (pos < rendered.size()) {
        size_t nl = rendered.find('\n', pos);
        if (nl == std::string::npos) nl = rendered.size();
        int len = static_cast<int>(nl - pos);
        if (len > maxW) maxW = len;
        ++lines;
        pos = nl + 1;
    }
    int fh = figlet::fontHeight(font);
    if (fh > 0 && lines > fh) lines = fh;
    int w = std::min(maxW + 6, (int)desk.b.x - 2);
    int h = std::min(lines + 3, (int)desk.b.y - 2);
    TRect r(x, y, x + w, y + h);
    TFigletTextWindow* win = new TFigletTextWindow(r, text, font, frameless, shadowless);
    app.deskTop->insert(win);
    app.registerWindow(win);
}

static TFigletTextWindow* findFigletWindow(TWwdosApp& app, const std::string& id) {
    TView* v = app.deskTop->first();
    for (; v; v = v->nextView()) {
        TFigletTextWindow* fw = dynamic_cast<TFigletTextWindow*>(v);
        if (fw) {
            if (id == "auto" || id.empty()) return fw;
            const char* t = fw->getTitle(256);
            if (t && id == t) return fw;
        }
    }
    return nullptr;
}

std::string api_figlet_set_text(TWwdosApp& app, const std::string& id, const std::string& text) {
    TFigletTextWindow* w = findFigletWindow(app, id);
    if (!w || !w->getFigletView()) return "err no figlet window found";
    w->getFigletView()->setText(text);
    return "ok";
}

std::string api_figlet_set_font(TWwdosApp& app, const std::string& id, const std::string& font) {
    TFigletTextWindow* w = findFigletWindow(app, id);
    if (!w || !w->getFigletView()) return "err no figlet window found";
    w->getFigletView()->setFont(font);
    return "ok";
}

static uint32_t parseHexColor(const std::string& s) {
    std::string hex = s;
    if (!hex.empty() && hex[0] == '#') hex = hex.substr(1);
    if (hex.size() != 6) return 0;
    return (uint32_t)strtoul(hex.c_str(), nullptr, 16);
}

std::string api_figlet_set_color(TWwdosApp& app, const std::string& id,
                                 const std::string& fg, const std::string& bg) {
    TFigletTextWindow* w = findFigletWindow(app, id);
    if (!w || !w->getFigletView()) return "err no figlet window found";
    TFigletTextView* v = w->getFigletView();
    if (!fg.empty()) {
        uint32_t c = parseHexColor(fg);
        v->setFgColor(TColorRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF));
    }
    if (!bg.empty()) {
        uint32_t c = parseHexColor(bg);
        v->setBgColor(TColorRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF));
    }
    return "ok";
}

std::string api_figlet_list_fonts() {
    auto fonts = figlet::listFonts();
    std::string json = "{\"fonts\":[";
    for (size_t i = 0; i < fonts.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + fonts[i] + "\"";
    }
    json += "]}";
    return json;
}

std::string api_list_figlet_fonts() {
    const auto& fonts = figlet::allFontsSorted();
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < fonts.size(); i++) {
        if (i > 0) os << ",";
        os << "\"" << fonts[i] << "\"";
    }
    os << "]";
    return os.str();
}

std::string api_preview_figlet(const std::string& text, const std::string& font, int width) {
    if (width <= 0) width = 80;
    auto lines = figlet::renderLines(text, font, width);
    if (lines.empty()) return "err render failed (bad font?)";
    std::string result;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) result += '\n';
        result += lines[i];
    }
    return result;
}
