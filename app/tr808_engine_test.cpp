// tr808_engine_test: step timing, swing, transport, pattern edits, presets.
// Drives tr808::Engine with a fake millisecond clock. Exit code = failures.
#include "tr808_engine.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace tr808;

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++fails; std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

// Step index and time of every step fired between t0 and t1, polling every 1 ms.
struct Hit { int step; double t; };
static std::vector<Hit> run(Engine& e, double t0, double t1) {
    std::vector<Hit> out;
    StepEvent first = e.start(t0);
    out.push_back({first.step, t0});
    for (double t = t0 + 1; t <= t1; t += 1)
        for (const auto& ev : e.poll(t)) out.push_back({ev.step, t});
    return out;
}

static void testDefaults() {
    Engine e;
    CHECK(e.tempo() == 120);
    CHECK(e.slotLabel() == "A1-A");
    CHECK(!e.playing() && e.step() == -1);
    CHECK(e.getParam(BD, "level") == 80);
    CHECK(e.getParam(BD, "tune") == 50);
    CHECK(e.getParam(RS, "tune") == 0);         // RS has no tune
    CHECK(!e.setParam(RS, "tune", 10));
    CHECK(voiceIndex("BD") == BD && voiceIndex("ch") == CH && voiceIndex("accent") == kAccentSel);
    CHECK(voiceIndex("zz") == -1);
    CHECK(presetNames().size() == 8 && presetNames()[0] == "classic-house");
}

static void testTiming() {
    Engine e;
    CHECK(near(e.baseStepMs(), 125.0));          // 120 bpm 16ths
    e.setScale(Scale::S32);
    CHECK(near(e.baseStepMs(), 62.5));
    e.setScale(Scale::T8);
    CHECK(near(e.baseStepMs(), 500.0 / 3));
    e.setScale(Scale::S16);
    e.setTempo(400, 0);
    CHECK(e.tempo() == 300);
    e.setTempo(10, 0);
    CHECK(e.tempo() == 35);
    e.setTempo(120, 0);

    // Straight: 2 s at 120 bpm = 16 steps + the closing downbeat, 125 ms apart.
    auto hits = run(e, 1000, 3000);
    CHECK(hits.size() == 17);
    for (size_t i = 0; i < hits.size(); ++i) {
        CHECK(hits[i].step == (int)(i % 16));
        CHECK(near(hits[i].t, 1000 + 125.0 * i, 1.0));
    }
    e.stop();
    CHECK(!e.playing() && e.step() == -1 && e.poll(5000).empty());
    CHECK(e.msUntilNext(5000) < 0);
}

static void testSwing() {
    Engine e;
    e.setSwing(66);
    // The gap before an odd step grows, the gap before an even step shrinks.
    CHECK(near(e.gapAfter(0), 125 * 1.16));
    CHECK(near(e.gapAfter(1), 125 * 0.84));
    CHECK(near(e.gapAfter(0) + e.gapAfter(1), 250));  // pairs keep the tempo
    auto hits = run(e, 0, 1000);
    // Steps land at 0, 145, 250, 395, 500, ...
    CHECK(hits.size() >= 9);
    CHECK(near(hits[1].t, 145, 1.0));
    CHECK(near(hits[2].t, 250, 1.0));
    CHECK(near(hits[3].t, 395, 1.0));
    CHECK(near(hits[4].t, 500, 1.0));
    CHECK(near(hits[8].t, 1000, 1.0));
    e.setSwing(150);
    CHECK(e.swing() == 100);
}

static void testLastStepAndTempoChange() {
    Engine e;
    e.setLastStep(5);
    auto hits = run(e, 0, 125 * 10);
    std::vector<int> steps;
    for (auto& h : hits) steps.push_back(h.step);
    CHECK((steps == std::vector<int>{0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0}));
    e.setLastStep(99);
    CHECK(e.pattern().lastStep == 16);

    // A tempo change while playing schedules the next step from now.
    Engine f;
    f.start(0);
    f.poll(125);          // step 1 at 125
    f.setTempo(60, 130);  // base 250
    CHECK(near(f.msUntilNext(130), 250));
    CHECK(f.poll(379).empty());
    CHECK(f.poll(380).size() == 1 && f.step() == 2);
}

