// Residents' tools: small programs a resident writes that become windows.
//
// A program is plain text in a tiny BASIC of this app's own (no shell, no files, no network): it
// draws cells in its own window, reads the clock and its window's keys, and calls the registry
// commands on a short list as its author. Saved as WWDOS_HOME/TOOLS/<NAME>.BAS with its author in
// the first line. Language, commands and limits: docs/resident-life.md (Tools of their own).
#ifndef RESIDENT_TOOL_H
#define RESIDENT_TOOL_H

#include "resident_input.h"

#include <string>

class TWindow;

namespace tools {

// Writes the two worked tools (CLOCK.BAS by Wob, ETCH.BAS by Wib) when TOOLS/ lacks them.
void seed();
bool isToolWindow(TWindow* w);
bool isToolListWindow(TWindow* w);
// Opens the list of tools (the menu's entry).
void openList();
bool owns(const std::string& cmd);
std::string command(const std::string& name, const resident::KV& kv);
// Runs every open tool's clock (called from the life's tick).
void tick();

}  // namespace tools

#endif
