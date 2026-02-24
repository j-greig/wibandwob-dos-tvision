/*---------------------------------------------------------*/
/*                                                         */
/*   text_editor_view.cpp - API-Controllable Text Editor  */
/*                                                         */
/*   Multi-line text editor that can receive content      */
/*   via API calls for dynamic text/ASCII art display     */
/*                                                         */
/*---------------------------------------------------------*/

#include "text_editor_view.h"
#include "text_wrap.h"
#include "figlet_utils.h"

#define Uses_TWindow
#define Uses_TFrame
#define Uses_TColorAttr
#define Uses_TDrawBuffer
#define Uses_TKeys
#include <tvision/tv.h>

#include <cstring>
#include <algorithm>
#include <sstream>

/*---------------------------------------------------------*/
/* Construction                                            */
/*---------------------------------------------------------*/

TTextEditorView::TTextEditorView(const TRect &bounds)
    : TView(bounds), 
      cursorLine(0), 
      cursorCol(0),
      scrollTop(0), 
      scrollLeft(0),
      wordWrap(true),
      readOnly(false),
      showCursor(true),
      hScrollBar(nullptr),
      vScrollBar(nullptr)
{
    options |= ofSelectable;
    growMode = gfGrowHiX | gfGrowHiY;
    eventMask |= evBroadcast | evKeyboard;
    
    lines.push_back("");
    
    normalColor = TColorAttr(TColorRGB(220, 220, 220), TColorRGB(0, 0, 0));
    selectedColor = TColorAttr(TColorRGB(255, 255, 255), TColorRGB(0, 100, 200));

    rebuildVisualMap();
}

TTextEditorView::~TTextEditorView() {}

/*---------------------------------------------------------*/
/* Visual line mapping (logical <-> display)               */
/*---------------------------------------------------------*/

void TTextEditorView::rebuildVisualMap()
{
    visualMap.clear();
    int wrapWidth = size.x > 0 ? size.x : 80;

    for (size_t li = 0; li < lines.size(); ++li) {
        const std::string& line = lines[li];
        if (!wordWrap || line.empty()) {
            // One visual line per logical line (or empty)
            visualMap.push_back({li, 0, line.size()});
        } else {
            // Wrap at width, hard-break (no word-boundary for editor — simpler cursor math)
            size_t pos = 0;
            while (pos < line.size()) {
                size_t chunk = std::min(static_cast<size_t>(wrapWidth), line.size() - pos);
                visualMap.push_back({li, pos, chunk});
                pos += chunk;
            }
            if (line.empty() || (line.size() % wrapWidth == 0)) {
                // If line length is exact multiple of width, add empty continuation
                // so cursor can sit at end
            }
        }
    }
    if (visualMap.empty())
        visualMap.push_back({0, 0, 0});
}

size_t TTextEditorView::visualLineForCursor() const
{
    for (size_t vi = 0; vi < visualMap.size(); ++vi) {
        const auto& vm = visualMap[vi];
        if (vm.logicalLine == cursorLine) {
            if (cursorCol >= vm.startCol && cursorCol <= vm.startCol + vm.len) {
                // Check if cursor is at end of this visual line AND there's a next visual
                // line for the same logical line — cursor belongs to next visual line
                if (cursorCol == vm.startCol + vm.len && vi + 1 < visualMap.size() &&
                    visualMap[vi + 1].logicalLine == cursorLine) {
                    continue;  // cursor belongs to next visual line
                }
                return vi;
            }
        }
    }
    return visualMap.size() - 1;
}

void TTextEditorView::cursorFromVisualLine(size_t vi, size_t visualCol)
{
    if (vi >= visualMap.size()) vi = visualMap.size() - 1;
    const auto& vm = visualMap[vi];
    cursorLine = vm.logicalLine;
    size_t maxCol = vm.startCol + vm.len;
    cursorCol = std::min(vm.startCol + visualCol, maxCol);
    // Clamp to actual line length
    if (cursorCol > lines[cursorLine].size())
        cursorCol = lines[cursorLine].size();
}

/*---------------------------------------------------------*/
/* Draw                                                    */
/*---------------------------------------------------------*/

