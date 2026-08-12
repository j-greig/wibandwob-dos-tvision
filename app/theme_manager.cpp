#include "theme_manager.h"

#include <algorithm>
#include <cctype>

// Dark pastel palette (v1 - single blue only)
// Background: #000000
// Primary text: #d0d0d0
// Secondary text: #cfcfcf
// Blue accent: #57c7ff (ONLY blue - excludes #66e0ff)
// Pink accent: #f07f8f
// Green accent: #b7ff3c

// Helper to create RGB color from hex
static TColorRGB hexToRGB(uint32_t hex) {
    return TColorRGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

TColorAttr ThemeManager::getColor(ThemeRole role, ThemeMode mode, ThemeVariant variant) {
    // Monochrome variant uses default Turbo Vision palette indices
    if (variant == ThemeVariant::Monochrome) {
        switch (role) {
            case ThemeRole::Background:
                return TColorAttr(0x07); // Light gray on black
            case ThemeRole::Foreground:
                return TColorAttr(0x07); // Light gray
            case ThemeRole::ForegroundSecondary:
                return TColorAttr(0x08); // Dark gray
            case ThemeRole::AccentPrimary:
                return TColorAttr(0x0F); // Bright white
            case ThemeRole::AccentSecondary:
                return TColorAttr(0x0E); // Yellow
            case ThemeRole::AccentTertiary:
                return TColorAttr(0x0A); // Light green
            case ThemeRole::Frame:
                return TColorAttr(0x07); // Light gray
            case ThemeRole::Selection:
                return TColorAttr(0x70); // Inverse: black on light gray
            case ThemeRole::Warning:
                return TColorAttr(0x0C); // Light red
        }
    }

    // Dark pastel variant
    if (variant == ThemeVariant::DarkPastel) {
        TColorRGB black = hexToRGB(0x000000);
        TColorRGB lightText = hexToRGB(0xd0d0d0);
        TColorRGB secondaryText = hexToRGB(0xcfcfcf);
        TColorRGB blue = hexToRGB(0x57c7ff);    // Single blue only
        TColorRGB pink = hexToRGB(0xf07f8f);
        TColorRGB green = hexToRGB(0xb7ff3c);

        switch (role) {
            case ThemeRole::Background:
                return TColorAttr(lightText, black);
            case ThemeRole::Foreground:
                return TColorAttr(lightText, black);
            case ThemeRole::ForegroundSecondary:
                return TColorAttr(secondaryText, black);
            case ThemeRole::AccentPrimary:
                return TColorAttr(blue, black);
            case ThemeRole::AccentSecondary:
                return TColorAttr(pink, black);
            case ThemeRole::AccentTertiary:
                return TColorAttr(green, black);
            case ThemeRole::Frame:
                return TColorAttr(secondaryText, black);
            case ThemeRole::Selection:
                return TColorAttr(black, blue);  // Inverse: black on blue
            case ThemeRole::Warning:
                return TColorAttr(pink, black);
        }
    }

    // Fallback
    return TColorAttr(0x07);
}

ThemeMode ThemeManager::parseModeString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (lower == "dark") return ThemeMode::Dark;
    return ThemeMode::Light;  // Default to light
}

ThemeVariant ThemeManager::parseVariantString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (lower == "dark_pastel") return ThemeVariant::DarkPastel;
    return ThemeVariant::Monochrome;  // Default to monochrome
}

std::string ThemeManager::modeToString(ThemeMode mode) {
    switch (mode) {
        case ThemeMode::Light: return "light";
        case ThemeMode::Dark: return "dark";
    }
    return "light";
}

std::string ThemeManager::variantToString(ThemeVariant variant) {
    switch (variant) {
        case ThemeVariant::Monochrome: return "monochrome";
        case ThemeVariant::DarkPastel: return "dark_pastel";
    }
    return "monochrome";
}

bool& ThemeManager::cgaChrome() {
    static bool flag = false;
    return flag;
}

