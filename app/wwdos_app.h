/*---------------------------------------------------------*/
/*                                                         */
/*   wwdos_app.h - TWwdosApp class declaration              */
/*   Extracted verbatim from wwdos_app.cpp (monolith split  */
/*   stage 3) — zero behaviour change, proves the include   */
/*   set closes.                                            */
/*                                                         */
/*---------------------------------------------------------*/

#ifndef WWDOS_APP_H
#define WWDOS_APP_H

#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TView
#include <tvision/tv.h>

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <chrono>

#include "api_ipc.h"
#include "scramble_engine.h"
#include "scramble_view.h"
#include "windows/pattern_windows.h"

// Forward declarations for types referenced only as friend-function
// parameter/return types (pointers) — avoids pulling in their full headers.
class TGenerativeLabView;
class TPaintCanvasView;
struct BackroomsChannel;

/*---------------------------------------------------------*/
/* TWwdosApp - Main application class               */
/*---------------------------------------------------------*/
class TWwdosApp : public TApplication
{
public:
    TWwdosApp();
    virtual void handleEvent(TEvent& event);
    virtual void idle();
    virtual void run();
    virtual TPalette& getPalette() const;
    static TMenuBar* initMenuBar(TRect);
    static TStatusLine* initStatusLine(TRect);
    static TDeskTop* initDeskTop(TRect);

    /// Connection status for the API indicator in the status line.
    ApiIpcServer::ConnectionStatus getIpcStatus() const {
        return ipcServer ? ipcServer->getConnectionStatus() : ApiIpcServer::ConnectionStatus{};
    }

private:
    void newTestWindow();
    void newTestWindow(const TRect& bounds);
    void newGradientWindow(TGradientWindow::GradientType type);
    void newGradientWindow(TGradientWindow::GradientType type, const TRect& bounds);
    // void newMechWindow();
    void newDonutWindow();
    void newBrowserWindow();
    void newBrowserWindow(const TRect& bounds);
    void newWibWobWindow();
    void newWibWobTestWindowA();
    void newWibWobTestWindowB();
    void newWibWobTestWindowC();
    void newRoomChatWindow();
    void openAnimationFile();
    void openAnimationFilePath(const std::string& path);
    void openAnimationFilePath(const std::string& path, const TRect& bounds, bool frameless = false, bool shadowless = false, const std::string& title = "");
    void openTransparentTextFile();
    void openMonodrawFile(const char* fileName);
    void openWorkspace();
    bool openWorkspacePath(const std::string& path);
    void cascade();
    void tile();
    void closeAll();
    void takeScreenshot(bool showDialog = true);
    void setPatternMode(bool continuous);
    void showApiKeyDialog();
    void saveWorkspace();
    void saveWorkspaceAs();
    void manageWorkspaces();
    bool saveWorkspacePath(const std::string& path);
    TRect calculateWindowBounds(const std::string& filePath);
    // Place a w×h window where it overlaps existing windows the least, so
    // successive spawns spread across the desktop instead of stacking.
    TRect findSpreadRect(int w, int h);
    std::string buildWorkspaceJson();
    bool loadWorkspaceFromFile(const std::string& path);
public:
    // JSON parse helpers (public for workspace preview + free functions)
    static bool parseBool(const std::string &s, size_t &pos, bool &out);
    static bool parseString(const std::string &s, size_t &pos, std::string &out);
    static bool parseNumber(const std::string &s, size_t &pos, int &out);
    static void skipWs(const std::string &s, size_t &pos);
    static bool consume(const std::string &s, size_t &pos, char ch);
    static bool parseKeyedString(const std::string &s, size_t objStart, const char *key, std::string &out);
    static bool parseKeyedNumber(const std::string &s, size_t objStart, const char *key, int &out);
    static bool parseKeyedBool(const std::string &s, size_t objStart, const char *key, bool &out);
    static bool parseBounds(const std::string &s, size_t objStart, int &x,int &y,int &w,int &h);
private:

    int windowNumber;
    static const int maxWindows = 99;

