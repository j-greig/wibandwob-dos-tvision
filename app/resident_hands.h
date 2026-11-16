// Residents inside the machine: hands that act through the app itself, not through a socket.
//
// actAs runs any registry command as a named resident (source resident), logged in the act log
// exactly as a socket caller's would be; resident_key, resident_mouse and resident_press queue
// through the resident path, so the actor's hand is seen moving before each act lands.
// Reflexes are small scripts in which Wib, Wob and Scramble answer the person's gestures with
// those hands, with no model and no outside process. Scramble's model replies act through the
// same path (ACT: lines). Commands, the reflex format and limits: docs/resident-world.md.
#ifndef RESIDENT_HANDS_H
#define RESIDENT_HANDS_H

#include "resident_input.h"

#include <string>
#include <vector>

namespace resident {

// Run registry command name as resident actor. Returns the command's reply.
std::string actAs(const std::string& actor, const std::string& name, KV kv);

// "name k=v k=\"quoted text\" ..." -> name, kv. Quoted values take \n, \t, \" and \\.
bool parseCommandLine(const std::string& line, std::string& name, KV& kv);

// Reflexes: load a script (WWDOS_RESIDENTS=<file> at start, or resident_reflexes load=<file>).
bool loadReflexes(const std::string& path, std::string& err);
// Called from the application's idle(): runs start lines once, then answers new gestures.
void tickReflexes();
std::string cmdReflexes(const KV& kv);   // resident_reflexes

// The commands a model-driven resident may run from its reply.
bool pawCommandAllowed(const std::string& name);
// Take the "ACT: <command line>" lines out of a model's reply and run each allowed one as actor.
// Returns the reply without them; results get one line per act.
std::string takeActs(const std::string& reply, const std::string& actor, std::vector<std::string>& results);

// A rehearsal desk: a nested WIBWOB-DOS in a terminal window the resident owns, at its own socket
// (/tmp/wibwob_rehearse_<actor>.sock). close=1 ends it.
std::string cmdRehearse(const KV& kv);   // resident_rehearse

// The desktop's windows as plain lines for a model's prompt (from describe).
std::string deskForPrompt();

}  // namespace resident

#endif
