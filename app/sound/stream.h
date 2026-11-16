// The machine's sound stream: one mixer (mixer.h) played without a break through an
// AudioQueue, and the clock every sound, the TR-808's steps and the event scores take
// their time from. WWDOS_SOUND_OUT picks the output: coreaudio (the default on macOS),
// null (the stream runs at real time and plays nowhere: tests), off (no stream; the
// clock is the steady clock and the TR-808 falls back to afplay).
#pragma once

#include <string>

#include "mixer.h"

namespace sound {

Mixer& mixer();
void startStream();          // idempotent; called when the app sets the sound host
const char* streamOut();     // "coreaudio", "null" or "off"
bool streamRunning();
// Milliseconds on the stream: the sample being heard now, extrapolated between blocks,
// never going back. Used by clockMs() when the stream runs.
double streamMs();
// clockMs() while the stream runs: the stream's milliseconds plus where the clock stood when it started.
double streamClockMs();
// How far ahead of the clock a sound asked for now is placed, so it is never late.
double leadMs();
// The mixer sample for clock time ms plus the lead (where a sound asked for at ms goes).
int64_t sampleForClock(double clockMsAt);
// A TR-808 voice's recording (bd.wav ... ch.wav, 22050 Hz mono), loaded once.
Pcm drumSample(const std::string& id);
// The browser build: render ahead and hand blocks to WebAudio (from poll()); no-op elsewhere.
void webPump();
bool webAudioRunning();
// exec_command name=sound action=state|capture.
std::string streamCommand(const std::string& action, double captureMs);

}  // namespace sound
