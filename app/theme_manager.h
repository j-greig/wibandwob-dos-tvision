#pragma once

#include <string>

#define Uses_TColorAttr
#include <tvision/tv.h>

// Theme modes: light or dark (auto mode deferred to future PR)
enum class ThemeMode {
    Light,
    Dark
};

// Theme variants: monochrome (default) or dark_pastel
enum class ThemeVariant {
    Monochrome,
    DarkPastel
};

// Semantic color roles for UI elements
enum class ThemeRole {
    Background,          // Main background
    Foreground,          // Primary text
    ForegroundSecondary, // Secondary/muted text
    AccentPrimary,       // Primary accent (blue in dark pastel)
    AccentSecondary,     // Secondary accent (pink in dark pastel)
    AccentTertiary,      // Tertiary accent (green in dark pastel)
    Frame,               // Window frames/borders
    Selection,           // Selected items
    Warning              // Warning/error states
};

// ThemeManager - Pure functional color lookup based on mode/variant/role
class ThemeManager {
public:
    // Get color for a specific role given current mode and variant
    static TColorAttr getColor(ThemeRole role, ThemeMode mode, ThemeVariant variant);

    // Parse mode from string ("light" or "dark")
    static ThemeMode parseModeString(const std::string& str);

    // Parse variant from string ("monochrome" or "dark_pastel")
    static ThemeVariant parseVariantString(const std::string& str);

    // Convert mode to string
    static std::string modeToString(ThemeMode mode);

    // Convert variant to string
    static std::string variantToString(ThemeVariant variant);

    // Chrome variant flag: true = classic DOS/CGA chrome (frames, menus,
    // chunky title-tab frames, solid black shadows). Single source of truth —
    // getPalette(), TCGAFrame and the shadow logic all consult this.
    static bool& cgaChrome();

    // Authentic IBM CGA 16-colour palette (single source; index 6 = brown).
    static const TColorRGB* cgaPalette();
    static TColorRGB cgaColor(int idx);

    // CGA colour as packed 0xRRGGBB (for TWibWobBackground::setColorRgb).
    static uint32_t cgaRgb(int idx);

    // Active skin name ("" = none). Set by api_set_skin, reported in /state.
    static std::string& activeSkin();
};

// A named CGA skin preset — palette recipes decoded from the Figma refs
// (design/figma-refs/, 2026-08). All colour fields are CGA indices 0-15.
struct CgaSkin {
    const char* name;
    const char* texture;   // desktop fill glyph (UTF-8), "" = solid
    int deskFg, deskBg;    // desktop dither fg/bg
    int paperBg, paperFg;  // default viewer-window colours ("paper")
    int dialogBg, dialogFg;// accent window colours ("dialog")
    // Chrome (window frames / menus) as BIOS attr bytes (bg<<4|fg), -1 =
    // keep the classic cpAppColor chrome. Dark skins NEED these — otherwise
    // midnight wears daylight window borders.
    int framePassive, frameActive, menuAttr;
};

// Skin registry (single source). nullptr if unknown name.
const CgaSkin* findCgaSkin(const std::string& name);
// All skins, for capability listings. Terminated by a {nullptr,...} row.
const CgaSkin* allCgaSkins();
