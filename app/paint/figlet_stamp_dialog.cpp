/*---------------------------------------------------------*/
/*   figlet_stamp_dialog.cpp — FIGlet stamp picker dialog  */
/*---------------------------------------------------------*/

#include "figlet_stamp_dialog.h"
#include "../figlet_utils.h"

#define Uses_TEvent
#define Uses_TKeys
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

TFigletStampDialog::TFigletStampDialog(const std::string& initialText,
                                       const std::string& initialFont)
    : TWindowInit(&TFigletStampDialog::initFrame),
      TDialog(TRect(2, 1, 76, 22), "Stamp FIGlet Text")
{

    // ── Text input ──
    textInput_ = new TInputLine(TRect(8, 1, 72, 2), 255);
    insert(textInput_);
    insert(new TLabel(TRect(2, 1, 7, 2), "~T~ext", textInput_));
    if (!initialText.empty()) {
        std::strncpy(lastText_, initialText.c_str(), sizeof(lastText_) - 1);
        textInput_->setData(lastText_);
    }

    // ── Font list (left) ──
    fontScroll_ = new TScrollBar(TRect(23, 3, 24, 13));
    insert(fontScroll_);
    fontList_ = new TListBox(TRect(2, 3, 23, 13), 1, fontScroll_);
    insert(fontList_);
    insert(new TLabel(TRect(2, 2, 10, 3), "~F~onts", fontList_));

    // Populate font list — use TStringCollection (sorted)
    const auto& fonts = figlet::allFontsSorted();
    fontNames_ = new TStringCollection((short)fonts.size(), 1);
    for (size_t i = 0; i < fonts.size(); i++) {
        fontNames_->insert(newStr(fonts[i].c_str()));
    }
    fontList_->newList(fontNames_);

    // Select initial font in the sorted collection
    currentFont_ = initialFont.empty() ? "standard" : initialFont;
    lastFontIdx_ = 0;  // focusItem crashes during construction; start at 0

    // ── Preview (right) ──
    preview_ = new TFigletPreview(TRect(25, 3, 72, 13));
    insert(preview_);

    // ── Buttons ──
    insert(new TButton(TRect(48, 14, 58, 16), "~O~K", cmOK, bfDefault));
    insert(new TButton(TRect(60, 14, 72, 16), "Cancel", cmCancel, bfNormal));

    // Initial preview
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