const TColorRGB* ThemeManager::cgaPalette() {
    static const TColorRGB pal[16] = {
        TColorRGB(0x00,0x00,0x00), // 0 Black
        TColorRGB(0x00,0x00,0xAA), // 1 Blue
        TColorRGB(0x00,0xAA,0x00), // 2 Green
        TColorRGB(0x00,0xAA,0xAA), // 3 Cyan
        TColorRGB(0xAA,0x00,0x00), // 4 Red
        TColorRGB(0xAA,0x00,0xAA), // 5 Magenta
        TColorRGB(0xAA,0x55,0x00), // 6 Brown (CGA's special-cased colour)
        TColorRGB(0xAA,0xAA,0xAA), // 7 Light gray
        TColorRGB(0x55,0x55,0x55), // 8 Dark gray
        TColorRGB(0x55,0x55,0xFF), // 9 Light blue
        TColorRGB(0x55,0xFF,0x55), // 10 Light green
        TColorRGB(0x55,0xFF,0xFF), // 11 Light cyan
        TColorRGB(0xFF,0x55,0x55), // 12 Light red
        TColorRGB(0xFF,0x55,0xFF), // 13 Light magenta
        TColorRGB(0xFF,0xFF,0x55), // 14 Yellow
        TColorRGB(0xFF,0xFF,0xFF), // 15 White
    };
    return pal;
}

TColorRGB ThemeManager::cgaColor(int idx) {
    if (idx < 0) idx = 0;
    if (idx > 15) idx = 15;
    return cgaPalette()[idx];
}

