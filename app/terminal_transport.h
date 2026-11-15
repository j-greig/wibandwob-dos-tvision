// Terminal transport: pause and resume what runs in a terminal window, for the person and
// the residents alike. Commands, keys and limits: docs/terminal-transport.md.
#ifndef TERMINAL_TRANSPORT_H
#define TERMINAL_TRANSPORT_H

#include "resident_senses.h"

#include <map>
#include <string>

class TWibWobTerminalWindow;

namespace termtransport {

// action: pause | resume | toggle. Changes the window's program, logs the act in who's name
// (the Feed shows it) and returns the reply: {"ok":true,"id":..,"state":..,"changed":..} or "err ...".
std::string act(TWibWobTerminalWindow* w, const std::string& action, const resident::Who& who);

// The terminal a command means: id= (or window_id=), else the focused terminal, else the only one.
TWibWobTerminalWindow* target(const std::map<std::string, std::string>& kv, std::string& err);

// {"id":..,"state":"running|paused|exited","program":..,"seconds_paused":..,"keyboard":..}
std::string stateJson(TWibWobTerminalWindow* w);

// Menu and status-line commands that reach the application (no terminal window focused).
bool handleCommand(unsigned short cmd);

}  // namespace termtransport

// Registry commands terminal_pause|resume|toggle|state; handled=false for any other name.
std::string terminal_registry_command(const std::string& name, const std::map<std::string, std::string>& kv,
                                      bool* handled);

#endif