void TTextEditorView::draw()
{
    int W = size.x, H = size.y;
    if (W <= 0 || H <= 0) return;

    size_t curVisual = visualLineForCursor();

    for (int y = 0; y < H; ++y) {
        size_t vi = scrollTop + y;
        TDrawBuffer b;

        if (vi < visualMap.size()) {
            const auto& vm = visualMap[vi];
            const std::string& line = lines[vm.logicalLine];
            
            std::string visible;
            if (wordWrap) {
                visible = line.substr(vm.startCol, vm.len);
            } else {
                size_t start = std::min(scrollLeft, line.size());
                size_t end = std::min(scrollLeft + W, line.size());
                visible = line.substr(start, end - start);
            }

            int col = 0;
            if (!visible.empty()) {
                ushort w = b.moveCStr(col, visible.c_str(),
                    TAttrPair{normalColor, normalColor}, W - col);
                col += (w > 0 ? w : 0);
            }
            if (col < W)
                b.moveChar(col, ' ', normalColor, (ushort)(W - col));
        } else {
            b.moveChar(0, ' ', normalColor, (ushort)W);
        }

        writeLine(0, y, W, 1, b);
    }

    // Cursor
    if (showCursor && (state & sfFocused)) {
        if (curVisual >= scrollTop && curVisual < scrollTop + H) {
            const auto& vm = visualMap[curVisual];
            int cursorX, cursorY;
            if (wordWrap) {
                cursorX = static_cast<int>(cursorCol - vm.startCol);
            } else {
                cursorX = static_cast<int>(cursorCol - scrollLeft);
            }
            cursorY = static_cast<int>(curVisual - scrollTop);
            if (cursorX >= 0 && cursorX < W && cursorY >= 0 && cursorY < H) {
                setCursor(cursorX, cursorY);
            }
        }
    }
}

/*---------------------------------------------------------*/
/* Keyboard handling                                       */
/*---------------------------------------------------------*/

void TTextEditorView::handleEvent(TEvent &ev)
{
    TView::handleEvent(ev);
    if (readOnly) return;

    if (ev.what == evKeyDown) {
        bool handled = true;

        switch (ev.keyDown.keyCode) {
            case kbUp: {
                size_t vi = visualLineForCursor();
                size_t visualCol = cursorCol - visualMap[vi].startCol;
                if (vi > 0) {
                    cursorFromVisualLine(vi - 1, visualCol);
                }
                scrollToCursor();
                break;
            }
            case kbDown: {
                size_t vi = visualLineForCursor();
                size_t visualCol = cursorCol - visualMap[vi].startCol;
                if (vi + 1 < visualMap.size()) {
                    cursorFromVisualLine(vi + 1, visualCol);
                }
                scrollToCursor();
                break;
            }
            case kbLeft:
                if (cursorCol > 0) {
                    cursorCol--;
                } else if (cursorLine > 0) {
                    cursorLine--;
                    cursorCol = lines[cursorLine].size();
                }
                scrollToCursor();
                break;

            case kbRight:
                if (cursorCol < lines[cursorLine].size()) {
                    cursorCol++;
                } else if (cursorLine < lines.size() - 1) {
                    cursorLine++;
                    cursorCol = 0;
                }
                scrollToCursor();
                break;

            case kbHome:
                if (wordWrap) {
                    size_t vi = visualLineForCursor();
                    cursorCol = visualMap[vi].startCol;
                } else {
                    cursorCol = 0;
                }
                scrollToCursor();
                break;

            case kbEnd:
                if (wordWrap) {
                    size_t vi = visualLineForCursor();
                    cursorCol = visualMap[vi].startCol + visualMap[vi].len;
                    if (cursorCol > lines[cursorLine].size())
                        cursorCol = lines[cursorLine].size();
                } else {
                    cursorCol = lines[cursorLine].size();
                }
                scrollToCursor();
                break;

            case kbPgUp: {
                size_t vi = visualLineForCursor();
                size_t visualCol = cursorCol - visualMap[vi].startCol;
                size_t target = (vi > (size_t)size.y) ? vi - size.y : 0;
                cursorFromVisualLine(target, visualCol);
                scrollToCursor();
                break;
            }
            case kbPgDn: {
                size_t vi = visualLineForCursor();
                size_t visualCol = cursorCol - visualMap[vi].startCol;
                size_t target = std::min(vi + size.y, visualMap.size() - 1);
                cursorFromVisualLine(target, visualCol);
                scrollToCursor();
                break;
            }

            case kbEnter:
                if (cursorLine < lines.size()) {
                    std::string current = lines[cursorLine];
                    lines[cursorLine] = current.substr(0, cursorCol);
                    lines.insert(lines.begin() + cursorLine + 1, current.substr(cursorCol));
                    cursorLine++;
                    cursorCol = 0;
                    rebuildVisualMap();
                    scrollToCursor();
                }
                break;

            case kbBack:
                if (cursorCol > 0) {
                    lines[cursorLine].erase(cursorCol - 1, 1);
                    cursorCol--;
                    rebuildVisualMap();
                } else if (cursorLine > 0) {
                    cursorCol = lines[cursorLine - 1].size();
                    lines[cursorLine - 1] += lines[cursorLine];
                    lines.erase(lines.begin() + cursorLine);
                    cursorLine--;
                    rebuildVisualMap();
                }
                scrollToCursor();
                break;

            case kbDel:
                if (cursorCol < lines[cursorLine].size()) {
                    lines[cursorLine].erase(cursorCol, 1);
                    rebuildVisualMap();
                } else if (cursorLine < lines.size() - 1) {
                    lines[cursorLine] += lines[cursorLine + 1];
                    lines.erase(lines.begin() + cursorLine + 1);
                    rebuildVisualMap();
                }
                break;

            default:
                if (ev.keyDown.charScan.charCode >= 32 &&
                    ev.keyDown.charScan.charCode < 127) {
                    char ch = ev.keyDown.charScan.charCode;
                    lines[cursorLine].insert(cursorCol, 1, ch);
                    cursorCol++;
                    rebuildVisualMap();
                    scrollToCursor();
                } else {
                    handled = false;
                }
                break;
        }

        if (handled) {
            drawView();
            clearEvent(ev);
        }
    }
}

