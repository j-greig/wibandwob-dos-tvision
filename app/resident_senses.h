// Resident senses: looking at the desktop without touching it.
//
// A resident (an agent living in the desktop) reads the desktop at three depths
// (glance, read, study), asks what changed since it last looked, and leaves a
// mark on the frame of the window it reads so the person can see its attention.
// No look command changes focus, z-order or content.
// Commands and limits: docs/resident-senses.md.
#ifndef RESIDENT_SENSES_H
#define RESIDENT_SENSES_H

#include "resident_input.h"

namespace resident {

// Registry commands (see command_registry.cpp).
std::string cmdGlance(const KV& kv);    // look_glance
std::string cmdRead(const KV& kv);      // look_read
std::string cmdStudy(const KV& kv);     // look_study
std::string cmdChanges(const KV& kv);   // look_changes
std::string cmdMark(const KV& kv);      // look_mark

// Called on every pass of the event loop (from afterBase): moves marks with their
// windows and takes them down when their time is up.
void tickSenses();

}  // namespace resident

#endif
