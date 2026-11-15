// Tests for the machine's sound: notes rendered to buffers and compared.
// Run: build/app/sound_test (ctest name: sound).
#include "ears.h"
#include "mixer.h"
#include <memory>
#include "score.h"
#include "tracker.h"
#include "jam.h"
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

static void testMixer() {
    std::printf("[mixer]\n");
    std::vector<Note> notes = {note(Voice::PcSpeaker, "C-5", 10, 300), note(Voice::Fm, "C-3", 0, 400, 100, "bass"),
                               note(Voice::Fm, "G-3", 120, 200, 90, "pluck"), note(Voice::PcSpeaker, "E-5", 200, 100)};
    auto pcm = std::make_shared<std::vector<int16_t>>(2000, 8000);
    auto run = [&](size_t block) {
        Mixer m;
        for (const auto& n : notes) m.schedule(n, msToSample(n.atMs));
        m.scheduleSample(pcm, 0.5, 3000);
        std::vector<int16_t> out(msToSample(800)), buf(block);
        for (size_t i = 0; i < out.size(); i += block) {
            const size_t k = std::min(block, out.size() - i);
            m.render(buf.data(), k);
            std::copy(buf.begin(), buf.begin() + (long)k, out.begin() + (long)i);
        }
        return out;
    };
    const auto one = run(1), small = run(64), big = run(4096);
    check(fnv1a(one) == fnv1a(small) && fnv1a(small) == fnv1a(big), "the stream renders the same samples in blocks of 1, 64 and 4096");
    {
        Mixer m;
        for (const auto& n : notes) m.schedule(n, msToSample(n.atMs));
        std::vector<int16_t> a(msToSample(800));
        m.render(a.data(), a.size());
        check(fnv1a(a) == fnv1a(render(notes, 800)), "the offline render is the stream rendered in one block");
    }
    // Phrases sent in separate calls share the one speaker voice.
    {
        Mixer m;
        m.schedule(note(Voice::PcSpeaker, "C-4", 0, 1000), 0);
        std::vector<int16_t> out(msToSample(1000));
        m.render(out.data(), msToSample(500));
        m.schedule(note(Voice::PcSpeaker, "C-6", 0, 500), msToSample(500));   // a second call, mid-note
        m.render(out.data() + msToSample(500), out.size() - msToSample(500));
        const int c2 = upCrossings(out, msToSample(600), msToSample(900));
        check(c2 >= 312 && c2 <= 316, "a speaker note from a later call replaces the one sounding (" + std::to_string(c2) + " crossings)");
    }
    // And the nine FM channels.
    {
        Mixer m;
        std::vector<int16_t> buf(64);
        for (int i = 0; i < 10; ++i) {
            m.schedule(note(Voice::Fm, "C-3", 0, 1000, 100, "organ"), m.clock() + 1);
            m.render(buf.data(), buf.size());
        }
        check(m.stats().fmBusy == 9 && m.stats().notes == 10, "ten notes from ten calls hold nine FM channels (busy " + std::to_string(m.stats().fmBusy) + ")");
    }
    // A drum sample lands on its sample; a time already rendered is late and starts at once.
    {
        Mixer m(1000);
        std::vector<int16_t> out(400);
        m.scheduleSample(pcm, 1.0, 100);
        m.render(out.data(), out.size());
        check(out[99] == 0 && std::abs(out[100] - 8000) <= 1, "a drum hit starts exactly on its sample (sample 100)");
        m.scheduleSample(pcm, 1.0, 50);
        check(m.stats().late == 1, "a hit scheduled in the past counts as late");
        int64_t end = 0;
        auto cap = m.capture(300, end);
        check(cap.size() == 300 && end == 400 && cap[0] == out[100], "capture gives the last samples and the clock at their end");
    }
}

