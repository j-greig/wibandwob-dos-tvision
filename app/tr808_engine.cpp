#include "tr808_engine.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace tr808 {

// ── Voices ────────────────────────────────────────────────────────────────

#define LVL {"level", "LVL", 0, 100, 80, 5}
#define TUN {"tune", "TUN", 0, 100, 50, 5}
#define DEC {"decay", "DEC", 0, 100, 50, 5}
#define ATK {"attack", "ATK", 0, 100, 50, 5}
#define TON {"tone", "TON", 0, 100, 50, 5}
#define SNP {"snappy", "SNP", 0, 100, 50, 5}

static const VoiceDef kDefs[kVoices] = {
    {"bd", "Bass Drum",     "BD", 4, {TUN, ATK, DEC, LVL}},
    {"sd", "Snare Drum",    "SD", 5, {TUN, TON, SNP, DEC, LVL}},
    {"lt", "Low Tom",       "LT", 3, {TUN, DEC, LVL}},
    {"mt", "Mid Tom",       "MT", 3, {TUN, DEC, LVL}},
    {"ht", "Hi Tom",        "HT", 3, {TUN, DEC, LVL}},
    {"rs", "Rim Shot",      "RS", 1, {LVL}},
    {"cb", "Cowbell",       "CB", 1, {LVL}},
    {"cp", "Hand Clap",     "CP", 2, {SNP, LVL}},
    {"ma", "Maracas",       "MA", 1, {LVL}},
    {"cl", "Claves",        "CL", 1, {LVL}},
    {"cy", "Cymbal",        "CY", 3, {TUN, DEC, LVL}},
    {"oh", "Open Hi-Hat",   "OH", 2, {DEC, LVL}},
    {"ch", "Closed Hi-Hat", "CH", 1, {LVL}},
};

#undef LVL
#undef TUN
#undef DEC
#undef ATK
#undef TON
#undef SNP

const VoiceDef& voiceDef(int v) { return kDefs[std::max(0, std::min(kVoices - 1, v))]; }

static std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

int voiceIndex(const std::string& raw) {
    std::string id = lower(raw);
    if (id == "accent" || id == "ac") return kAccentSel;
    for (int v = 0; v < kVoices; ++v)
        if (id == kDefs[v].id) return v;
    return -1;
}

int paramIndex(int v, const std::string& raw) {
    if (v < 0 || v >= kVoices) return -1;
    std::string id = lower(raw);
    const VoiceDef& d = kDefs[v];
    for (int i = 0; i < d.nParams; ++i)
        if (id == d.params[i].id) return i;
    return -1;
}

Pattern::Pattern() {
    for (int v = 0; v < kVoices; ++v)
        for (int i = 0; i < kDefs[v].nParams; ++i)
            params[v][i] = kDefs[v].params[i].def;
}

const char* scaleId(Scale s) {
    switch (s) {
        case Scale::S16: return "16th";
        case Scale::S32: return "32nd";
        case Scale::T8: return "8th-triplet";
    }
    return "16th";
}

const char* scaleLabel(Scale s) {
    switch (s) {
        case Scale::S16: return "16th";
        case Scale::S32: return "32nd";
        case Scale::T8: return "8T";
    }
    return "16th";
}

bool parseScale(const std::string& raw, Scale& out) {
    std::string s = lower(raw);
    if (s == "16th" || s == "16") { out = Scale::S16; return true; }
    if (s == "32nd" || s == "32") { out = Scale::S32; return true; }
    if (s == "8th-triplet" || s == "8t" || s == "triplet") { out = Scale::T8; return true; }
    return false;
}

// ── Presets (same data and order as engine.ts) ───────────────────────────

struct PresetRow { const char* inst; std::vector<int> steps; };
struct Preset { const char* name; std::vector<PresetRow> rows; };

