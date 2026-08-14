/*---------------------------------------------------------*/
/*                                                         */
/*   api_desktop.cpp - desktop/theme/skin/screensaver       */
/*   api_* bridge functions. Moved verbatim from            */
/*   wwdos_app.cpp (monolith split stage 6a).                */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TView
#define Uses_TColorAttr
#define Uses_TScreen
#include <tvision/tv.h>

#include <string>
#include <vector>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "api_desktop.h"
#include "wwdos_app.h"
#include "core/json_utils.h"
#include "theme_manager.h"
#include "wibwob_background.h"
#include "ww_view_utils.h"
#include "frame_file_player_view.h"

// TVision global (defined in tview.cpp): the attribute painted over cells
// under a window shadow. CGA chrome swaps it for solid black.
extern TColorAttr shadowAttr;
static const TColorAttr kDefaultShadowAttr = shadowAttr;

void api_note_input(TWwdosApp& app) { app.noteInput(); }

std::string api_screensaver(TWwdosApp& app, const std::string& action, int minutes)
{
    if (minutes >= 0) app.setSaverTimeoutMins(minutes);
    // minutes=0 with no explicit action means DISABLE — the old default-to
    // "on" branch saw timeout 0 and helpfully re-armed it to 10, so the one
    // obvious disable syntax silently kept the saver alive (2026-08-14).
    if (action == "now") { app.activateScreensaver(); return "ok"; }
    if (action == "off" || (action.empty() && minutes == 0)) {
        app.dismissScreensaver();
        app.setSaverTimeoutMins(0);
        return "ok";
    }
    if (action == "on" || action.empty()) {
        if (app.saverTimeoutMins() == 0) app.setSaverTimeoutMins(10);
        app.noteInput();
        return "ok";
    }
    return "err unknown action (now|on|off)";
}

std::string api_list_skins(TWwdosApp& app) {
    (void)app;
    std::string json = "{\"skins\":[";
    bool first = true;
    for (const CgaSkin& s : allCgaSkins()) {
        if (!first) json += ",";
        json += "{\"name\":\"" + json_escape(s.name) + "\",\"builtin\":"
              + (s.builtin ? "true" : "false") + "}";
        first = false;
    }
    json += "],\"active\":\"" + json_escape(ThemeManager::activeSkin()) + "\"}";
    return json;
}

std::string api_reload_skins(TWwdosApp& app) {
    int n = loadUserSkins("skins");
    // re-apply the active skin so edits take effect immediately
    if (!ThemeManager::activeSkin().empty()) {
        extern std::string api_set_skin(TWwdosApp&, const std::string&);
        api_set_skin(app, ThemeManager::activeSkin());
    }
    return "ok loaded " + std::to_string(n);
}

std::string api_skin_save(TWwdosApp& app, const std::string& name) {
    (void)app;
    std::string target = name.empty() ? ThemeManager::activeSkin() : name;
    if (target.empty()) return "err no active skin and no name given";
    const CgaSkin* s = findCgaSkin(target);
    if (!s) return "err unknown skin";
    mkdir("skins", 0755);
    std::string path = "skins/" + target + ".skin";
    return saveSkinFile(*s, path) ? ("ok " + path) : "err write failed";
}

std::string api_set_theme_mode(TWwdosApp& app, const std::string& mode) {
    (void)app;
    if (mode != "light" && mode != "dark")
        return "err invalid theme mode";
    return "ok";
}

std::string api_set_theme_variant(TWwdosApp& app, const std::string& variant) {
    if (variant == "cga") {
        ThemeManager::cgaChrome() = true;
        shadowAttr = TColorAttr(TColorRGB(0, 0, 0), TColorRGB(0, 0, 0));  // solid black
    } else if (variant == "monochrome" || variant == "dark_pastel") {
        ThemeManager::cgaChrome() = false;
        shadowAttr = kDefaultShadowAttr;
    } else {
        return "err invalid theme variant (monochrome|dark_pastel|cga)";
    }
    app.redraw();  // repaint all chrome with the new palette + shadows
    return "ok";
}

