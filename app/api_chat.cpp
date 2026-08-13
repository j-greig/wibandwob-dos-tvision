/*---------------------------------------------------------*/
/*                                                         */
/*   api_chat.cpp - Scramble cat / Wib&Wob chat /            */
/*   room-chat / terminal api_* bridge functions.            */
/*   Moved verbatim from wwdos_app.cpp                      */
/*   (monolith split stage 6d).                               */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TView
#define Uses_TWindow
#define Uses_TDeskTop
#define Uses_TEvent
#define Uses_TProgram
#define Uses_TRect
#include <tvision/tv.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

#include "api_chat.h"
#include "api_windows.h"
#include "wwdos_app.h"
#include "wwdos_commands.h"
#include "scramble_view.h"
#include "scramble_engine.h"
#include "wibwob_view.h"
#include "room_chat_view.h"
#include "tvterm_view.h"

void api_toggle_scramble(TWwdosApp& app) { app.cycleScramble(); }

void api_expand_scramble(TWwdosApp& app) { app.cycleScramble(); }

std::string api_scramble_say(TWwdosApp& app, const std::string& text) {
    if (!app.scrambleWin()) return "err scramble not open";
    // Simulate user sending a message — same as onSubmit
    auto* msgView = app.scrambleWin()->getMessageView();
    if (msgView) msgView->addMessage("you", text);

    // Use async path — response arrives via cmScrambleReply event
    std::string syncResult;
    bool isAsync = app.scramble().askAsync(text, syncResult,
        [&app](const std::string& response) {
            // Queue for event-loop delivery (never drawView from idle)
            app.setPendingScrambleReply(response.empty() ? "(no response from model — try again)" : response);
            TEvent event;
            event.what = evCommand;
            event.message.command = cmScrambleReply;
            event.message.infoPtr = nullptr;
            app.putEvent(event);
        });

    if (!isAsync) {
        // Slash command or fallback — deliver immediately
        app.setPendingScrambleReply(syncResult.empty() ? "(no response from model — try again)" : syncResult);
        app.deliverScrambleReply();
        return syncResult;
    }

    return "*thinking* /ᐠ｡ꞈ｡ᐟ\\";  // Async started — response will arrive via event
}

std::string api_scramble_pet(TWwdosApp& app) {
    if (!app.scrambleWin()) return "err scramble not open";

    static const char* petReactions[] = {
        "...fine. /ᐠ- -ᐟ\\",
        "*allows it* (=^..^=)",
        "adequate petting technique. /ᐠ｡ꞈ｡ᐟ\\",
        "i did not ask for this. and yet. (=^..^=)",
        "*purrs once. stops. stares* /ᐠ°ᆽ°ᐟ\\",
    };
    std::string response = petReactions[std::rand() % 5];

    if (app.scrambleWin()->getView()) {
        app.scrambleWin()->getView()->setPose(spDefault);
        app.scrambleWin()->getView()->say(response);
    }
    auto* msgView = app.scrambleWin()->getMessageView();
    if (msgView) msgView->addMessage("scramble", response);
    return response;
}

std::string api_chat_receive(TWwdosApp& app, const std::string& sender, const std::string& text) {
    // Display a remote chat message in Scramble without AI processing.
    if (!app.scrambleWin()) return "err scramble not open";
    auto* msgView = app.scrambleWin()->getMessageView();
    if (!msgView) return "err no message view";
    msgView->addMessage(sender, text);
    return "ok";
}

static TWibWobWindow* findTargetWibWobWindow(TWwdosApp& app) {
    if (!app.deskTop) return nullptr;

    // Prefer the focused Wib&Wob window so ask/history operate on the same UI.
    if (auto* ww = dynamic_cast<TWibWobWindow*>(app.deskTop->current)) {
        return ww;
    }

    // Fallback: first Wib&Wob window on the desktop.
    TView* v = app.deskTop->first();
    if (!v) return nullptr;
    TView* start = v;
    do {
        if (auto* ww = dynamic_cast<TWibWobWindow*>(v)) {
            return ww;
        }
        v = v->next;
    } while (v != start);
    return nullptr;
}

std::string api_wibwob_ask(TWwdosApp& app, const std::string& text) {
    // Inject a user message into the Wib&Wob chat window, triggering LLM response.
    fprintf(stderr, "[wibwob_ask] called, text_len=%zu text=%.60s\n",
            text.size(), text.c_str());
    // Find the target Wib&Wob window (prefer current/focused).
    TWibWobWindow* chatWin = nullptr;
    int windowCount = 0;
    if (app.deskTop) {
        TView* v = app.deskTop->first();
        if (v) {
            TView* start = v;
            do {
                windowCount++;
                if (!chatWin) chatWin = dynamic_cast<TWibWobWindow*>(v);
                v = v->next;
            } while (v != start);
        }
    }
    if (auto* preferred = findTargetWibWobWindow(app)) {
        chatWin = preferred;
    }
    if (!chatWin) {
        fprintf(stderr, "[wibwob_ask] ERROR: no TWibWobWindow found (%d views on desktop)\n", windowCount);
        return "err no wibwob chat window open";
    }
    fprintf(stderr, "[wibwob_ask] target chatWin=%p, sending window event\n", (void*)chatWin);
    // Send to the chosen Wib&Wob window only. ApiIpcServer::poll() runs on the
    // Turbo Vision thread, so direct delivery is safe and avoids cross-window drift.
    static std::string pendingAsk;
    pendingAsk = text;
    message(chatWin, evBroadcast, 0xF0F0, &pendingAsk); // cmWibWobAskPending
    return "ok queued";
}