static const std::vector<Preset>& presets() {
    static const std::vector<Preset> p = {
        {"classic-house", {{"bd", {0, 4, 8, 12}}, {"sd", {4, 12}}, {"ch", {0, 2, 4, 6, 8, 10, 12, 14}},
                           {"oh", {2, 6, 10, 14}}, {"cp", {4, 12}}, {"accent", {0, 4, 8, 12}}}},
        {"electro", {{"bd", {0, 3, 6, 10, 12}}, {"sd", {4, 12}}, {"ch", {0, 2, 4, 6, 8, 10, 12, 14}},
                     {"cp", {4, 12}}, {"cb", {0, 4, 8, 12}}, {"rs", {2, 10}}, {"accent", {0, 6, 12}}}},
        {"trap", {{"bd", {0, 7, 8, 11}}, {"sd", {4, 12}},
                  {"ch", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
                  {"oh", {6, 14}}, {"accent", {0, 4, 8, 12}}}},
        {"bossa", {{"bd", {0, 6, 10}}, {"rs", {2, 5, 8, 12}}, {"ch", {0, 2, 4, 6, 8, 10, 12, 14}},
                   {"ma", {1, 3, 5, 7, 9, 11, 13, 15}}, {"accent", {0, 6, 10}}}},
        {"breakbeat", {{"bd", {0, 6, 10}}, {"sd", {4, 12}}, {"ch", {0, 2, 4, 6, 8, 10, 12, 14}},
                       {"oh", {3, 11}}, {"lt", {14}}, {"mt", {13}}, {"accent", {0, 4, 10, 12}}}},
        {"reggaeton", {{"bd", {0, 3, 4, 7, 8, 11, 12, 15}}, {"sd", {4, 12}},
                       {"ch", {0, 2, 4, 6, 8, 10, 12, 14}}, {"rs", {2, 6, 10, 14}},
                       {"accent", {0, 4, 8, 12}}}},
        {"minimal", {{"bd", {0, 8}}, {"ch", {0, 2, 4, 6, 8, 10, 12, 14}}, {"rs", {4, 12}},
                     {"accent", {0, 8}}}},
        {"afrobeat", {{"bd", {0, 5, 10}}, {"sd", {4, 12}},
                      {"ch", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
                      {"cb", {2, 6, 10, 14}}, {"cl", {3, 7, 11, 15}}, {"ht", {1, 9}},
                      {"accent", {0, 4, 10}}}},
    };
    return p;
}

const std::vector<std::string>& presetNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> n;
        for (const auto& p : presets()) n.push_back(p.name);
        return n;
    }();
    return names;
}

// ── Engine ────────────────────────────────────────────────────────────────

Engine::Engine() = default;

int Engine::slotIndex() const {
    return ((bank_ == 'B') ? 16 : 0) + (number_ - 1) * 2 + ((variation_ == 'B') ? 1 : 0);
}

Pattern& Engine::pattern() { return patterns_[slotIndex()]; }
const Pattern& Engine::pattern() const { return patterns_[slotIndex()]; }

void Engine::setSlot(char bank, int number, char variation) {
    bank = (char)std::toupper((unsigned char)bank);
    variation = (char)std::toupper((unsigned char)variation);
    if (bank == 'A' || bank == 'B') bank_ = bank;
    if (number >= 1 && number <= 8) number_ = number;
    if (variation == 'A' || variation == 'B') variation_ = variation;
}

std::string Engine::slotLabel() const {
    std::string s;
    s += bank_;
    s += (char)('0' + number_);
    s += '-';
    s += variation_;
    return s;
}

bool Engine::toggleStep(int step, int sel) {
    if (sel < 0) sel = selected_;
    if (step < 0 || step >= kSteps || sel > kAccentSel) return false;
    Pattern& p = pattern();
    bool& cell = (sel == kAccentSel) ? p.accent[step] : p.steps[sel][step];
    cell = !cell;
    return true;
}

bool Engine::setStep(int step, bool on, int sel) {
    if (sel < 0) sel = selected_;
    if (step < 0 || step >= kSteps || sel > kAccentSel) return false;
    Pattern& p = pattern();
    ((sel == kAccentSel) ? p.accent[step] : p.steps[sel][step]) = on;
    return true;
}

bool Engine::getStep(int step, int sel) const {
    if (sel < 0) sel = selected_;
    if (step < 0 || step >= kSteps || sel > kAccentSel) return false;
    const Pattern& p = pattern();
    return (sel == kAccentSel) ? p.accent[step] : p.steps[sel][step];
}

int Engine::countSteps(int sel) const {
    int n = 0;
    for (int i = 0; i < kSteps; ++i) n += getStep(i, sel) ? 1 : 0;
    return n;
}

int Engine::getParam(int v, const std::string& id) const {
    int i = paramIndex(v, id);
    return i < 0 ? 0 : pattern().params[v][i];
}

bool Engine::setParam(int v, const std::string& id, int value) {
    int i = paramIndex(v, id);
    if (i < 0) return false;
    const ParamDef& d = kDefs[v].params[i];
    pattern().params[v][i] = std::max(d.min, std::min(d.max, value));
    return true;
}

void Engine::toggleMute(int v) { if (v >= 0 && v < kVoices) muted_[v] = !muted_[v]; }
void Engine::toggleSolo(int v) { if (v >= 0 && v < kVoices) soloed_[v] = !soloed_[v]; }

bool Engine::hasSolo() const {
    return std::any_of(soloed_.begin(), soloed_.end(), [](bool b) { return b; });
}

bool Engine::shouldPlay(int v) const {
    if (hasSolo()) return soloed_[v];
    return !muted_[v];
}

void Engine::setTempo(int bpm, double nowMs) {
    tempo_ = std::max(35, std::min(300, bpm));
    if (playing_) nextAt_ = nowMs + gapAfter(step_);
}

