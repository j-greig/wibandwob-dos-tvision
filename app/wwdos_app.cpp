/*---------------------------------------------------------*/
/*                                                         */
/*   wwdos_app.cpp - Test Pattern Window Spawner   */
/*   Unlimited resizable windows with test patterns       */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TKeys
#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TStaticText
#define Uses_TButton
#define Uses_TListBox
#define Uses_TStringCollection
#define Uses_TMenuBar
#define Uses_TMenuBox
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TMenu
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TScroller
#define Uses_TScrollBar
#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TText
#define Uses_MsgBox
#define Uses_cmTile
#define Uses_cmCascade
#define Uses_TFileDialog
#define Uses_TBackground
#define Uses_TInputLine
#define Uses_TLabel
#include <tvision/tv.h>

#include "test_pattern.h"
#include "core/json_utils.h"
#include "gradient.h"
#include "ui/ui_helpers.h"
#include "glitch_engine.h"
#include "frame_capture.h"
#include "frame_file_player_view.h"
#include "disk_library_view.h"
#include "theme_manager.h"
#include "tweet_shader_view.h"
#include "ascii_image_view.h"
// Animated blocks view/window
#include "animated_blocks_view.h"
#include "backrooms_tv_view.h"
// Animated gradient view/window
#include "animated_gradient_view.h"
// Animated score (ASCII score) view/window
#include "animated_score_view.h"
// Generative art: Verse Field
#include "generative_verse_view.h"
// Generative art experiments
#include "generative_orbit_view.h"
#include "generative_mycelium_view.h"
// Generative art: Torus Field
#include "generative_torus_view.h"
// Generative art: Cube Spinner
#include "generative_cube_view.h"
// Generative art: Monster Portal (emoji tiler)
#include "generative_monster_portal_view.h"
// Generative art: Monster Verse (Verse engine + monsters)
#include "generative_monster_verse_view.h"
// Generative art: Monster Cam (Emoji)
#include "generative_monster_cam_view.h"
#include "contour_map_view.h"
#include "generative_lab_view.h"
#include "game_of_life_view.h"
#include "animated_ascii_view.h"
// Generative art: ASCII Cam
// DISABLED: #include "generative_ascii_cam_view.h"
// API-controllable text editor
#include "text_editor_view.h"
// Unified auth config
#include "llm/base/auth_config.h"
// Wib&Wob AI chat interface
#include "wibwob_view.h"
// Scrollbar fix prototypes (test versions)
#include "wibwob_scroll_test.h"
// Custom frame for windows without titles
#include "notitle_frame.h"
#include "windows/frame_animation_window.h"
// Test pattern & gradient windows (extracted from this file, monolith split stage 1)
#include "windows/pattern_windows.h"
// Transparent background text view
#include "transparent_text_view.h"
// TUI Browser window
#include "browser_view.h"
// Scramble cat presence
#include "scramble_view.h"
#include "scramble_engine.h"
// Multi-user PartyKit room chat
#include "room_chat_view.h"
// Paint canvas window
#include "paint/paint_window.h"
#include "paint/paint_wwp_codec.h"
// Micropolis ASCII MVP window
#include "micropolis_ascii_view.h"
// Quadra falling blocks game
#include "quadra_view.h"
// Snake game
#include "snake_view.h"
// WibWob Rogue dungeon crawler
#include "rogue_view.h"
// Deep Signal space scanner game
#include "deep_signal_view.h"
// Terminal emulator window (tvterm)
#include "tvterm_view.h"
// Desktop texture & gallery mode
#include "wibwob_background.h"
// App launcher (E011)
#include "app_launcher_view.h"
#include "ascii_gallery_view.h"
#include "figlet_text_view.h"
#include "figlet_utils.h"
// Factory for ASCII grid demo window (implemented in ascii_grid_view.cpp).
class TWindow; TWindow* createAsciiGridDemoWindow(const TRect &bounds);
// #include "mech_window.h" // deferred feature; header not present yet
#include <sstream>
#include <fstream>
#include <chrono>
#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <sys/stat.h>
#include <cstdio>
#include <fstream>
#include <vector>
#include <cstring>
#include <map>
#include <deque>
#include <dirent.h>
#include <algorithm>
// Local API IPC bridge (Unix domain socket)
#include "api_ipc.h"
#include "command_registry.h"
#include "window_type_registry.h"
#include "api_windows.h"

// Find first existing primer directory across module paths.
// Checks modules-private/*/primers/ then modules/*/primers/ then legacy app/primers/.
std::string findPrimerDir() {
    const char* moduleDirs[] = { "modules-private", "modules" };
    for (const char* base : moduleDirs) {
        DIR* dir = opendir(base);
        if (!dir) continue;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            std::string candidate = std::string(base) + "/" + entry->d_name + "/primers";
            struct stat st;
            if (stat(candidate.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                closedir(dir);
                return candidate;
            }
        }
        closedir(dir);
    }
    // Legacy fallback
    struct stat st;
    if (stat("app/primers", &st) == 0 && S_ISDIR(st.st_mode))
        return "app/primers";
    return "primers";
}

// Configuration - Toggle pattern display mode
// true  = Continuous mode (pattern flows like text, wraps at line ends creating diagonals)
// false = Tiled mode (pattern resets at start of each line, crops at edges)
bool USE_CONTINUOUS_PATTERN = true;  // Made non-const so it can be changed at runtime

// Command constants
// File menu commands
#include "wwdos_commands.h"
#include "ww_view_utils.h"
#include "workspace_io.h"
// kMaxRecentWorkspaces / scanRecentWorkspacePaths / countWindowsInWorkspace /
// recentWorkspaceLabel now live in workspace_io.cpp (monolith split stage
// 7b); declared in workspace_io.h, called here by
// buildRecentWorkspacesSubmenuItem (stays until the stage-8 app_chrome
// split).


// Forward declarations
class TTestPatternView;
class TTestPatternWindow;
class TGradientWindow;
class TWwdosApp;

// TCustomMenuBar / TCustomStatusLine moved to app_chrome.cpp (monolith
// split stage 8).
#include "app_chrome.h"

// TWwdosApp class declaration extracted to wwdos_app.h (monolith split stage 3).
#include "wwdos_app.h"

// Static member definition
std::string TWwdosApp::runtimeApiKey;

// Accessor for runtime API key (used by wibwob_view.cpp)
std::string getAppRuntimeApiKey() {
    return TWwdosApp::runtimeApiKey;
}

TWwdosApp::TWwdosApp() :
    TProgInit(&TWwdosApp::initStatusLine,
              &TWwdosApp::initMenuBar,
              &TWwdosApp::initDeskTop),
    windowNumber(0),
    scrambleWindow(nullptr),
    scrambleState(sdsHidden)
{
    recentWorkspaces_ = scanRecentWorkspacePaths("workspaces", kMaxRecentWorkspaces);

    // Sync module primers (modules-private/*/primers/) into backrooms primers/
    // so they appear in the TV dialog and the CLI can resolve them immediately.
    syncModulePrimers();

    // Start IPC server for local API control (best-effort; ignore failures)
    ipcServer = new ApiIpcServer(this);

    // Socket path: /tmp/wwdos.sock (default) or /tmp/wibwob_N.sock (multi-instance).
    std::string sockPath = "/tmp/wwdos.sock";
    const char* inst = std::getenv("WIBWOB_INSTANCE");
    if (inst && inst[0] != '\0')
        sockPath = std::string("/tmp/wibwob_") + inst + ".sock";
    fprintf(stderr, "[wibwob] IPC socket: %s\n", sockPath.c_str());

    if (!ipcServer->start(sockPath)) {
        fprintf(stderr, "[wibwob] ERROR: IPC server failed to start on %s\n", sockPath.c_str());
        delete ipcServer;
        ipcServer = nullptr;
    } else {
        fprintf(stderr, "[wibwob] IPC server started on %s\n", sockPath.c_str());
    }

    // Load user skin files (skins/*.skin shadow/extend built-ins), then
    // restore the last active skin — a reskinned desktop shouldn't revert
    // to house grey just because it relaunched.
    loadUserSkins("skins");
    {
        std::ifstream sk(".wwdos_skin");
        std::string skinName;
        if (sk && std::getline(sk, skinName) && !skinName.empty()) {
            extern std::string api_set_skin(TWwdosApp&, const std::string&);
            fprintf(stderr, "[wibwob] Restoring skin: %s\n", skinName.c_str());
            api_set_skin(*this, skinName);
        }
    }

    // Auto-restore layout from env var (room deployment).
    const char* layoutPath = std::getenv("WIBWOB_LAYOUT_PATH");
    if (layoutPath && layoutPath[0] != '\0') {
        fprintf(stderr, "[wibwob] Restoring layout from WIBWOB_LAYOUT_PATH=%s\n", layoutPath);
        if (!loadWorkspaceFromFile(layoutPath)) {
            fprintf(stderr, "[wibwob] WARNING: Failed to restore layout from %s\n", layoutPath);
        }
    }

    // Sprites / multi-user mode: no hardcoded layout — use WIBWOB_LAYOUT_PATH
    // to point at workspaces/sprites-default.json (or any custom workspace).
    // See scripts/sprite-user-session.sh for env var wiring.

    // Init Scramble engine (KB + Haiku client).
    scrambleEngine.init(".");

    // Auto-open Scramble if WIBWOB_SCRAMBLE_DEFAULT is set (e.g. "tall" or "smol")
    const char* scrambleDefault = std::getenv("WIBWOB_SCRAMBLE_DEFAULT");
    if (scrambleDefault && scrambleDefault[0]) {
        // cycleScramble() creates in smol mode first
        cycleScramble();
        // If "tall", cycle again to expand
        if (std::string(scrambleDefault) == "tall") {
            cycleScramble();
        }
    }
}