std::string api_reset_theme(TWwdosApp& app) {
    (void)app;
    return "ok";
}

static TWibWobBackground* getWibWobBg(TWwdosApp& app) {
    return dynamic_cast<TWibWobBackground*>(app.deskTop->background);
}

std::string api_desktop_preset(TWwdosApp& app, const std::string& preset) {
    auto* bg = getWibWobBg(app);
    if (!bg) return "err no TWibWobBackground";
    // Validate preset name
    for (auto& p : getDesktopPresets()) {
        if (preset == p.name) {
            bg->setPreset(preset);
            return "ok";
        }
    }
    return "err unknown preset";
}

std::string api_desktop_texture(TWwdosApp& app, const std::string& ch) {
    auto* bg = getWibWobBg(app);
    if (!bg) return "err no TWibWobBackground";
    if (ch.empty()) return "err empty char";
    bg->setTextureUtf8(ch);  // UTF-8 aware — ▒ ░ etc render whole, not first-byte
    return "ok";
}

std::string api_desktop_rulers(TWwdosApp& app, bool on) {
    auto* bg = getWibWobBg(app);
    if (!bg) return "err no TWibWobBackground";
    bg->setRulers(on);
    return "ok";
}

std::string api_desktop_color(TWwdosApp& app, int fg, int bg_color) {
    auto* bg = getWibWobBg(app);
    if (!bg) return "err no TWibWobBackground";
    if (fg < 0 || fg > 15 || bg_color < 0 || bg_color > 15) return "err color out of range 0-15";
    if (ThemeManager::cgaChrome()) {
        // Authentic CGA RGB — terminal palettes (Ghostty etc) remap indexed
        // colours to their own theme, washing the sea to slate. Windows
        // already paint via cgaPalette(); the desktop must drink from the
        // same well or the skin falls apart.
        bg->setColorRgb(ThemeManager::cgaRgb(fg), ThemeManager::cgaRgb(bg_color));
    } else {
        bg->setColor(static_cast<uchar>(fg), static_cast<uchar>(bg_color));
    }
    return "ok";
}

// ── Terminal palette programming (OSC 4) — the DOS DAC registers ─────
// Indexed colours (0x07-style attrs everywhere) render through the
// TERMINAL's 16 ANSI slots — Ghostty's "black" is #2A2A2A, which is why
// legacy views looked grey on dark skins no matter what we patched.
// The canon fix: set_skin reprograms the slots to authentic CGA RGB
// (mono skins get luminance-mapped ramps — swapping the monitor, not
// the app). Every indexed draw in every view then obeys the skin.
// Resolve index i to the colour a skin actually WANTS it to look like: its
// termPal override when set, else authentic CGA. Single source shared by
// emitTerminalPalette (indexed/terminal-slot rendering) and the desktop's
// setColorRgb paint below (true-colour rendering — bypasses terminal slots
// entirely, so it must bake the override in directly or the OSC4 remap has
// no effect on it at all: skin reports correctly via /state, desktop pixels
// silently stay authentic-CGA. Bug found + fixed 2026-08-13, see runbook).
static uint32_t skinRgb(const CgaSkin* sk, int idx)
{
    return (sk && sk->termPal[idx] != CgaSkin::kPalDerive)
         ? sk->termPal[idx] : ThemeManager::cgaRgb(idx);
}

static void emitTerminalPalette(const CgaSkin* sk)
{
    std::string out;
    char seq[48];
    for (int i = 0; i < 16; ++i) {
        std::snprintf(seq, sizeof seq, "\033]4;%d;#%06X\033\\", i, skinRgb(sk, i));
        out += seq;
    }
    ::write(STDOUT_FILENO, out.data(), out.size());
}

static void resetTerminalPalette()
{
    static const char kReset[] = "\033]104\033\\";
    ::write(STDOUT_FILENO, kReset, sizeof(kReset) - 1);
}