void Engine::setAccentLevel(int v) { accentLevel_ = std::max(0, std::min(100, v)); }
void Engine::setMaster(int v) { master_ = std::max(0, std::min(100, v)); }
void Engine::setSwing(int v) { swing_ = std::max(0, std::min(100, v)); }

double Engine::baseStepMs() const {
    const double beat = 60000.0 / tempo_;
    switch (scale_) {
        case Scale::S16: return beat / 4;
        case Scale::S32: return beat / 8;
        case Scale::T8: return beat / 3;
    }
    return beat / 4;
}

double Engine::gapAfter(int step) const {
    const double base = baseStepMs();
    if (swing_ == 50) return base;
    const int last = std::max(1, pattern().lastStep);
    const int next = ((step < 0 ? -1 : step) + 1) % last;
    const double r = swing_ / 100.0 - 0.5;
    return (next % 2 == 1) ? base * (1 + r) : base * (1 - r);
}

StepEvent Engine::advance() {
    const Pattern& p = pattern();
    step_ = (step_ + 1) % std::max(1, p.lastStep);
    StepEvent ev;
    ev.step = step_;
    for (int v = 0; v < kVoices; ++v)
        if (p.steps[v][step_] && shouldPlay(v)) ev.voices.push_back(v);
    ev.accent = p.accent[step_];
    ev.gapMs = gapAfter(step_);
    return ev;
}

StepEvent Engine::start(double nowMs) {
    if (playing_) stop();
    playing_ = true;
    step_ = -1;
    StepEvent ev = advance();
    nextAt_ = nowMs + ev.gapMs;
    return ev;
}

void Engine::stop() {
    playing_ = false;
    step_ = -1;
    nextAt_ = 0;
}

std::vector<StepEvent> Engine::poll(double nowMs) {
    std::vector<StepEvent> out;
    if (!playing_) return out;
    if (nowMs - nextAt_ > std::max(40.0, baseStepMs())) {
        // Stalled (window hidden, debugger, sleep): skip ahead, play one step.
        StepEvent ev = advance();
        nextAt_ = nowMs + ev.gapMs;
        out.push_back(ev);
        return out;
    }
    // 0.5 ms tolerance: timers fire on the ms grid.
    while (nowMs + 0.5 >= nextAt_) {
        StepEvent ev = advance();
        nextAt_ += ev.gapMs;
        out.push_back(ev);
    }
    return out;
}

double Engine::msUntilNext(double nowMs) const {
    if (!playing_) return -1;
    return std::max(0.0, nextAt_ - nowMs);
}

void Engine::clearInstrument(int sel) {
    if (sel < 0) sel = selected_;
    Pattern& p = pattern();
    if (sel == kAccentSel) p.accent.fill(false);
    else if (sel < kVoices) p.steps[sel].fill(false);
}

void Engine::clearPattern() {
    Pattern& p = pattern();
    for (auto& row : p.steps) row.fill(false);
    p.accent.fill(false);
}

void Engine::setLastStep(int n) { pattern().lastStep = std::max(1, std::min(kSteps, n)); }

bool Engine::loadPreset(const std::string& raw) {
    const std::string name = lower(raw);
    for (const auto& pr : presets()) {
        if (name != pr.name) continue;
        clearPattern();
        Pattern& p = pattern();
        for (const auto& row : pr.rows) {
            int sel = voiceIndex(row.inst);
            for (int s : row.steps) {
                if (sel == kAccentSel) p.accent[s] = true;
                else if (sel >= 0) p.steps[sel][s] = true;
            }
        }
        return true;
    }
    return false;
}

void Engine::copyPattern() {
    clip_ = pattern();
    hasClip_ = true;
}

bool Engine::pastePattern() {
    if (!hasClip_) return false;
    Pattern& p = pattern();
    p.steps = clip_.steps;
    p.accent = clip_.accent;
    p.lastStep = clip_.lastStep;
    return true;
}

void Engine::randomize(double density) {
    std::uniform_real_distribution<double> U(0.0, 1.0);
    clearPattern();
    Pattern& p = pattern();
    p.steps[BD][0] = true;
    if (U(rng_) > 0.3) p.steps[BD][8] = true;
    p.steps[SD][4] = true;
    p.steps[SD][12] = true;
    const bool eighths = U(rng_) > 0.5;
    for (int i = 0; i < kSteps; ++i)
        if (!eighths || i % 2 == 0) p.steps[CH][i] = true;
    for (int v = 0; v < kVoices; ++v) {
        if (v == BD || v == SD || v == CH) continue;
        for (int i = 0; i < kSteps; ++i)
            if (U(rng_) < density * 0.5) p.steps[v][i] = true;
    }
    for (int i = 0; i < kSteps; ++i)
        if (i != 0 && i != 8 && U(rng_) < density) p.steps[BD][i] = true;
    for (int i = 0; i < kSteps; ++i)
        if (U(rng_) < density * 0.4) p.accent[i] = true;
}

}  // namespace tr808
