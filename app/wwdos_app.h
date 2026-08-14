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
// Screensaver default idle timeout, minutes. SINGLE SOURCE — the member
// default below and api_screensaver's re-arm branch both drink from here
// (Zilla 2026-08-14: 10 was firing mid-work, annoying; 0 disables).
constexpr int kDefaultSaverTimeoutMins = 20;

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

    // ── API surface (stage 4 friend-ectomy) ─────────────────────────────
    // The members below were private and reached only via the ~60
    // `friend api_*` declarations that used to sit at the bottom of this
    // class. Now that the api_* free functions in wwdos_app.cpp are no
    // longer friends (bar the one survivor below), they call these public
    // members/accessors instead. Signatures of the api_* functions are
    // unchanged — only their bodies moved from raw private-member access
    // to this surface.

    void newTestWindow();
    void newTestWindow(const TRect& bounds);
    void newGradientWindow(TGradientWindow::GradientType type);
    void newGradientWindow(TGradientWindow::GradientType type, const TRect& bounds);
    void newBrowserWindow();
    void newBrowserWindow(const TRect& bounds);
    void openAnimationFilePath(const std::string& path);
    void openAnimationFilePath(const std::string& path, const TRect& bounds, bool frameless = false, bool shadowless = false, const std::string& title = "");
    bool openWorkspacePath(const std::string& path);
    void cascade();
    void tile();
    void closeAll();
    void takeScreenshot(bool showDialog = true);
    bool saveWorkspacePath(const std::string& path);
    TRect calculateWindowBounds(const std::string& filePath);
    // Place a w×h window where it overlaps existing windows the least, so
    // successive spawns spread across the desktop instead of stacking.
    TRect findSpreadRect(int w, int h);

    // Window registry (winToId/idToWin) — register/find/forget by id.
    std::string registerWindow(TWindow* w, bool emit_event = true);
    TWindow* findWindowById(const std::string& id);
    // Drop a single window from the id registry (used by api_close_window).
    void forgetWindow(TWindow* w, const std::string& id) {
        winToId.erase(w);
        idToWin.erase(id);
    }
    // Purge registry entries for windows no longer present in activeWins,
    // without reassigning ids for windows that are still alive.
    void syncWindowRegistry(const std::vector<TWindow*>& activeWins) {
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
    // Read-only view of the id→window map (e.g. direct lookup without the
    // registry-sync side effects that findWindowById performs).
    const std::map<std::string, TWindow*>& windowIds() const { return idToWin; }
    // windowNumber++ as a single call — returns the post-increment value.
    int nextWindowNumber() { return ++windowNumber; }
    // Consume (read + clear) the id of the most recently registered window.
    std::string takeLastRegisteredWindowId() {
        std::string out = lastRegisteredWindowId_;
        lastRegisteredWindowId_.clear();
        return out;
    }
    // Fire-and-forget IPC event publish; no-ops if the server isn't up.
    void publishEvent(const char* name, const std::string& payload) {
        if (ipcServer) ipcServer->publish_event(name, payload);
    }

    // Scramble cat overlay accessors.
    ScrambleEngine& scramble() { return scrambleEngine; }
    TScrambleWindow* scrambleWin() const { return scrambleWindow; }
    void setPendingScrambleReply(const std::string& s) { pendingScrambleReply = s; }
    void cycleScramble();
    void deliverScrambleReply();

    // Chat log for multiplayer relay (outgoing messages from local Scramble).
    struct ChatEntry { int seq; std::string sender; std::string text; };
    std::deque<ChatEntry>& chatLog() { return chatLog_; }

    // Desktop texture & gallery mode.
    bool galleryMode() const { return galleryMode_; }
    void setGalleryMode(bool on) { galleryMode_ = on; }

    // Screensaver.
    void activateScreensaver();
    void dismissScreensaver();
    int saverTimeoutMins() const { return saverTimeoutMins_; }
    void setSaverTimeoutMins(int mins) { saverTimeoutMins_ = mins; }
    void noteInput() { lastInputMs_ = wwNowMs(); }

private:
    // void newMechWindow();
    void newDonutWindow();
    void newWibWobWindow();
    void newWibWobTestWindowA();
    void newWibWobTestWindowB();
    void newWibWobTestWindowC();
    void newRoomChatWindow();
    void openAnimationFile();
    void openTransparentTextFile();
    void openMonodrawFile(const char* fileName);
    void openWorkspace();
    void showApiKeyDialog();
    void saveWorkspace();
    void saveWorkspaceAs();
    void manageWorkspaces();
    void setPatternMode(bool continuous);
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
    void wireScrambleInput();

    // Runtime API key (shared across all chat windows)
    static std::string runtimeApiKey;
    std::vector<std::string> recentWorkspaces_;
    std::string currentWorkspacePath_;  // path of last loaded/saved workspace
    friend std::string getAppRuntimeApiKey();

    // Chat log for multiplayer relay (outgoing messages from local Scramble)
    std::deque<ChatEntry> chatLog_;
    int chatSeq_ = 0;
    static constexpr int kChatLogMax = 50;

    // API/IPC registry for per-window control
    int apiIdCounter = 1;
    std::map<TWindow*, std::string> winToId;
    std::map<std::string, TWindow*> idToWin;
    std::string lastRegisteredWindowId_;

    // IPC server
    ApiIpcServer* ipcServer = nullptr;

    bool galleryMode_ = false;

    // ── screensaver: idle timeout → fullscreen shader, any input wakes ──
    TWindow* saverWin_ = nullptr;
    long long lastInputMs_ = 0;
    int saverTimeoutMins_ = kDefaultSaverTimeoutMins;   // 0 disables
    static long long wwNowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};

#endif // WWDOS_APP_H