/*---------------------------------------------------------*/
/* Scrolling                                               */
/*---------------------------------------------------------*/

void TTextEditorView::scrollToCursor()
{
    size_t vi = visualLineForCursor();
    if (vi < scrollTop) {
        scrollTop = vi;
    } else if (vi >= scrollTop + size.y) {
        scrollTop = vi - size.y + 1;
    }

    if (!wordWrap) {
        if (cursorCol < scrollLeft)
            scrollLeft = cursorCol;
        else if (cursorCol >= scrollLeft + size.x)
            scrollLeft = cursorCol - size.x + 1;
    }

    updateScrollBars();
}

void TTextEditorView::scrollToEnd()
{
    if (visualMap.size() > (size_t)size.y)
        scrollTop = visualMap.size() - size.y;
    else
        scrollTop = 0;

    cursorLine = lines.size() - 1;
    cursorCol = lines[cursorLine].size();
    scrollToCursor();
}

void TTextEditorView::updateScrollBars()
{
    if (vScrollBar) {
        int maxY = std::max(0, static_cast<int>(visualMap.size()) - size.y);
        vScrollBar->setParams(static_cast<int>(scrollTop), 0, maxY, size.y - 1, 1);
    }
}

/*---------------------------------------------------------*/
/* Content API                                             */
/*---------------------------------------------------------*/

void TTextEditorView::sendText(const std::string& content, const std::string& mode, const std::string& position)
{
    if (mode == "replace") {
        replaceContent(content);
    } else if (mode == "append") {
        appendText(content);
    } else if (mode == "insert") {
        if (position == "cursor")
            insertText(content, cursorLine, cursorCol);
        else if (position == "start")
            insertText(content, 0, 0);
        else
            appendText(content);
    }

    rebuildVisualMap();
    scrollToCursor();
    drawView();
}

void TTextEditorView::clearContent()
{
    lines.clear();
    lines.push_back("");
    cursorLine = 0;
    cursorCol = 0;
    scrollTop = 0;
    scrollLeft = 0;
    rebuildVisualMap();
    drawView();
}

