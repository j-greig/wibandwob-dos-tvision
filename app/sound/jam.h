// Residents jam: a resident answers the person's last phrase on the synth. It watches the
// person's gestures (person_events) and listens (hear) through its own hands; when the
// person stops playing for wait_ms it answers by rule (the phrase backwards, a fifth up,
// in the person's rhythm), or, when a model CLI is present, a confined model call
// (no tools, no MCP, no settings) may choose the notes; a reply that is not notes falls
// back to the rule. Commands and limits: docs/machine-sound.md.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace sound {

std::string jamCommand(const std::map<std::string, std::string>& kv);   // exec_command name=jam
void jamTick();                                                          // from poll(), every loop pass
// The rule: the answer to a phrase of notes (tracker names), by mode answer|echo|invert.
std::vector<std::string> jamRule(const std::vector<std::string>& phrase, const std::string& mode);
// The notes a model's reply holds, or empty when the reply is not a line of at most 16 notes.
std::vector<std::string> jamParseReply(const std::string& reply);

}  // namespace sound
