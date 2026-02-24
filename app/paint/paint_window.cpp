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
#include <fstream>
#include <sstream>
#include <cstring>
#include <sys/stat.h>

// Paint-window-local command constants
// Paint-local commands — offset to 600+ to avoid collision with tvterm (500-503)
static const ushort cmPaintClose      = 600;
static const ushort cmPaintSave       = 601;
static const ushort cmPaintSaveAs     = 602;
static const ushort cmPaintOpen       = 603;
static const ushort cmPaintExportAnsi = 604;
static const ushort cmPaintClear      = 610;
static const ushort cmPaintCopyText   = 611;
static const ushort cmPaintModeFull   = 620;
static const ushort cmPaintModeHalfY  = 621;
static const ushort cmPaintModeHalfX  = 622;
static const ushort cmPaintModeQuarter = 623;
static const ushort cmPaintModeText   = 624;

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
            *new TMenuItem("~S~ave", cmPaintSave, kbCtrlS) +
            *new TMenuItem("Save ~A~s...", cmPaintSaveAs, kbNoKey) +
            *new TMenuItem("~O~pen...", cmPaintOpen, kbCtrlO) +
            newLine() +
            *new TMenuItem("E~x~port ANSI...", cmPaintExportAnsi, kbNoKey) +
            newLine() +
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
            case cmPaintSave:
                doSave();
                clearEvent(event);
                break;
            case cmPaintSaveAs:
                doSaveAs();
                clearEvent(event);
                break;
            case cmPaintOpen:
                doOpen();
                clearEvent(event);
                break;
            case cmPaintExportAnsi:
                doExportAnsi();
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

// ── Title ─────────────────────────────────────────────────────────────────────

void TPaintWindow::updateTitle()
{
    std::string t = "Paint";
    if (!filePath_.empty()) {
        // Show just filename
        size_t slash = filePath_.find_last_of("/\\");
        t += " \xC4 "; // — dash
        t += (slash == std::string::npos) ? filePath_ : filePath_.substr(slash + 1);
    }
    if (dirty_) t += " *";
    delete[] (char*)title;
    title = newStr(t.c_str());
    if (frame) frame->drawView();
}

// ── Save (.wwp JSON) ─────────────────────────────────────────────────────────

static std::string buildWwpJson(TPaintCanvasView* cv)
{
    int cols = cv->getCols(), rows = cv->getRows();
    std::ostringstream os;
    os << "{\n  \"version\": 1,\n";
    os << "  \"cols\": " << cols << ",\n";
    os << "  \"rows\": " << rows << ",\n";
    os << "  \"cells\": [\n";
    bool first = true;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const auto& c = cv->cellAt(x, y);
            // Skip empty cells
            if (!c.uOn && !c.lOn && c.qMask == 0 && c.textChar == 0) continue;
            if (!first) os << ",\n";
            first = false;
            os << "    { \"x\": " << x << ", \"y\": " << y;
            if (c.uOn)  os << ", \"uOn\": true, \"uFg\": " << (int)c.uFg;
            if (c.lOn)  os << ", \"lOn\": true, \"lFg\": " << (int)c.lFg;
            if (c.qMask) os << ", \"qMask\": " << (int)c.qMask << ", \"qFg\": " << (int)c.qFg;
            if (c.textChar) os << ", \"textChar\": " << (int)(unsigned char)c.textChar
                              << ", \"textFg\": " << (int)c.textFg
                              << ", \"textBg\": " << (int)c.textBg;
            os << " }";
        }
    }
    os << "\n  ]\n}\n";
    return os.str();
}

static bool saveWwpFile(const std::string& path, const std::string& json)
{
    std::string tmp = path + ".tmp";
    std::ofstream out(tmp, std::ios::out | std::ios::trunc);
    if (!out) return false;
    out << json;
    out.close();
    if (!out.good()) { std::remove(tmp.c_str()); return false; }
    std::remove(path.c_str());
    std::rename(tmp.c_str(), path.c_str());
    return true;
}

void TPaintWindow::doSave()
{
    if (filePath_.empty()) { doSaveAs(); return; }
    if (!canvas) return;
    std::string json = buildWwpJson(canvas);
    if (saveWwpFile(filePath_, json)) {
        dirty_ = false;
        updateTitle();
        messageBox("Saved.", mfInformation | mfOKButton);
    } else {
        messageBox("Error saving file.", mfError | mfOKButton);
    }
}

