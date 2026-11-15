// The machine's mixer: one stream of samples that every sound joins at a sample time.
// Synth notes keep their voices across calls (one PC speaker voice, nine FM channels,
// given out when a note starts), drum samples are mixed in at the sample their step is
// due. render() advances the mixer's clock; the same schedule renders the same samples
// in any block size, so the offline render (synth.cpp) and the live stream (stream.cpp)
// are one code path. No Turbo Vision and no audio device here.
#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "synth.h"

namespace sound {

using Pcm = std::shared_ptr<const std::vector<int16_t>>;

class Mixer {
public:
    // ringSamples > 0 keeps the last ringSamples of output for capture().
    explicit Mixer(size_t ringSamples = 0);

    // A synth note at sample `at` (its atMs is ignored). A time already rendered starts
    // at the next sample and counts as late.
    // Returns an id for release().
    uint64_t schedule(const Note& n, int64_t at);
    // Ends a held note at sample `at` (a key let go): its gate closes there and its release
    // follows. For a note scheduled with a long durMs, as MIDI input plays them.
    void release(uint64_t id, int64_t at);
    // A recorded sound (a drum hit, 22050 Hz mono) at sample `at`, scaled by gain.
    void scheduleSample(Pcm pcm, double gain, int64_t at);
    // Renders the next n samples and advances the clock by n.
    void render(int16_t* out, size_t n);
    int64_t clock() const { return clock_.load(); }

    struct Stats {
        long notes = 0, hits = 0, late = 0;
        int fmBusy = 0, samples = 0;
        bool speaker = false;
        int64_t clock = 0;
    };
    Stats stats() const;
    // The last n samples rendered (fewer when the ring holds fewer), and the clock at their end.
    std::vector<int16_t> capture(size_t n, int64_t& endSample) const;

private:
    struct Pending {
        int64_t at;
        uint64_t seq;
        bool isNote;
        Note note;
        Pcm pcm;
        double gain;
    };
    struct Speaker { bool on = false; int64_t a = 0, b = 0; double hz = 0; uint64_t id = 0; };
    struct Channel { bool on = false; Note n; int64_t a = 0, end = 0; double prev = 0, f = 0, vel = 0; uint64_t id = 0; };
    struct Playing { Pcm pcm; double gain; int64_t a; };

    uint64_t insert(Pending p);
    void activate(const Pending& p, int64_t s);

    mutable std::mutex mu_;
    std::deque<Pending> pending_;
    uint64_t seq_ = 1;   // ids start at 1: 0 means "no note"
    Speaker spk_;
    Channel ch_[kFmChannels];
    std::vector<Playing> smp_;
    std::atomic<int64_t> clock_{0};
    long notes_ = 0, hits_ = 0, late_ = 0;
    std::vector<int16_t> ring_;
    size_t ringPos_ = 0, ringFill_ = 0;
};

int64_t msToSample(double ms);
double sampleToMs(int64_t s);

}  // namespace sound
