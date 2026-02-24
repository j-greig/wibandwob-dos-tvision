#pragma once

#define Uses_TBackground
#define Uses_TDrawBuffer
#include <tvision/tv.h>

#include <string>
#include <vector>

struct DesktopPreset {
    const char* name;
    char pattern;
    uchar fg;        // CGA 0-15 (used when useRgb == false)
    uchar bg;
    bool useRgb;     // true = use rgbFg/rgbBg instead of CGA
    uint32_t rgbFg;  // 0xRRGGBB
    uint32_t rgbBg;
};

inline const std::vector<DesktopPreset>& getDesktopPresets() {
    static const std::vector<DesktopPreset> presets = {
        {"default",       '\xB1', 7,  1,  false, 0, 0},            // ▒ light grey on blue (classic TV)
        {"jet_black",     ' ',    0,  0,  true,  0x000000, 0x000000}, // true black RGB
        {"dark_grey",     ' ',    8,  0,  true,  0x555555, 0x333333}, // dark grey
        {"terminal",      '\xB0', 8,  0,  true,  0x555555, 0x000000}, // ░ dark grey on black (CRT)
        {"cga_cyan",      '\xB1', 15, 3,  true,  0xFFFFFF, 0x00AAAA}, // ▒ white on CGA cyan
        {"cga_green",     '\xB0', 10, 0,  true,  0x55FF55, 0x000000}, // ░ CGA bright green on black
        {"noise",         '%',    8,  0,  true,  0x555555, 0x000000}, // grungy
        {"white_paper",   ' ',    15, 15, true,  0xFFFFFF, 0xFFFFFF}, // true white RGB
        {"gallery_wall",  ' ',    0,  0,  true,  0x000000, 0x000000}, // true black — gallery mode
    };
    return presets;
}

class TWibWobBackground : public TBackground {
public:
    TWibWobBackground(const TRect& bounds, char aPattern, uchar aFg, uchar aBg) noexcept;

    void setTexture(char ch);
    void setColor(uchar fg, uchar bg);
    void setColorRgb(uint32_t fg, uint32_t bg);
    void setPreset(const std::string& name);
    std::string getPresetName() const;

    virtual void draw() override;
    virtual void handleEvent(TEvent& event) override;

    uchar getFg() const { return fgColor; }
    uchar getBg() const { return bgColor; }
    bool  isRgb() const { return useRgb_; }
    uint32_t getRgbFg() const { return rgbFg_; }
    uint32_t getRgbBg() const { return rgbBg_; }
    char  getPattern() const { return pattern; }

private:
    uchar fgColor;
    uchar bgColor;
    bool useRgb_ = false;
    uint32_t rgbFg_ = 0;
    uint32_t rgbBg_ = 0;
};
