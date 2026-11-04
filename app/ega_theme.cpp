#define Uses_TColorAttr
#define Uses_TPalette
#define Uses_TView
#define Uses_TWindow
#define Uses_TGroup
#define Uses_TProgram
#define Uses_TApplication
#include <tvision/tv.h>

#include "ega_theme.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

// Shadow colour used by Turbo Vision's writer (tview.cpp).
extern TColorAttr shadowAttr;

namespace ww {

namespace {

Theme gTheme = Theme::Mono;

const uint8_t kEga[16][3] = {
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0xAA}, {0x00, 0xAA, 0x00}, {0x00, 0xAA, 0xAA},
    {0xAA, 0x00, 0x00}, {0xAA, 0x00, 0xAA}, {0xAA, 0x55, 0x00}, {0xAA, 0xAA, 0xAA},
    {0x55, 0x55, 0x55}, {0x55, 0x55, 0xFF}, {0x55, 0xFF, 0x55}, {0x55, 0xFF, 0xFF},
    {0xFF, 0x55, 0x55}, {0xFF, 0x55, 0xFF}, {0xFF, 0xFF, 0x55}, {0xFF, 0xFF, 0xFF},
};

// The app's monochrome palette (black and white, reversed main areas).
// 0-7 desktop, 8-15 blue window, 16-23 cyan window, 24-31 gray window, 32-63 dialogs,
// then menus, status line, help and the rest, as in Turbo Vision's cpAppColor.
const char kMono[] =
    "\x70\x70\x0F\x07\x70\x70\x70\x07\x0F\x07\x07\x07\x70\x07\x0F"
    "\x70\x0F\x70\x07\x07\x70\x07\x0F\x70\x7F\x7F\x70\x07\x70\x07\x0F"
    "\x70\x7F\x7F\x70\x07\x70\x70\x7F\x7F\x07\x70\x0F\x70\x0F\x70\x07"
    "\x0F\x0F\x0F\x70\x0F\x07\x70\x70\x70\x07\x70\x0F\x07\x07\x78\x00"
    "\x70\xF0\x0F\x70\x07\x70\x70\x0F\x0F\x07\xF0\x7F\x08\x7F\xF0\x70"
    "\x7F\x7F\x7F\x0F\x70\x70\x07\x70\x70\x70\x07\x7F\x70\x07\x08\x00"
    "\x70\x7F\x7F\x70\x07\x70\x70\x7F\x7F\x07\x0F\x0F\x78\x0F\x78\x07"
    "\x0F\x0F\x0F\x70\x0F\x07\x70\x70\x70\x07\x70\x0F\x07\x07\x78\x00"
    "\x07\x0F\x07\x70\x70\x07\x0F\x70";

// Turbo Vision's own cpAppColor (Borland 1990): grey menus with red hotkeys and green
// selection, blue/cyan/grey windows, grey dialogs with green buttons.
const char kEgaApp[] =
    "\x71\x70\x78\x74\x20\x28\x24\x17\x1F\x1A\x31\x31\x1E\x71\x1F"
    "\x37\x3F\x3A\x13\x13\x3E\x21\x3F\x70\x7F\x7A\x13\x13\x70\x7F\x7E"
    "\x70\x7F\x7A\x13\x13\x70\x70\x7F\x7E\x20\x2B\x2F\x78\x2E\x70\x30"
    "\x3F\x3E\x1F\x2F\x1A\x20\x72\x31\x31\x30\x2F\x3E\x31\x13\x38\x00"
    "\x17\x1F\x1A\x71\x71\x1E\x17\x1F\x1E\x20\x2B\x2F\x78\x2E\x10\x30"
    "\x3F\x3E\x70\x2F\x7A\x20\x12\x31\x31\x30\x2F\x3E\x31\x13\x38\x00"
    "\x37\x3F\x3A\x13\x13\x3E\x30\x3F\x3E\x20\x2B\x2F\x78\x2E\x30\x70"
    "\x7F\x7E\x1F\x2F\x1A\x20\x32\x31\x71\x70\x2F\x7E\x71\x13\x78\x00"
    "\x37\x3F\x3A\x13\x13\x30\x3E\x1E";

const int kBaseLen = 135;
const int kFamilyBase = kBaseLen + 1;  // first window-family index (136)
const int kFamilies = 8;

// Window-colour families, in colourIndex order of `kFamilyBg`. Each is the 8 TWindow
// palette slots: passive frame, active frame, frame icons, scrollbar page, scrollbar
// reserved, normal text, selected text, reserved.
const int kFamilyBg[kFamilies] = {1, 3, 7, 0, 5, 4, 2, 6};
const uint8_t kFamily[kFamilies][8] = {
    {0x17, 0x1F, 0x1A, 0x31, 0x31, 0x1E, 0x71, 0x1F},  // blue
    {0x30, 0x3F, 0x3A, 0x13, 0x13, 0x30, 0x21, 0x3F},  // cyan
    {0x70, 0x7F, 0x7A, 0x13, 0x13, 0x70, 0x7F, 0x7E},  // grey
    {0x07, 0x0F, 0x0A, 0x70, 0x70, 0x07, 0x70, 0x0F},  // black
    {0x57, 0x5F, 0x5E, 0x75, 0x75, 0x5F, 0x75, 0x5F},  // magenta
    {0x47, 0x4F, 0x4E, 0x74, 0x74, 0x4F, 0x74, 0x4F},  // red
    {0x20, 0x2F, 0x2E, 0x72, 0x72, 0x20, 0x72, 0x2F},  // green
    {0x67, 0x6F, 0x6E, 0x76, 0x76, 0x6E, 0x76, 0x6F},  // brown
};

TColorAttr biosToEga(uint8_t b) {
    return egaAttr(b & 0x0F, (b >> 4) & 0x0F);
}

std::vector<TColorAttr> buildApp(bool ega) {
    std::vector<TColorAttr> v;
    v.reserve(kBaseLen + kFamilies * 8);
    const char* src = ega ? kEgaApp : kMono;
    for (int i = 0; i < kBaseLen; ++i) {
        uint8_t b = (uint8_t)src[i];
        // Mono stays BIOS (unchanged output); EGA is exact 24-bit.
        v.push_back(ega ? biosToEga(b) : TColorAttr(b));
    }
    for (int f = 0; f < kFamilies; ++f)
        for (int k = 0; k < 8; ++k)
            v.push_back(biosToEga(kFamily[f][k]));
    return v;
}

int familyOfBg(int bg) {
    for (int f = 0; f < kFamilies; ++f)
        if (kFamilyBg[f] == bg) return f;
    return -1;
}

std::map<const TView*, WinColour>& colours() {
    static std::map<const TView*, WinColour> m;
    return m;
}

const TView* ownerWindow(const TView* v) {
    while (v) {
        if (dynamic_cast<const TWindow*>(v)) return v;
        v = v->owner;
    }
    return nullptr;
}

} // namespace

