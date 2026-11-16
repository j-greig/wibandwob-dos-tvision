// Resident input: a second operator's keys, pointer and dialog answers.
//
// The resident (an agent living in the desktop) sends keys, clicks, drags and
// scrolls, and answers dialogs and menus by name, beside the person at the
// keyboard. Its input has a queue of its own and can carry an address (a
// window), so it reaches that window without moving the person's focus.
// Commands, manners and limits: docs/resident-input.md.
#ifndef RESIDENT_INPUT_H
#define RESIDENT_INPUT_H

#include <functional>
#include <map>
#include <string>
#include <vector>

class TView;
class TWindow;
struct TEvent;

namespace resident {

using KV = std::map<std::string, std::string>;

struct Hooks {
    // Window id -> window, nullptr when unknown.
    std::function<TWindow*(const std::string&)> findWindow;
    // Window -> its API id ("" when it has none).
    std::function<std::string(TWindow*)> idOf;
    // Window -> its type slug (window type registry).
    std::function<std::string(TWindow*)> typeOf;
    // Text a window holds in its own model (editor lines, terminal output, chat,
    // paint canvas); false when the type has none and its cells are the text.
    std::function<bool(TWindow*, std::vector<std::string>&)> textOf;
    // One sentence of state for types whose model says more than their cells; "" otherwise.
    std::function<std::string(TWindow*)> stateOf;
    // Run a registry command in the app (resident_hands.cpp: actAs).
    std::function<std::string(const std::string&, const KV&)> exec;
};

void setHooks(const Hooks& h);
const Hooks& hooks();

// Who opened a window: "resident" for windows resident input opened, else "person".
std::string ownerOf(TView* v);

// Event loop integration, called from the application's getEvent()/putEvent().
// takeEvent runs before the base getEvent: true means ev is final (skip the base).
bool takeEvent(TEvent& ev);
// afterBase runs after the base getEvent: it notes and routes the person's input
// and delivers one resident item when the person is idle.
void afterBase(TEvent& ev);
// interceptPut: true when an event put by a view inside a resident dispatch was
// kept for that view's window instead of going to the person's focused window.
bool interceptPut(TEvent& ev);

// Registry commands (see command_registry.cpp).
std::string cmdKey(const KV& kv);      // resident_key
std::string cmdMouse(const KV& kv);    // resident_mouse
std::string cmdStatus(const KV& kv);   // resident_status
std::string cmdManners(const KV& kv);  // resident_manners
std::string cmdFront(const KV& kv);    // resident_front
std::string cmdPress(const KV& kv);    // resident_press
std::string cmdMenu(const KV& kv);     // resident_menu

}  // namespace resident

#endif
