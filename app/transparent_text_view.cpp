/*---------------------------------------------------------*/
/*                                                         */
/*   transparent_text_view.cpp - Text View with           */
/*   Transparent/Custom Background Support                */
/*                                                         */
/*---------------------------------------------------------*/

#include "transparent_text_view.h"
#include <algorithm>

/*---------------------------------------------------------*/
/* TTransparentTextView Implementation                    */
/*---------------------------------------------------------*/

TTransparentTextView::TTransparentTextView(const TRect& bounds,
                                           TScrollBar* hScroll,
                                           TScrollBar* vScroll,
                                           const std::string& filePath)
    : TScroller(bounds, hScroll, vScroll),
      fileName(filePath),
      bgColor(0, 0, 0),
      fgColor(220, 220, 220),
      useCustomBg(false),
      wordWrap(true)
{
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable;

    loadFile(filePath);
    rebuildDisplayLines();
}

void TTransparentTextView::loadFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        rawLines.push_back("Error: Could not open file");
        rawLines.push_back(path);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        rawLines.push_back(line);
    }

    if (rawLines.empty())
        rawLines.push_back("");
}

/*---------------------------------------------------------*/
/* Word wrapping — same algorithm as TWibWobMessageView  */
/*---------------------------------------------------------*/

std::vector<std::string> TTransparentTextView::wrapText(const std::string& text, int width)
{
    std::vector<std::string> lines;
    if (width <= 0) {
        lines.emplace_back("");
        return lines;
    }

    if (text.empty()) {
        lines.emplace_back("");
        return lines;
    }

    size_t pos = 0;
    while (pos < text.size()) {
        size_t remaining = text.size() - pos;
        size_t slice = remaining > static_cast<size_t>(width)
                       ? static_cast<size_t>(width) : remaining;
        bool trimmedSpace = false;

        if (remaining > static_cast<size_t>(width)) {
            size_t breakPos = text.find_last_of(" \t", pos + width - 1);
            if (breakPos != std::string::npos && breakPos >= pos) {
                size_t candidate = breakPos - pos;
                if (candidate > 0) {
                    slice = candidate;
                    trimmedSpace = true;
                }
            }
        }

        lines.push_back(text.substr(pos, slice));
        pos += slice;

        if (trimmedSpace) {
            while (pos < text.size() && text[pos] == ' ')
                ++pos;
        }
    }

    return lines;
}

void TTransparentTextView::rebuildDisplayLines()
{
    displayLines.clear();

    int wrapWidth = size.x > 0 ? size.x : 80;

    for (const auto& raw : rawLines) {
        if (wordWrap) {
            auto wrapped = wrapText(raw, wrapWidth);
            for (auto& wl : wrapped)
                displayLines.push_back(std::move(wl));
        } else {
            displayLines.push_back(raw);
        }
    }

    // Calculate max line width for horizontal scroll
    int maxWidth = 0;
    for (const auto& l : displayLines)
        maxWidth = std::max(maxWidth, static_cast<int>(l.size()));

    setLimit(wordWrap ? size.x : maxWidth, static_cast<int>(displayLines.size()));

    if (vScrollBar)
        vScrollBar->drawView();
}

void TTransparentTextView::changeBounds(const TRect& bounds)
{
    TScroller::changeBounds(bounds);
    rebuildDisplayLines();
}

void TTransparentTextView::setWordWrap(bool enable)
{
    if (wordWrap != enable) {
        wordWrap = enable;
        rebuildDisplayLines();
        drawView();
    }
}

void TTransparentTextView::draw()
{
    TDrawBuffer b;

    // Determine fill attribute/character.
    TColorAttr textAttr;
    char fillChar = ' ';

    if (useCustomBg) {
        textAttr = TColorAttr(fgColor, bgColor);
    } else {
        if (TProgram::deskTop && TProgram::deskTop->background) {
            TBackground *bg = TProgram::deskTop->background;
            textAttr = bg->getColor(0x01);
            fillChar = bg->pattern;
        } else {
            TColorDesired defaultFg = {};
            TColorDesired defaultBg = {};
            textAttr = TColorAttr(defaultFg, defaultBg);
        }
    }

    int totalLines = static_cast<int>(displayLines.size());

    for (int y = 0; y < size.y; y++)
    {
        int lineIdx = delta.y + y;

        b.moveChar(0, fillChar, textAttr, size.x);

        if (lineIdx >= 0 && lineIdx < totalLines)
        {
            const std::string& line = displayLines[lineIdx];

            // Apply horizontal scroll offset
            int hOff = delta.x;
            if (hOff < static_cast<int>(line.size())) {
                std::string visible = line.substr(hOff);
                b.moveStr(0, TStringView(visible.data(), visible.size()), textAttr);
            }
        }

        writeLine(0, y, size.x, 1, b);
    }
}

void TTransparentTextView::setBackgroundColor(TColorRGB color)
{
    bgColor = color;
    useCustomBg = true;
    drawView();
}

void TTransparentTextView::setBackgroundToDefault()
{
    useCustomBg = false;
    drawView();
}

/*---------------------------------------------------------*/
/* TTransparentTextWindow Implementation                  */
/*---------------------------------------------------------*/

TTransparentTextWindow::TTransparentTextWindow(const TRect& bounds,
                                             const std::string& title,
                                             const std::string& filePath)
    : TWindow(bounds, title.c_str(), wnNoNumber),
      TWindowInit(&TTransparentTextWindow::initFrame),
      vScrollBar(nullptr)
{
    options |= ofTileable;

    TRect interior = getExtent();
    interior.grow(-1, -1);

    // Vertical scroll bar on the right edge
    TRect sbRect = interior;
    sbRect.a.x = sbRect.b.x - 1;
    vScrollBar = new TScrollBar(sbRect);
    vScrollBar->growMode = gfGrowLoX | gfGrowHiX | gfGrowHiY;
    insert(vScrollBar);

    // Text view fills remaining space
    TRect tvRect = interior;
    tvRect.b.x -= 1;  // Leave room for scroll bar
    textView = new TTransparentTextView(tvRect, nullptr, vScrollBar, filePath);
    insert(textView);
}

void TTransparentTextWindow::changeBounds(const TRect& bounds)
{
    TWindow::changeBounds(bounds);
    setState(sfExposed, True);

    if (textView)
        textView->drawView();

    redraw();
}

const std::string& TTransparentTextWindow::getFilePath() const {
    return textView->getFileName();
}

TFrame* TTransparentTextWindow::initFrame(TRect r)
{
    return new TFrame(r);
}