std::string TWwdosApp::registerWindow(TWindow* w, bool emit_event) {
    if (!w) return std::string();
    auto it = winToId.find(w);
    if (it != winToId.end()) {
        lastRegisteredWindowId_ = it->second;
        return it->second;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "w%d", apiIdCounter++);
    std::string id(buf);
    winToId[w] = id;
    idToWin[id] = w;
    lastRegisteredWindowId_ = id;
    // Notify event subscribers that state has changed.
    if (emit_event && ipcServer) {
        std::string payload = std::string("{\"id\":\"") + id + "\"}";
        ipcServer->publish_event("state_changed", payload);
    }
    return id;
}

TWindow* TWwdosApp::findWindowById(const std::string& id) {
    // Scan desktop to discover unregistered windows and purge stale entries.
    // Must scan first so stale pointers are removed before we return one.
    // IMPORTANT: do NOT clear existing maps — that would reassign IDs for
    // already-known windows and cause multiplayer desync.
    std::vector<TWindow*> activeWins;
    TView *start = deskTop->first();
    if (start) {
        TView *v = start;
        do {
            TWindow *w = dynamic_cast<TWindow*>(v);
            if (w) {
                activeWins.push_back(w);
                if (winToId.find(w) == winToId.end()) {
                    // Unregistered window — give it a stable ID without firing an event.
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "w%d", apiIdCounter++);
                    std::string new_id(buf);
                    winToId[w] = new_id;
                    idToWin[new_id] = w;
                }
            }
            v = v->next;
        } while (v != start);
    }
    // Purge stale entries (windows closed since last scan).
    syncWindowRegistry(activeWins);
    auto it = idToWin.find(id);
    if (it != idToWin.end()) return it->second;
    return nullptr;
}

