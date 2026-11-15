// MIDI in and out (1983, so in period). Input from every MIDI source and from a virtual
// destination "WibWob-DOS" plays the synth with real key releases; the TR-808 and the
// tracker send clock (24 a beat), start, stop and their notes out of a virtual source
// "WibWob-DOS", timestamped on the one clock. WWDOS_MIDI picks the device: coremidi
// (the default on macOS), null (no device: tests inject input and read what was sent),
// off. No Turbo Vision here (CoreMIDI's headers clash with it). Commands: docs/machine-sound.md.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sound {
namespace midi {

struct Msg {
    double ms = 0;               // on the one clock: when it arrived, or when it is due to leave
    bool out = false;
    std::vector<uint8_t> bytes;
    std::string from;            // in: the source's name or "test"; out: "tr808", "tracker", ...
};

std::string describe(const std::vector<uint8_t>& b);   // "note on C-4 v100 ch1", "clock", "start", ...
bool parseHex(const std::string& s, std::vector<uint8_t>& out);   // "90 3C 64"
std::string hex(const std::vector<uint8_t>& b);

void start();                    // idempotent
const char* device();            // "coremidi", "null" or "off"
int sourcesConnected();
void setWake(void (*fn)());      // called (from the MIDI thread) when input arrives

void inject(const std::vector<uint8_t>& bytes, const std::string& from);   // as if it arrived now
std::vector<Msg> takeIn();       // input since the last call, in order
// Sends at clockMs (plus the sound stream's lead, so it lines up with the audio).
void send(const std::vector<uint8_t>& bytes, double clockMs, const std::string& from);

std::vector<Msg> recent(size_t n);   // the last n messages, in and out
long inCount();
long outCount();
bool clockOut();
void setClockOut(bool on);
int gmDrum(int tr808Voice);      // General MIDI drum note for a TR-808 voice (bd 36, sd 38, ch 42 ...)

}  // namespace midi
}  // namespace sound
