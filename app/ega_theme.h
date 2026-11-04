#pragma once
// EGA theme: the 16-colour Borland look, selectable at launch (WWDOS_THEME=ega) and live
// (set_theme_variant variant=ega), plus per-window colours (colour + ink) for the
// primer, text, figlet and editor windows.
//
// Every colour the theme draws is a 24-bit TColorRGB with the exact EGA value, so a
// mirror of the screen (tmux capture-pane -e) shows true EGA whatever the viewer's
// own 16-colour palette is.

#define Uses_TColorAttr
#define Uses_TPalette
#define Uses_TView
#include <tvision/tv.h>

#include <string>

namespace ww {

enum class Theme { Mono, DarkPastel, Ega };

Theme theme();
bool isEga();
// Sets the theme. Does not redraw; call applyThemeToApp() after.
void setTheme(Theme t);
// Name <-> theme: "monochrome", "dark_pastel", "ega". Unknown names return false.
bool parseTheme(const std::string& name, Theme& out);
const char* themeName(Theme t);
// Read WWDOS_THEME once at launch.
void initThemeFromEnv();

// The 16 EGA colours (index 0-15, BIOS order: 0 black 1 blue 2 green 3 cyan 4 red
// 5 magenta 6 brown 7 light grey 8 dark grey ... 15 white).
TColorRGB egaRGB(int idx);
TColorAttr egaAttr(int fg, int bg);

// Window colour names <-> background index. -1 for an unknown name.
int colourIndex(const std::string& name);
const char* colourName(int bgIdx);
// The ink a colour gets when none is given (white on blue, black on cyan/grey, ...).
int defaultInk(int bgIdx);

// The application palette for the current theme (mono or EGA), extended with the
// eight window-colour families at indices 136..199.
TPalette& appPalette();

// Per-window colour. bg/ink -1 = unset.
struct WinColour { int bg = -1; int ink = -1; };
void setWindowColour(const TView* window, WinColour c);
WinColour windowColour(const TView* window);
void forgetWindow(const TView* window);

// The palette a coloured window uses for its frame (one of the eight families), or
// nullptr when the window has no explicit colour.
TPalette* windowPalette(const TView* window);

// The colour a content view should draw its text in: the owner window's explicit
// colour, else (EGA theme) blue with `themeInk`. Returns false in mono with no
// explicit colour: the view keeps its own colours.
// allowTheme=false: only an explicit window colour applies (the view has colours of its own).
bool contentAttr(const TView* view, int themeInk, TColorAttr& out, bool allowTheme = true);

// Repaint a view and everything inside it now. Turbo Vision windows keep a cached
// screen buffer, so drawView() alone repaints the old colours; this frees the
// buffers first. Use after a theme or window-colour change.
void redrawDeep(TView* v);

} // namespace ww

// Drop into a TWindow subclass so an explicit window colour recolours its frame.
#define WW_COLOUR_PALETTE_OVERRIDE                                           \
    virtual TPalette& getPalette() const override {                          \
        if (TPalette* p = ww::windowPalette(this)) return *p;                \
        return TWindow::getPalette();                                        \
    }
