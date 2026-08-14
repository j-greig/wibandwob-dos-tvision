/*---------------------------------------------------------*/
/*                                                         */
/*   ascii_grid_view.h - Fixed Cell ASCII Grid View       */
/*                                                         */
/*---------------------------------------------------------*/

#ifndef ASCII_GRID_VIEW_H
#define ASCII_GRID_VIEW_H

#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TRect
#include <tvision/tv.h>
#include <vector>

#include "theme_manager.h"

class TAsciiGridView : public TView {
public:
    TAsciiGridView(const TRect &bounds, int gridW, int gridH);

    void resizeGrid(int gridW, int gridH);
    void clear(TColorAttr attr = ThemeManager::attr(SkinRole::Paper), char ch = ' ');
    void putChar(int x, int y, char ch, TColorAttr attr = ThemeManager::attr(SkinRole::Paper));
    void putGlyph(int x, int y, const std::string &utf8, TColorAttr attr = ThemeManager::attr(SkinRole::Paper));

    virtual void draw() override;

    int gridWidth() const { return gw; }
    int gridHeight() const { return gh; }

private:
    int gw, gh;
    std::vector<std::string> glyphs; // per-cell UTF-8 (1-cell leader of wide glyph)
    std::vector<uint8_t> flags;      // bit0: trail occupied (skip drawing)
    std::vector<TColorAttr> attrs;
};

#endif // ASCII_GRID_VIEW_H