uint32_t ThemeManager::cgaRgb(int idx) {
    TColorRGB c = cgaColor(idx);
    return (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | uint32_t(c.b);
}

std::string& ThemeManager::activeSkin() {
    static std::string skin;
    return skin;
}

// --- CGA skin registry -------------------------------------------------------
// Recipes decoded from the Figma refs (design/figma-refs/):
//   dflat    — D-Flat MemoPad: cyan chrome, grey paper, blue dialogs, red
//              hotkeys, ▒-dithered blue sea. The flagship nostalgia combo.
//   turbo    — Turbo Pascal IDE: blue desktop, yellow-on-blue editor paper,
//              grey dialogs, green action buttons.
//   terra    — Terra Time / GeoGraphics: green continents on blue, yellow
//              city-lights, cyan chrome.
//   pipeline — Pipeline (1991): near-black desktop, blue paper, magenta logo
//              pixels. The moody one.
static const CgaSkin kSkins[] = {
    //  name        texture  deskFg deskBg  paper   dialog  chrome: passive active menu
    { "dflat",      "\xe2\x96\x92", 9, 1,   7, 0,   1, 15,  -1,   -1,   -1   },
    { "turbo",      "\xe2\x96\x91", 7, 1,   1, 14,  7, 0,   -1,   -1,   -1   },
    { "terra",      "",             0, 1,   1, 10,  2, 0,   -1,   -1,   -1   },
    { "pipeline",   "",             8, 0,   0, 9,   0, 13,  0x08, 0x09, 0x09 },
    { "phosphor",   "",             2, 0,   0, 10,  2, 0,   0x02, 0x0A, 0x0A },
    { "hercules",   "\xe2\x96\x91", 6, 0,   0, 14,  6, 0,   0x06, 0x0E, 0x0E },
    { "paper",      "\xe2\x96\x91", 8, 7,   15, 0,  3, 0,   -1,   -1,   -1   },
    { "midnight",   "\xc2\xb7",      8, 0,   0, 11,  1, 15,  0x08, 0x0B, 0x0B },
    { nullptr,      "",             0, 0,   0, 0,   0, 0,   -1,   -1,   -1   },
};

const CgaSkin* allCgaSkins() { return kSkins; }

const CgaSkin* findCgaSkin(const std::string& name) {
    for (const CgaSkin* s = kSkins; s->name; ++s)
        if (name == s->name) return s;
    return nullptr;
}

// ── SkinRole resolver ─────────────────────────────────────────────────
// One switch, documented derivations (docs/development/theming-roles.md).

const CgaSkin* ThemeManager::skin() {
    if (!cgaChrome()) return nullptr;
    return findCgaSkin(activeSkin());
}

// Resolve role → CGA index pair for a specific skin row.
static void resolveRole(const CgaSkin& s, SkinRole r, int& fg, int& bg) {
    switch (r) {
        case SkinRole::Desk:   fg = s.deskFg;   bg = s.deskBg;   break;
        case SkinRole::Paper:  fg = s.paperFg;  bg = s.paperBg;  break;
        case SkinRole::Dialog: fg = s.dialogFg; bg = s.dialogBg; break;
        case SkinRole::Bar:
            if (s.menuAttr >= 0) { fg = s.menuAttr & 0x0F; bg = (s.menuAttr >> 4) & 0x0F; }
            else { fg = 0; bg = 15; }
            break;
        case SkinRole::BarSel: {
            int f, b; resolveRole(s, SkinRole::Bar, f, b);
            fg = b; bg = f; break;
        }
        case SkinRole::FramePassive:
            if (s.framePassive >= 0) { fg = s.framePassive & 0x0F; bg = (s.framePassive >> 4) & 0x0F; }
            else { fg = s.paperFg; bg = s.paperBg; }
            break;
        case SkinRole::FrameActive:
            if (s.frameActive >= 0) { fg = s.frameActive & 0x0F; bg = (s.frameActive >> 4) & 0x0F; }
            else { fg = s.dialogFg; bg = s.dialogBg; }
            break;
        case SkinRole::Dim:    fg = 8;  bg = s.paperBg; break;
        case SkinRole::Accent: fg = (s.dialogFg != s.paperBg) ? s.dialogFg
                                    : (s.frameActive >= 0 ? (s.frameActive & 0x0F) : 15);
                               bg = s.paperBg; break;
        case SkinRole::Floor:  fg = s.deskFg; bg = s.deskBg; break;
        case SkinRole::FloorInk: {
            // luminance pick against the floor bg
            TColorRGB c = ThemeManager::cgaColor(s.deskBg);
            int lum = (c.r * 299 + c.g * 587 + c.b * 114) / 1000;
            fg = lum > 128 ? 0 : 15; bg = s.deskBg; break;
        }
        case SkinRole::Ok: {
            // per-skin healthy colour: monochrome-ish skins use their own
            // light ink instead of universal green
            std::string n = s.name;
            if      (n == "midnight") fg = 11;   // light cyan
            else if (n == "pipeline") fg = 9;    // light blue
            else if (n == "hercules") fg = 14;   // amber
            else if (n == "phosphor") fg = 10;   // green (native)
            else                      fg = 10;
            bg = s.paperBg; break;
        }
        case SkinRole::Warn:   fg = 12; bg = s.paperBg; break;
        case SkinRole::Shadow: fg = 0;  bg = 0; break;
    }
}

// House defaults when no skin is active (classic monochrome-era look).
static void houseRole(SkinRole r, int& fg, int& bg) {
    switch (r) {
        case SkinRole::Bar: case SkinRole::BarSel: fg = 0; bg = 15; break;
        case SkinRole::Dim:    fg = 8;  bg = 0;  break;
        case SkinRole::Accent: fg = 15; bg = 0;  break;
        case SkinRole::Floor:  fg = 9;  bg = 1;  break;   // the classic library blue
        case SkinRole::FloorInk: fg = 15; bg = 1; break;
        case SkinRole::Ok:     fg = 10; bg = 0;  break;
        case SkinRole::Warn:   fg = 12; bg = 0;  break;
        case SkinRole::Shadow: fg = 0;  bg = 0;  break;
        case SkinRole::Dialog: fg = 15; bg = 1;  break;
        case SkinRole::FramePassive: case SkinRole::FrameActive:
        case SkinRole::Desk: case SkinRole::Paper:
        default:               fg = 7;  bg = 0;  break;
    }
}

static void roleIndices(SkinRole r, int& fg, int& bg) {
    if (const CgaSkin* s = ThemeManager::skin()) resolveRole(*s, r, fg, bg);
    else houseRole(r, fg, bg);
}

TColorAttr ThemeManager::attr(SkinRole role) {
    int fg, bg; roleIndices(role, fg, bg);
    return TColorAttr(cgaColor(fg), cgaColor(bg));
}

bool ThemeManager::tryAttr(SkinRole role, TColorAttr& out) {
    const CgaSkin* s = skin();
    if (!s) return false;
    // Bar roles only claim the pixels when the skin defines chrome
    if ((role == SkinRole::Bar || role == SkinRole::BarSel) && s->menuAttr < 0)
        return false;
    int fg, bg; resolveRole(*s, role, fg, bg);
    out = TColorAttr(cgaColor(fg), cgaColor(bg));
    return true;
}

unsigned char ThemeManager::bios(SkinRole role) {
    int fg, bg; roleIndices(role, fg, bg);
    return (unsigned char)(((bg & 0x0F) << 4) | (fg & 0x0F));
}

int ThemeManager::fgIndex(SkinRole role) { int f, b; roleIndices(role, f, b); return f; }
int ThemeManager::bgIndex(SkinRole role) { int f, b; roleIndices(role, f, b); return b; }
uint32_t ThemeManager::rgbFgRole(SkinRole role) { return cgaRgb(fgIndex(role)); }
uint32_t ThemeManager::rgbBgRole(SkinRole role) { return cgaRgb(bgIndex(role)); }

TColorAttr ThemeManager::attrIdx(int fg, int bg) {
    return TColorAttr(cgaColor(fg), cgaColor(bg));
}
