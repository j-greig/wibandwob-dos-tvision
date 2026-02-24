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
#include "paint_wwp_codec.h"

#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TKeys
#define Uses_TProgram
#define Uses_MsgBox
#define Uses_TClipboard
#define Uses_TFileDialog
#define Uses_MsgBox
#include <tvision/tv.h>

#include "../figlet_utils.h"
#include "figlet_stamp_dialog.h"

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
static const ushort cmPaintStampFiglet = 630;

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
            *new TMenuItem("~S~ave", cmPaintSave, kbNoKey) +
            *new TMenuItem("Save ~A~s...", cmPaintSaveAs, kbNoKey) +
            *new TMenuItem("~O~pen...", cmPaintOpen, kbNoKey) +
            newLine() +
            *new TMenuItem("E~x~port ANSI...", cmPaintExportAnsi, kbNoKey) +
            newLine() +
            *new TMenuItem("~C~lose", cmPaintClose, kbNoKey) +
        *new TSubMenu("~E~dit", kbAltE) +
            *new TMenuItem("C~l~ear Canvas", cmPaintClear, kbNoKey) +
            *new TMenuItem("~C~opy as Text", cmPaintCopyText, kbCtrlIns) +
            newLine() +
            *new TMenuItem("Stamp ~F~IGlet...", cmPaintStampFiglet, kbNoKey) +
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
            case cmPaintStampFiglet:
                doStampFiglet();
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
    mkdir("paintings", 0755);

    char fileName[MAXPATH];
    if (!filePath_.empty())
        std::strncpy(fileName, filePath_.c_str(), sizeof(fileName) - 1);
    else
        std::strcpy(fileName, "paintings/*.wwp");

    TFileDialog* dlg = new TFileDialog("paintings/*.wwp", "Save Paint As", "~N~ame", fdOKButton, 100);
    if (TProgram::application->executeDialog(dlg, fileName) == cmCancel)
        return;

    std::string path(fileName);
    if (path.empty()) return;

    // Ensure .wwp extension
    if (path.size() < 4 || path.substr(path.size() - 4) != ".wwp")
        path += ".wwp";

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

void TPaintWindow::doOpen()
{
    mkdir("paintings", 0755);
    char fileName[MAXPATH];
    std::strcpy(fileName, "paintings/*.wwp");

    TFileDialog* dlg = new TFileDialog("paintings/*.wwp", "Open Paint File", "~N~ame", fdOpenButton, 100);
    if (TProgram::application->executeDialog(dlg, fileName) == cmCancel)
        return;

    std::string pathStr(fileName);
    if (pathStr.empty()) return;

    std::ifstream in(pathStr);
    if (!in) {
        std::string msg = std::string("Cannot open: ") + pathStr;
        messageBox(msg.c_str(), mfError | mfOKButton);
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

    if (!loadWwpFromString(data, canvas)) {
        messageBox("Invalid .wwp file (missing cells).", mfError | mfOKButton);
        return;
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
    mkdir("paintings", 0755);

    char fileName[MAXPATH];
    if (!filePath_.empty()) {
        // Pre-fill with current name but .ans extension
        std::string def = filePath_;
        if (def.size() > 4 && def.substr(def.size() - 4) == ".wwp")
            def = def.substr(0, def.size() - 4) + ".ans";
        std::strncpy(fileName, def.c_str(), sizeof(fileName) - 1);
    } else {
        std::strcpy(fileName, "paintings/*.ans");
    }

    TFileDialog* dlg = new TFileDialog("paintings/*.ans", "Export ANSI", "~N~ame", fdOKButton, 100);
    if (TProgram::application->executeDialog(dlg, fileName) == cmCancel)
        return;

    std::string path(fileName);
    if (path.empty()) return;
    if (path.size() < 4 || path.substr(path.size() - 4) != ".ans")
        path += ".ans";

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

// ── Stamp FIGlet ──────────────────────────────────────────────────────────────

void TPaintWindow::doStampFiglet()
{
    auto* canvas = getCanvas();
    if (!canvas) return;

    auto* dlg = new TFigletStampDialog(figletText_, figletFont_);
    ushort cmd = TProgram::application->execView(dlg);
    FigletStampResult r;
    if (cmd == cmOK) {
        r = dlg->getResult();
    }
    TObject::destroy(dlg);
    if (cmd != cmOK) return;

    if (r.text.empty()) return;

    // Render
    int cols = canvas->getCols();
    auto lines = figlet::renderLines(r.text, r.font, cols);
    if (lines.empty()) {
        messageBox("FIGlet render failed (bad font name?).", mfError | mfOKButton);
        return;
    }

    // Stamp at cursor — transparent spaces (skip space chars)
    int ox = canvas->getX();
    int oy = canvas->getY();
    uint8_t fg = canvas->getFg();
    uint8_t bg = canvas->getBg();

    for (int row = 0; row < (int)lines.size(); row++) {
        int cy = oy + row;
        if (cy < 0 || cy >= canvas->getRows()) continue;
        const std::string& line = lines[row];
        for (int col = 0; col < (int)line.size(); col++) {
            char ch = line[col];
            if (ch == ' ' || ch == '\0') continue;  // transparent
            int cx = ox + col;
            if (cx < 0 || cx >= canvas->getCols()) continue;
            PaintCell& cell = canvas->cellAt(cx, cy);
            cell.textChar = ch;
            cell.textFg = fg;
            cell.textBg = bg;
        }
    }

    figletText_ = r.text;
    figletFont_ = r.font;
    dirty_ = true;
    canvas->drawView();
}

TWindow* createPaintWindow(const TRect &bounds)
{
    return new TPaintWindow(bounds);
}