void TWwdosApp::handleEvent(TEvent& event)
{
    // Screensaver: real input resets the idle clock; if the saver is up,
    // the waking event is swallowed (before TApplication sees it).
    if (event.what == evKeyDown || event.what == evMouseDown) {
        lastInputMs_ = wwNowMs();
        if (saverWin_) {
            dismissScreensaver();
            clearEvent(event);
            return;
        }
    }

    TApplication::handleEvent(event);
    
    if (event.what == evCommand)
    {
        // Handle font selection from Edit → FIGlet Font → Category submenus
        // (range check before switch — can't use case for ranges)
        ushort cmd = event.message.command;
        if (cmd == cmSkinBase + 9) {
            extern std::string api_reload_skins(TWwdosApp&);
            api_reload_skins(*this);
            clearEvent(event);
            return;
        }
        if (cmd >= cmSkinBase && cmd <= cmSkinBase + 8) {
            static const char* kMenuSkinNames[9] = {
                "dflat", "turbo", "terra", "pipeline", "phosphor",
                "hercules", "paper", "midnight", "off" };
            extern std::string api_set_skin(TWwdosApp&, const std::string&);
            api_set_skin(*this, kMenuSkinNames[cmd - cmSkinBase]);
            clearEvent(event);
            return;
        }
        if (cmd >= cmFigletCatFontBase && cmd < cmFigletCatFontBase + 200) {
            TFigletTextWindow* fw = dynamic_cast<TFigletTextWindow*>(
                deskTop->current);
            if (fw && fw->getFigletView()) {
                const std::string& fname = figlet::fontByIndex(cmd - cmFigletCatFontBase);
                if (!fname.empty())
                    fw->getFigletView()->setFont(fname);
            }
            clearEvent(event);
            return;
        }
        if (cmd >= cmRecentWorkspace && cmd < cmRecentWorkspace + kMaxRecentWorkspaces) {
            int idx = cmd - cmRecentWorkspace;
            if (idx >= 0 && idx < (int)recentWorkspaces_.size())
                loadWorkspaceFromFile(recentWorkspaces_[idx]);
            clearEvent(event);
            return;
        }

        switch (event.message.command)
        {
            case cmNewWindow:
                newTestWindow();
                clearEvent(event);
                break;
            case cmNewGradientH:
                newGradientWindow(TGradientWindow::gtHorizontal);
                clearEvent(event);
                break;
            case cmNewGradientV:
                newGradientWindow(TGradientWindow::gtVertical);
                clearEvent(event);
                break;
            case cmNewGradientR:
                newGradientWindow(TGradientWindow::gtRadial);
                clearEvent(event);
                break;
            case cmNewGradientD:
                newGradientWindow(TGradientWindow::gtDiagonal);
                clearEvent(event);
                break;
            // case cmNewMechs:
            //     newMechWindow();
            //     clearEvent(event);
            //     break;
            case cmNewDonut:
                newDonutWindow();
                clearEvent(event);
                break;
            case cmOpenAnimation:
                if (event.message.infoPtr) {
                    // Called from gallery with a specific file path
                    openAnimationFilePath((const char*)event.message.infoPtr);
                } else {
                    openAnimationFile();
                }
                clearEvent(event);
                break;
            case cmOpenTransparentText:
                openTransparentTextFile();
                clearEvent(event);
                break;
            case cmOpenWorkspace:
                openWorkspace();
                clearEvent(event);
                break;
            case cmSaveWorkspace:
                saveWorkspace();
                clearEvent(event);
                break;
            case cmSaveWorkspaceAs:
                saveWorkspaceAs();
                clearEvent(event);
                break;
            case cmManageWorkspaces:
                manageWorkspaces();
                clearEvent(event);
                break;
            case cmPatternContinuous:
                setPatternMode(true);
                clearEvent(event);
                break;
            case cmPatternTiled:
                setPatternMode(false);
                clearEvent(event);
                break;
            case cmScreenshot:
                takeScreenshot();
                clearEvent(event);
                break;
            case cmCascade:
                cascade();
                clearEvent(event);
                break;
            case cmTile:
                tile();
                clearEvent(event);
                break;
            case cmCloseAll:
                closeAll();
                clearEvent(event);
                break;
            case cmSendToBack: {
                // Move the current window directly in front of the desktop background (i.e., to back).
                if (deskTop && deskTop->current && deskTop->background)
                    deskTop->current->putInFrontOf((TView*)deskTop->background);
                clearEvent(event);
                break;
            }
                
            // Edit menu commands
                
            // REMOVED E009: cmZoomIn/Out/ActualSize/FullScreen (placeholders, no menu items)
            case cmTextEditor: {
                TRect r = deskTop->getExtent();
                r.grow(-5, -3); // Leave some margin
                auto *w = createTextEditorWindow(r);
                deskTop->insert(w);
                registerWindow(w);
                clearEvent(event);
                break;
            }
            case cmBrowser:
                newBrowserWindow();
                clearEvent(event);
                break;
            case cmScrambleCat:
                cycleScramble();
                clearEvent(event);
                break;
            case cmScrambleExpand:
                cycleScramble();
                clearEvent(event);
                break;
            case cmAsciiGridDemo: {
                TRect r = deskTop->getExtent();
                r.grow(-10, -5);
                deskTop->insert(createAsciiGridDemoWindow(r));
                clearEvent(event);
                break;
            }
            case cmAnimatedBlocks: {
                TRect r = deskTop->getExtent();
                r.grow(-10, -5);
                deskTop->insert(createAnimatedBlocksWindow(r));
                clearEvent(event);
                break;
            }
            case cmAnimatedGradient: {
                TRect r = deskTop->getExtent();
                r.grow(-10, -5);
                deskTop->insert(createAnimatedGradientWindow(r));
                clearEvent(event);
                break;
            }
            case cmAnimatedScore: {
                TRect r = deskTop->getExtent();
                r.grow(-12, -6);
                deskTop->insert(createAnimatedScoreWindow(r));
                clearEvent(event);
                break;
            }
            case cmVerseField: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1); // almost full-screen to emphasise immersion
                deskTop->insert(createGenerativeVerseWindow(r));
                clearEvent(event);
                break;
            }
            case cmOrbitField: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                deskTop->insert(createGenerativeOrbitWindow(r));
                clearEvent(event);
                break;
            }
            case cmMyceliumField: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                deskTop->insert(createGenerativeMyceliumWindow(r));
                clearEvent(event);
                break;
            }
            case cmTorusField: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                deskTop->insert(createGenerativeTorusWindow(r));
                clearEvent(event);
                break;
            }
            case cmCubeField: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                deskTop->insert(createGenerativeCubeWindow(r));
                clearEvent(event);
                break;
            }
            case cmMonsterPortal: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                deskTop->insert(createGenerativeMonsterPortalWindow(r));
                clearEvent(event);
                break;
            }
            case cmMonsterVerse: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                deskTop->insert(createGenerativeMonsterVerseWindow(r));
                clearEvent(event);
                break;
            }
            case cmMonsterCam: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                deskTop->insert(createGenerativeMonsterCamWindow(r));
                clearEvent(event);
                break;
            }
            case cmBackroomsTv: {
                BackroomsChannel channel;
                if (showBackroomsTvDialog(channel)) {
                    TRect r = deskTop->getExtent();
                    r.grow(-2, -1);
                    TWindow *w = createBackroomsTvWindow(r, channel);
                    deskTop->insert(w);
                    registerWindow(w);
                }
                clearEvent(event);
                break;
            }
            case cmMicropolisAscii: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                TWindow* w = createMicropolisAsciiWindow(r);
                deskTop->insert(w);
                registerWindow(w);
                clearEvent(event);
                break;
            }
            case cmQuadra: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                TWindow* w = createQuadraWindow(r);
                deskTop->insert(w);
                registerWindow(w);
                clearEvent(event);
                break;
            }
            case cmSnake: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                TWindow* w = createSnakeWindow(r);
                deskTop->insert(w);
                registerWindow(w);
                clearEvent(event);
                break;
            }
            case cmRogue: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                TWindow* w = createRogueWindow(r);
                deskTop->insert(w);
                registerWindow(w);
                clearEvent(event);
                break;
            }
            case cmDeepSignal: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                TWindow* w = createDeepSignalWindow(r);
                deskTop->insert(w);
                registerWindow(w);
                clearEvent(event);
                break;
            }
            case cmOpenTerminal: {
                TRect r = deskTop->getExtent();
                r.grow(-2, -1);
                TWindow* w = createTerminalWindow(r);
                if (w) {
                    deskTop->insert(w);
                    registerWindow(w);
                }
                clearEvent(event);
                break;
            }
            case cmTweetShader: {
                extern void api_spawn_shader(TWwdosApp&, const TRect* bounds, const std::string& shader);
                api_spawn_shader(*this, nullptr, "");
                clearEvent(event);
                break;
            }
            case cmDiskLibrary: {
                extern void api_spawn_disks(TWwdosApp&, const TRect* bounds);
                api_spawn_disks(*this, nullptr);
                clearEvent(event);
                break;
            }
            case cmAppLauncher: {
                TRect desk = deskTop->getExtent();
                int ww = 63, hh = 20;
                int x = (desk.b.x - ww) / 2;
                int y = (desk.b.y - hh) / 2;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                TRect r(x, y, x + ww, y + hh);
                TWindow* w = createAppLauncherWindow(r);
                deskTop->insert(w);
                registerWindow(w);
                clearEvent(event);
                break;
            }
            case cmCtxGalleryToggle:
            case cmDeskGallery: {
                extern std::string api_desktop_gallery(TWwdosApp&, bool);
                api_desktop_gallery(*this, !galleryMode_);
                clearEvent(event);
                break;
            }
            case cmAsciiGallery: {
                TRect desk = deskTop->getExtent();
                int ww = desk.b.x * 9 / 10;
                int hh = desk.b.y * 9 / 10;
                int x = (desk.b.x - ww) / 2;
                int y = (desk.b.y - hh) / 2;
                TRect r(x, y, x + ww, y + hh);
                TWindow* w = createAsciiGalleryWindow(r);
                deskTop->insert(w);
                registerWindow(w);
                clearEvent(event);
                break;
            }
            case cmRogueHackTerminal: {
                // Spawn a small terminal window for roguelike hacking
                TRect desk = deskTop->getExtent();
                int tw = 52, th = 18;
                int tx = desk.b.x - tw - 1;
                int ty = 1;
                if (tx < 0) tx = 0;
                TRect termBounds(tx, ty, tx + tw, ty + th);
                TWindow* tw2 = createTerminalWindow(termBounds);
                if (tw2) {
                    deskTop->insert(tw2);
                    registerWindow(tw2);
                    auto* termWin = dynamic_cast<TWibWobTerminalWindow*>(tw2);
                    if (termWin) {
                        bool hackSuccess = (event.message.infoInt != 0);
                        if (hackSuccess) {
                            termWin->sendText(
                                "clear && echo '=== DUNGEON TERMINAL v2.7 ===' && "
                                "echo '> Inserting data chip...' && "
                                "sleep 0.3 && echo '> AUTHENTICATION: GRANTED' && "
                                "sleep 0.2 && echo '> Downloading floor map...' && "
                                "sleep 0.3 && echo '  [########----------] 42%%' && "
                                "sleep 0.2 && echo '  [################--] 84%%' && "
                                "sleep 0.1 && echo '  [####################] 100%%' && "
                                "sleep 0.2 && echo '> MAP DATA EXTRACTED' && "
                                "echo '> Patching health subsystem...' && "
                                "sleep 0.2 && echo '  +10 HP restored' && "
                                "echo '> Mining XP cache...' && "
                                "sleep 0.1 && echo '  +5 XP acquired' && "
                                "sleep 0.2 && echo '' && "
                                "echo '=== HACK COMPLETE ===' && "
                                "echo '' && echo 'Type exit to close terminal'\n"
                            );
                        } else {
                            termWin->sendText(
                                "clear && echo '=== DUNGEON TERMINAL v2.7 ===' && "
                                "echo '> Scanning credentials...' && "
                                "sleep 0.3 && echo '> ERROR: No data chip detected' && "
                                "sleep 0.2 && echo '' && "
                                "echo '  *** ACCESS DENIED ***' && "
                                "echo '' && "
                                "echo 'Insert a data chip (d) to hack this terminal.' && "
                                "echo 'Find data chips scattered in the dungeon.' && "
                                "echo '' && echo 'Type exit to close terminal'\n"
                            );
                        }
                    }
                }
                clearEvent(event);
                break;
            }
            case cmDeepSignalTerminal: {
                // Spawn a terminal window for signal/anomaly analysis
                TRect desk = deskTop->getExtent();
                int tw = 56, th = 20;
                int tx = desk.b.x - tw - 1;
                int ty = 1;
                if (tx < 0) tx = 0;
                TRect termBounds(tx, ty, tx + tw, ty + th);
                TWindow* tw2 = createTerminalWindow(termBounds);
                if (tw2) {
                    deskTop->insert(tw2);
                    registerWindow(tw2);
                    auto* termWin = dynamic_cast<TWibWobTerminalWindow*>(tw2);
                    if (termWin) {
                        int sigId = event.message.infoInt;
                        if (sigId >= 0 && sigId <= 4) {
                            // Signal decode animations (5 unique sequences)
                            const char* scripts[] = {
                                // Signal 0: Nav Beacon
                                "clear && echo '=== SIGNAL ANALYZER v3.1 ===' && echo '' && "
                                "echo 'Scanning frequency bands...' && sleep 0.4 && "
                                "echo '  _/\\  /\\  /\\  /\\  /\\_' && "
                                "echo ' /    \\/  \\/  \\/  \\/   ' && sleep 0.3 && "
                                "echo '' && echo 'Signal type: NAVIGATION BEACON' && "
                                "echo 'Origin: Automated relay station' && "
                                "echo 'Message: SAFE HARBOR AT SECTOR 0,0' && "
                                "echo '' && echo '[SIGNAL 1/5 DECODED]' && "
                                "echo '' && echo 'Type exit to close'\n",
                                // Signal 1: Distress Call
                                "clear && echo '=== SIGNAL ANALYZER v3.1 ===' && echo '' && "
                                "echo 'Intercepting transmission...' && sleep 0.3 && "
                                "echo '...---... ...---... ...---...' && sleep 0.4 && "
                                "echo '' && echo 'Pattern: SOS (universal distress)' && "
                                "echo 'Signal age: 847 standard years' && sleep 0.3 && "
                                "echo 'Origin: Colony ship MERIDIAN' && "
                                "echo 'Status: No life signs detected' && "
                                "echo '' && echo '[SIGNAL 2/5 DECODED]' && "
                                "echo '' && echo 'Type exit to close'\n",
                                // Signal 2: Science Data
                                "clear && echo '=== SIGNAL ANALYZER v3.1 ===' && echo '' && "
                                "echo 'Receiving data burst...' && sleep 0.3 && "
                                "echo '01001000 01000101 01001100' && sleep 0.2 && "
                                "echo '01010000 00100000 01010101' && sleep 0.2 && "
                                "echo '01010011 00100000 01010000' && sleep 0.3 && "
                                "echo '' && echo 'Decoding binary stream...' && sleep 0.4 && "
                                "echo 'Content: STELLAR CARTOGRAPHY DATA' && "
                                "echo 'Catalog: 2,847 uncharted systems' && "
                                "echo '' && echo '[SIGNAL 3/5 DECODED]' && "
                                "echo '' && echo 'Type exit to close'\n",
                                // Signal 3: Warning
                                "clear && echo '=== SIGNAL ANALYZER v3.1 ===' && echo '' && "
                                "echo 'Encrypted signal detected!' && sleep 0.3 && "
                                "echo 'Attempting decryption...' && sleep 0.2 && "
                                "echo '  [####----------] 28%%' && sleep 0.3 && "
                                "echo '  [########------] 57%%' && sleep 0.2 && "
                                "echo '  [############--] 85%%' && sleep 0.2 && "
                                "echo '  [################] 100%%' && sleep 0.3 && "
                                "echo '' && echo '*** WARNING BUOY ***' && "
                                "echo 'ANOMALOUS REGION - DO NOT APPROACH' && "
                                "echo 'SPATIAL DISTORTION DETECTED' && "
                                "echo '' && echo '[SIGNAL 4/5 DECODED]' && "
                                "echo '' && echo 'Type exit to close'\n",
                                // Signal 4: First Contact
                                "clear && echo '=== SIGNAL ANALYZER v3.1 ===' && echo '' && "
                                "echo 'Unknown modulation detected!' && sleep 0.5 && "
                                "echo 'Frequency: Non-standard' && sleep 0.3 && "
                                "echo 'Pattern analysis...' && sleep 0.4 && "
                                "echo '' && echo '  *   *   *' && sleep 0.2 && "
                                "echo '   * * * *' && sleep 0.2 && "
                                "echo '    * * *' && sleep 0.2 && "
                                "echo '   * * * *' && sleep 0.2 && "
                                "echo '  *   *   *' && sleep 0.3 && "
                                "echo '' && echo 'Mathematical structure detected!' && "
                                "echo 'Content: PRIME SEQUENCE + COORDINATES' && "
                                "echo 'Assessment: FIRST CONTACT PROTOCOL' && "
                                "echo '' && echo '=== ALL 5 SIGNALS DECODED ===' && "
                                "echo 'MISSION COMPLETE!' && "
                                "echo '' && echo 'Type exit to close'\n",
                            };
                            termWin->sendText(scripts[sigId]);
                        } else if (sigId >= 10 && sigId <= 12) {
                            // Anomaly analysis (3 unique)
                            const char* anomScripts[] = {
                                // Anomaly 0: Spatial Rift
                                "clear && echo '=== ANOMALY SCANNER ===' && echo '' && "
                                "echo 'Spatial distortion detected!' && sleep 0.3 && "
                                "echo '' && echo '     .  * .     ' && "
                                "echo '   ~~ * ~~ ~~   ' && "
                                "echo '  ~~~~ * ~~~~   ' && "
                                "echo '   ~~ * ~~ ~~   ' && "
                                "echo '     .  * .     ' && "
                                "echo '' && echo 'Type: Spatial Rift' && "
                                "echo 'Diameter: 4.7 AU' && "
                                "echo 'Status: Stable but impassable' && "
                                "echo '' && echo 'Type exit to close'\n",
                                // Anomaly 1: Energy Signature
                                "clear && echo '=== ANOMALY SCANNER ===' && echo '' && "
                                "echo 'Unidentified energy source!' && sleep 0.3 && "
                                "echo 'Power output: 4.2 x 10^26 watts' && sleep 0.2 && "
                                "echo '' && echo 'Spectrum analysis:' && "
                                "echo '  low  |####          |' && "
                                "echo '  mid  |########      |' && "
                                "echo '  high |##############|' && "
                                "echo '' && echo 'Type: Unknown - possibly artificial' && "
                                "echo 'Origin: Pre-dates known civilizations' && "
                                "echo '' && echo 'Type exit to close'\n",
                                // Anomaly 2: Temporal Echo
                                "clear && echo '=== ANOMALY SCANNER ===' && echo '' && "
                                "echo 'Chronometric disturbance!' && sleep 0.4 && "
                                "echo '' && echo 'Temporal readings:' && sleep 0.2 && "
                                "echo '  Past:    ##########' && sleep 0.2 && "
                                "echo '  Present: ####' && sleep 0.2 && "
                                "echo '  Future:  ########' && sleep 0.3 && "
                                "echo '' && echo 'Type: Temporal Echo' && "
                                "echo 'Events from multiple timelines' && "
                                "echo 'overlap at this coordinate.' && "
                                "echo '' && echo 'Type exit to close'\n",
                            };
                            int aIdx = sigId - 10;
                            if (aIdx >= 0 && aIdx < 3)
                                termWin->sendText(anomScripts[aIdx]);
                        }
                    }
                    // Re-select the Deep Signal game window so it keeps focus
                    deskTop->forEach([](TView *v, void *) {
                        auto *w = dynamic_cast<TWindow*>(v);
                        if (w && w->title) {
                            std::string t(w->title);
                            if (t == "Deep Signal") w->select();
                        }
                    }, nullptr);
                }
                clearEvent(event);
                break;
            }
            // DISABLED: ASCII Cam (file not in repo)
            // case cmASCIICam: {
            //     TRect r = deskTop->getExtent();
            //     r.grow(-2, -1);
            //     deskTop->insert(createGenerativeASCIICamWindow(r));
            //     clearEvent(event);
            //     break;
            // }
            case cmScoreBgColor: {
                // Try to find an Animated Score view in the current window.
                auto findScore = [](TView *p, void *out) -> Boolean {
                    if (!p) return False;
                    TAnimatedScoreView **pp = (TAnimatedScoreView**)out;
                    if (*pp) return False;
                    if (auto *v = dynamic_cast<TAnimatedScoreView*>(p)) { *pp = v; return True; }
                    return False;
                };
                TAnimatedScoreView *score = nullptr;
                if (deskTop && deskTop->current) {
                    TView *cur = deskTop->current;
                    // If current is a window/group, search its children; else, check itself.
                    if (auto *grp = dynamic_cast<TGroup*>(cur))
                        grp->firstThat(findScore, &score);
                    if (!score)
                        score = dynamic_cast<TAnimatedScoreView*>(cur);
                }
                if (!score) {
                    // Fallback: search desktop for any score view.
                    if (deskTop)
                        deskTop->firstThat(findScore, &score);
                }
                if (score) {
                    score->openBackgroundPaletteDialog();
                } else {
                    messageBox("No Animated Score view is active.", mfInformation | mfOKButton);
                }
                clearEvent(event);
                break;
            }
            // REMOVED E009: cmWindowBgColor (Background Color retired, no menu item)
                
            // Tools menu commands
            case cmWibWobChat:
                newWibWobWindow();
                clearEvent(event);
                break;
            case cmRoomChat:
                newRoomChatWindow();
                clearEvent(event);
                break;
            case cmRoomPresence: {
                auto* participants =
                    static_cast<std::vector<RoomParticipant>*>(event.message.infoPtr);
                if (participants) {
                    if (TRoomChatWindow* win = getRoomChatWindow())
                        win->updatePresence(*participants);
                    delete participants;
                }
                clearEvent(event);
                break;
            }
            case cmRoomChatReceive: {
                auto* msg = static_cast<RoomChatMessage*>(event.message.infoPtr);
                if (msg) {
                    if (TRoomChatWindow* win = getRoomChatWindow())
                        win->receiveMessage(*msg);
                    delete msg;
                }
                clearEvent(event);
                break;
            }
            // REMOVED E009: cmWibWobTestA/B/C (dev-only, no menu items)
            case cmRepaint:
                if (deskTop) {
                    deskTop->drawView();
                }
                clearEvent(event);
                break;
            // REMOVED E009: cmAnsiEditor, cmPaintTools, cmAnimationStudio (placeholders, no menu items)
            case cmQuantumPrinter:
                messageBox("🚀 QUANTUM PRINTER ACTIVATED! 🚀\n\nPrinting reality at 42Hz...", mfInformation | mfOKButton);
                clearEvent(event);
                break;
            case cmApiKey:
                showApiKeyDialog();
                clearEvent(event);
                break;

            // Help menu commands
            case cmAbout:
                messageBox("WIBWOBWORLD Test Pattern Generator\n\nBuilt with Turbo Vision\nつ◕‿◕‿◕༽つ", mfInformation | mfOKButton);
                clearEvent(event);
                break;
            case cmKeyboardShortcuts:
                messageBox(
                    "Keyboard Shortcuts\n\n"
                    "Ctrl+N   New Test Pattern\n"
                    "Ctrl+D   New Animation\n"
                    "Ctrl+O   Open Text/Animation\n"
                    "Ctrl+B   Browser\n"
                    "Ctrl+S   Save Workspace\n"
                    "Ctrl+P   Screenshot\n"
                    "Ctrl+Ins Copy Page\n"
                    "F5       Repaint\n"
                    "F6       Next Window\n"
                    "Shift+F6 Previous Window\n"
                    "F8       Scramble (cycle)\n"
                    "F12      Wib&Wob Chat\n"
                    "Alt+F3   Close Window\n"
                    "Alt+X    Exit",
                    mfInformation | mfOKButton);
                clearEvent(event);
                break;
            case cmApiKeyHelp:
                messageBox(
                    "API Key (ANTHROPIC_API_KEY)\n\n"
                    "Fallback auth for Scramble Cat and\n"
                    "Wib&Wob Chat when Claude Code CLI\n"
                    "is not logged in.\n\n"
                    "Set via:\n"
                    "  1. ANTHROPIC_API_KEY env var\n"
                    "  2. Tools > API Key dialog\n\n"
                    "Preferred: run 'claude /login'\n"
                    "See Help > LLM Status for details.",
                    mfInformation | mfOKButton);
                clearEvent(event);
                break;
            case cmLlmStatus:
            {
                std::string summary = AuthConfig::instance().statusSummary();
                messageBox(summary.c_str(),
                    mfInformation | mfOKButton);
                clearEvent(event);
                break;
            }

            case cmScrambleReply:
            {
                deliverScrambleReply();
                clearEvent(event);
                break;
            }

            case cmScrambleCancel:
            {
                // Cancel async scramble request (ESC during thinking)
                if (scrambleEngine.isBusy()) {
                    scrambleEngine.haiku().cancelAsync();
                }
                clearEvent(event);
                break;
            }

            // REMOVED E009: entire Glitch Effects submenu handlers
            // (cmToggleGlitchMode, cmGlitchScatter, cmGlitchColorBleed,
            //  cmGlitchRadialDistort, cmGlitchDiagonalScatter,
            //  cmCaptureGlitchedFrame, cmResetGlitchParams, cmGlitchSettings)
                
            case cmNewPaintCanvas: {
                TRect d = deskTop->getExtent();
                int dw = d.b.x - d.a.x;
                int dh = d.b.y - d.a.y;
                int w = (int)(dw * 0.9);
                int h = (int)(dh * 0.9);
                int left = d.a.x + (dw - w) / 2;
                int top  = d.a.y + (dh - h) / 2;
                TRect r(left, top, left + w, top + h);
                TWindow* pw = createPaintWindow(r);
                deskTop->insert(pw);
                registerWindow(pw);
                clearEvent(event);
                break;
            }
            case cmNewFigletText: {
                extern void api_spawn_figlet_text(TWwdosApp&, const TRect*,
                    const std::string& text, const std::string& font,
                    bool frameless, bool shadowless);
                api_spawn_figlet_text(*this, nullptr, "Hello", "standard", false, false);
                clearEvent(event);
                break;
            }
            case cmFigletEditText: {
                // Route to focused figlet window
                TFigletTextWindow* fw = dynamic_cast<TFigletTextWindow*>(
                    deskTop->current);
                if (fw && fw->getFigletView())
                    fw->getFigletView()->showEditTextDialog();
                clearEvent(event);
                break;
            }
            case cmFigletMoreFonts: {
                TFigletTextWindow* fw = dynamic_cast<TFigletTextWindow*>(
                    deskTop->current);
                if (fw && fw->getFigletView())
                    fw->getFigletView()->showFontListDialog();
                clearEvent(event);
                break;
            }
            case cmOpenImageFile: {
                char fileName[MAXPATH];
                strcpy(fileName, "*.{png,jpg,jpeg}");
                TFileDialog* dialog = new TFileDialog("*.{png,jpg,jpeg}", "Open Image File", "~N~ame", fdOpenButton, 101);
                if (executeDialog(dialog, fileName) != cmCancel) {
                    windowNumber++;
                    // Cascade-like default bounds
                    int offset = (windowNumber - 1) % 10;
                    TRect bounds(2 + offset * 2, 1 + offset, 70 + offset * 2, 25 + offset);
                    if (TWindow *w = createAsciiImageWindowFromFile(bounds, fileName)) {
                        deskTop->insert(w);
                        registerWindow(w);
                    }
                }
                clearEvent(event);
                break;
            }

            case cmOpenMonodraw: {
                char fileName[MAXPATH];
                strcpy(fileName, "*.monojson");
                TFileDialog* dialog = new TFileDialog("*.monojson", "Open Monodraw File", "~N~ame", fdOpenButton, 101);
                if (executeDialog(dialog, fileName) != cmCancel) {
                    openMonodrawFile(fileName);
                }
                clearEvent(event);
                break;
            }

            default:
                break;
        }
    }
}