static void testStall() {
    Engine e;
    e.start(0);
    auto evs = e.poll(5000);  // 5 s stall: one step, not forty
    CHECK(evs.size() == 1);
    CHECK(near(e.msUntilNext(5000), 125));
    // A short slip (timer late by 20 ms) plays the step, no resync.
    CHECK(e.poll(5145).size() == 1);
    CHECK(near(e.msUntilNext(5145), 105));
    // 300 ms behind (a menu was open): one step, not two.
    CHECK(e.poll(5550).size() == 1);
}

static void testEditsAndEvents() {
    Engine e;
    CHECK(e.toggleStep(0));                // selected = BD
    CHECK(e.getStep(0, BD));
    CHECK(e.toggleStep(0, BD) && !e.getStep(0, BD));
    CHECK(!e.toggleStep(16));
    CHECK(e.setStep(4, true, SD));
    CHECK(e.setStep(4, true, kAccentSel));
    CHECK(e.setStep(4, true, CH));
    e.select(SD);
    CHECK(e.selected() == SD);
    e.clearInstrument();                   // clears SD
    CHECK(!e.getStep(4, SD) && e.getStep(4, CH));

    e.loadPreset("classic-house");
    CHECK(e.countSteps(BD) == 4 && e.countSteps(CH) == 8 && e.countSteps(kAccentSel) == 4);
    CHECK(e.getStep(4, CH) && e.getStep(4, SD));
    CHECK(!e.loadPreset("nope"));

    // Step events carry the voices that sound, mute and solo applied.
    StepEvent s0 = e.start(0);
    CHECK(s0.step == 0 && s0.accent);
    CHECK((s0.voices == std::vector<int>{BD, CH}));
    e.stop();
    e.toggleMute(CH);
    s0 = e.start(0);
    CHECK((s0.voices == std::vector<int>{BD}));
    e.stop();
    e.toggleSolo(CH);                      // solo beats mute
    s0 = e.start(0);
    CHECK((s0.voices == std::vector<int>{CH}));
    e.stop();
    e.toggleSolo(CH);
    e.toggleMute(CH);

    // Slots are separate patterns; copy/paste moves steps across.
    e.copyPattern();
    e.setSlot('B', 3, 'B');
    CHECK(e.slotLabel() == "B3-B");
    CHECK(e.countSteps(BD) == 0);
    CHECK(e.pastePattern());
    CHECK(e.countSteps(BD) == 4);
    e.setSlot('x', 9, 'q');                // invalid: unchanged
    CHECK(e.slotLabel() == "B3-B");
    e.clearPattern();
    CHECK(e.countSteps(BD) == 0 && e.countSteps(kAccentSel) == 0);
    e.setSlot('A', 1, 'A');
    CHECK(e.countSteps(BD) == 4);

    // Params clamp to their range.
    CHECK(e.setParam(BD, "tune", 140) && e.getParam(BD, "tune") == 100);
    CHECK(e.setParam(SD, "snappy", -3) && e.getParam(SD, "snappy") == 0);

    // Randomize keeps the TS anchors: BD on 1, SD on 5 and 13.
    e.seed(7);
    e.randomize(0.3);
    CHECK(e.getStep(0, BD) && e.getStep(4, SD) && e.getStep(12, SD));
    CHECK(e.countSteps(CH) == 8 || e.countSteps(CH) == 16);
}

int main() {
    testDefaults();
    testTiming();
    testSwing();
    testLastStepAndTempoChange();
    testStall();
    testEditsAndEvents();
    if (fails) std::fprintf(stderr, "%d check(s) failed\n", fails);
    else std::printf("tr808_engine_test: all checks passed\n");
    return fails;
}
