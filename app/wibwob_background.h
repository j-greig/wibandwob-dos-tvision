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
    // UTF-8 aware: multi-byte glyphs (▒ ░ etc) render correctly; a single
    // ASCII byte behaves exactly like setTexture(char).
    void setTextureUtf8(const std::string& glyph);
    void setColor(uchar fg, uchar bg);
    void setColorRgb(uint32_t fg, uint32_t bg);
    void setPreset(const std::string& name);
    std::string getPresetName() const;

    virtual void draw() override;
    virtual void handleEvent(TEvent& event) override;

    // MSDOS-style edge rulers: repeating digits along the top row (magenta on
    // black) and the left column (red on teal), as per the CGA mockups.
    void setRulers(bool on) { rulers_ = on; drawView(); }
    bool rulers() const { return rulers_; }

    uchar getFg() const { return fgColor; }
    uchar getBg() const { return bgColor; }
    bool  isRgb() const { return useRgb_; }
    uint32_t getRgbFg() const { return rgbFg_; }
    uint32_t getRgbBg() const { return rgbBg_; }
    char  getPattern() const { return pattern; }
    const std::string& getPatternUtf8() const { return patternUtf8_; }

private:
    uchar fgColor;
    uchar bgColor;
    std::string patternUtf8_;  // multi-byte fill glyph; wins over `pattern` when set
    bool useRgb_ = false;
    bool rulers_ = false;
    uint32_t rgbFg_ = 0;
    uint32_t rgbBg_ = 0;
};
