/*---------------------------------------------------------*/
/*                                                         */
/*   room_chat_view.h — Multi-user PartyKit room chat     */
/*                                                         */
/*   Two (or more) humans share a WibWob-DOS room.        */
/*   They talk here. No AI coupling — agents join later   */
/*   as plain participants with a different sender label.  */
/*                                                         */
/*---------------------------------------------------------*/

#ifndef ROOM_CHAT_VIEW_H
#define ROOM_CHAT_VIEW_H

#define Uses_TView
#define Uses_TRect
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TDrawBuffer
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TScrollBar
#define Uses_TScroller
#define Uses_TGroup
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TWindowInit
#include <tvision/tv.h>

#include <string>
#include <vector>

// ── Command constants ──────────────────────────────────────────────────────
const ushort cmRoomChat        = 182;  // open RoomChatWindow
const ushort cmRoomChatReceive = 183;  // IPC: incoming message
const ushort cmRoomChatSend    = 184;  // user submitted input
const ushort cmRoomPresence    = 185;  // IPC: presence update

// ── Data structures ────────────────────────────────────────────────────────

struct RoomChatMessage {
    std::string sender;   // e.g. "human:1", "human:2", "wib", "system"
    std::string text;
    std::string ts;       // HH:MM
};

struct RoomParticipant {
    std::string id;       // e.g. "human:1"
    std::string name;     // display name (defaults to id)
};

// ── TRoomParticipantStrip ──────────────────────────────────────────────────
// Left panel: who's in the room.
// Draws one "● name" line per participant + participant count at bottom.

class TRoomParticipantStrip : public TView {
public:
    explicit TRoomParticipantStrip(const TRect& bounds);

    virtual void draw() override;

    void setParticipants(const std::vector<RoomParticipant>& p);

private:
    std::vector<RoomParticipant> participants_;
};

// ── TRoomMessageView ───────────────────────────────────────────────────────
// Right panel: scrolling message log.
// Each entry: "[HH:MM] sender  text" — sender coloured by hash.

struct WrappedRoomLine {
    std::string text;
    bool isSenderLine = false;
    std::string sender;   // carried for colour lookup
};

class TRoomMessageView : public TScroller {
public:
    TRoomMessageView(const TRect& bounds, TScrollBar* vScroll);

    virtual void draw() override;
    virtual void changeBounds(const TRect& bounds) override;

    void addMessage(const RoomChatMessage& msg);
    void scrollToBottom();

private:
    std::vector<RoomChatMessage> messages_;
    std::vector<WrappedRoomLine> lines_;

    void rebuild();
};

// ── TRoomChatWindow ────────────────────────────────────────────────────────
// The full window.  Owns the strip, the scroller, and a one-line input.
// Handles cmRoomChatReceive and cmRoomPresence from IPC,
// and cmRoomChatSend from its own input line.

class TRoomChatWindow : public TWindow {
public:
    TRoomChatWindow(const TRect& bounds, const char* title, int num);
    virtual ~TRoomChatWindow();

    virtual void handleEvent(TEvent& event) override;

    // Called by IPC handlers
    void receiveMessage(const RoomChatMessage& msg);
    void updatePresence(const std::vector<RoomParticipant>& participants);

    // Pending outbound messages — bridge polls via room_chat_pending IPC
    std::vector<std::string> pendingOutbound;
    std::vector<std::string> drainPending();  // returns + clears queue

private:
    TRoomParticipantStrip* strip_ = nullptr;
    TRoomMessageView*      msgView_ = nullptr;

    // Simple inline input state (no TInputLine — avoids focus complexities)
    std::string inputBuf_;
    TView*      inputView_ = nullptr;

    void drawInput();
};

// ── Factory ────────────────────────────────────────────────────────────────
TWindow* createRoomChatWindow(const TRect& bounds);

// ── Global accessor ────────────────────────────────────────────────────────
// Returns the most-recently-created live TRoomChatWindow, or nullptr.
// Cleared automatically when the window is destroyed.
TRoomChatWindow* getRoomChatWindow();

#endif // ROOM_CHAT_VIEW_H
