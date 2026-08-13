#pragma once

#include <string>

#define Uses_TColorAttr
#include <tvision/tv.h>

// Semantic skin roles (defined below CgaSkin; forward-declared for the API)
enum class SkinRole;

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

    // ── SkinRole API (enum defined below struct CgaSkin) ──
    // Active skin row, or nullptr when none/chrome off.
    static const struct CgaSkin* skin();
    // attr(): safe everywhere — house default when unskinned.
    static TColorAttr attr(SkinRole role);
    // tryAttr(): true only when a skin is active — for mapColor overrides
    // that must fall through to TVision's own mapping.
    static bool tryAttr(SkinRole role, TColorAttr& out);
    // Packed BIOS byte (bg<<4|fg) for palette-string patching.
    static unsigned char bios(SkinRole role);
    // Raw CGA indices / RGB for index- and RGB-taking APIs.
    static int fgIndex(SkinRole role);
    static int bgIndex(SkinRole role);
    static uint32_t rgbFgRole(SkinRole role);
    static uint32_t rgbBgRole(SkinRole role);
    // The canonical cga(fg,bg) helper every view used to reinvent.
    static TColorAttr attrIdx(int fg, int bg);
};

#include <vector>

// A named CGA skin — built-in (kSkins seeds) or loaded from skins/*.skin
// files at runtime (user skins shadow built-ins by name). Colour fields are
// CGA indices 0-15; -1 = derive (docs/development/theming-roles.md).
struct CgaSkin {
    std::string name;
    std::string texture;   // desktop fill glyph (UTF-8), "" = solid
    int deskFg = 7, deskBg = 0;      // desktop dither fg/bg
    int paperBg = 7, paperFg = 0;    // default viewer-window colours ("paper")
    int dialogBg = 1, dialogFg = 15; // accent window colours ("dialog")
    // Chrome (window frames / menus) as BIOS attr bytes (bg<<4|fg), -1 =
    // keep the classic cpAppColor chrome. Dark skins NEED these.
    int framePassive = -1, frameActive = -1, menuAttr = -1;
    // Optional role inks, -1 = derive (see resolver)
    int dimFg = -1, accentFg = -1;
    int floorFg = -1, floorBg = -1;
    int okFg = -1, warnFg = -1;
    // Terminal ANSI palette (OSC 4) per slot, 0xRRGGBB. kPalDerive = use
    // authentic CGA. A skin file remapping these swaps the whole monitor.
    static const uint32_t kPalDerive = 0xFF000000u;
    uint32_t termPal[16] = { kPalDerive, kPalDerive, kPalDerive, kPalDerive,
                             kPalDerive, kPalDerive, kPalDerive, kPalDerive,
                             kPalDerive, kPalDerive, kPalDerive, kPalDerive,
                             kPalDerive, kPalDerive, kPalDerive, kPalDerive };
    bool builtin = false;
};

// ── Semantic skin roles ──────────────────────────────────────────────
// One vocabulary, one resolver (docs/development/theming-roles.md).
// Views ask for a role; the resolver derives colours from the active
// CgaSkin row, with documented fallbacks when a field is -1 or no skin
// is active. Rule of thumb: UI affordance = role; depicted thing = content.
enum class SkinRole {
    Desk,          // desktop dither cell
    Paper,         // default viewer window body
    Dialog,        // accent window body
    Bar,           // menu/status bars, normal
    BarSel,        // menu/status bars, selected
    FramePassive,  // unfocused window border
    FrameActive,   // focused window border
    Dim,           // hint/secondary text on Paper
    Accent,        // highlight ink on Paper
    Floor,         // chromeless room interior (library, gallery)
    FloorInk,      // primary ink on Floor
    Ok,            // healthy indicator
    Warn,          // warning indicator
    Shadow,        // window drop shadow
};

// Skin registry (single source: built-ins + loaded skins/*.skin files).
// nullptr if unknown name. User skins shadow built-ins by name.
const CgaSkin* findCgaSkin(const std::string& name);
const std::vector<CgaSkin>& allCgaSkins();

// Hot skin files. loadUserSkins parses every skins/*.skin in `dir`,
// replacing same-name registry entries (built-ins can be shadowed) or
// appending new ones; returns the number of files loaded. saveSkinFile
// writes a registry-format .skin file. parseSkinFile fills `out` from one
// file (key-value lines; unknown keys ignored; see skins/README.md).
int  loadUserSkins(const std::string& dir);
bool parseSkinFile(const std::string& path, CgaSkin& out);
bool saveSkinFile(const CgaSkin& s, const std::string& path);