void TPaintWindow::doSaveAs()
{
    if (!canvas) return;
    char name[256] = {};
    if (!filePath_.empty()) {
        size_t slash = filePath_.find_last_of("/\\");
        std::string fn = (slash == std::string::npos) ? filePath_ : filePath_.substr(slash + 1);
        // Strip .wwp
        if (fn.size() > 4 && fn.substr(fn.size() - 4) == ".wwp")
            fn = fn.substr(0, fn.size() - 4);
        std::strncpy(name, fn.c_str(), sizeof(name) - 1);
    }
    if (inputBox("Save Paint As", "~N~ame:", name, sizeof(name) - 1) != cmOK)
        return;
    std::string nameStr(name);
    if (nameStr.empty()) return;
    // Sanitise
    for (char& c : nameStr)
        if (!std::isalnum((unsigned char)c) && c != '-' && c != '_' && c != '.') c = '_';

    mkdir("paintings", 0755);
    std::string path = "paintings/" + nameStr + ".wwp";

    // Overwrite check
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        std::string msg = nameStr + ".wwp already exists. Overwrite?";
        if (messageBox(msg.c_str(), mfConfirmation | mfYesButton | mfNoButton) != cmYes)
            return;
    }

    std::string json = buildWwpJson(canvas);
    if (saveWwpFile(path, json)) {
        filePath_ = path;
        dirty_ = false;
        updateTitle();
        messageBox("Saved.", mfInformation | mfOKButton);
    } else {
        messageBox("Error saving file.", mfError | mfOKButton);
    }
}

// ── Load (.wwp JSON) ──────────────────────────────────────────────────────────

static int parseIntAfter(const std::string& s, size_t pos, const char* key)
{
    std::string needle = std::string("\"") + key + "\":";
    size_t k = s.find(needle, pos);
    if (k == std::string::npos) {
        // Try with space before colon
        needle = std::string("\"") + key + "\" :";
        k = s.find(needle, pos);
    }
    if (k == std::string::npos) return -1;
    size_t vStart = s.find_first_of("-0123456789", k + needle.size());
    if (vStart == std::string::npos) return -1;
    return std::atoi(s.c_str() + vStart);
}

static bool parseBoolAfter(const std::string& s, size_t pos, const char* key)
{
    std::string needle = std::string("\"") + key + "\":";
    size_t k = s.find(needle, pos);
    if (k == std::string::npos) {
        needle = std::string("\"") + key + "\" :";
        k = s.find(needle, pos);
    }
    if (k == std::string::npos) return false;
    size_t vStart = k + needle.size();
    while (vStart < s.size() && s[vStart] == ' ') vStart++;
    return (vStart < s.size() && s[vStart] == 't');
}

void TPaintWindow::doOpen()
{
    char path[512] = {};
    if (inputBox("Open Paint File", "~P~ath (.wwp):", path, sizeof(path) - 1) != cmOK)
        return;
    std::string pathStr(path);
    if (pathStr.empty()) return;

    std::ifstream in(pathStr);
    if (!in) {
        messageBox("Cannot open file.", mfError | mfOKButton);
        return;
    }
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    int fileCols = parseIntAfter(data, 0, "cols");
    int fileRows = parseIntAfter(data, 0, "rows");
    if (fileCols <= 0 || fileRows <= 0) {
        messageBox("Invalid .wwp file (missing cols/rows).", mfError | mfOKButton);
        return;
    }

    if (!canvas) return;
    canvas->clear();
    // Resize canvas if needed
    if (fileCols != canvas->getCols() || fileRows != canvas->getRows()) {
        // Resize window to fit — bounded by desktop
        TRect ext = owner ? owner->getExtent() : TRect(0, 0, 120, 40);
        int maxW = ext.b.x - ext.a.x;
        int maxH = ext.b.y - ext.a.y;
        int wantW = fileCols + 38; // tools(16) + palette(20) + frame(2)
        int wantH = fileRows + 4;  // menu(1) + status(1) + frame(2)
        if (wantW > maxW) wantW = maxW;
        if (wantH > maxH) wantH = maxH;
        TRect newBounds(origin.x, origin.y, origin.x + wantW, origin.y + wantH);
        changeBounds(newBounds);
    }

    // Parse cells
    size_t cellsArr = data.find("\"cells\"");
    if (cellsArr == std::string::npos) {
        messageBox("Invalid .wwp file (missing cells).", mfError | mfOKButton);
        return;
    }

    size_t pos = data.find('[', cellsArr);
    size_t arrEnd = data.find(']', pos);
    if (pos == std::string::npos || arrEnd == std::string::npos) return;

    while (pos < arrEnd) {
        size_t objStart = data.find('{', pos);
        if (objStart == std::string::npos || objStart >= arrEnd) break;
        // Find matching close brace (no nesting in cell objects)
        size_t objEnd = data.find('}', objStart);
        if (objEnd == std::string::npos) break;

        int cx = parseIntAfter(data, objStart, "x");
        int cy = parseIntAfter(data, objStart, "y");
        if (cx >= 0 && cy >= 0 && cx < canvas->getCols() && cy < canvas->getRows()) {
            PaintCell& cell = canvas->cellAt(cx, cy);
            cell.uOn = parseBoolAfter(data, objStart, "uOn");
            cell.lOn = parseBoolAfter(data, objStart, "lOn");
            int v;
            v = parseIntAfter(data, objStart, "uFg");  if (v >= 0) cell.uFg = (uint8_t)v;
            v = parseIntAfter(data, objStart, "lFg");  if (v >= 0) cell.lFg = (uint8_t)v;
            v = parseIntAfter(data, objStart, "qMask"); if (v >= 0) cell.qMask = (uint8_t)v;
            v = parseIntAfter(data, objStart, "qFg");  if (v >= 0) cell.qFg = (uint8_t)v;
            v = parseIntAfter(data, objStart, "textChar"); if (v >= 0) cell.textChar = (char)v;
            v = parseIntAfter(data, objStart, "textFg"); if (v >= 0) cell.textFg = (uint8_t)v;
            v = parseIntAfter(data, objStart, "textBg"); if (v >= 0) cell.textBg = (uint8_t)v;
        }
        pos = objEnd + 1;
    }

    filePath_ = pathStr;
    dirty_ = false;
    updateTitle();
    canvas->drawView();
}