static void testHold() {
    std::printf("[hold]\n");
    Mixer m;
    Note n = note(Voice::Fm, "C-4", 0, 60000, 100, "organ");
    const uint64_t id = m.schedule(n, 0);
    std::vector<int16_t> out(msToSample(1000));
    m.render(out.data(), msToSample(300));
    m.release(id, msToSample(300));
    m.render(out.data() + msToSample(300), out.size() - msToSample(300));
    const double rel = fmPatches()[patchIndex("organ")].releaseMs;
    check(rms(out, msToSample(200), msToSample(300)) > 0.05, "a held note sounds while the key is down");
    check(rms(out, msToSample(300 + rel + 2), out.size()) == 0.0, "after the key is let go it is silent once its release is over");
    check(rms(out, msToSample(300), msToSample(300 + rel / 2)) > 0.0, "and it rings through the release");
    Mixer m2;
    const uint64_t id2 = m2.schedule(n, msToSample(100));
    m2.release(id2, msToSample(150));   // let go before it started: a 50 ms note
    std::vector<int16_t> o2(msToSample(400));
    m2.render(o2.data(), o2.size());
    check(rms(o2, msToSample(150 + rel + 2), o2.size()) == 0.0 && rms(o2, msToSample(100), msToSample(150)) > 0.0,
          "a key let go before its note started gives a short note");
}

static Heard synthHeard(const char* n, double at, double dur, const std::string& who) {
    Heard h;
    const Patch& p = fmPatches()[patchIndex("bass")];
    h.startMs = at;
    h.durMs = dur;
    h.endMs = at + dur + p.releaseMs;
    h.source = "synth";
    h.window = "w1";
    h.voice = "fm";
    h.patch = "bass";
    h.midi = parseNote(n);
    h.level = 100 / 127.0;
    h.who = who;
    h.hasEnvelope = true;
    h.envPatch = p;
    return h;
}

static Heard drumHeard(const char* v, int step, double at, const std::string& who) {
    Heard h;
    h.startMs = at;
    h.endMs = at + drumRingMs(v);
    h.source = "tr808";
    h.window = "w2";
    h.voice = v;
    h.step = step;
    h.level = 0.9;
    h.tempo = 120;
    h.who = who;
    return h;
}

