#define Uses_TBackground
#define Uses_TDrawBuffer
#include <tvision/tv.h>

#include "wibwob_background.h"

TWibWobBackground::TWibWobBackground(const TRect& bounds, char aPattern, uchar aFg, uchar aBg) noexcept
    : TBackground(bounds, aPattern),
      fgColor(aFg),
      bgColor(aBg)
{
}

void TWibWobBackground::setTexture(char ch)
{
    pattern = ch;
    drawView();
}

void TWibWobBackground::setColor(uchar fg, uchar bg)
{
    fgColor = fg;
    bgColor = bg;
    useRgb_ = false;
    drawView();
}

void TWibWobBackground::setColorRgb(uint32_t fg, uint32_t bg)
{
    rgbFg_ = fg;
    rgbBg_ = bg;
    useRgb_ = true;
    drawView();
}

void TWibWobBackground::setPreset(const std::string& name)
{
    for (auto& p : getDesktopPresets()) {
        if (name == p.name) {
            pattern = p.pattern;
            fgColor = p.fg;
            bgColor = p.bg;
            useRgb_ = p.useRgb;
            rgbFg_ = p.rgbFg;
            rgbBg_ = p.rgbBg;
            drawView();
            return;
        }
    }
}

std::string TWibWobBackground::getPresetName() const
{
    for (auto& p : getDesktopPresets()) {
        if (p.pattern == pattern && p.fg == fgColor && p.bg == bgColor
            && p.useRgb == useRgb_ && p.rgbFg == rgbFg_ && p.rgbBg == rgbBg_)
            return p.name;
    }
    return "custom";
}

void TWibWobBackground::draw()
{
    TDrawBuffer b;
    TColorAttr color;
    if (useRgb_) {
        color = TColorAttr(TColorRGB(rgbFg_), TColorRGB(rgbBg_));
    } else {
        color = TColorAttr(fgColor, bgColor);
    }
    b.moveChar(0, pattern, color, size.x);
    writeLine(0, 0, size.x, size.y, b);
}
