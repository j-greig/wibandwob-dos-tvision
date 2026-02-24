/*---------------------------------------------------------*/
/*   figlet_stamp_dialog.cpp — FIGlet stamp picker dialog  */
/*---------------------------------------------------------*/

#include "figlet_stamp_dialog.h"
#include "../figlet_utils.h"
#include "../ui/ui_helpers.h"

#define Uses_TEvent
#define Uses_TKeys
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_MsgBox
#include <tvision/tv.h>

#include <cstring>
#include <algorithm>

// ── TFigletPreview ────────────────────────────────────────

TFigletPreview::TFigletPreview(const TRect& bounds)
    : TView(bounds)
{
    growMode = gfGrowHiX | gfGrowHiY;
}

void TFigletPreview::setLines(const std::vector<std::string>& lines)
{
    lines_ = lines;
    drawView();
}

void TFigletPreview::draw()
{
    TColorAttr attr{0x1F}; // white on blue — safe hardcoded
    TDrawBuffer b;
    for (int y = 0; y < size.y; y++) {
        b.moveChar(0, ' ', attr, size.x);
        if (y < (int)lines_.size()) {
            const std::string& line = lines_[y];
            int len = std::min((int)line.size(), (int)size.x);
            b.moveStr(0, TStringView(line.data(), len), attr);
        }
        writeLine(0, y, size.x, 1, b);
    }
}

// ── TFigletStampDialog ───────────────────────────────────

/*
   Layout (roughly 70×20):
   ┌─ Stamp FIGlet Text ──────────────────────────────────┐
   │ Text: [______________________________________]        │
   │ ┌─ Fonts ──────────┐ ┌─ Preview ──────────────────┐  │
   │ │ standard         ↑│ │  _   _ ___               │  │
   │ │ big              ││ │ | | | |_ _|              │  │
   │ │ banner           ││ │ | |_| || |               │  │
   │ │ slant            ↓│ │ |  _  || |               │  │
   │ └──────────────────┘ │ |_| |_|___|              │  │
   │                       └───────────────────────────┘  │
   │                              [ OK ]  [ Cancel ]       │
   └──────────────────────────────────────────────────────┘
*/

static TRect calcStampDialogBounds() {
    TRect d = TProgram::deskTop->getExtent();
    int dw = d.b.x - d.a.x;
    int dh = d.b.y - d.a.y;
    int w = std::max(60, (int)(dw * 0.85));
    int h = std::max(18, (int)(dh * 0.85));
    int left = d.a.x + (dw - w) / 2;
    int top  = d.a.y + (dh - h) / 2;
    return TRect(left, top, left + w, top + h);
}

TFigletStampDialog::TFigletStampDialog(const std::string& initialText,
                                       const std::string& initialFont)
    : TWindowInit(&TFigletStampDialog::initFrame),
      TDialog(calcStampDialogBounds(), "Stamp FIGlet Text")
{
    int W = size.x;   // inner width
    int H = size.y;   // inner height
    int listW = 22;
    int listTop = 3;
    int listBot = H - 4;
    int prevLeft = listW + 3;

    // ── Text input (full width) ──
    textInput_ = new TInputLine(TRect(8, 1, W - 2, 2), 255);
    insert(textInput_);
    insert(new TLabel(TRect(2, 1, 7, 2), "~T~ext", textInput_));
    if (!initialText.empty()) {
        std::strncpy(lastText_, initialText.c_str(), sizeof(lastText_) - 1);
        textInput_->setData(lastText_);
    }

    // ── Font list (left column) ──
    fontScroll_ = new TScrollBar(TRect(listW + 1, listTop, listW + 2, listBot));
    insert(fontScroll_);
    fontList_ = new TListBox(TRect(2, listTop, listW + 1, listBot), 1, fontScroll_);
    insert(fontList_);
    insert(new TLabel(TRect(2, 2, 10, 3), "~F~onts", fontList_));

    // Populate font list
    const auto& fonts = figlet::allFontsSorted();
    fontNames_ = makeStringCollection(fonts);
    fontList_->newList(fontNames_);

    // Select initial font
    currentFont_ = initialFont.empty() ? "standard" : initialFont;
    lastFontIdx_ = 0;

    // ── Preview (right, fills remaining space) ──
    preview_ = new TFigletPreview(TRect(prevLeft, listTop, W - 2, listBot));
    insert(preview_);

    // ── Buttons (bottom right) ──
    insert(new TButton(TRect(W - 26, H - 3, W - 16, H - 1), "~O~K", cmOK, bfDefault));
    insert(new TButton(TRect(W - 14, H - 3, W - 2, H - 1), "Cancel", cmCancel, bfNormal));

    updatePreview();
    textInput_->select();
}

int TFigletStampDialog::findFontIndex(const std::string& name)
{
    if (!fontNames_) return -1;
    for (int i = 0; i < fontNames_->getCount(); i++) {
        const char* s = (const char*)fontNames_->at(i);
        if (s && name == s) return i;
    }
    return -1;
}

void TFigletStampDialog::updatePreview()
{
    char buf[256] = {};
    textInput_->getData(buf);
    std::string text(buf);
    if (text.empty()) text = "Preview";

    int previewW = preview_->size.x;
    auto lines = figlet::renderLines(text, currentFont_, previewW);
    if (lines.empty())
        lines.push_back("(no preview)");
    preview_->setLines(lines);
}

void TFigletStampDialog::handleEvent(TEvent& event)
{
    TDialog::handleEvent(event);

    // Check if font selection changed
    int idx = fontList_->focused;
    if (idx != lastFontIdx_ && idx >= 0 && idx < fontNames_->getCount()) {
        lastFontIdx_ = idx;
        const char* s = (const char*)fontNames_->at(idx);
        if (s) {
            currentFont_ = s;
            updatePreview();
        }
    }

    // Check if text changed
    char buf[256] = {};
    textInput_->getData(buf);
    if (std::strcmp(buf, lastText_) != 0) {
        std::strncpy(lastText_, buf, sizeof(lastText_) - 1);
        updatePreview();
    }
}

FigletStampResult TFigletStampDialog::getResult() const
{
    FigletStampResult r;
    char buf[256] = {};
    const_cast<TInputLine*>(textInput_)->getData(buf);
    r.text = buf;
    r.font = currentFont_;
    r.ok = true;
    return r;
}
