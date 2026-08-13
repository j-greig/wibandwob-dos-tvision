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
// Built-ins seeded programmatically; skins/*.skin files loaded at runtime
// shadow (by name) or extend them. Recipes decoded from the Figma refs
// (design/figma-refs/): dflat, turbo, terra, pipeline + the mono set.
static CgaSkin mkSkin(const char* name, const char* tex,
                      int dFg, int dBg, int pBg, int pFg, int dlBg, int dlFg,
                      int fp = -1, int fa = -1, int mn = -1) {
    CgaSkin s;
    s.name = name; s.texture = tex;
    s.deskFg = dFg; s.deskBg = dBg;
    s.paperBg = pBg; s.paperFg = pFg;
    s.dialogBg = dlBg; s.dialogFg = dlFg;
    s.framePassive = fp; s.frameActive = fa; s.menuAttr = mn;
    s.builtin = true;
    return s;
}

// luminance-mapped mono ramp for a whole terminal palette (green/amber CRTs)
static void monoRamp(CgaSkin& s, bool amber) {
    for (int i = 0; i < 16; ++i) {
        TColorRGB c = ThemeManager::cgaColor(i);
        int lum = (c.r * 299 + c.g * 587 + c.b * 114) / 1000;
        s.termPal[i] = amber
            ? ((uint32_t)lum << 16) | ((uint32_t)(lum * 7 / 10) << 8)
            : ((uint32_t)(lum / 4) << 16) | ((uint32_t)lum << 8) | (uint32_t)(lum / 4);
    }
}

static std::vector<CgaSkin> buildBuiltinSkins() {
    std::vector<CgaSkin> v;
    v.push_back(mkSkin("dflat",    "\xe2\x96\x92", 9, 1, 7, 0,  1, 15));
    v.back().chunkyFrames = true;   // the Figma mock look stays on its home skin
    v.push_back(mkSkin("turbo",    "\xe2\x96\x91", 7, 1, 1, 14, 7, 0));
    v.back().paperVariants = { {3, 0}, {7, 1} };   // teal inner panel, grey card w/ blue ink
    v.back().accentFg = 2;                          // green buttons/hotkeys
    v.back().okFg = 2;
    v.push_back(mkSkin("terra",    "",             0, 1, 1, 10, 2, 0));
    v.back().paperVariants = { {2, 0}, {7, 1} };
    v.back().accentFg = 14;
    v.push_back(mkSkin("pipeline", "",             8, 0, 0, 9,  0, 13, 0x08, 0x09, 0x09));
    v.back().okFg = 9;
    v.push_back(mkSkin("phosphor", "",             2, 0, 0, 10, 2, 0,  0x02, 0x0A, 0x0A));
    v.back().okFg = 10; monoRamp(v.back(), false);
    v.push_back(mkSkin("hercules", "\xe2\x96\x91", 6, 0, 0, 14, 6, 0,  0x06, 0x0E, 0x0E));
    v.back().okFg = 14; monoRamp(v.back(), true);
    v.push_back(mkSkin("paper",    "\xe2\x96\x91", 8, 7, 15, 0, 3, 0));
    v.push_back(mkSkin("midnight", "\xc2\xb7",      8, 0, 0, 11, 1, 15, 0x08, 0x0B, 0x0B));
    v.back().okFg = 11;
    return v;
}

static std::vector<CgaSkin>& skinRegistry() {
    static std::vector<CgaSkin> reg = buildBuiltinSkins();
    return reg;
}

const std::vector<CgaSkin>& allCgaSkins() { return skinRegistry(); }

const CgaSkin* findCgaSkin(const std::string& name) {
    for (const CgaSkin& s : skinRegistry())
        if (s.name == name) return &s;
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
        case SkinRole::Dim:    fg = s.dimFg >= 0 ? s.dimFg : 8;  bg = s.paperBg; break;
        case SkinRole::Accent: fg = s.accentFg >= 0 ? s.accentFg
                                    : (s.dialogFg != s.paperBg) ? s.dialogFg
                                    : (s.frameActive >= 0 ? (s.frameActive & 0x0F) : 15);
                               bg = s.paperBg; break;
        case SkinRole::Floor:
            fg = s.floorFg >= 0 ? s.floorFg : s.deskFg;
            bg = s.floorBg >= 0 ? s.floorBg : s.deskBg; break;
        case SkinRole::FloorInk: {
            // luminance pick against the floor bg
            int fbg = s.floorBg >= 0 ? s.floorBg : s.deskBg;
            TColorRGB c = ThemeManager::cgaColor(fbg);
            int lum = (c.r * 299 + c.g * 587 + c.b * 114) / 1000;
            fg = lum > 128 ? 0 : 15; bg = fbg; break;
        }
        case SkinRole::Ok:     fg = s.okFg >= 0 ? s.okFg : 10; bg = s.paperBg; break;
        case SkinRole::Warn:   fg = s.warnFg >= 0 ? s.warnFg : 12; bg = s.paperBg; break;
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

// ── skin files: parse / load / save ──────────────────────────────────
#include <dirent.h>
#include <fstream>
#include <cstdio>

static int parseIntTok(const std::string& t) {
    if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0)
        return (int)strtol(t.c_str(), nullptr, 16);
    return atoi(t.c_str());
}

