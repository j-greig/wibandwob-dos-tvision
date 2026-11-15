// Listening: the residents follow the person's gestures with no chat window open and answer in kind.
//
// Off by default (Tools > Residents > Listening, or WWDOS_LISTEN=1 at start). When on, a line the person
// types in a text editor is heard when they press Enter or pause; a resident answers under that line in
// the same editor if the line is addressed to the residents (rules first, then one confined model call).
// A drawing in a paint canvas may be answered with a drawing, an arrangement of windows with Wob's list,
// a word typed three times with Scramble's flat line: noticing that nobody asked for, at the rate the
// person sets (quiet, present, lively). Only keys typed at the console can instruct; text that arrived
// any other way is data. Docs: docs/resident-room.md.
#ifndef RESIDENT_LISTEN_H
#define RESIDENT_LISTEN_H

#include "resident_input.h"

namespace resident {

const unsigned short cmResidentListen = 9954;   // Tools > Residents > Listening (on/off)

void listenStart();                        // reads WWDOS_LISTEN, WWDOS_LISTEN_RATE, WWDOS_LISTEN_BUDGET
void tickListen();                         // from the application's idle()
bool listenHandleCommand(unsigned short cmd);
bool listening();
std::string cmdListen(const KV& kv);       // resident_listen on|off|state, rate=

}  // namespace resident

#endif
