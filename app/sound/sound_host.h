// Glue between the machine's sound (synth, ears, scores) and the desktop app.
// The app fills a Host once at start; every sound command goes through command().
// Commands and limits: docs/machine-sound.md.
#pragma once

#define Uses_TWindow
#define Uses_TRect
#include <tvision/tv.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "synth.h"

namespace sound {

using KV = std::map<std::string, std::string>;

struct Host {
    std::function<std::string(TWindow*)> insert;           // put on the desktop, register, return its API id
    std::function<TWindow*(const std::string&)> find;      // window by API id
    std::function<std::string(TWindow*)> idOf;             // API id of a window ("" when none)
};
void setHost(const Host& h);
const Host& host();

// True for the names command() answers: open_synth, synth.
bool handles(const std::string& name);
std::string command(const std::string& name, const KV& kv);
// One sentence of state for look_glance; "" when w is not a sound window.
std::string stateOf(TWindow* w);

// Milliseconds on a steady clock since the first call (the app's start, in practice).
double clockMs();
// Name of the author of the act being handled (resident::currentWho().actor).
std::string currentActor();

// Synth audio out. Default on; WWDOS_SYNTH_AUDIO=0 starts it off (tests). Off, the
// notes are still rendered, heard by the ears and counted, but nothing is sent to
// the speakers.
bool synthAudioEnabled();
void setSynthAudioEnabled(bool on);
bool synthAudioAvailable();
// Renders the notes (atMs from now) to one WAV and sends it to the speakers. Returns
// true when sent, false when silenced or no player.
bool sendToSpeakers(const std::vector<Note>& notes);

// Small helpers shared by the sound files.
std::string jsonEscape(const std::string& s);
std::string kvGet(const KV& kv, const char* k, const std::string& dflt = "");
bool kvHas(const KV& kv, const char* k);
int toInt(const std::string& s, int dflt);
bool truthy(const std::string& s);

}  // namespace sound