void TWwdosApp::newTestWindow()
{
    // Create window title
    windowNumber++;
    std::stringstream title;
    title << "Test Pattern " << windowNumber;
    
    // Calculate window position (cascade effect)
    int offset = (windowNumber - 1) % 10;
    TRect bounds(
        2 + offset * 2,           // left
        1 + offset,               // top
        50 + offset * 2,          // right
        15 + offset               // bottom
    );
    
    // Create and insert window
    TTestPatternWindow* window = new TTestPatternWindow(bounds, title.str().c_str());
    deskTop->insert(window);
}

void TWwdosApp::newTestWindow(const TRect& bounds)
{
    // Create window title
    windowNumber++;
    std::stringstream title;
    title << "Test Pattern " << windowNumber;
    
    // Create and insert window with provided bounds
    TTestPatternWindow* window = new TTestPatternWindow(bounds, title.str().c_str());
    deskTop->insert(window);
    registerWindow(window);
}

void TWwdosApp::newBrowserWindow()
{
    TRect r = deskTop->getExtent();
    r.grow(-3, -2);
    TWindow* window = createBrowserWindow(r);
    deskTop->insert(window);
    registerWindow(window);
    if (auto* browser = dynamic_cast<TBrowserWindow*>(window))
        browser->fetchUrl("https://symbient.life");
}

