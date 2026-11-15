// Tests for the machine's sound: notes rendered to buffers and compared.
// Run: build/app/sound_test (ctest name: sound).
#include "synth.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace sound;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const std::string& what) {
    if (ok) { ++g_pass; std::printf("  PASS: %s\n", what.c_str()); }
    else { ++g_fail; std::printf("  FAIL: %s\n", what.c_str()); }
}

static Note note(Voice v, const char* name, double at, double dur, int vel = 100, const char* patch = "bass") {
    Note n;
    n.voice = v;
    n.midi = parseNote(name);
    n.atMs = at;
    n.durMs = dur;
    n.vel = vel;
    n.patch = fmPatches()[patchIndex(patch)];
    return n;
}

static size_t ms(double m) { return (size_t)std::floor(m * kRate / 1000.0 + 0.5); }

static void testSynth() {
    std::printf("[synth]\n");
    check(parseNote("C-4") == 60 && parseNote("A-4") == 69 && parseNote("C#4") == 61 && parseNote("Db4") == 61 &&
          parseNote("c4") == 60 && parseNote("60") == 60 && parseNote("B-3") == 59 && parseNote("C-0") == 12,
          "note names parse in tracker form (C-4 = 60, A-4 = 69, C#4 = Db4 = 61)");
    check(parseNote("H-4") < 0 && parseNote("") < 0 && parseNote("C") < 0 && parseNote("200") < 0, "bad note names are refused");
    check(noteName(61) == "C#4" && noteName(60) == "C-4" && noteName(69) == "A-4", "note names print in tracker form");
    check(speakerDivisor(69) == 2712 && std::fabs(speakerHz(69) - 439.966) < 0.01,
          "PC speaker A-4 loads divisor 2712 into the timer chip and sounds at 439.97 Hz");

    auto a = render({note(Voice::PcSpeaker, "A-4", 0, 1000)}, 1000);
    const int cross = upCrossings(a, 0, a.size());
    check(a.size() == 22050, "one second renders 22050 samples");
    check(cross >= 438 && cross <= 441, "PC speaker A-4 crosses zero upward 440 times in a second (got " + std::to_string(cross) + ")");
    bool twoLevels = true;
    for (int16_t v : a) if (std::abs(v) != std::abs(a[0]) || v == 0) { twoLevels = false; break; }
    check(twoLevels, "PC speaker output holds two levels only (a square wave)");

    auto a2 = render({note(Voice::PcSpeaker, "A-4", 0, 1000)}, 1000);
    check(fnv1a(a) == fnv1a(a2), "the same notes render the same samples");

    // One speaker: the second note cuts the first.
    auto mono = render({note(Voice::PcSpeaker, "C-4", 0, 1000), note(Voice::PcSpeaker, "C-6", 500, 500)}, 1000);
    const int c1 = upCrossings(mono, ms(100), ms(400)), c2 = upCrossings(mono, ms(600), ms(900));
    check(c1 >= 76 && c1 <= 80 && c2 >= 312 && c2 <= 316,
          "the PC speaker is one voice: C-6 replaces C-4 when it starts (" + std::to_string(c1) + ", " + std::to_string(c2) + " crossings in 300 ms)");

    // FM: sounds through the gate, rings out over the release, then silence.
    const Note b = note(Voice::Fm, "C-3", 0, 250);
    auto fb = render({b}, 600);
    const double during = rms(fb, 0, ms(250));
    const double after = rms(fb, ms(250 + b.patch.releaseMs + 1), fb.size());
    check(during > 0.05, "FM bass note has level during its gate (rms " + std::to_string(during) + ")");
    check(after == 0.0, "FM bass note is silent after gate plus release");
    auto quiet = render({note(Voice::Fm, "C-3", 0, 250, 30)}, 600);
    check(rms(quiet, 0, ms(250)) < during * 0.5, "a soft FM note is quieter than a loud one");
    auto bell = render({note(Voice::Fm, "C-5", 0, 250, 100, "bell")}, 600);
    check(fnv1a(bell) != fnv1a(render({note(Voice::Fm, "C-5", 0, 250, 100, "organ")}, 600)), "patches sound different (bell, organ)");

    // Nine FM channels: a tenth note at once takes the first channel.
    std::vector<Note> ten;
    for (int i = 0; i < 10; ++i) ten.push_back(note(Voice::Fm, "C-3", i * 1.0, 400, 100, "organ"));
    auto tenS = render(ten, 600);
    check(peak(tenS) > 0 && peak(tenS) <= 32767, "ten FM notes at once render and stay in range");

    // Envelope.
    const Patch& br = fmPatches()[patchIndex("brass")];
    check(envelope(br, 0, 500) == 0.0 && std::fabs(envelope(br, br.attackMs, 500) - 1.0) < 1e-9 &&
          std::fabs(envelope(br, 400, 500) - br.sustain) < 1e-9 && envelope(br, 500 + br.releaseMs, 500) == 0.0,
          "envelope: 0 at start, 1 after attack, sustain while held, 0 after release");

    // WAV file.
    const char* path = "/tmp/wwdos_sound_test.wav";
    check(writeWav(path, a), "writes a WAV file");
    struct stat st;
    check(::stat(path, &st) == 0 && st.st_size == 44 + 2 * 22050, "WAV is 44 header bytes plus 2 per sample");
    std::remove(path);
}

int main(int argc, char** argv) {
    std::string only = argc > 1 ? argv[1] : "";
    if (only.empty() || only == "synth") testSynth();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
