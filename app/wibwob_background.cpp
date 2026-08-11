#define Uses_TBackground
#define Uses_TDrawBuffer
#define Uses_TMenuBox
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TRect
#define Uses_TGroup
#define Uses_TView
#include <tvision/tv.h>

#include "wibwob_background.h"

// Must match constants in wwdos_app.cpp
static const ushort cmDeskPresetBase = 260;
static const ushort cmDeskGallery = 270;

TWibWobBackground::TWibWobBackground(const TRect& bounds, char aPattern, uchar aFg, uchar aBg) noexcept
    : TBackground(bounds, aPattern),
      fgColor(aFg),
      bgColor(aBg)
{
    eventMask |= evMouseDown;
}

void TWibWobBackground::setTexture(char ch)
{
    pattern = ch;
    patternUtf8_.clear();
    drawView();
}

void TWibWobBackground::setTextureUtf8(const std::string& glyph)
{
    if (glyph.size() <= 1) {
        setTexture(glyph.empty() ? ' ' : glyph[0]);
        return;
    }
    patternUtf8_ = glyph;
    pattern = ' ';  // legacy field: keep sane for preset comparisons
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

void TWibWobBackground::handleEvent(TEvent& event)
{
    if (event.what == evMouseDown && event.mouse.buttons == 0x02) {
        // Build preset submenu
        const auto& presets = getDesktopPresets();
        TMenuItem* last = nullptr;
        // Build in reverse so linked list is in order
        for (int i = (int)presets.size() - 1; i >= 0; --i) {
            auto* item = new TMenuItem(presets[i].name,
                cmDeskPresetBase + i, kbNoKey, hcNoContext, nullptr, last);
            last = item;
        }
        TMenu* presetSub = new TMenu(*last);

        TMenu* popup = new TMenu(
            *new TMenuItem("~P~reset", kbNoKey, presetSub, hcNoContext,
            new TMenuItem("~G~allery Mode", cmDeskGallery, kbNoKey, hcNoContext, nullptr,
            nullptr)));

        // event.mouse.where is in owner (desktop) coords
        TRect ownerExt = owner->getExtent();
        TRect r(event.mouse.where.x, event.mouse.where.y,
                ownerExt.b.x, ownerExt.b.y);

        TMenuBox* box = new TMenuBox(r, popup, nullptr);
        ushort cmd = owner->execView(box);
        destroy(box);

        if (cmd >= cmDeskPresetBase && cmd < cmDeskPresetBase + (ushort)presets.size()) {
            setPreset(presets[cmd - cmDeskPresetBase].name);
        } else if (cmd == cmDeskGallery) {
            // Broadcast gallery toggle to app
            TEvent galEvt = {};
            galEvt.what = evCommand;
            galEvt.message.command = cmDeskGallery;
            putEvent(galEvt);
        }

        clearEvent(event);
        return;
    }
    TBackground::handleEvent(event);
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
    if (!patternUtf8_.empty()) {
        // UTF-8 fill glyph (▒ ░ …): build one row of repeated glyphs.
        std::string row;
        row.reserve(patternUtf8_.size() * size.x);
        for (int i = 0; i < size.x; ++i) row += patternUtf8_;
        b.moveStr(0, row, color);
    } else {
        b.moveChar(0, pattern, color, size.x);
    }
    writeLine(0, 0, size.x, size.y, b);

    if (rulers_ && size.x > 2 && size.y > 2) {
        // Top ruler: repeating digits, magenta on black (mockup style).
        TColorAttr topAttr(TColorRGB(0xFF, 0x55, 0xFF), TColorRGB(0x00, 0x00, 0x00));
        TDrawBuffer tb;
        for (int x = 0; x < size.x; ++x) {
            char d = (char)('0' + (x % 10));
            tb.moveChar(x, d, topAttr, 1);
        }
        writeLine(0, 0, size.x, 1, tb);

        // Left ruler: vertical digits, red on teal.
        TColorAttr leftAttr(TColorRGB(0xAA, 0x00, 0x00), TColorRGB(0x00, 0xAA, 0xAA));
        for (int y = 1; y < size.y; ++y) {
            TDrawBuffer lb;
            lb.moveChar(0, (char)('0' + (y % 10)), leftAttr, 1);
            writeLine(0, y, 1, 1, lb);
        }
    }
}