void TWwdosApp::newBrowserWindow(const TRect& bounds)
{
    TWindow* window = createBrowserWindow(bounds);
    deskTop->insert(window);
    registerWindow(window);
    if (auto* browser = dynamic_cast<TBrowserWindow*>(window))
        browser->fetchUrl("https://symbient.life");
}

void TWwdosApp::deliverScrambleReply()
{
    if (pendingScrambleReply.empty() || !scrambleWindow) return;
    std::string reply = std::move(pendingScrambleReply);
    pendingScrambleReply.clear();

    if (scrambleWindow->getView()) {
        scrambleWindow->getView()->setPose(spCurious);
        scrambleWindow->getView()->say(reply);
    }
    if (scrambleWindow->getMessageView()) {
        scrambleWindow->getMessageView()->addMessage("scramble", reply);
    }
}

void TWwdosApp::wireScrambleInput()
{
    if (!scrambleWindow || !scrambleWindow->getInputView()) return;

    scrambleWindow->getInputView()->onSubmit = [this](const std::string& input) {
        if (!scrambleWindow) return;

        // Add user message to history
        if (scrambleWindow->getMessageView()) {
            scrambleWindow->getMessageView()->addMessage("you", input);
        }

        // Log non-slash messages for multiplayer chat relay
        if (input.empty() || input[0] != '/') {
            chatLog_.push_back({++chatSeq_, "you", input});
            if ((int)chatLog_.size() > kChatLogMax)
                chatLog_.pop_front();
        }

        // Slash commands: check registry first, then fall through to engine.
        // /cascade, /screenshot, /scramble_pet → execute the registry command.
        // /scramble_say hello cat → cmdName="scramble_say", args="hello cat"
        if (!input.empty() && input[0] == '/' && input.size() > 1) {
            std::string rest = input.substr(1);
            // Split on first space: "scramble_say hello" → name="scramble_say" args="hello"
            std::string cmdName;
            std::string cmdArgs;
            size_t sp = rest.find(' ');
            if (sp != std::string::npos) {
                cmdName = rest.substr(0, sp);
                cmdArgs = rest.substr(sp + 1);
                // Strip leading spaces from args
                while (!cmdArgs.empty() && cmdArgs.front() == ' ') cmdArgs.erase(cmdArgs.begin());
            } else {
                cmdName = rest;
            }
            for (char& c : cmdName) if (c >= 'A' && c <= 'Z') c += 32;
            while (!cmdName.empty() && cmdName.back() == ' ') cmdName.pop_back();

            const auto& caps = get_command_capabilities();
            for (const auto& cap : caps) {
                if (cmdName == cap.name) {
                    // Pass args under all plausible param names — each command reads only one
                    std::map<std::string, std::string> kv;
                    if (!cmdArgs.empty()) {
                        kv["text"] = cmdArgs;
                        kv["path"] = cmdArgs;
                        kv["mode"] = cmdArgs;
                        kv["variant"] = cmdArgs;
                    }
                    std::string result = exec_registry_command(*this, cmdName, kv);
                    // If command returned a response (e.g. scramble_say returns text), show it
                    std::string ack;
                    if (result == "ok") {
                        ack = "done. /ᐠ- -ᐟ\\";
                    } else if (result.rfind("err", 0) == 0) {
                        ack = result + " (=^..^=)";
                    } else {
                        ack = result; // e.g. scramble_say returns the response text
                    }
                    // Only show ack in message view if scramble_say didn't already add it
                    if (cmdName != "scramble_say" && cmdName != "scramble_pet") {
                        if (scrambleWindow->getView()) {
                            scrambleWindow->getView()->say(ack);
                        }
                        if (scrambleWindow->getMessageView()) {
                            scrambleWindow->getMessageView()->addMessage("scramble", ack);
                        }
                    }
                    return;
                }
            }
            // Not in registry — engine handles /help, /who, /cmds, unknown
        }

        // Query engine (async for LLM, sync for slash commands)
        std::string syncResult;
        bool isAsync = scrambleEngine.askAsync(input, syncResult,
            [this](const std::string& response) {
                // Callback fires from poll() inside idle() — do NOT drawView() here.
                pendingScrambleReply = response.empty() ? "(no response from model — try again)" : response;
                // Clear spinner
                if (scrambleWindow && scrambleWindow->getInputView())
                    scrambleWindow->getInputView()->setThinking(false);
                TEvent event;
                event.what = evCommand;
                event.message.command = cmScrambleReply;
                event.message.infoPtr = nullptr;
                putEvent(event);
            });

        if (isAsync && scrambleWindow->getInputView()) {
            scrambleWindow->getInputView()->setThinking(true);
        }

        if (!isAsync) {
            // Slash command or fallback — deliver immediately (not from idle)
            pendingScrambleReply = syncResult.empty() ? "(no response from model — try again)" : syncResult;
            deliverScrambleReply();
        } else {
            // Show "thinking" state while async runs
            if (scrambleWindow && scrambleWindow->getView()) {
                scrambleWindow->getView()->setPose(spCurious);
                scrambleWindow->getView()->say("... *thinking* /ᐠ｡ꞈ｡ᐟ\\");
            }
        }
    };
}

void TWwdosApp::cycleScramble()
{
    if (!scrambleWindow || scrambleState == sdsHidden) {
        // Create at bottom-right corner of desktop in smol mode
        TRect desktop = deskTop->getExtent();
        int w = 38;
        int h = 14;
        TRect r(desktop.b.x - w - 1, desktop.b.y - h,
                desktop.b.x - 1,     desktop.b.y);
        scrambleWindow = static_cast<TScrambleWindow*>(createScrambleWindow(r, sdsSmol));
        scrambleState = sdsSmol;
        // Wire engine into view
        if (scrambleWindow->getView()) {
            scrambleWindow->getView()->setEngine(&scrambleEngine);
        }
        wireScrambleInput();
        deskTop->insert(scrambleWindow);
        // Put behind other windows (just in front of background)
        if (deskTop->background) {
            scrambleWindow->putInFrontOf((TView*)deskTop->background);
        }
    } else if (scrambleState == sdsSmol) {
        // Expand: smol -> tall
        TRect desktop = deskTop->getExtent();
        int w = 40;
        TRect r(desktop.b.x - w - 1, desktop.a.y,
                desktop.b.x - 1,     desktop.b.y);
        scrambleWindow->changeBounds(r);
        scrambleWindow->setDisplayState(sdsTall);
        scrambleState = sdsTall;
        scrambleWindow->focusInput();
        // Welcome message if history is empty
        if (scrambleWindow->getMessageView() &&
            scrambleWindow->getMessageView()->getMessages().empty()) {
            scrambleWindow->getMessageView()->addMessage("scramble", "mrrp! ask me anything (=^..^=)");
        }
    } else if (scrambleState == sdsTall) {
        // Remove existing Scramble window
        destroy(scrambleWindow);
        scrambleWindow = nullptr;
        scrambleState = sdsHidden;
    }
}

void TWwdosApp::cascade()
{
    deskTop->cascade(deskTop->getExtent());
}

void TWwdosApp::tile()
{
    deskTop->tile(deskTop->getExtent());
}

void TWwdosApp::closeAll()
{
    // Close all regular windows on the desktop (iterating safely over circular list)
    std::vector<TWindow*> toClose;
    TView *start = deskTop->first();
    if (start) {
        TView *v = start;
        do {
            TView *nextV = v->next; // cache next to avoid invalidation issues
            if (TWindow *w = dynamic_cast<TWindow*>(v)) {
                // Skip non-user windows if any (none expected here)
                toClose.push_back(w);
            }
            v = nextV;
        } while (v != start);
    }
    for (auto *w : toClose) {
        if (w && (w->flags & wfClose))
            w->close();
    }
}

