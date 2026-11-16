// Errors that teach, and a self-test a resident runs on itself.
//
// An unknown command or window type answers with the nearest names ("did you mean"); help name=
// says what a command does, whether it is a rule, and whether this caller may run it and why not;
// resident_selftest has the calling resident try what a person can do (open, type, read back, answer
// a dialog, drag, close) through its own hands and report each step. Docs: docs/resident-room.md.
#ifndef RESIDENT_TEACH_H
#define RESIDENT_TEACH_H

#include "resident_input.h"

#include <string>
#include <vector>

namespace resident {

// Commands the socket handles itself (not in the registry), with what each takes.
struct SocketCommand { const char* name; const char* description; };
const std::vector<SocketCommand>& socketCommands();

// Up to three known names close to name (edit distance, then shared prefix or substring).
std::vector<std::string> didYouMean(const std::string& name, const std::vector<std::string>& known);
std::vector<std::string> allCommandNames();
// "err unknown cmd: X; did you mean: a, b? help lists every command"
std::string unknownCommand(const std::string& name);
std::string unknownWindowType(const std::string& type);

// Why a caller may not run a command ("" when it may): rules need the console with the key turned.
std::string whyNot(const std::string& name, const KV& kv);

std::string cmdHelp(const KV& kv);        // help
std::string cmdSelftest(const KV& kv);    // resident_selftest
void tickSelftest();                      // from the application's idle()

}  // namespace resident

#endif
