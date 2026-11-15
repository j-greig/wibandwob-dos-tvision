// Resident senses: looking at the desktop without touching it.
//
// A resident (an agent living in the desktop) reads the desktop at three depths
// (glance, read, study), asks what changed since it last looked, and leaves a
// mark on the frame of the window it reads so the person can see its attention.
// No look command changes focus, z-order or content.
// Commands and limits: docs/resident-senses.md.
#ifndef RESIDENT_SENSES_H
#define RESIDENT_SENSES_H

#include "resident_input.h"

#include <cstdint>

namespace resident {

// Registry commands (see command_registry.cpp).
std::string cmdGlance(const KV& kv);    // look_glance
std::string cmdRead(const KV& kv);      // look_read
std::string cmdStudy(const KV& kv);     // look_study
std::string cmdChanges(const KV& kv);   // look_changes
std::string cmdMark(const KV& kv);      // look_mark
std::string cmdDescribe(const KV& kv);  // describe: every window says what it is (describe.h)

// Called on every pass of the event loop (from afterBase): moves marks with their
// windows, takes marks and hands down when their time is up.
void tickSenses();

// ---------------------------------------------------------------- authorship

// Where an instruction or an act came from. Console: the keyboard and mouse at the
// machine. Resident: a resident's own commands. Guest: a person at another seat
// (the room chat). Visitor: any other caller on the socket or the API. File: text
// read from a file. App: the app itself (timers, boot).
enum class Source { Console, Resident, Guest, Visitor, File, App };
struct Who {
    std::string actor;
    Source source;
};
const char* sourceName(Source s);
bool parseSource(const std::string& s, Source& out);
Who currentWho();
Who personWho();   // {"person", Console}
// The author of the event getEvent last returned (the person for terminal input).
Who eventWho();

// The app acts for w while the scope lives (a resident dispatch, an IPC command).
class WhoScope {
public:
    explicit WhoScope(const Who& w);
    ~WhoScope();
    WhoScope(const WhoScope&) = delete;
    WhoScope& operator=(const WhoScope&) = delete;
};

// The event getEvent is about to return came from w.
void setEventWho(const Who& w);
// TWwdosApp::getEvent: settle() first (window changes since the last event go to that
// event's author), then the base getEvent, then noteOrigin (whose event this is).
void settle();
void noteOrigin(const TEvent& ev, bool fromPut);
// TWwdosApp::putEvent: an event put now carries the current author.
void notePut(const TEvent& ev);

// One ordered log of acts by everyone (acts command).
// type: the window's type when the window is gone (a close).
void addAct(const Who& w, const std::string& what, const std::string& window, const std::string& result,
            uint64_t rseq = 0, const std::string& type = "");
void updateAct(uint64_t rseq, const std::string& result);
// Who opened a window ("" when not known).
std::string ownerName(TView* v);

// IPC: every command runs as the caller it names (actor=, source=).
void ipcBegin(const std::string& cmd, const KV& kv);
void ipcEnd(const std::string& cmd, const KV& kv, const std::string& resp);

// The visible hand: false while the actor's pointer is still moving to (x, y) in
// screen cells; the act waits until it is there.
bool handReady(const std::string& actor, int x, int y);
void handFollow(const std::string& actor, int x, int y);
// Takes hands down after they have been still for a while (called with tickSenses).
void tickActs();

std::string cmdActs(const KV& kv);    // acts

// ---------------------------------------------------------------- the person's gestures

// Every act by the console (the person) is also a gesture: an ordered stream, numbered g, that a
// resident reads with person_events since=G, waits on with person_wait (the socket holds the
// reply until a gesture comes or ms pass), or receives pushed on subscribe_events as "gesture".
// A continued act (typing, a drag, a scroll, a run of moves) comes again with update=true.
std::string cmdPersonEvents(const KV& kv);   // person_events (and person_wait's immediate answer)

// The act log for in-app readers (the Feed window): the last count acts, oldest first.
struct ActRecord {
    uint64_t n;
    long long ms;
    std::string actor, source, what, window, result;
};
void lastActs(size_t count, std::vector<ActRecord>& out);
uint64_t actsRevision();     // changes whenever the log does
long long actsClockMs();     // the log's clock now (ms since the app started)
uint64_t gestureLast();
std::string gestureJson(uint64_t g);
std::string gesturesSince(uint64_t since, int limit, const std::string& kinds, int& count);
std::string cmdHand(const KV& kv);    // resident_hand
std::string cmdOwner(const KV& kv);   // window_owner

// Undo of a resident's own last change (open, move or resize, retitle, a run of typing in an editor).
// It refuses when someone else changed the window since. The person's changes are not kept.
void noteTextBefore(const std::string& actor, TView* window);   // before a resident's keys reach an editor
std::string cmdUndo(const KV& kv);   // resident_undo (list=1 shows the stack)

// ---------------------------------------------------------------- who is speaking

// Menu commands of the Residents menu (handled by handleCommand).
const unsigned short cmResidentKey = 9950;     // Console key: turn it (the console only)
const unsigned short cmResidentMarks = 9951;   // Reading marks on/off (a rule)
const unsigned short cmResidentHand = 9952;    // Visible hand on/off (a rule)
const unsigned short cmResidentAbout = 9953;   // About the residents

// True when the command was one of the Residents menu's; its author is currentWho().
bool handleCommand(unsigned short cmd);

// The keylock on the console. Off at start; only the console turns it.
bool keyTurned();
// A change to the residents' rules (manners, owners, standing): "" when allowed (the
// console with the key turned), else the refusal to return. Logged either way.
std::string ruleGate(const std::string& what);
// The standing of an author: keyholder, operator, resident, guest, visitor, data, app.
std::string standingOf(const Who& w);
// An instruction a resident receives, with its author, source and standing in front;
// logged as an act ("instruction to <to>").
std::string stampInstruction(const std::string& text, const std::string& to);

bool marksOn();
void setMarksOn(bool on);
bool handOn();
void setHandOn(bool on);

std::string cmdStanding(const KV& kv);   // standing
std::string cmdAbout(const KV& kv);      // residents_about
std::string aboutText();

}  // namespace resident

#endif