void TWwdosApp::newGradientWindow(TGradientWindow::GradientType type)
{
    // Create window title
    windowNumber++;
    std::stringstream title;
    
    switch (type)
    {
        case TGradientWindow::gtHorizontal:
            title << "Horizontal Gradient " << windowNumber;
            break;
        case TGradientWindow::gtVertical:
            title << "Vertical Gradient " << windowNumber;
            break;
        case TGradientWindow::gtRadial:
            title << "Radial Gradient " << windowNumber;
            break;
        case TGradientWindow::gtDiagonal:
            title << "Diagonal Gradient " << windowNumber;
            break;
    }
    
    // Calculate window position (cascade effect)
    int offset = (windowNumber - 1) % 10;
    TRect bounds(
        2 + offset * 2,           // left
        1 + offset,               // top
        50 + offset * 2,          // right
        15 + offset               // bottom
    );
    
    // Create and insert window
    TGradientWindow* window = new TGradientWindow(bounds, title.str().c_str(), type);
    deskTop->insert(window);
    registerWindow(window);
}

void TWwdosApp::newGradientWindow(TGradientWindow::GradientType type, const TRect& bounds)
{
    // Create window title
    windowNumber++;
    std::stringstream title;
    
    switch (type)
    {
        case TGradientWindow::gtHorizontal:
            title << "Horizontal Gradient " << windowNumber;
            break;
        case TGradientWindow::gtVertical:
            title << "Vertical Gradient " << windowNumber;
            break;
        case TGradientWindow::gtRadial:
            title << "Radial Gradient " << windowNumber;
            break;
        case TGradientWindow::gtDiagonal:
            title << "Diagonal Gradient " << windowNumber;
            break;
    }
    
    // Create and insert window with provided bounds
    TGradientWindow* window = new TGradientWindow(bounds, title.str().c_str(), type);
    deskTop->insert(window);
    registerWindow(window);
}

// void TWwdosApp::newMechWindow()
// {
//     // Create window title
//     windowNumber++;
//     std::stringstream title;
//     title << "Mechs Grid " << windowNumber;
//     
//     // Calculate window position (cascade effect)
//     int offset = (windowNumber - 1) % 10;
//     TRect bounds(
//         2 + offset * 2,           // left
//         1 + offset,               // top
//         70 + offset * 2,          // right (wider for mech grid)
//         30 + offset               // bottom (taller for mech grid)
//     );
//     
//     // Create and insert window
//     TMechWindow* window = new TMechWindow(bounds, title.str().c_str(), windowNumber);
//     deskTop->insert(window);
//     registerWindow(window);
// }

void TWwdosApp::newDonutWindow()
{
    // Create window title
    windowNumber++;
    std::stringstream title;
    title << "Donut Animation " << windowNumber;
    
    // Calculate window position (cascade effect)
    int offset = (windowNumber - 1) % 10;
    TRect bounds(
        2 + offset * 2,           // left
        1 + offset,               // top
        50 + offset * 2,          // right
        15 + offset               // bottom
    );
    
    // Create and insert window with donut.txt file (no title for minimalist aesthetic)
    // Path is relative to repo root (CWD when launched via ./build/app/test_pattern)
    TFrameAnimationWindow* window = new TFrameAnimationWindow(bounds, "", "app/donut.txt");
    deskTop->insert(window);
    registerWindow(window);
}

void TWwdosApp::newWibWobWindow()
{
    // Only one W&W chat window per instance — raise existing if already open.
    if (deskTop) {
        TView* v = deskTop->first();
        if (v) {
            TView* start = v;
            do {
                if (auto* ww = dynamic_cast<TWibWobWindow*>(v)) {
                    deskTop->setCurrent(ww, TDeskTop::normalSelect);
                    return;
                }
                v = v->next;
            } while (v != start);
        }
    }

    TRect bounds(2, 1, 82, 28);
    TWindow* window = createWibWobWindow(bounds, "Wib&Wob Chat");
    if (!window) {
        messageBox("Failed to create Wib&Wob Chat window.", mfError | mfOKButton);
        return;
    }

    deskTop->insert(window);
    registerWindow(window);
    window->select();
}

void TWwdosApp::newRoomChatWindow()
{
    windowNumber++;
    int offset = (windowNumber - 1) % 8;
    TRect bounds(
        4 + offset * 2,
        2 + offset,
        100 + offset * 2,
        28 + offset
    );
    TWindow* window = createRoomChatWindow(bounds);
    if (!window) {
        messageBox("Failed to create Room Chat window.", mfError | mfOKButton);
        return;
    }
    deskTop->insert(window);
    registerWindow(window);
    window->select();
}

void TWwdosApp::newWibWobTestWindowA()
{
    // Test window A: standardScrollBar() fix (minimal change approach)
    windowNumber++;
    std::stringstream title;
    title << "Test A: stdScrollBar " << windowNumber;

    int offset = (windowNumber - 1) % 10;
    TRect bounds(
        2 + offset * 2,
        1 + offset,
        82 + offset * 2,
        28 + offset
    );

    TWindow* window = createWibWobTestWindowA(bounds, title.str());
    if (!window) {
        messageBox("Failed to create Test A window.", mfError | mfOKButton);
        return;
    }

    deskTop->insert(window);
    registerWindow(window);
    window->select();
}

void TWwdosApp::newWibWobTestWindowB()
{
    // Test window B: TScroller-based (proper TV architecture)
    windowNumber++;
    std::stringstream title;
    title << "Test B: TScroller " << windowNumber;

    int offset = (windowNumber - 1) % 10;
    TRect bounds(
        2 + offset * 2,
        1 + offset,
        82 + offset * 2,
        28 + offset
    );

    TWindow* window = createWibWobTestWindowB(bounds, title.str());
    if (!window) {
        messageBox("Failed to create Test B window.", mfError | mfOKButton);
        return;
    }

    deskTop->insert(window);
    registerWindow(window);
    window->select();
}

void TWwdosApp::newWibWobTestWindowC()
{
    // Test window C: Split view architecture (MessageView + InputView)
    windowNumber++;
    std::stringstream title;
    title << "Test C: Split Arch " << windowNumber;

    int offset = (windowNumber - 1) % 10;
    TRect bounds(
        2 + offset * 2,
        1 + offset,
        82 + offset * 2,
        28 + offset
    );

    TWindow* window = createWibWobTestWindowC(bounds, title.str());
    if (!window) {
        messageBox("Failed to create Test C window.", mfError | mfOKButton);
        return;
    }

    deskTop->insert(window);
    registerWindow(window);
    window->select();
}

void TWwdosApp::openAnimationFile()
{
    char fileName[MAXPATH];
    std::string primerGlob = findPrimerDir() + "/*.txt";
    strcpy(fileName, primerGlob.c_str());

    TFileDialog* dialog = new TFileDialog(primerGlob.c_str(), "Open Text/Animation File", "~N~ame", fdOpenButton, 100);
    if (executeDialog(dialog, fileName) != cmCancel)
    {
        windowNumber++;
        
        // Auto-size window to file content
        TRect bounds = calculateWindowBounds(fileName);
        // Create window without title (empty string)
        TFrameAnimationWindow* window = new TFrameAnimationWindow(bounds, "", fileName);
        deskTop->insert(window);
        registerWindow(window);
    }
}

void TWwdosApp::openAnimationFilePath(const std::string& filePath)
{
    // Determine file type and create appropriate title
    windowNumber++;
    std::stringstream title;
    
    if (hasFrameDelimiters(filePath)) {
        title << "Animation " << windowNumber;
    } else {
        // Extract filename without path for text files
        size_t lastSlash = filePath.find_last_of("/\\");
        std::string baseName = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;
        title << baseName << " - Text " << windowNumber;
    }
    
    // Auto-size window to file content
    TRect bounds = calculateWindowBounds(filePath);
    // Create and insert window with selected file. The computed title was
    // historically dropped ("" passed instead) leaving a bare gap in the
    // top frame — the filename belongs in the title tab (Zilla 2026-08-13).
    TFrameAnimationWindow* window = new TFrameAnimationWindow(bounds, title.str().c_str(), filePath);
    deskTop->insert(window);
    registerWindow(window);
}

void TWwdosApp::openAnimationFilePath(const std::string& filePath, const TRect& bounds, bool frameless, bool shadowless, const std::string& title)
{
    windowNumber++;
    // Empty title = derive from filename — a titleless text window leaves a
    // bare gap in the top frame (frameless windows skip this: no frame,
    // nowhere for a title to live).
    std::string t = title;
    if (t.empty() && !frameless) {
        size_t lastSlash = filePath.find_last_of("/\\");
        t = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;
    }
    TFrameAnimationWindow* window = new TFrameAnimationWindow(bounds, t.c_str(), filePath, frameless, shadowless);
    deskTop->insert(window);
    registerWindow(window);
}

void TWwdosApp::openTransparentTextFile()
{
    char fileName[MAXPATH];
    std::string primerGlob = findPrimerDir() + "/*.txt";
    strcpy(fileName, primerGlob.c_str());

    TFileDialog* dialog = new TFileDialog(primerGlob.c_str(), "Open Text File (Transparent BG)", "~N~ame", fdOpenButton, 100);
    if (executeDialog(dialog, fileName) != cmCancel)
    {
        windowNumber++;

        // Extract filename without path for title
        std::string filePath(fileName);
        size_t lastSlash = filePath.find_last_of("/\\");
        std::string baseName = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;

        std::stringstream title;
        title << baseName << " (Transparent)";

        // Calculate window position (cascade effect)
        int offset = (windowNumber - 1) % 10;
        TRect bounds(
            2 + offset * 2,           // left
            1 + offset,               // top
            82 + offset * 2,          // right (80 cols + frame)
            25 + offset               // bottom (24 rows + frame)
        );

        // Create transparent background text window
        TTransparentTextWindow* window = new TTransparentTextWindow(bounds, title.str(), filePath);
        deskTop->insert(window);
        registerWindow(window);
    }
}