std::string api_get_chat_history(TWwdosApp& app) {
    // Use the same target selection as api_wibwob_ask() to avoid reading the
    // history from a different Wib&Wob window than the one that processed input.
    TWibWobWindow* chatWin = findTargetWibWobWindow(app);
    if (!chatWin) return "err no wibwob chat window open";
    auto* msgView = chatWin->getMessageView();
    if (!msgView) return "err no wibwob message view";
    return std::string("{\"messages\":") + msgView->getHistoryJson() + "}";
}

void api_spawn_room_chat(TWwdosApp& app, const TRect* bounds) {
    TRect r;
    if (bounds) {
        r = *bounds;
    } else {
        TRect d = app.deskTop->getExtent();
        int dw = d.b.x - d.a.x;
        int dh = d.b.y - d.a.y;
        int width = std::max(10, std::min(100, dw));
        int height = std::max(6, std::min(28, dh));
        int left = d.a.x + (dw - width) / 2;
        int top = d.a.y + (dh - height) / 2;
        r = TRect(left, top, left + width, top + height);
    }
    TWindow* window = createRoomChatWindow(r);
    app.deskTop->insert(window);
    app.registerWindow(window);
}

std::string api_room_chat_receive(TWwdosApp& /*app*/,
                                  const std::string& sender,
                                  const std::string& text,
                                  const std::string& ts) {
    TRoomChatWindow* win = getRoomChatWindow();
    if (!win) return "err no room_chat window open";

    RoomChatMessage msg;
    msg.sender = sender;
    msg.text = text;
    msg.ts = ts.empty() ? "??:??" : ts;

    TEvent ev;
    ev.what = evCommand;
    ev.message.command = cmRoomChatReceive;
    ev.message.infoPtr = new RoomChatMessage(msg);
    TProgram::application->putEvent(ev);
    return "ok";
}

std::string api_room_presence(TWwdosApp& /*app*/,
                              const std::string& participants_json) {
    TRoomChatWindow* win = getRoomChatWindow();
    if (!win) return "err no room_chat window open";

    auto* participants = new std::vector<RoomParticipant>();
    std::string json = participants_json;
    size_t pos = 0;
    while ((pos = json.find("\"id\"", pos)) != std::string::npos) {
        pos = json.find(':', pos);
        if (pos == std::string::npos) break;
        pos = json.find('"', pos);
        if (pos == std::string::npos) break;
        size_t start = pos + 1;
        size_t end = json.find('"', start);
        if (end == std::string::npos) break;
        RoomParticipant p;
        p.id = json.substr(start, end - start);
        p.name = p.id;
        size_t nameField = json.find("\"name\"", end);
        size_t nextEntry = json.find('{', end);
        if (nameField != std::string::npos &&
            (nextEntry == std::string::npos || nameField < nextEntry)) {
            size_t npos2 = json.find(':', nameField);
            npos2 = json.find('"', npos2);
            if (npos2 != std::string::npos) {
                size_t nstart = npos2 + 1;
                size_t nend = json.find('"', nstart);
                if (nend != std::string::npos)
                    p.name = json.substr(nstart, nend - nstart);
            }
        }
        participants->push_back(p);
        pos = end + 1;
    }

    TEvent ev;
    ev.what = evCommand;
    ev.message.command = cmRoomPresence;
    ev.message.infoPtr = participants;
    TProgram::application->putEvent(ev);
    return "ok";
}

std::string api_get_room_chat_pending(TWwdosApp& /*app*/) {
    TRoomChatWindow* win = getRoomChatWindow();
    if (!win) return "[]";
    auto msgs = win->drainPending();
    std::string json = "[";
    for (size_t i = 0; i < msgs.size(); ++i) {
        if (i) json += ",";
        json += "\"";
        for (char c : msgs[i]) {
            if (c == '"') json += "\\\"";
            else if (c == '\\') json += "\\\\";
            else if (c == '\n') json += "\\n";
            else json += c;
        }
        json += "\"";
    }
    json += "]";
    return json;
}

std::string api_get_room_chat_display_name(TWwdosApp& /*app*/) {
    TRoomChatWindow* win = getRoomChatWindow();
    if (!win) return "";
    return win->getDisplayName();
}

static TWibWobTerminalWindow* find_terminal_by_zorder(TWwdosApp& app) {
    TView* start = app.deskTop->first();
    if (!start) return nullptr;
    TView* v = start;
    do {
        if (auto *tw = dynamic_cast<TWibWobTerminalWindow*>(v))
            return tw;
        v = v->next;
    } while (v != start);
    return nullptr;
}

std::string api_terminal_write(TWwdosApp& app, const std::string& text, const std::string& window_id) {
    TWibWobTerminalWindow* termWin = nullptr;
    if (!window_id.empty()) {
        const auto& ids = app.windowIds();
        auto it = ids.find(window_id);
        if (it != ids.end())
            termWin = dynamic_cast<TWibWobTerminalWindow*>(it->second);
        if (!termWin) return "err window not found or not a terminal";
    } else {
        termWin = find_terminal_by_zorder(app);
    }
    if (!termWin) return "err no terminal window";
    termWin->sendText(text);
    return "ok";
}

std::string api_terminal_read(TWwdosApp& app, const std::string& window_id) {
    TWibWobTerminalWindow* termWin = nullptr;
    if (!window_id.empty()) {
        const auto& ids = app.windowIds();
        auto it = ids.find(window_id);
        if (it != ids.end())
            termWin = dynamic_cast<TWibWobTerminalWindow*>(it->second);
        if (!termWin) return "err window not found or not a terminal";
    } else {
        termWin = find_terminal_by_zorder(app);
    }
    if (!termWin) return "err no terminal window";
    return termWin->getOutputText();
}