// ── Export ANSI ───────────────────────────────────────────────────────────────

void TPaintWindow::doExportAnsi()
{
    if (!canvas) return;
    char name[256] = {};
    if (!filePath_.empty()) {
        size_t slash = filePath_.find_last_of("/\\");
        std::string fn = (slash == std::string::npos) ? filePath_ : filePath_.substr(slash + 1);
        if (fn.size() > 4 && fn.substr(fn.size() - 4) == ".wwp")
            fn = fn.substr(0, fn.size() - 4);
        std::strncpy(name, fn.c_str(), sizeof(name) - 1);
    }
    if (inputBox("Export ANSI", "~N~ame:", name, sizeof(name) - 1) != cmOK)
        return;
    std::string nameStr(name);
    if (nameStr.empty()) return;
    for (char& c : nameStr)
        if (!std::isalnum((unsigned char)c) && c != '-' && c != '_' && c != '.') c = '_';

    mkdir("paintings", 0755);
    std::string path = "paintings/" + nameStr + ".ans";

    // Map CGA index to SGR 38;5;N
    auto sgrFg = [](uint8_t c) -> std::string { return "\x1b[38;5;" + std::to_string(c) + "m"; };
    auto sgrBg = [](uint8_t c) -> std::string { return "\x1b[48;5;" + std::to_string(c) + "m"; };

    std::ostringstream os;
    int cols = canvas->getCols(), rows = canvas->getRows();
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const auto& c = canvas->cellAt(x, y);
            if (c.textChar != 0) {
                os << sgrFg(c.textFg) << sgrBg(c.textBg) << c.textChar;
            } else if (c.uOn && c.lOn) {
                os << sgrFg(c.uFg) << "\xe2\x96\x88"; // █
            } else if (c.uOn) {
                os << sgrFg(c.uFg) << "\xe2\x96\x80"; // ▀
            } else if (c.lOn) {
                os << sgrFg(c.lFg) << "\xe2\x96\x84"; // ▄
            } else if (c.qMask != 0) {
                os << sgrFg(c.qFg);
                if (c.qMask == 0x0F) os << "\xe2\x96\x88";
                else if (c.qMask == 0x05) os << "\xe2\x96\x8c";
                else if (c.qMask == 0x0A) os << "\xe2\x96\x90";
                else if (c.qMask & 0x03) os << "\xe2\x96\x80";
                else os << "\xe2\x96\x84";
            } else {
                os << " ";
            }
        }
        os << "\x1b[0m\n"; // reset at end of line
    }

    std::ofstream out(path);
    if (!out) { messageBox("Error writing file.", mfError | mfOKButton); return; }
    out << os.str();
    out.close();
    std::string msg = "Exported to " + path;
    messageBox(msg.c_str(), mfInformation | mfOKButton);
}

TWindow* createPaintWindow(const TRect &bounds)
{
    return new TPaintWindow(bounds);
}