void TWwdosApp::openMonodrawFile(const char* fileName)
{
    // Use curl to call the Monodraw API endpoint
    std::stringstream cmd;
    cmd << "curl -s -X POST 'http://127.0.0.1:8089/monodraw/load' "
        << "-H 'Content-Type: application/json' "
        << "-d '{\"file_path\": \"" << fileName << "\", "
        << "\"target\": \"text_editor\", "
        << "\"mode\": \"replace\", "
        << "\"flatten\": true, "
        << "\"insert_header\": true}' "
        << "> /dev/null 2>&1 &";  // Background, suppress output

    int result = std::system(cmd.str().c_str());

    if (result == 0) {
        std::string msg = "Importing Monodraw file to text editor...";
        messageBox(msg.c_str(), mfInformation | mfOKButton);
    } else {
        messageBox("Failed to import Monodraw file. Is API server running?", mfError | mfOKButton);
    }
}

void TWwdosApp::setPatternMode(bool continuous)
{
    USE_CONTINUOUS_PATTERN = continuous;

    // Show confirmation message
    std::string mode = continuous ? "Continuous (Diagonal)" : "Tiled (Cropped)";
    std::stringstream msg;
    msg << "Pattern mode set to: " << mode;
    messageBox(msg.str().c_str(), mfInformation | mfOKButton);
}


void TWwdosApp::showApiKeyDialog()
{
    // Build dialog
    TRect dlgRect(0, 0, 56, 14);
    dlgRect.move((TProgram::deskTop->size.x - 56) / 2,
                 (TProgram::deskTop->size.y - 14) / 2);

    TDialog* dlg = new TDialog(dlgRect, "API Key");

    dlg->insert(new TStaticText(TRect(3, 2, 53, 3),
        "Used by Scramble (Haiku) and Wib&Wob Chat."));
    dlg->insert(new TLabel(TRect(3, 4, 53, 5), "Anthropic API key (sk-ant-...):", nullptr));

    TRect inputRect(3, 5, 53, 6);
    TInputLine* input = new TInputLine(inputRect, 256);
    dlg->insert(input);

    // Status line showing current key state
    std::string status = runtimeApiKey.empty()
        ? "No key set (or use ANTHROPIC_API_KEY env var)"
        : "Key configured (runtime override active)";
    dlg->insert(new TStaticText(TRect(3, 7, 53, 8), status.c_str()));
    dlg->insert(new TStaticText(TRect(3, 8, 53, 9),
        "See Help > API Key Help for details."));

    TRect okRect(12, 10, 24, 12);
    TRect cancelRect(30, 10, 42, 12);
    dlg->insert(new TButton(okRect, "~O~K", cmOK, bfDefault));
    dlg->insert(new TButton(cancelRect, "Cancel", cmCancel, bfNormal));

    ushort result = TProgram::deskTop->execView(dlg);
    if (result == cmOK) {
        char keyBuf[256];
        input->getData(keyBuf);
        std::string key(keyBuf);
        // Trim whitespace
        while (!key.empty() && (key.back() == ' ' || key.back() == '\0'))
            key.pop_back();
        while (!key.empty() && key.front() == ' ')
            key.erase(key.begin());

        if (!key.empty()) {
            runtimeApiKey = key;
            // Wire to Scramble engine so it can use Haiku immediately.
            scrambleEngine.setApiKey(key);
            // Broadcast to all views (WibWobView picks this up to re-init its engine).
            message(deskTop, evBroadcast, cmApiKeyChanged, nullptr);
            fprintf(stderr, "[app] api key set via dialog (len=%zu) — wired to runtimeApiKey + scrambleEngine + broadcast\n",
                    key.size());
            if (key.substr(0, 6) == "sk-ant") {
                messageBox("API key set. Chat will use Anthropic API.",
                           mfInformation | mfOKButton);
            } else {
                messageBox("Key set, but doesn't look like an Anthropic key (expected sk-ant-...).",
                           mfWarning | mfOKButton);
            }
        } else {
            messageBox("No key entered.", mfWarning | mfOKButton);
        }
    }
    TObject::destroy(dlg);
}

void TWwdosApp::takeScreenshot(bool showDialog)
{
    // Create logs/screenshots/ directory if it doesn't exist.
    mkdir("logs", 0755);
    mkdir("logs/screenshots", 0755);

    // Generate timestamp for filenames.
    time_t rawtime;
    struct tm* timeinfo;
    char timestamp[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);

    // 1) Authoritative in-process capture from Turbo Vision screen buffer.
    std::string base = std::string("logs/screenshots/tui_") + timestamp;
    std::string txtPath = base + ".txt";
    std::string ansiPath = base + ".ans";
    auto frame = getFrameCapture().captureScreen();

    CaptureOptions txtOpts;
    txtOpts.format = CaptureFormat::PlainText;
    txtOpts.addTimestamp = true;
    txtOpts.includeMetadata = true;
    bool txtOk = getFrameCapture().saveFrame(frame, txtPath, txtOpts);

    CaptureOptions ansiOpts;
    ansiOpts.format = CaptureFormat::AnsiEscapes;
    ansiOpts.addTimestamp = true;
    ansiOpts.includeMetadata = true;
    bool ansiOk = getFrameCapture().saveFrame(frame, ansiPath, ansiOpts);

    if (showDialog) {
        if (txtOk || ansiOk) {
            std::stringstream msg;
            msg << "Saved capture:";
            if (txtOk) msg << " " << txtPath;
            if (ansiOk) msg << " " << ansiPath;
            messageBox(msg.str().c_str(), mfInformation | mfOKButton);
        } else {
            messageBox("Capture failed (screen buffer paths failed).", mfError | mfOKButton);
        }
    }
}

// Custom monochrome palette with reversed main areas
// Palette indices:
// 0-7:   Desktop/Background (black background with pattern)
// 8-15:  Window frame colors 
// 16-23: Cyan window (alternate)
// 24-31: Gray window (alternate)
// 32-39: Dialog colors
// 40-47: Menu colors (reversed to black bg)
// 48-55: Status line colors (reversed to black bg)
// 56-63: Help colors
// 64+:   Additional elements

#define cpMonochrome \
    "\x70\x70\x0F\x07\x70\x70\x70\x07\x0F\x07\x07\x07\x70\x07\x0F" \
    "\x70\x0F\x70\x07\x07\x70\x07\x0F\x70\x7F\x7F\x70\x07\x70\x07\x0F" \
    "\x70\x7F\x7F\x70\x07\x70\x70\x7F\x7F\x07\x70\x0F\x70\x0F\x70\x07" \
    "\x0F\x0F\x0F\x70\x0F\x07\x70\x70\x70\x07\x70\x0F\x07\x07\x78\x00" \
    "\x70\xF0\x0F\x70\x07\x70\x70\x0F\x0F\x07\xF0\x7F\x08\x7F\xF0\x70" \
    "\x7F\x7F\x7F\x0F\x70\x70\x07\x70\x70\x70\x07\x7F\x70\x07\x08\x00" \
    "\x70\x7F\x7F\x70\x07\x70\x70\x7F\x7F\x07\x0F\x0F\x78\x0F\x78\x07" \
    "\x0F\x0F\x0F\x70\x0F\x07\x70\x70\x70\x07\x70\x0F\x07\x07\x78\x00" \
    "\x07\x0F\x07\x70\x70\x07\x0F\x70"

// Chrome variant flag lives in ThemeManager (single source; TCGAFrame reads it too).
#include "theme_manager.h"

TPalette& TWwdosApp::getPalette() const
{
    static TPalette mono(cpMonochrome, sizeof(cpMonochrome)-1);
    static TPalette cga(cpAppColor, sizeof(cpAppColor)-1);
    if (!ThemeManager::cgaChrome()) return mono;

    // Skin-aware chrome: dark skins patch the classic app palette so window
    // frames, scrollbars and menus match the room instead of wearing the
    // daylight slate everywhere (Zilla, 2026-08-12).
    const CgaSkin* sk = findCgaSkin(ThemeManager::activeSkin());
    if (!sk || sk->framePassive < 0) return cga;

    static TPalette skinned(cpAppColor, sizeof(cpAppColor)-1);
    static std::string patchedFor;
    if (patchedFor != sk->name) {
        char buf[sizeof(cpAppColor)];
        std::memcpy(buf, cpAppColor, sizeof(cpAppColor));  // incl. NUL
        auto put = [&](int entry, int attr) {
            // palette entries are 1-based positions in the attr string
            if (entry >= 1 && entry < (int)sizeof(cpAppColor) - 0)
                buf[entry - 1] = (char)attr;
        };
        int fp = ThemeManager::bios(SkinRole::FramePassive);
        int fa = ThemeManager::bios(SkinRole::FrameActive);
        int mn = ThemeManager::bios(SkinRole::Bar);
        int mnSel = ThemeManager::bios(SkinRole::BarSel);
        // menus + status line (entries 2..7)
        for (int e = 2; e <= 7; ++e) put(e, (e == 5 || e == 6) ? mnSel : mn);
        // three window palettes + gray dialog: frame passive/active/icon,
        // scrollbar page/controls
        const int groups[4] = { 8, 16, 24, 32 };
        for (int g : groups) {
            put(g + 0, fp);        // frame passive
            put(g + 1, fa);        // frame active
            put(g + 2, fa);        // frame icon
            put(g + 3, fp);        // scrollbar page
            put(g + 4, fa);        // scrollbar controls
        }
        skinned = TPalette(buf, sizeof(cpAppColor)-1);
        patchedFor = sk->name;
    }
    return skinned;
}