    // Scramble cat overlay
    TScrambleWindow* scrambleWindow;
    ScrambleEngine scrambleEngine;
    std::string pendingScrambleReply;  // Queued async response for event-loop delivery
    ScrambleDisplayState scrambleState;
    void cycleScramble();
    void wireScrambleInput();
    void deliverScrambleReply();

    // Runtime API key (shared across all chat windows)
    static std::string runtimeApiKey;
    std::vector<std::string> recentWorkspaces_;
    std::string currentWorkspacePath_;  // path of last loaded/saved workspace
    friend std::string getAppRuntimeApiKey();

    // Chat log for multiplayer relay (outgoing messages from local Scramble)
    struct ChatEntry { int seq; std::string sender; std::string text; };
    std::deque<ChatEntry> chatLog_;
    int chatSeq_ = 0;
    static constexpr int kChatLogMax = 50;

    // API/IPC registry for per-window control
    int apiIdCounter = 1;
    std::map<TWindow*, std::string> winToId;
    std::map<std::string, TWindow*> idToWin;
    std::string lastRegisteredWindowId_;

    std::string registerWindow(TWindow* w, bool emit_event = true) {
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

    TWindow* findWindowById(const std::string& id) {
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
        {
            auto it = winToId.begin();
            while (it != winToId.end()) {
                bool alive = false;
                for (auto* aw : activeWins) { if (aw == it->first) { alive = true; break; } }
                if (!alive) {
                    idToWin.erase(it->second);
                    it = winToId.erase(it);
                } else {
                    ++it;
                }
            }
        }
        auto it = idToWin.find(id);
        if (it != idToWin.end()) return it->second;
        return nullptr;
    }

    // IPC server
    ApiIpcServer* ipcServer = nullptr;

    // Friend API helper functions implemented below to bridge IPC calls.
    friend void api_spawn_test(TWwdosApp&);
    friend void api_spawn_gradient(TWwdosApp&, const std::string&);
    friend void api_open_animation_path(TWwdosApp&, const std::string&);
    friend void api_open_text_view_path(TWwdosApp&, const std::string&, const TRect* bounds);
    friend void api_spawn_test(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_gradient(TWwdosApp&, const std::string&, const TRect* bounds);
    friend void api_open_animation_path(TWwdosApp&, const std::string&, const TRect* bounds, bool frameless, bool shadowless, const std::string& title);
    friend void api_cascade(TWwdosApp&);
    friend void api_toggle_scramble(TWwdosApp&);
    friend void api_expand_scramble(TWwdosApp&);
    friend std::string api_scramble_say(TWwdosApp&, const std::string&);
    friend std::string api_scramble_pet(TWwdosApp&);
    friend std::string api_chat_receive(TWwdosApp&, const std::string&, const std::string&);
    friend void api_tile(TWwdosApp&);
    friend void api_close_all(TWwdosApp&);
    friend void api_set_pattern_mode(TWwdosApp&, const std::string&);
    friend void api_save_workspace(TWwdosApp&);
    friend bool api_save_workspace_path(TWwdosApp&, const std::string&);
    friend bool api_open_workspace_path(TWwdosApp&, const std::string&);
    friend void api_screenshot(TWwdosApp&);
    friend std::string api_get_state(TWwdosApp&);
    friend std::string api_move_window(TWwdosApp&, const std::string&, int, int);
    friend std::string api_set_window_bg(TWwdosApp&, const std::string&, int);
    friend std::string api_set_window_fg(TWwdosApp&, const std::string&, int);
    friend std::string api_resize_window(TWwdosApp&, const std::string&, int, int);
    friend std::string api_focus_window(TWwdosApp&, const std::string&);
    friend std::string api_raise_window(TWwdosApp&, const std::string&);
    friend std::string api_lower_window(TWwdosApp&, const std::string&);
    friend std::string api_close_window(TWwdosApp&, const std::string&);
    friend std::string api_get_canvas_size(TWwdosApp&);
    friend void api_spawn_text_editor(TWwdosApp&, const TRect* bounds, const std::string& title);
    friend void api_spawn_browser(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_room_chat(TWwdosApp&, const TRect* bounds);
    friend std::string api_room_chat_receive(TWwdosApp&, const std::string& sender, const std::string& text, const std::string& ts);
    friend std::string api_room_presence(TWwdosApp&, const std::string& participants_json);
    friend std::string api_get_room_chat_pending(TWwdosApp&);
    friend std::string api_get_room_chat_display_name(TWwdosApp&);
    friend std::string api_take_last_registered_window_id(TWwdosApp&);
    friend void api_spawn_disks(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_shader(TWwdosApp&, const TRect* bounds, const std::string& shader);
    friend void api_spawn_verse(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_mycelium(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_orbit(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_torus(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_cube(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_life(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_blocks(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_score(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_ascii(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_animated_gradient(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_monster_cam(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_contour_map(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_generative_lab(TWwdosApp&, const TRect* bounds);
    friend TGenerativeLabView* api_find_gen_lab_view(TWwdosApp&, const std::string&);
    friend void api_spawn_backrooms_tv(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_backrooms_tv(TWwdosApp&, const TRect* bounds, const BackroomsChannel* ch);
    friend void api_spawn_monster_verse(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_monster_portal(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_micropolis_ascii(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_quadra(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_snake(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_rogue(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_deep_signal(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_app_launcher(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_gallery(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_figlet_text(TWwdosApp&, const TRect*,
        const std::string& text, const std::string& font,
        bool frameless, bool shadowless);
    friend void api_spawn_figlet_text_at(TWwdosApp&,
        const std::string& text, const std::string& font, int x, int y,
        bool frameless, bool shadowless);
    friend std::string api_figlet_set_text(TWwdosApp&, const std::string& id, const std::string& text);
    friend std::string api_figlet_set_font(TWwdosApp&, const std::string& id, const std::string& font);
    friend std::string api_figlet_set_color(TWwdosApp&, const std::string& id, const std::string& fg, const std::string& bg);
    friend std::string api_gallery_list(TWwdosApp&, const std::string& tab);
    friend void api_spawn_terminal(TWwdosApp&, const TRect* bounds);
    friend void api_spawn_wibwob(TWwdosApp&, const TRect* bounds);
    friend std::string api_terminal_write(TWwdosApp&, const std::string& text, const std::string& window_id);
    friend std::string api_terminal_read(TWwdosApp&, const std::string& window_id);
    friend void api_spawn_paint(TWwdosApp&, const TRect* bounds);
    friend TPaintCanvasView* api_find_paint_canvas(TWwdosApp&, const std::string&);
    friend std::string api_browser_fetch(TWwdosApp&, const std::string& url);
    friend std::string api_send_text(TWwdosApp&, const std::string&, const std::string&,
                                     const std::string&, const std::string&);
    friend std::string api_send_figlet(TWwdosApp&, const std::string&, const std::string&,
                                       const std::string&, int, const std::string&);
    // Per-window toggles
    friend std::string api_window_shadow(TWwdosApp&, const std::string&, bool);
    friend std::string api_window_title(TWwdosApp&, const std::string&, const std::string&);
    // Desktop texture & gallery mode
    friend std::string api_desktop_preset(TWwdosApp&, const std::string&);
    friend std::string api_desktop_texture(TWwdosApp&, const std::string&);
    friend std::string api_desktop_color(TWwdosApp&, int, int);
    friend std::string api_desktop_gallery(TWwdosApp&, bool);
    friend std::string api_desktop_get(TWwdosApp&);
    friend std::string api_set_skin(TWwdosApp&, const std::string&);
    bool galleryMode_ = false;

    // ── screensaver: idle timeout → fullscreen shader, any input wakes ──
    friend std::string api_screensaver(TWwdosApp&, const std::string&, int);
    TWindow* saverWin_ = nullptr;
    long long lastInputMs_ = 0;
    int saverTimeoutMins_ = 10;      // 0 disables
    void activateScreensaver();
    void dismissScreensaver();
    static long long wwNowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};

#endif // WWDOS_APP_H
