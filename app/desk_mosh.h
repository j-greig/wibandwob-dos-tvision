// Desk mosh: the window manager breaks the live desktop in time with a beat.
//
// On each clock step the mosh opens, moves, refills, restacks and closes real Turbo
// Vision windows ("shards") made from the desktop's own cells: strips that shear apart
// (tear), copies read at the wrong width (wrap), blocks that keep old glyphs and drift
// (mosh), copies with rotated colours and decayed glyphs (bleed), zoomed, scrolled and
// stale copies (raw). The real windows stay open and usable underneath; turning the
// mosh off closes every shard and puts back what it changed.
// Commands, the clock and the limits: docs/desk-mosh.md.
#ifndef DESK_MOSH_H
#define DESK_MOSH_H

#define Uses_TEvent
#define Uses_TView
#include <tvision/tv.h>

#include <cstdint>
#include <functional>
#include <map>
#include <string>

class TWindow;

namespace deskmosh {

using KV = std::map<std::string, std::string>;

// True for a window the mosh made.
bool isShard(const TView* v);

// Called from TWwdosApp::idle(): runs the steps that are due and draws them.
void tick();
// Called from TWwdosApp::getEvent() after the base: Esc twice within 0.8 s stops the mosh.
void noteEvent(const TEvent& ev);
// The optional screen layer (mosh_screen): TWwdosApp::getEvent calls screenBefore() just
// before the base getEvent (which flushes the screen) and screenAfter() when it returns.
void screenBefore();
void screenAfter();
// Called with every TR-808 event (type, JSON body without braces). Steps are queued and
// run from tick(), never from inside the TR-808's own timer handler.
void noteTr808(const char* type, const std::string& body);

// Registry commands: mosh, mosh_on, mosh_off, mosh_panic, mosh_way, mosh_amount,
// mosh_tempo, mosh_seed, mosh_scope, mosh_count, mosh_step, mosh_screen, mosh_status.
bool isCommand(const std::string& name);
std::string command(const std::string& name, const KV& kv);

// Menu commands (View > Desk Mosh, Alt-M).
const unsigned short cmMoshToggle = 9960;
const unsigned short cmMoshPanic = 9961;
// True when cmd was one of the mosh's; its author is resident::currentWho().
bool handleCommand(unsigned short cmd);

// For the residents' senses: whether a mosh is showing, and one JSON object that
// summarises it ({"on":..,"way":..,"shards":..,"step":..,"opened":..,"closed":..}).
bool on();
std::string summaryJson();
uint64_t stepCount();
uint64_t openedTotal();
uint64_t closedTotal();

}  // namespace deskmosh

#endif
