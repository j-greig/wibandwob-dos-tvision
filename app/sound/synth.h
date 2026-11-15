// The machine's own synth: a PC speaker square voice and a two-operator FM voice
// in the manner of the AdLib card (OPL2). No Turbo Vision dependency and no clock
// of its own: notes carry their start time, and render() turns a list of notes
// into 16-bit mono samples at kRate. The same notes always give the same samples,
// so tests compare buffers. Commands and limits: docs/machine-sound.md.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sound {

constexpr int kRate = 22050;          // samples per second, mono
constexpr double kPitHz = 1193182.0;  // the PC's timer chip clock (8253 PIT)
constexpr int kFmChannels = 9;        // OPL2 melodic channels

enum class Voice { PcSpeaker, Fm };
const char* voiceId(Voice v);         // "pcspk", "fm"
bool parseVoice(const std::string& s, Voice& out);  // pcspk|speaker|pc|fm|adlib

// One FM patch: modulator and carrier frequency ratios, modulation index (peak,
// scaled by the envelope so the tone darkens as it decays), modulator self-feedback,
// an ADSR envelope (attack, decay and release in ms, sustain 0-1) and a level.
struct Patch {
    std::string name;
    double modRatio, carRatio, index, feedback;
    double attackMs, decayMs, sustain, releaseMs;
    double level;
};
const std::vector<Patch>& fmPatches();       // bass, organ, bell, brass, lead, pluck, chip
int patchIndex(const std::string& name);     // -1 when unknown
const Patch& speakerPatch();                 // the PC speaker: on or off, no envelope

struct Note {
    double atMs = 0;     // start, from the start of the render
    Voice voice = Voice::Fm;
    int midi = 60;       // MIDI note number, 60 = C-4
    int vel = 100;       // 1-127; the PC speaker ignores it (it has no volume)
    double durMs = 250;  // gate length; FM notes ring on for the patch's release
    Patch patch;         // the FM patch (ignored for the PC speaker)
};

// "C-4", "C#4", "c4", "Db3", or a number "60". -1 when not a note. Octave 4 holds
// middle C (MIDI 60), as trackers write it.
int parseNote(const std::string& s);
std::string noteName(int midi);              // tracker form: "C-4", "C#4"
double noteHz(int midi);                     // equal temperament, A-4 = 440 Hz
unsigned speakerDivisor(int midi);           // the PIT divisor the PC would load
double speakerHz(int midi);                  // kPitHz / divisor: the pitch the speaker really makes

// Envelope level 0-1 of a patch at tMs after note start, for a gate of durMs.
double envelope(const Patch& p, double tMs, double durMs);
// How long a note sounds: the gate, plus the release for FM.
double soundingMs(const Note& n);

// Renders notes into totalMs of samples (rounded to whole samples). The PC speaker
// is one voice: a speaker note stops when the next speaker note starts. FM notes
// mix (at most kFmChannels at once; a further note takes the oldest channel).
std::vector<int16_t> render(const std::vector<Note>& notes, double totalMs);
// Length that holds every note to its end.
double renderLengthMs(const std::vector<Note>& notes);

bool writeWav(const std::string& path, const std::vector<int16_t>& samples);
uint64_t fnv1a(const std::vector<int16_t>& samples);
double rms(const std::vector<int16_t>& s, size_t from = 0, size_t to = (size_t)-1);
int peak(const std::vector<int16_t>& s);
// Zero crossings going upward in [from, to): a pitch estimate for tests.
int upCrossings(const std::vector<int16_t>& s, size_t from, size_t to);

}  // namespace sound
