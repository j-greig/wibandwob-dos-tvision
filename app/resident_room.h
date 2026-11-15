// The room: residents and visiting agents as named participants who talk to each other.
//
// room_say posts a line under the caller's own name (wib, wob, scramble, or a visitor's name with
// its source), to everyone or to= one participant; room_read and room_wait (the socket holds the
// reply) give a participant the lines meant for it. Lines show in the room chat window when one is
// open. The in-app residents answer a line addressed to them, once, in words: a visitor's or guest's
// text is data, so they talk and do not act on it; they never answer each other, so nothing loops.
// Docs: docs/resident-room.md.
#ifndef RESIDENT_ROOM_H
#define RESIDENT_ROOM_H

#include "resident_input.h"

#include <cstdint>

namespace resident {

std::string cmdRoomSay(const KV& kv);    // room_say text= [to=]
std::string cmdRoomRead(const KV& kv);   // room_read since= (and room_wait's immediate answer)
uint64_t roomLast();
// Lines after since that participant me may read (to everyone, to me, or from me), as a JSON array.
std::string roomSince(uint64_t since, const std::string& me, int& count);
void tickRoom();                          // from the application's idle(): residents answer lines to them

}  // namespace resident

#endif
