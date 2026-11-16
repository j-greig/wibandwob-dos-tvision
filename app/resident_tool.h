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

// Writes the worked tools (CLOCK.BAS by Wob, ETCH.BAS by Wib, PURR.BAS by Scramble) when TOOLS/ lacks them.
void seed();
bool isToolWindow(TWindow* w);
bool isToolListWindow(TWindow* w);
// Opens the list of tools (the menu's entry).
void openList();
bool owns(const std::string& cmd);
std::string command(const std::string& name, const resident::KV& kv);
// Runs every open tool's clock (called from the life's tick).
void tick();
// Writes every open tool's state (NAME.STATE), as at the app's close.
void saveAll();
// The tool a window runs ("" for other windows), and a tool opened again at a place (the restore).
std::string nameOf(TWindow* w);
TWindow* reopen(const std::string& name, int x, int y, int w, int h, std::string& err);

}  // namespace tools

#endif
