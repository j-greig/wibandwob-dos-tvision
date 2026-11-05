// TR-808 sequencer engine: patterns, transport, params, mute/solo, swing.
// Port of microapps/wibwob-tr808/engine.ts from the Bun/TS WibWob-DOS.
// No Turbo Vision dependency: the clock is passed in (milliseconds, any
// monotonic origin), so tests drive it with a fake clock and the view drives
// it with std::chrono::steady_clock.
#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace tr808 {

constexpr int kSteps = 16;
constexpr int kVoices = 13;
constexpr int kAccentSel = kVoices;   // selection index of the accent row
constexpr int kMaxParams = 5;

enum Voice { BD, SD, LT, MT, HT, RS, CB, CP, MA, CL, CY, OH, CH };

struct ParamDef {
    const char* id;     // "tune", "attack", "tone", "snappy", "decay", "level"
    const char* label;  // "TUN", "ATK", ...
    int min, max, def, step;
};

struct VoiceDef {
    const char* id;       // "bd"
    const char* label;    // "Bass Drum"
    const char* shortLabel; // "BD"
    int nParams;
    ParamDef params[kMaxParams];
};

const VoiceDef& voiceDef(int v);
// "bd".."ch" -> 0..12, "accent"/"ac" -> kAccentSel, else -1. Case-insensitive.
int voiceIndex(const std::string& id);
// Index of param `id` in voiceDef(v).params, or -1.
int paramIndex(int v, const std::string& id);

struct Pattern {
    std::array<std::array<bool, kSteps>, kVoices> steps{};
    std::array<bool, kSteps> accent{};
    std::array<std::array<int, kMaxParams>, kVoices> params{};
    int lastStep = kSteps;
    Pattern();
};

enum class Scale { S16, S32, T8 };
const char* scaleId(Scale s);     // "16th", "32nd", "8th-triplet"
const char* scaleLabel(Scale s);  // "16th", "32nd", "8T"
bool parseScale(const std::string& s, Scale& out);

struct StepEvent {
    int step = 0;
    std::vector<int> voices;  // voices that sound (mute/solo applied)
    bool accent = false;
    double gapMs = 0;         // time until the following step
};

const std::vector<std::string>& presetNames();

class Engine {
public:
    Engine();

    // ── Pattern slots: 2 banks x 8 numbers x 2 variations ──
    Pattern& pattern();
    const Pattern& pattern() const;
    char bank() const { return bank_; }
    int number() const { return number_; }
    char variation() const { return variation_; }
    // bank/variation 'A'|'B' (0 keeps), number 1-8 (0 keeps). Invalid values are ignored.
    void setSlot(char bank, int number, char variation);
    std::string slotLabel() const;  // "A1-A"

    // ── Selection ──
    int selected() const { return selected_; }
    void select(int sel) { if (sel >= 0 && sel <= kAccentSel) selected_ = sel; }

    // ── Steps (sel = -1 means the selected row) ──
    bool toggleStep(int step, int sel = -1);
    bool setStep(int step, bool on, int sel = -1);
    bool getStep(int step, int sel = -1) const;
    int countSteps(int sel) const;

    // ── Params ──
    int getParam(int v, const std::string& id) const;
    bool setParam(int v, const std::string& id, int value);

    // ── Mute / solo ──
    void toggleMute(int v);
    void toggleSolo(int v);
    bool isMuted(int v) const { return v >= 0 && v < kVoices && muted_[v]; }
    bool isSoloed(int v) const { return v >= 0 && v < kVoices && soloed_[v]; }
    bool hasSolo() const;
    bool shouldPlay(int v) const;

    // ── Globals ──
    int tempo() const { return tempo_; }
    void setTempo(int bpm, double nowMs);  // 35-300; reschedules when playing
    int accentLevel() const { return accentLevel_; }
    void setAccentLevel(int v);
    int master() const { return master_; }
    void setMaster(int v);
    Scale scale() const { return scale_; }
    void setScale(Scale s) { scale_ = s; }
    int swing() const { return swing_; }
    void setSwing(int v);  // 0-100, 50 = straight

    // ── Timing ──
    double baseStepMs() const;
    // Time from step `step` to the next one. Swing lengthens the gap before an
    // odd step and shortens the gap before an even one (50 straight, 66 standard, 75 heavy).
    double gapAfter(int step) const;

    // ── Transport ──
    bool playing() const { return playing_; }
    int step() const { return step_; }  // -1 when stopped
    // Fires step 0 at once and returns it.
    StepEvent start(double nowMs);
    void stop();
    // Every step that is due at nowMs, in order. If the caller fell more than a
    // step (at least 40 ms) behind, it plays one step and resyncs the clock
    // instead of bursting the missed steps.
    std::vector<StepEvent> poll(double nowMs);
    // Milliseconds until the next step (0 when due, -1 when stopped).
    double msUntilNext(double nowMs) const;

    // ── Pattern operations ──
    void clearInstrument(int sel = -1);
    void clearPattern();
    void setLastStep(int n);
    bool loadPreset(const std::string& name);
    void copyPattern();
    bool pastePattern();
    bool hasClipboard() const { return hasClip_; }
    void randomize(double density = 0.3);
    void seed(uint32_t s) { rng_.seed(s); }

private:
    StepEvent advance();
    int slotIndex() const;

    std::array<Pattern, 32> patterns_{};
    char bank_ = 'A';
    int number_ = 1;
    char variation_ = 'A';
    int selected_ = BD;
    bool playing_ = false;
    int step_ = -1;
    int tempo_ = 120;
    Scale scale_ = Scale::S16;
    int accentLevel_ = 80;
    int master_ = 100;
    int swing_ = 50;
    double nextAt_ = 0;
    std::array<bool, kVoices> muted_{};
    std::array<bool, kVoices> soloed_{};
    Pattern clip_{};
    bool hasClip_ = false;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace tr808