void TTextEditorView::insertText(const std::string& content, size_t lineIndex, size_t colIndex)
{
    std::istringstream iss(content);
    std::string line;
    std::vector<std::string> newLines;
    while (std::getline(iss, line))
        newLines.push_back(line);
    if (newLines.empty()) return;

    lineIndex = std::min(lineIndex, lines.size() - 1);
    colIndex = std::min(colIndex, lines[lineIndex].size());

    if (newLines.size() == 1) {
        lines[lineIndex].insert(colIndex, newLines[0]);
        cursorLine = lineIndex;
        cursorCol = colIndex + newLines[0].size();
    } else {
        std::string current = lines[lineIndex];
        std::string left = current.substr(0, colIndex);
        std::string right = current.substr(colIndex);

        lines[lineIndex] = left + newLines[0];
        for (size_t i = 1; i < newLines.size() - 1; ++i)
            lines.insert(lines.begin() + lineIndex + i, newLines[i]);
        lines.insert(lines.begin() + lineIndex + newLines.size() - 1,
                     newLines.back() + right);

        cursorLine = lineIndex + newLines.size() - 1;
        cursorCol = newLines.back().size();
    }
}

void TTextEditorView::appendText(const std::string& content)
{
    if (lines.empty()) lines.push_back("");
    cursorLine = lines.size() - 1;
    cursorCol = lines[cursorLine].size();
    insertText(content, cursorLine, cursorCol);
}

void TTextEditorView::replaceContent(const std::string& content)
{
    lines.clear();
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line))
        lines.push_back(line);
    if (lines.empty()) lines.push_back("");

    cursorLine = 0;
    cursorCol = 0;
    scrollTop = 0;
    scrollLeft = 0;
}

/*---------------------------------------------------------*/
/* View state                                              */
/*---------------------------------------------------------*/

void TTextEditorView::setState(ushort s, Boolean en)
{
    TView::setState(s, en);
    if ((s & sfFocused) != 0) drawView();
}

void TTextEditorView::changeBounds(const TRect& b)
{
    TView::changeBounds(b);
    rebuildVisualMap();
    scrollToCursor();
    drawView();
}

/*---------------------------------------------------------*/
/* Figlet                                                  */
/*---------------------------------------------------------*/

std::string TTextEditorView::runFiglet(const std::string& text, const std::string& font, int width)
{
    return figlet::render(text, font, width);
}

void TTextEditorView::sendFigletText(const std::string& text, const std::string& font, int width, const std::string& mode)
{
    int figletWidth = (width > 0) ? width : size.x - 2;
    std::string figletOutput = runFiglet(text, font, figletWidth);
    if (!figletOutput.empty())
        sendText(figletOutput, mode, "end");
    else
        sendText("[ " + text + " ]\n", mode, "end");
}

/*---------------------------------------------------------*/
/* Window wrapper                                          */
/*---------------------------------------------------------*/

TTextEditorWindow::TTextEditorWindow(const TRect &r, const char *title)
    : TWindow(r, title ? title : "Text Editor", wnNoNumber),
      TWindowInit(&TTextEditorWindow::initFrame),
      editorView(nullptr)
{
}

void TTextEditorWindow::setup()
{
    options |= ofTileable;
    TRect c = getExtent();
    c.grow(-1, -1);

    // Vertical scroll bar
    TRect sbRect = c;
    sbRect.a.x = sbRect.b.x - 1;
    auto* vsb = new TScrollBar(sbRect);
    vsb->growMode = gfGrowLoX | gfGrowHiX | gfGrowHiY;
    insert(vsb);

    // Editor view (leave room for scroll bar)
    TRect edRect = c;
    edRect.b.x -= 1;
    editorView = new TTextEditorView(edRect);
    editorView->vScrollBar = vsb;
    insert(editorView);
}

void TTextEditorWindow::changeBounds(const TRect& b)
{
    TWindow::changeBounds(b);
    setState(sfExposed, True);
    redraw();
}

TFrame* TTextEditorWindow::initFrame(TRect r)
{
    return new TFrame(r);
}

TWindow* createTextEditorWindow(const TRect &bounds, const char *title)
{
    auto *w = new TTextEditorWindow(bounds, title);
    w->setup();
    return w;
}
