// The residents' voices as an instrument: a word or syllable sung on a note. Each word is
// rendered once per singer with macOS 'say' (Wib: Sandy (English (UK)), Wob: Grandpa
// (English (UK)); another English voice when one is absent), cached, pitched to the note by
// resampling against the word's own measured pitch, cut to the gate, and mixed on the one
// stream like a drum sample. WWDOS_SAY names a stand-in for 'say' (tests). No Turbo Vision.
// Commands and limits: docs/machine-sound.md.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "mixer.h"

namespace sound {
namespace voice {

// Which macOS voice sings for a resident ("wib", "wob"; anyone else sings as wib), and
// whether it is the house voice or a stand-in English voice because the house one is absent.
struct Singer {
    std::string who;
    std::string voice;
    bool fallback = false;
    double gain = 1.0;
};
Singer singer(const std::string& who);

// A word as 'say' speaks it (22050 Hz mono), cached; nullptr when it cannot be rendered.
// Words are letters, apostrophes, hyphens and spaces, at most 24 characters.
Pcm spoken(const std::string& who, const std::string& word, std::string* err = nullptr);
bool validWord(const std::string& word);

// The median pitch of voiced speech in pcm (70-500 Hz), 0 when none is found.
double pitchOf(const std::vector<int16_t>& pcm);
// The word sung on midi for durMs: resampled from its own pitch to the note's, cut to durMs
// with a 10 ms fade. Cached per (singer, word, note, length).
Pcm sung(const std::string& who, const std::string& word, int midi, double durMs, std::string* err = nullptr);

long renders();     // how many times 'say' has run
std::string sayPath();

}  // namespace voice
}  // namespace sound