Theme theme() { return gTheme; }
bool isEga() { return gTheme == Theme::Ega; }

void setTheme(Theme t) {
    gTheme = t;
    if (t == Theme::Ega)
        shadowAttr = egaAttr(8, 0);
    else
        shadowAttr = TColorAttr(0x08);
}

bool parseTheme(const std::string& name, Theme& out) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    if (n == "ega" || n == "dos" || n == "borland") { out = Theme::Ega; return true; }
    if (n == "monochrome" || n == "mono" || n.empty()) { out = Theme::Mono; return true; }
    if (n == "dark_pastel") { out = Theme::DarkPastel; return true; }
    return false;
}

const char* themeName(Theme t) {
    switch (t) {
        case Theme::Ega: return "ega";
        case Theme::DarkPastel: return "dark_pastel";
        default: return "monochrome";
    }
}

void initThemeFromEnv() {
    const char* e = std::getenv("WWDOS_THEME");
    Theme t;
    if (e && parseTheme(e, t)) setTheme(t);
}

TColorRGB egaRGB(int idx) {
    idx = std::max(0, std::min(15, idx));
    return TColorRGB(kEga[idx][0], kEga[idx][1], kEga[idx][2]);
}

TColorAttr egaAttr(int fg, int bg) {
    return TColorAttr(egaRGB(fg), egaRGB(bg));
}

