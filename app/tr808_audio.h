// TR-808 local preview audio: plays the pre-rendered voice WAVs with afplay.
// One worker thread spawns and reaps the afplay processes, so the UI thread
// only pushes onto a queue. It reaps only the pids it started (the app has
// other children: terminal windows).
#pragma once

#include <string>

namespace tr808 {

// Directory holding bd.wav .. ch.wav. Order: $WWDOS_TR808_SAMPLES,
// $WWDOS_REPO/modules/tr808/samples, ./modules/tr808/samples, the source tree.
std::string samplesDir();

// Local audio on/off. Default on; WWDOS_TR808_AUDIO=0 starts it off.
bool audioEnabled();
void setAudioEnabled(bool on);

// Queue one hit. volume is afplay's -v (1.0 = file level). No-op when audio is off
// or the sample is missing.
void playVoice(const char* voiceId, double volume);

// True when afplay and at least bd.wav exist.
bool audioAvailable();

}  // namespace tr808
