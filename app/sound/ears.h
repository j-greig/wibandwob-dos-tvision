// Ears: what the machine is sounding now and what it sounded in the last N seconds,
// kept as the engines played it (voice, note or step, level, who), so a resident can
// answer in time instead of playing blind. No Turbo Vision dependency: times are
// passed in (milliseconds on the app's clock). Commands: docs/machine-sound.md.
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "synth.h"

namespace sound {

struct Heard {
    double startMs = 0, endMs = 0;
    std::string source;      // "synth" or "tr808"
    std::string window;      // API id of the window that sounded it
    std::string voice;       // synth: pcspk|fm; tr808: bd, sd, ... ch
    std::string patch;       // synth FM patch, else ""
    int midi = -1;           // synth note, -1 for a drum
    int step = -1;           // tr808 step 0-15, -1 for a synth note
    double level = 0;        // at the start: synth velocity x patch level; tr808 the hit's playback volume
    bool accent = false;     // tr808 accent
    int tempo = 0;           // tr808 tempo
    std::string who;         // who played it (the synth) or started the pattern (the TR-808)
    // A tracker's sound: its channel, how many channels the song has, and the cell's text.
    int channel = -1;
    int channels = 0;
    std::string cell;
    // For the synth's level over time: its envelope.
    bool hasEnvelope = false;
    Patch envPatch;
    double durMs = 0;
};

class Ears {
public:
    explicit Ears(size_t keep = 4096) : keep_(keep) {}
    void hear(const Heard& h);
    void clear() { log_.clear(); }
    size_t size() const { return log_.size(); }
    double lastStartMs() const { return log_.empty() ? -1e300 : log_.back().startMs; }

    // Level 0-1 of h at time t: the synth by its envelope, a drum decaying linearly.
    static double levelAt(const Heard& h, double t);
    std::vector<Heard> sounding(double now, const std::string& source = "") const;
    // Started in [now - ms, now].
    std::vector<Heard> since(double now, double ms, const std::string& source = "") const;

    // A meter of the last `ms` in buckets of rowMs, one character per bucket, oldest first.
    std::string meter(double now, double ms, double rowMs, const std::string& source = "") const;
    // Tracker rows of the last `ms`, one per rowMs: "-0.750  C-4 E-4      | BD CH".
    std::vector<std::string> rows(double now, double ms, double rowMs, const std::string& source = "") const;
    // A tracker's rows as the ears heard them, in the tracker's own form ("03 C-2 01 ... | BD- 7F ..."):
    // rows start at originMs + k * rowMs; the rows that started in [now - ms, now) are given; the
    // row number is k modulo songRows.
    std::vector<std::string> cellRows(double now, double ms, double rowMs, double originMs, const std::string& window,
                                      int songRows) const;
    // One plain sentence of what was heard and what sounds now.
    std::string summary(double now, double ms, const std::string& source = "") const;
    // The whole answer of the hear command, as JSON. window filters by window id.
    std::string report(double now, double seconds, const std::string& source, double rowMs,
                       const std::string& window = "") const;

private:
    size_t keep_;
    std::deque<Heard> log_;
};

// The app's ears (one for the desktop).
Ears& ears();
// How long a TR-808 voice rings, for "sounding now" (bd 400 ms ... ch 60 ms).
double drumRingMs(const std::string& voice);

}  // namespace sound