bool parseSkinFile(const std::string& path, CgaSkin& out) {
    std::ifstream in(path);
    if (!in) return false;
    CgaSkin s;   // defaults + kPalDerive palette
    bool named = false;
    std::string line;
    while (std::getline(in, line)) {
        // Strip whole-line comments only — '#' is also the RGB-hex sigil
        // used by palN entries ("pal1 #40318D"), so a naive line.find('#')
        // here truncates every palette override in every skin file down to
        // just its key, silently. termPal[] then stays kPalDerive forever:
        // set_skin reports the skin correctly (/state) but OSC4 remap and
        // the desktop's true-colour paint both fall back to authentic CGA
        // — exactly the "skin active but pixels stay default" bug (found
        // + fixed 2026-08-13, see runbook). Only treat '#' as a comment
        // when it opens the line (mirrors every comment actually written
        // in skins/*.skin — none are inline).
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos && line[firstNonSpace] == '#')
            line.clear();
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
            line.pop_back();
        if (line.empty()) continue;
        // tokenize
        std::vector<std::string> tok;
        size_t p = 0;
        while (p < line.size()) {
            while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) ++p;
            size_t q = p;
            while (q < line.size() && line[q] != ' ' && line[q] != '\t') ++q;
            if (q > p) tok.push_back(line.substr(p, q - p));
            p = q;
        }
        if (tok.empty()) continue;
        const std::string& k = tok[0];
        auto iv = [&](size_t i) { return i < tok.size() ? parseIntTok(tok[i]) : -1; };
        if      (k == "name" && tok.size() > 1)   { s.name = tok[1]; named = true; }
        else if (k == "texture" && tok.size() > 1) s.texture = tok[1];
        else if (k == "desk")   { s.deskFg = iv(1);  s.deskBg = iv(2); }
        else if (k == "paper")  { s.paperBg = iv(1); s.paperFg = iv(2); }
        else if ((k == "paper2" || k == "paper3" || k == "paper4") && tok.size() > 2)
            s.paperVariants.push_back({iv(1), iv(2)});
        else if (k == "dialog") { s.dialogBg = iv(1); s.dialogFg = iv(2); }
        else if (k == "framePassive") s.framePassive = iv(1);
        else if (k == "frameActive")  s.frameActive = iv(1);
        else if (k == "menu")   s.menuAttr = iv(1);
        else if (k == "dim")    s.dimFg = iv(1);
        else if (k == "accent") s.accentFg = iv(1);
        else if (k == "floor")  { s.floorFg = iv(1); s.floorBg = iv(2); }
        else if (k == "ok")     s.okFg = iv(1);
        else if (k == "warn")   s.warnFg = iv(1);
        else if (k == "frames" && tok.size() > 1) s.chunkyFrames = (tok[1] == "chunky");
        else if (k.rfind("pal", 0) == 0 && tok.size() > 1 && tok[1][0] == '#') {
            int slot = atoi(k.c_str() + 3);
            if (slot >= 0 && slot < 16)
                s.termPal[slot] = (uint32_t)strtol(tok[1].c_str() + 1, nullptr, 16);
        }
        // unknown keys ignored (forward compatibility)
    }
    if (!named) return false;
    out = s;
    return true;
}

int loadUserSkins(const std::string& dir) {
    DIR* d = opendir(dir.c_str());
    if (!d) return 0;
    int loaded = 0;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string fn = e->d_name;
        if (fn.size() < 6 || fn.substr(fn.size() - 5) != ".skin") continue;
        CgaSkin s;
        if (!parseSkinFile(dir + "/" + fn, s)) continue;
        // shadow same-name entry (built-in or earlier user skin) or append
        bool replaced = false;
        for (CgaSkin& existing : skinRegistry()) {
            if (existing.name == s.name) {
                bool wasBuiltin = existing.builtin;
                existing = s;
                existing.builtin = wasBuiltin;  // remember its origin
                replaced = true;
                break;
            }
        }
        if (!replaced) skinRegistry().push_back(s);
        ++loaded;
        fprintf(stderr, "[skins] loaded %s/%s\n", dir.c_str(), fn.c_str());
    }
    closedir(d);
    return loaded;
}

bool saveSkinFile(const CgaSkin& s, const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << "# " << s.name << ".skin — written by skin_save\n";
    out << "name " << s.name << "\n";
    if (!s.texture.empty()) out << "texture " << s.texture << "\n";
    out << "desk "   << s.deskFg  << " " << s.deskBg  << "\n";
    out << "paper "  << s.paperBg << " " << s.paperFg << "\n";
    for (size_t i = 0; i < s.paperVariants.size() && i < 3; ++i)
        out << "paper" << (i + 2) << " " << s.paperVariants[i].first
            << " " << s.paperVariants[i].second << "\n";

    out << "dialog " << s.dialogBg << " " << s.dialogFg << "\n";
    char hx[16];
    auto attr = [&](const char* k, int v) {
        if (v >= 0) { snprintf(hx, sizeof hx, "0x%02X", v); out << k << " " << hx << "\n"; }
    };
    attr("framePassive", s.framePassive);
    attr("frameActive",  s.frameActive);
    attr("menu",         s.menuAttr);
    if (s.dimFg >= 0)    out << "dim "    << s.dimFg    << "\n";
    if (s.accentFg >= 0) out << "accent " << s.accentFg << "\n";
    if (s.floorFg >= 0)  out << "floor "  << s.floorFg  << " " << s.floorBg << "\n";
    if (s.okFg >= 0)     out << "ok "     << s.okFg     << "\n";
    if (s.warnFg >= 0)   out << "warn "   << s.warnFg   << "\n";
    if (s.chunkyFrames)  out << "frames chunky\n";
    for (int i = 0; i < 16; ++i)
        if (s.termPal[i] != CgaSkin::kPalDerive) {
            snprintf(hx, sizeof hx, "#%06X", s.termPal[i]);
            out << "pal" << i << " " << hx << "\n";
        }
    return out.good();
}