// Build "FIGlet ~F~ont ▶ { categories... | More Fonts... }" submenu item
// for the Edit menu bar. Returns a TSubMenu& that can be appended with +.



void TWwdosApp::run()
{
    // Call parent run to initialize everything first
    TApplication::run();
}

TRect TWwdosApp::calculateWindowBounds(const std::string& filePath)
{
    // If the file contains animation frame delimiters, size to the
    // largest frame (width/height). Otherwise, size to full text
    // dimensions (longest line, total lines).
    auto capToDesktop = [&](int &w, int &h) {
        TRect screenBounds = deskTop->getExtent();
        int screenWidth = screenBounds.b.x;
        int screenHeight = screenBounds.b.y;
        // Hard caps: allow max width to use full desktop width; keep height within desktop.
        if (w > screenWidth) w = screenWidth;
        if (h > screenHeight - 2) h = screenHeight - 2; // never taller than app
        // Minimum sensible size
        if (w < 20) w = 20;
        if (h < 5) h = 5;
    };

    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        int ww = 50, hh = 15;
        capToDesktop(ww, hh);
        return TRect(2, 1, 2 + ww, 1 + hh);
    }

    const std::string delim = "----";
    bool treatAsAnimation = hasFrameDelimiters(filePath);

    int maxWidth = 0;
    int maxHeight = 0;

    if (treatAsAnimation) {
        // Track width/height per frame; split on exact delimiter lines (CR before LF allowed).
        std::string line;
        int curHeight = 0;
        int curWidthMax = 0;
        auto commitFrame = [&]() {
            if (curHeight > 0 || curWidthMax > 0) {
                if (curWidthMax > maxWidth) maxWidth = curWidthMax;
                if (curHeight > maxHeight) maxHeight = curHeight;
            }
            curHeight = 0;
            curWidthMax = 0;
        };
        while (std::getline(file, line)) {
            // Trim trailing CR
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line == delim) {
                commitFrame();
                continue;
            }
            // Measure display columns (UTF-8 aware) instead of byte length.
            int lineWidth = (int)TText::width(TStringView(line.c_str(), line.size()));
            if (lineWidth > curWidthMax) curWidthMax = lineWidth;
            curHeight++;
        }
        commitFrame();
        // Fallback: if no delimiters in content (edge case), use collected totals
        if (maxHeight == 0 && maxWidth == 0) {
            // Treat whole file as one frame
            file.clear();
            file.seekg(0);
            int h = 0, w = 0;
            while (std::getline(file, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                w = std::max(w, (int)TText::width(TStringView(line.c_str(), line.size())));
                h++;
            }
            maxWidth = w;
            maxHeight = h;
        }
    } else {
        // Plain text: longest line and total line count
        std::string line;
        int height = 0;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            int lineWidth = (int)TText::width(TStringView(line.c_str(), line.size()));
            if (lineWidth > maxWidth) maxWidth = lineWidth;
            height++;
        }
        maxHeight = height;
    }
    file.close();

    // Add padding for window frame (borders): +2 width, +2 height
    int windowWidth = maxWidth + 2;
    int windowHeight = maxHeight + 2;
    capToDesktop(windowWidth, windowHeight);

    // Spread placement: least-overlap spot instead of always centring
    // (centring stacked every window on top of the previous one).
    return findSpreadRect(windowWidth, windowHeight);
}

TRect TWwdosApp::findSpreadRect(int w, int h)
{
    TRect ext = deskTop->getExtent();
    const int SW = ext.b.x, SH = ext.b.y;
    if (w > SW) w = SW;
    if (h > SH) h = SH;

    // Collect visible window rects on the desktop.
    std::vector<TRect> occupied;
    if (TView* start = deskTop->first()) {
        TView* v = start;
        do {
            if ((v->state & sfVisible) && v->size.x > 2 && v->size.y > 2) {
                TRect r(v->origin.x, v->origin.y,
                        v->origin.x + v->size.x, v->origin.y + v->size.y);
                occupied.push_back(r);
            }
            v = v->next;
        } while (v != start);
    }

    // Empty desktop → centre.
    if (occupied.empty()) {
        int x = std::max(0, (SW - w) / 2);
        int y = std::max(0, (SH - h) / 2);
        return TRect(x, y, x + w, y + h);
    }

    // Grid-scan candidate positions; score = total overlap area with existing
    // windows. Ties broken by distance from the centroid of existing windows
    // (farther = better) so zero-overlap spots spread out rather than cluster.
    long cx = 0, cy = 0;
    for (const auto& r : occupied) { cx += (r.a.x + r.b.x) / 2; cy += (r.a.y + r.b.y) / 2; }
    cx /= (long)occupied.size(); cy /= (long)occupied.size();

    const int stepX = 4, stepY = 2;
    long bestScore = -1; long bestDist = -1; int bestX = 0, bestY = 0;
    for (int y = 0; y <= SH - h; y += stepY) {
        for (int x = 0; x <= SW - w; x += stepX) {
            long overlap = 0;
            for (const auto& r : occupied) {
                int ox = std::min(x + w, (int)r.b.x) - std::max(x, (int)r.a.x);
                int oy = std::min(y + h, (int)r.b.y) - std::max(y, (int)r.a.y);
                if (ox > 0 && oy > 0) overlap += (long)ox * oy;
            }
            long dx = (x + w / 2) - cx, dy = ((y + h / 2) - cy) * 2; // cell aspect ≈ 2
            long dist = dx * dx + dy * dy;
            if (bestScore < 0 || overlap < bestScore ||
                (overlap == bestScore && dist > bestDist)) {
                bestScore = overlap; bestDist = dist; bestX = x; bestY = y;
            }
        }
    }
    return TRect(bestX, bestY, bestX + w, bestY + h);
}

void TWwdosApp::idle()
{
    TApplication::idle();

    // Poll IPC server for incoming API commands
    if (ipcServer) ipcServer->poll();

    // Screensaver idle check (lastInputMs_ seeds at first idle pass)
    if (lastInputMs_ == 0) lastInputMs_ = wwNowMs();
    if (saverTimeoutMins_ > 0 && !saverWin_ &&
        wwNowMs() - lastInputMs_ > (long long)saverTimeoutMins_ * 60000LL)
        activateScreensaver();
    // Poll Scramble async LLM calls (non-blocking)
    scrambleEngine.poll();

    // DISABLED: Update animated kaomoji in menu bar (causing crashes + freezes)
    // if (menuBar) {
    //     auto* customMenuBar = dynamic_cast<TCustomMenuBar*>(menuBar);
    //     if (customMenuBar) {
    //         customMenuBar->update();
    //     }
    // }

    // Broadcast terminal update check so tvterm windows refresh
    message(this, evBroadcast, TWibWobTerminalWindow::termConsts.cmCheckTerminalUpdates, nullptr);
}

int main()
{
    // Detect auth mode once before any LLM consumer initialises.
    AuthConfig::instance().detect();

    TWwdosApp app;
    app.run();
    return 0;
}

// ---- IPC API helper functions (friend) ----
// Backward compatibility overloads

// New overloads with bounds support


















// ww_get_child_view lives in ww_view_utils.h (split stage 2)

// Set the text colour (CGA/ANSI index 0-15, -1 = auto) of a viewer window.

// Set the solid background colour (CGA/ANSI index 0-15) of a viewer window.









// --- Minimal JSON parsing helpers (subset tailored to our schema) ---







// ── Workspace layout preview ──────────────────────────────────────────────────
// Renders a miniature ASCII wireframe of window positions from workspace JSON.













// Generative / animated art windows — spawnable via IPC create_window type=X

// In-process command execution for views (disk library boots its floppies
// through this). Main thread only.


void TWwdosApp::activateScreensaver()
{
    if (saverWin_) return;
    // random resident, seeded by the clock
    int idx = (int)(wwNowMs() / 1000 % (long long)shaderCount());
    TRect r = deskTop->getExtent();
    TWindow* w = createTweetShaderWindow(r, shaderName(idx));
    w->flags = 0;                       // no move/grow/close — input wakes instead
    deskTop->insert(w);
    saverWin_ = w;
    fprintf(stderr, "[saver] on (%s)\n", shaderName(idx));
}

void TWwdosApp::dismissScreensaver()
{
    if (!saverWin_) return;
    TWindow* w = saverWin_;
    saverWin_ = nullptr;
    w->flags |= wfClose;
    w->close();
    fprintf(stderr, "[saver] off\n");
}

// action: now | off | on (arm) ; minutes > 0 sets the timeout (0 = disable)


















// Overload: with explicit channel (for API/IPC — no dialog)

// Original signature for backward compat (registry dispatch calls this)










// ── FIGlet text window spawn + control API ────────────────────────────────────


















// --- Desktop texture, colour & gallery mode ---






// ── Terminal palette programming (OSC 4) — the DOS DAC registers ─────
// Indexed colours (0x07-style attrs everywhere) render through the
// TERMINAL's 16 ANSI slots — Ghostty's "black" is #2A2A2A, which is why
// legacy views looked grey on dark skins no matter what we patched.
// The canon fix: set_skin reprograms the slots to authentic CGA RGB
// (mono skins get luminance-mapped ramps — swapping the monitor, not
// the app). Every indexed draw in every view then obeys the skin.


// Apply a named CGA skin preset in one shot: chrome variant + solid shadows,
// desktop texture + colour (authentic CGA RGB), and default paper colours on
// every colourable window. Dialog-accent colours stay per-window calls
// (set_window_bg/fg) so scenes can mix paper and dialogs like the refs.
// "off"/"monochrome" restores the house grey chrome.







// Paint wrappers for command_registry (avoids tvision include dependency)
// Clamp paint coordinates to canvas bounds to prevent crashes











