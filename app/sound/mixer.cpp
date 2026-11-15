#include "mixer.h"

#include <algorithm>
#include <cmath>

namespace sound {

static const double kPi = 3.14159265358979323846;

int64_t msToSample(double ms) { return (int64_t)std::floor(ms * kRate / 1000.0 + 0.5); }
double sampleToMs(int64_t s) { return (double)s * 1000.0 / kRate; }

Mixer::Mixer(size_t ringSamples) : ring_(ringSamples, 0) {}

uint64_t Mixer::insert(Pending p) {
    std::lock_guard<std::mutex> lk(mu_);
    const int64_t now = clock_.load();
    if (p.at < now) { p.at = now; ++late_; }
    p.seq = seq_++;
    // Sorted by time, then by arrival: notes at one time keep the caller's order.
    auto it = std::upper_bound(pending_.begin(), pending_.end(), p, [](const Pending& a, const Pending& b) {
        return a.at < b.at || (a.at == b.at && a.seq < b.seq);
    });
    const uint64_t id = p.seq;
    pending_.insert(it, std::move(p));
    if (pending_.size() > 8192) pending_.pop_back();   // runaway: drop the latest
    return id;
}

uint64_t Mixer::schedule(const Note& n, int64_t at) {
    Pending p{at, 0, true, n, nullptr, 0};
    return insert(std::move(p));
}

void Mixer::release(uint64_t id, int64_t at) {
    std::lock_guard<std::mutex> lk(mu_);
    at = std::max(at, clock_.load());
    for (auto& p : pending_)
        if (p.isNote && p.seq == id) { p.note.durMs = std::max(1.0, sampleToMs(at - p.at)); return; }
    if (spk_.on && spk_.id == id) { spk_.b = std::min(spk_.b, at); return; }
    for (auto& ch : ch_)
        if (ch.on && ch.id == id) {
            const double gate = std::max(0.0, sampleToMs(at - ch.a));
            if (gate < ch.n.durMs) {
                ch.n.durMs = gate;
                ch.end = at + msToSample(ch.n.patch.releaseMs);
            }
            return;
        }
}

void Mixer::scheduleSample(Pcm pcm, double gain, int64_t at) {
    if (!pcm || pcm->empty()) return;
    Pending p{at, 0, false, Note(), std::move(pcm), gain};
    insert(std::move(p));
}

void Mixer::activate(const Pending& p, int64_t s) {
    if (!p.isNote) {
        ++hits_;
        if (smp_.size() >= 64) smp_.erase(smp_.begin());   // too many ringing: drop the oldest
        smp_.push_back({p.pcm, p.gain, s});
        return;
    }
    ++notes_;
    const Note& n = p.note;
    if (n.voice == Voice::PcSpeaker) {
        // One speaker: this note replaces the one sounding.
        spk_.on = true;
        spk_.a = s;
        spk_.b = s + msToSample(n.durMs);
        spk_.hz = speakerHz(n.midi);
        spk_.ph = 0;
        spk_.id = p.seq;
        return;
    }
    int c = -1;
    for (int i = 0; i < kFmChannels; ++i) if (!ch_[i].on || ch_[i].end <= s) { c = i; break; }
    if (c < 0) {   // nine sounding: take the channel that started first
        c = 0;
        for (int i = 1; i < kFmChannels; ++i) if (ch_[i].a < ch_[c].a) c = i;
    }
    Channel& ch = ch_[c];
    ch.on = true;
    ch.n = n;
    ch.a = s;
    ch.end = s + msToSample(soundingMs(n));
    ch.prev = 0;
    ch.f = noteHz(n.midi);
    ch.vel = std::max(1, std::min(127, n.vel)) / 127.0;
    ch.phC = ch.phM = 0;
    ch.id = p.seq;
}

void Mixer::render(int16_t* out, size_t n) {
    std::lock_guard<std::mutex> lk(mu_);
    const int64_t base = clock_.load();
    const double bendMul = std::pow(2.0, bend_.load() / 12.0);
    for (size_t i = 0; i < n; ++i) {
        const int64_t s = base + (int64_t)i;
        while (!pending_.empty() && pending_.front().at <= s) {
            activate(pending_.front(), s);
            pending_.pop_front();
        }
        double mix = 0;
        if (spk_.on) {
            if (s >= spk_.b) spk_.on = false;
            else {
                const double amp = speakerPatch().level * 0.5;
                mix += spk_.ph < 0.5 ? amp : -amp;
                spk_.ph = std::fmod(spk_.ph + spk_.hz * bendMul / kRate, 1.0);
            }
        }
        for (auto& ch : ch_) {
            if (!ch.on) continue;
            if (s >= ch.end) { ch.on = false; continue; }
            const Patch& p = ch.n.patch;
            const double t = (double)(s - ch.a) / kRate;
            const double env = envelope(p, t * 1000.0, ch.n.durMs);
            const double mod = std::sin(2 * kPi * ch.phM + p.feedback * ch.prev) * p.index * (0.4 + 0.6 * env);
            ch.prev = mod;
            mix += std::sin(2 * kPi * ch.phC + mod) * env * ch.vel * p.level * 0.35;
            ch.phM = std::fmod(ch.phM + ch.f * bendMul * p.modRatio / kRate, 1.0);
            ch.phC = std::fmod(ch.phC + ch.f * bendMul * p.carRatio / kRate, 1.0);
        }
        for (size_t k = 0; k < smp_.size();) {
            const int64_t idx = s - smp_[k].a;
            if (idx >= (int64_t)smp_[k].pcm->size()) { smp_.erase(smp_.begin() + (long)k); continue; }
            mix += (*smp_[k].pcm)[(size_t)idx] / 32768.0 * smp_[k].gain;
            ++k;
        }
        if (mix > 1.0) mix = 1.0;
        if (mix < -1.0) mix = -1.0;
        const int16_t v = (int16_t)std::lrint(mix * 32767.0);
        if (out) out[i] = muted_.load() ? 0 : v;
        if (!ring_.empty()) {
            ring_[ringPos_] = v;
            ringPos_ = (ringPos_ + 1) % ring_.size();
            ringFill_ = std::min(ring_.size(), ringFill_ + 1);
        }
    }
    clock_.store(base + (int64_t)n);
}

Mixer::Stats Mixer::stats() const {
    std::lock_guard<std::mutex> lk(mu_);
    Stats st;
    st.notes = notes_;
    st.hits = hits_;
    st.late = late_;
    st.clock = clock_.load();
    for (const auto& c : ch_) if (c.on && c.end > st.clock) ++st.fmBusy;
    st.speaker = spk_.on && spk_.b > st.clock;
    st.samples = (int)smp_.size();
    return st;
}

std::vector<int16_t> Mixer::capture(size_t n, int64_t& endSample) const {
    std::lock_guard<std::mutex> lk(mu_);
    endSample = clock_.load();
    n = std::min(n, ringFill_);
    std::vector<int16_t> out(n);
    for (size_t i = 0; i < n; ++i) out[i] = ring_[(ringPos_ + ring_.size() - n + i) % ring_.size()];
    return out;
}

}  // namespace sound
