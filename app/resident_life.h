// Resident life: the residents' self on disk, waking, notes, the logbook and guestbook,
// and sleep when nobody is there.
//
// The self lives in one directory (WWDOS_HOME, default ~/.wwdos) as plain text files:
// SELF.INI (wakings), DESK.TXT (windows open at the last close, with owners), a NOTE.TXT
// per resident, LOG.TXT (the residents' diary) and GUESTS.TXT (visitor notes).
// Everything here is written by rule from real state; nothing calls a model.
// Commands, formats and limits: docs/resident-life.md.
#ifndef RESIDENT_LIFE_H
#define RESIDENT_LIFE_H

#include "resident_input.h"

#include <functional>
#include <string>
#include <vector>

class TWindow;
struct TEvent;

namespace life {

using KV = resident::KV;

struct Hooks {
    // Inserts a window the life module made on the desktop and registers it (gives it an id).
    std::function<void(TWindow*)> insertWindow;
    // Opens a text editor window holding text; onClose (may be empty) receives its lines when
    // it is closed. Returns the window.
    std::function<TWindow*(const std::string& title, const std::vector<std::string>& lines,
                           std::function<void(const std::vector<std::string>&)> onClose)> openEditor;
    // True when a model is reachable through the app's confined calls (shown at boot only).
    std::function<bool()> modelPresent;
    // The claude CLI's path when the app reaches a model through it ("" otherwise). Notes and dreams
    // call it with Scramble's confined flags: no tools, no MCP servers, no user settings, own prompt.
    std::function<std::string()> modelCli;
    // Writes what a window holds (editor text, paint canvas) under dir as base.<ext>; returns the
    // file name written ("" when the window keeps nothing).
    std::function<std::string(TWindow*, const std::string& dir, const std::string& base)> saveState;
    // Opens a window of a type at a rectangle (desktop cells) with a title, filled from a file
    // saveState wrote ("" for none). nullptr and err when it cannot.
    std::function<TWindow*(const std::string& type, int x, int y, int w, int h, const std::string& title,
                           const std::string& stateFile, std::string& err)> restoreWindow;
    // What an editor openEditor made holds now; its onClose is dropped, so a later close does not
    // run it again (the app quitting with a sign page open). False for other windows.
    std::function<bool(TWindow*, std::vector<std::string>&)> takeEditor;
    // The operator's panel: mute the sound stream, switch the app's model calls off (safe mode), and
    // run a registry command directly (desk mosh).
    std::function<void(bool)> mute;
    std::function<void(bool)> setSafe;
    std::function<std::string(const std::string&, const KV&)> registry;
};
void setHooks(const Hooks& h);

// The directory the self lives in (WWDOS_HOME or ~/.wwdos), created on first use.
std::string home();

// Called once from the application's constructor, after the resident hooks are set:
// reads the self from disk, counts the waking, opens the boot screen.
void wake();
// Called when the application's run loop ends: writes the self back.
void close(const std::string& how);

// Called on every pass of the idle loop.
void tick();
// Called from getEvent with the person's own input (keyboard and mouse at the console),
// after noteOrigin. True means the event was used up (it woke the residents or closed the
// boot screen) and must not be delivered.
bool consoleInput(TEvent& ev);

// The look of a life window (boot screen, dream): its text and one sentence of state.
// False / "" for other windows.
bool textOf(TWindow* w, std::vector<std::string>& out);
std::string stateOf(TWindow* w);
bool isBootWindow(TWindow* w);
bool isDreamWindow(TWindow* w);
bool isOwnWindow(TWindow* w);   // a resident's time of its own
bool isFilmWindow(TWindow* w);  // a waking played back from the acts log
bool isDoorWindow(TWindow* w);  // who came in by the door
bool isPanelWindow(TWindow* w); // the operator's panel

// Acts of earlier wakings read back from ACTS.TXT and its rotations, newest first (the Feed window
// scrolls into them). Returns how many earlier acts there are on disk.
struct EarlierAct {
    int waking;
    std::string when, actor, source, window, what, result;
};
size_t earlierActs(size_t skip, size_t count, std::vector<EarlierAct>& out);

// True for the commands this module answers (the socket routes them to the registry).
bool owns(const std::string& cmd);
// Answers one of its commands; "" when the name is not one of them.
std::string command(const std::string& name, const KV& kv);

// What the menu bar's kaomoji slot and the status line show for the residents' state;
// nullptr / "" when there is nothing to add.
const char* kaomoji();
std::string statusWord();

// Menu commands of Tools > Residents handled here.
// 9940-9949 are the life's (9950s are the residents' menu, 9960 the Feed, 9975-9976 the desk mosh).
const unsigned short cmLifeLogbook = 9940;
const unsigned short cmLifeGuestbook = 9941;
const unsigned short cmLifeSign = 9942;
const unsigned short cmLifeWill = 9943;
const unsigned short cmLifeOwn = 9944;
const unsigned short cmLifeTools = 9945;
const unsigned short cmLifeFilm = 9946;
const unsigned short cmLifeDoor = 9947;
const unsigned short cmLifePanel = 9948;
const unsigned short cmLifeStopAll = 9949;   // Alt+Q: pause the residents, mute, stop a mosh (again: go on)

// Inserts a window the residents' life made (tools, film) on the desktop and gives it an id.
void insertLifeWindow(TWindow* w);
bool handleCommand(unsigned short cmd);

}  // namespace life

#endif