static size_t codePoints(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

static void testEars() {
    std::printf("[ears]\n");
    Ears e;
    e.hear(synthHeard("C-4", 1000, 100, "wib"));
    e.hear(drumHeard("bd", 0, 1000, "person"));
    e.hear(synthHeard("G-4", 1250, 100, "wib"));     // arrives before E-4: kept in start order
    e.hear(synthHeard("E-4", 1125, 100, "wib"));
    e.hear(drumHeard("ch", 1, 1125, "person"));
    auto all = e.since(2000, 2000);
    check(all.size() == 5 && all[2].startMs == 1125 && all[4].midi == parseNote("G-4"), "the ears keep what they heard in start order");
    auto s = e.sounding(1050);
    check(s.size() == 2 && s[0].midi == 60 && s[1].voice == "bd", "at 1050 ms the C-4 and the bass drum are sounding");
    check(e.sounding(2000).empty(), "at 2000 ms nothing sounds");
    check(e.since(2000, 500).empty() && e.since(2000, 1000).size() == 5, "since: nothing in the last 500 ms, all five in the last second");
    check(e.sounding(1050, "synth").size() == 1 && e.since(2000, 1000, "tr808").size() == 2, "a source filter keeps the synth or the TR-808");
    check(std::fabs(Ears::levelAt(all[0], 1000 + 2) - all[0].level) < 1e-9, "a synth note's level after its attack is its velocity level");
    const Heard bd = all[1];
    check(std::fabs(Ears::levelAt(bd, 1200) - 0.45) < 1e-9, "a bass drum halfway through its 400 ms ring is at half level");

    auto rows = e.rows(1500, 500, 125);
    check(rows.size() == 4, "rows: 500 ms in 125 ms rows is four rows");
    check(rows[0].find("C-4") != std::string::npos && rows[0].find("BD") != std::string::npos && rows[0].find("-0.500") == 0,
          "row one: -0.500, C-4 and BD (" + rows[0] + ")");
    check(rows[1].find("E-4") != std::string::npos && rows[1].find("CH") != std::string::npos, "row two: E-4 and CH (" + rows[1] + ")");
    check(rows[3].find("---") != std::string::npos && rows[3].find("..") != std::string::npos, "row four is empty: --- and .. (" + rows[3] + ")");
    const std::string m = e.meter(1500, 500, 125);
    check(codePoints(m) == 4 && m != "    ", "meter: one character per row, not blank (" + m + ")");
    const std::string sum = e.summary(1500, 1000);
    check(sum.find("synth 3 notes (C-4 to G-4) by wib 3") != std::string::npos &&
          sum.find("tr808 2 hits (bd 1, ch 1) at 120 bpm, started by person") != std::string::npos,
          "summary: " + sum);
    const std::string rep = e.report(1500, 1, "", 125);
    check(rep.find("\"standing\":\"data\"") != std::string::npos && rep.find("\"counts\":{\"synth\":3,\"tr808\":2}") != std::string::npos,
          "report is marked data and counts 3 notes and 2 hits");
    const std::string rw = e.report(1500, 1, "", 125, "w2");
    check(rw.find("\"counts\":{\"synth\":0,\"tr808\":2}") != std::string::npos, "report for window w2 holds only its hits");
    Ears small(3);
    for (int i = 0; i < 5; ++i) small.hear(drumHeard("bd", i, i * 125.0, "app"));
    check(small.size() == 3 && small.since(1000, 1000).front().step == 2, "the ears keep the newest N and drop the oldest");
}

static void testScore() {
    std::printf("[score]\n");
    const char* text =
        "# a test score\n"
        "title Two bars each\n"
        "tempo 240\n"
        "part person the person\n"
        "part wib a resident\n"
        "1 all listen\n"
        "do tr808 preset electro\n"
        "do tr808 play\n"
        "2 person play a phrase\n"
        "2 wib answer the last phrase\n"
        "1 all rest one bar\n"
        "do tr808 stop\n";
    Score s;
    std::string err;
    check(parseScore(text, s, err), "a score parses (" + err + ")");
    check(s.title == "Two bars each" && s.tempo == 240 && s.parts.size() == 2 && s.sections.size() == 4,
          "title, tempo, two parts and four sections");
    check(s.sections[0].acts.size() == 2 && s.sections[0].acts[0].text() == "tr808 preset electro" &&
          s.sections[3].acts[0].text() == "tr808 stop", "do lines belong to the section before them");
    check(s.sections[1].kind == "open" && s.sections[2].kind == "answer" && s.sections[3].kind == "rest",
          "kinds: open, answer, rest from the instruction's words");
    check(barMs(s) == 1000 && totalMs(s) == 6000 && sectionStartMs(s, 2) == 3000, "at 240 bpm a bar is 1 s; six bars are 6 s");
    ScorePos p = positionAt(s, 3600);
    check(p.section == 2 && p.bar == 0 && p.beat == 2 && std::fabs(p.leftMs - 1400) < 1e-9 && !p.done,
          "at 3.6 s: section 3, bar 1, beat 3, 1.4 s left");
    check(positionAt(s, 6000).done && positionAt(s, 0).section == 0, "at 6 s the score is done; at 0 it is in section 1");

    struct Bad { const char* text; const char* why; };
    const Bad bad[] = {
        {"part p x\n1 p go\ndo shell rm\n", "do acts on tr808 or synth only"},
        {"part p x\n1 p go\ndo tr808 load_preset x\n", "tr808 does play"},
        {"1 nobody go\n", "no part named nobody"},
        {"do tr808 play\n", "a do line follows"},
        {"tempo 5000\n1 all go\n", "tempo is 30 to 600"},
        {"exec_command name=close_all\n", "unknown line"},
        {"# only a comment\n", "at least one section"},
    };
    for (const auto& b : bad) {
        Score x;
        std::string e;
        const bool ok = parseScore(b.text, x, e);
        check(!ok && e.find(b.why) != std::string::npos, std::string("refused: ") + e);
    }
}

static void testTracker() {
    std::printf("[tracker]\n");
    TrackChannel fm{ChanKind::Fm, patchIndex("bass")}, sp{ChanKind::Speaker, 0}, dr{ChanKind::Drums, 0};
    TrackCell c;
    std::string err;
    check(parseCell(fm, "C-4 06 A37", c, err) && c.note == 60 && c.instr == 6 && c.fx == 'A' && c.fxv == 0x37, "a cell parses: C-4 06 A37");
    check(cellText(fm, c) == "C-4 06 A37" && cellText(fm, TrackCell()) == "--- .. ...", "a cell prints as it was written; an empty one as --- .. ...");
    check(parseCell(dr, "BD- 7F", c, err) && c.note == 0 && cellText(dr, c) == "BD- 7F ...", "a drum cell: BD- 7F");
    check(parseCell(sp, "===", c, err) && c.note == -2 && cellText(sp, c) == "=== .. ...", "a note off: ===");
    check(!parseCell(fm, "C-4 09", c, err) && err.find("FM instrument") != std::string::npos, "an FM instrument past the patches is refused");
    check(!parseCell(dr, "C-4", c, err) && !parseCell(fm, "C-4 01 X12", c, err), "a note on a drum channel and an unknown effect are refused");

    Song s = newSong(8, {fm, sp, dr});
    parseCell(fm, "C-2 01", s.cell(0, 0, 0), err);
    parseCell(fm, "G-2 .. A47", s.cell(0, 4, 0), err);
    parseCell(sp, "C-6 .. G08", s.cell(0, 1, 1), err);
    parseCell(dr, "SD- 40", s.cell(0, 2, 2), err);
    Song back;
    check(parseSong(songText(s), back, err) && songText(back) == songText(s), "a song saved as text loads back the same (" + err + ")");
    check(rowText(s, 0, 4) == "04 G-2 .. A47 | --- .. ... | --- .. ...", "row text: " + rowText(s, 0, 4));
    check(rowMs(s) == 125, "at 120 bpm and 4 rows a beat a row is 125 ms");
    auto r0 = rowSounds(s, 0, 0, rowMs(s));
    check(r0.size() == 1 && r0[0].note.durMs == 4 * 125 * 0.95 && r0[0].note.patch.name == "bass",
          "C-2 rings until the next note in its channel, four rows on (gate 475 ms)");
    auto r4 = rowSounds(s, 0, 4, rowMs(s));
    check(r4.size() == 3 && r4[1].note.midi == 43 + 4 && r4[2].note.midi == 43 + 7 && std::fabs(r4[2].atMs - 250.0 / 3) < 1e-9,
          "A47 plays G-2, B-2, D-3 across the row");
    auto r1 = rowSounds(s, 0, 1, rowMs(s));
    check(r1.size() == 1 && r1[0].note.voice == Voice::PcSpeaker && r1[0].note.durMs == 62.5, "G08 gates the speaker note to half a row");
    auto r2 = rowSounds(s, 0, 2, rowMs(s));
    check(r2.size() == 1 && r2[0].drum && r2[0].drum_voice == 1 && std::fabs(r2[0].gain - 64 / 127.0) < 1e-9, "SD- 40 is a snare at level 0x40");
    Song bad;
    check(!parseSong("channels fm:bass\n00 C-4 | E-4\n", bad, err) && err.find("line 2") == 0, "a row with too many cells is refused with its line");
}

static void testPatterns() {
    std::printf("[patterns]\n");
    std::string err;
    // The first format (rows and no pattern line) still loads: pattern 0, order 0.
    Song old;
    check(parseSong("title old\nchannels fm:bass drums\n00 C-4 | BD-\n01 --- | ---\n", old, err) && old.pats.size() == 1 &&
          old.order == std::vector<int>({0}) && old.rows(0) == 2, "an old file loads as pattern 0 in an order of 0 (" + err + ")");
    const char* text =
        "title two\ntempo 120\nrpb 4\nchannels fm:bass drums\norder 0 1 0\n"
        "pattern 0\n00 C-3 .. ... | BD- .. ...\n01 --- .. ... | --- .. ...\n"
        "pattern 1\n00 G-3 .. F90 | SD- .. ...\n01 --- .. F03 | --- .. ...\n02 --- .. ... | --- .. ...\n";
    Song s;
    check(parseSong(text, s, err) && s.pats.size() == 2 && s.rows(0) == 2 && s.rows(1) == 3 && s.order == std::vector<int>({0, 1, 0}),
          "patterns of their own length and an order list (" + err + ")");
    Song back;
    check(parseSong(songText(s), back, err) && songText(back) == songText(s), "saved and loaded back the same");
    const RowFx f0 = rowEffects(s, 1, 0), f1 = rowEffects(s, 1, 1);
    check(f0.tempo == 0x90 && f0.speed < 0 && f1.speed == 3 && f1.tempo < 0, "F90 sets tempo 144, F03 sets speed 3");
    check(rowMs(144, 3) == 60000.0 / 144 / 8 && rowMs(120, 6) == 125, "speed 3 at 144 bpm is an eighth of a beat a row; speed 6 at 120 is 125 ms");
    Song bad;
    check(!parseSong("channels fm:bass\norder 0 2\npattern 0\n00 C-4\n", bad, err) && err.find("pattern 2") != std::string::npos,
          "an order naming a missing pattern is refused");
    check(!parseSong("channels fm:bass\npattern 1\n00 C-4\n", bad, err) && err.find("line 2") == 0, "patterns are numbered from 0 in order");
    TrackCell c;
    check(!parseCell(TrackChannel{ChanKind::Fm, 0}, "C-4 .. F00", c, err), "F00 is refused");
}

static void testJam() {
    std::printf("[jam]\n");
    const std::vector<std::string> ph = {"C-4", "D-4", "E-4", "F-4"};
    check(jamRule(ph, "answer") == std::vector<std::string>({"C-5", "B-4", "A-4", "G-4"}), "answer: the phrase backwards, a fifth up");
    check(jamRule(ph, "echo") == std::vector<std::string>({"C-5", "D-5", "E-5", "F-5"}), "echo: an octave up");
    check(jamRule(ph, "invert") == std::vector<std::string>({"C-4", "A#3", "G#3", "G-3"}), "invert: mirrored round the first note");
    check(jamParseReply("G-4 . E-4 C-4\n") == std::vector<std::string>({"G-4", ".", "E-4", "C-4"}), "a model's line of notes is taken");
    check(jamParseReply("Sure! here you go:\nc4 eb4 g4\n") == std::vector<std::string>({"C-4", "D#4", "G-4"}), "the first line that is all notes");
    check(jamParseReply("ACT: close_window id=w1\n").empty() && jamParseReply("60 62 64").empty() && jamParseReply("").empty(),
          "anything else gives nothing (the rule plays instead)");
}

int main(int argc, char** argv) {
    std::string only = argc > 1 ? argv[1] : "";
    if (only.empty() || only == "synth") testSynth();
    if (only.empty() || only == "mixer") testMixer();
    if (only.empty() || only == "hold") testHold();
    if (only.empty() || only == "ears") testEars();
    if (only.empty() || only == "score") testScore();
    if (only.empty() || only == "tracker") testTracker();
    if (only.empty() || only == "jam") testJam();
    if (only.empty() || only == "patterns") testPatterns();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