int colourIndex(const std::string& name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    if (n == "black") return 0;
    if (n == "blue") return 1;
    if (n == "green") return 2;
    if (n == "cyan") return 3;
    if (n == "red") return 4;
    if (n == "magenta") return 5;
    if (n == "brown") return 6;
    if (n == "grey" || n == "gray") return 7;
    return -1;
}

const char* colourName(int bg) {
    switch (bg) {
        case 0: return "black";
        case 1: return "blue";
        case 2: return "green";
        case 3: return "cyan";
        case 4: return "red";
        case 5: return "magenta";
        case 6: return "brown";
        case 7: return "grey";
        default: return "";
    }
}

int defaultInk(int bg) {
    switch (bg) {
        case 0: return 7;
        case 1: return 15;
        case 2: return 0;
        case 3: return 0;
        case 4: return 15;
        case 5: return 15;
        case 6: return 14;
        case 7: return 0;
        default: return 15;
    }
}

TPalette& appPalette() {
    static std::vector<TColorAttr> mono = buildApp(false);
    static std::vector<TColorAttr> ega = buildApp(true);
    static TPalette pMono(mono.data(), (ushort)mono.size());
    static TPalette pEga(ega.data(), (ushort)ega.size());
    return isEga() ? pEga : pMono;
}

void setWindowColour(const TView* window, WinColour c) {
    if (c.bg < 0 && c.ink < 0)
        colours().erase(window);
    else
        colours()[window] = c;
}

WinColour windowColour(const TView* window) {
    auto it = colours().find(window);
    return it == colours().end() ? WinColour{} : it->second;
}

void forgetWindow(const TView* window) { colours().erase(window); }

TPalette* windowPalette(const TView* window) {
    auto it = colours().find(window);
    if (it == colours().end() || it->second.bg < 0) return nullptr;
    int f = familyOfBg(it->second.bg);
    if (f < 0) return nullptr;
    static TPalette* fams[kFamilies] = {};
    if (!fams[f]) {
        char idx[8];
        for (int k = 0; k < 8; ++k) idx[k] = (char)(kFamilyBase + f * 8 + k);
        fams[f] = new TPalette(idx, 8);
    }
    return fams[f];
}

bool contentAttr(const TView* view, int themeInk, TColorAttr& out, bool allowTheme) {
    const TView* win = ownerWindow(view);
    WinColour c = win ? windowColour(win) : WinColour{};
    if (c.bg >= 0 || c.ink >= 0) {
        int bg = c.bg >= 0 ? c.bg : (isEga() ? 1 : 0);
        int ink = c.ink >= 0 ? c.ink : defaultInk(bg);
        out = egaAttr(ink, bg);
        return true;
    }
    if (allowTheme && isEga()) {
        out = egaAttr(themeInk, 1);
        return true;
    }
    return false;
}

static void (*gRepaintHook)(TView*) = nullptr;
void setRepaintHook(void (*hook)(TView*)) { gRepaintHook = hook; }
void runRepaintHook(TView* v) { if (gRepaintHook && v) gRepaintHook(v); }

void freeBuffersDeep(TView* v) {
    if (gRepaintHook && v) gRepaintHook(v);
    auto* g = dynamic_cast<TGroup*>(v);
    if (!g) return;
    g->forEach([](TView* c, void*) { freeBuffersDeep(c); }, nullptr);
    g->freeBuffer();
}

void redrawDeep(TView* v) {
    if (!v) return;
    freeBuffersDeep(v);
    // The application's buffer is the screen itself: its draw() only flushes it, so
    // repaint its children instead.
    if (auto* prog = dynamic_cast<TProgram*>(v))
        prog->redraw();
    else
        v->drawView();
}

} // namespace ww