// Apply a named CGA skin preset in one shot: chrome variant + solid shadows,
// desktop texture + colour (authentic CGA RGB), and default paper colours on
// every colourable window. Dialog-accent colours stay per-window calls
// (set_window_bg/fg) so scenes can mix paper and dialogs like the refs.
// "off"/"monochrome" restores the house grey chrome.
std::string api_set_skin(TWwdosApp& app, const std::string& name) {
    extern std::string api_set_theme_variant(TWwdosApp&, const std::string&);
    if (name == "off" || name == "monochrome") {
        ThemeManager::activeSkin().clear();
        std::remove(".wwdos_skin");
        resetTerminalPalette();
        return api_set_theme_variant(app, "monochrome");
    }
    const CgaSkin* s = findCgaSkin(name);
    if (!s) {
        loadUserSkins("skins");   // maybe it was just written — hot path
        s = findCgaSkin(name);
    }
    if (!s) return "err unknown skin (list_skins for the registry; skins/*.skin hot-load)";
    api_set_theme_variant(app, "cga");
    if (auto* bg = getWibWobBg(app)) {
        bg->setTextureUtf8(s->texture.empty() ? std::string(" ") : s->texture);
        bg->setColorRgb(skinRgb(s, s->deskFg), skinRgb(s, s->deskBg));
    }
    // Paper every colourable window, distributing the skin's paper
    // VARIANTS round-robin — the refs' richness is several window
    // identities per scheme, not one uniform paper (Zilla, 2026-08-13).
    {
        std::vector<std::pair<int,int>> papers;
        papers.push_back({s->paperBg, s->paperFg});
        for (auto& pv : s->paperVariants) papers.push_back(pv);
        int wi = 0;
        if (TView* start = app.deskTop->first()) {
            TView* v = start;
            do {
                if (auto* w = dynamic_cast<TWindow*>(v)) {
                    auto& p = papers[wi % papers.size()];
                    if (auto* fp = ww_get_child_view<FrameFilePlayerView>(w)) {
                        fp->setBackgroundIndex(p.first);
                        fp->setForegroundIndex(p.second);
                        if (w->frame) w->frame->drawView();
                        ++wi;
                    } else if (auto* tv = ww_get_child_view<TTextFileView>(w)) {
                        tv->setBackgroundIndex(p.first);
                        tv->setForegroundIndex(p.second);
                        if (w->frame) w->frame->drawView();
                        ++wi;
                    }
                }
                v = v->next;
            } while (v != start);
        }
    }
    ThemeManager::activeSkin() = name;
    emitTerminalPalette(s);   // reprogram the terminal's 16 ANSI slots
    app.redraw();
    // persist for relaunch (constructor reads .wwdos_skin at boot)
    { std::ofstream sk(".wwdos_skin", std::ios::trunc); if (sk) sk << name << "\n"; }
    return "ok";
}

std::string api_desktop_gallery(TWwdosApp& app, bool on) {
    if (!app.menuBar || !app.statusLine) return "err no chrome views";

    app.setGalleryMode(on);
    app.menuBar->setState(sfVisible, !on);
    app.statusLine->setState(sfVisible, !on);

    // Recalculate desktop bounds to fill freed/restored rows
    TRect r = app.getExtent();
    if (!on) {
        r.a.y = 1;       // leave room for menu bar
        r.b.y--;          // leave room for status line
    }
    // else: full extent (row 0 to bottom)
    app.deskTop->changeBounds(r);
    app.deskTop->drawView();

    return "ok";
}

std::string api_desktop_get(TWwdosApp& app) {
    auto* bg = getWibWobBg(app);
    if (!bg) return "{}";
    std::string json = "{";
    json += "\"char\":\"";
    char ch = bg->getPattern();
    if (ch == '"') json += "\\\"";
    else if (ch == '\\') json += "\\\\";
    else json += ch;
    json += "\",";
    json += "\"fg\":" + std::to_string((int)bg->getFg()) + ",";
    json += "\"bg\":" + std::to_string((int)bg->getBg()) + ",";
    json += "\"gallery\":" + std::string(app.galleryMode() ? "true" : "false") + ",";
    json += "\"preset\":\"" + bg->getPresetName() + "\"";
    json += "}";
    return json;
}
