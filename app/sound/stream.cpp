#include "stream.h"

#include "tr808_audio.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace sound {

// From sound_host.cpp. Its header pulls in Turbo Vision, whose Boolean clashes with MacTypes.h.
double clockMs();
std::string jsonEscape(const std::string& s);

static const int kBlock = 256;           // samples per buffer (11.6 ms)
static const int kBuffers = 3;           // AudioQueue buffers in flight
static const int64_t kLead = 1024;       // samples between "now" and where a new sound starts (46 ms)

Mixer& mixer() {
    static Mixer* m = new Mixer((size_t)kRate * 10);   // never destroyed: the audio thread may outlive main
    return *m;
}

static std::atomic<int> g_kind{0};       // 0 off, 1 null, 2 coreaudio
static std::string g_note;               // why coreaudio was not used
static double g_offsetMs = 0;            // clockMs() when the stream started
static std::atomic<int64_t> g_blockEnd{0};
static std::atomic<double> g_blockSteady{0};
static std::atomic<double> g_lastMs{0};
static int64_t g_queued = 0;             // samples rendered ahead of what is heard

static double steadyMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

static void rendered() {
    g_blockEnd.store(mixer().clock());
    g_blockSteady.store(steadyMs());
}

const char* streamOut() { return g_kind == 2 ? "coreaudio" : g_kind == 1 ? "null" : "off"; }
bool streamRunning() { return g_kind != 0; }
double leadMs() { return streamRunning() ? sampleToMs(kLead) : 0.0; }

double streamMs() {
    const int64_t end = g_blockEnd.load();
    double ms = sampleToMs(end - g_queued) + (steadyMs() - g_blockSteady.load());
    ms = std::min(ms, sampleToMs(end + kBlock));          // never ahead of what is about to be rendered
    double last = g_lastMs.load();
    while (ms > last && !g_lastMs.compare_exchange_weak(last, ms)) {}
    return std::max(ms, g_lastMs.load());
}

double streamClockMs() { return g_offsetMs + streamMs(); }

int64_t sampleForClock(double clockMsAt) {
    return msToSample(clockMsAt - g_offsetMs) + kLead;
}

#ifdef __APPLE__
static AudioQueueRef g_queue = nullptr;

static void aqCallback(void*, AudioQueueRef q, AudioQueueBufferRef b) {
    const size_t n = b->mAudioDataBytesCapacity / 2;
    mixer().render(static_cast<int16_t*>(b->mAudioData), n);
    b->mAudioDataByteSize = (UInt32)(n * 2);
    rendered();
    AudioQueueEnqueueBuffer(q, b, 0, nullptr);
}

static bool startCoreAudio(std::string& why) {
    AudioStreamBasicDescription f;
    std::memset(&f, 0, sizeof(f));
    f.mSampleRate = kRate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    f.mBytesPerPacket = 2;
    f.mFramesPerPacket = 1;
    f.mBytesPerFrame = 2;
    f.mChannelsPerFrame = 1;
    f.mBitsPerChannel = 16;
    OSStatus st = AudioQueueNewOutput(&f, aqCallback, nullptr, nullptr, nullptr, 0, &g_queue);
    if (st != noErr) { why = "AudioQueueNewOutput " + std::to_string((int)st); return false; }
    for (int i = 0; i < kBuffers; ++i) {
        AudioQueueBufferRef b = nullptr;
        if (AudioQueueAllocateBuffer(g_queue, kBlock * 2, &b) != noErr) { why = "AudioQueueAllocateBuffer"; return false; }
        aqCallback(nullptr, g_queue, b);
    }
    st = AudioQueueStart(g_queue, nullptr);
    if (st != noErr) { why = "AudioQueueStart " + std::to_string((int)st); return false; }
    return true;
}
#endif

static void nullThread() {
    std::vector<int16_t> buf(kBlock);
    const double t0 = steadyMs();
    int64_t done = 0;
    for (;;) {
        mixer().render(buf.data(), buf.size());
        done += kBlock;
        rendered();
        const double due = t0 + sampleToMs(done);
        const double wait = due - steadyMs();
        if (wait > 0) std::this_thread::sleep_for(std::chrono::microseconds((long long)(wait * 1000)));
    }
}

void startStream() {
    static std::once_flag once;
    std::call_once(once, [] {
        std::string want = "coreaudio";
        if (const char* e = std::getenv("WWDOS_SOUND_OUT")) if (*e) want = e;
#ifndef __APPLE__
        if (want == "coreaudio") want = "null";
#endif
        if (want == "off") return;
        g_offsetMs = clockMs();
        rendered();
#ifdef __APPLE__
        if (want == "coreaudio") {
            g_queued = (int64_t)kBlock * kBuffers;
            std::string why;
            if (startCoreAudio(why)) { g_kind = 2; return; }
            g_note = "coreaudio failed (" + why + "), running null";
            std::fprintf(stderr, "[sound] %s\n", g_note.c_str());
        }
#endif
        g_queued = 0;
        g_kind = 1;
        std::thread(nullThread).detach();
    });
}

// ── Drum recordings ─────────────────────────────────────────────────────────

static std::vector<int16_t> readWav(const std::string& path) {
    std::vector<int16_t> out;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    unsigned char h[12];
    if (std::fread(h, 1, 12, f) != 12 || std::memcmp(h, "RIFF", 4) || std::memcmp(h + 8, "WAVE", 4)) { std::fclose(f); return out; }
    int channels = 1, bits = 16;
    unsigned char ck[8];
    while (std::fread(ck, 1, 8, f) == 8) {
        const uint32_t len = ck[4] | (ck[5] << 8) | (ck[6] << 16) | ((uint32_t)ck[7] << 24);
        if (!std::memcmp(ck, "fmt ", 4)) {
            std::vector<unsigned char> fm(len);
            if (std::fread(fm.data(), 1, len, f) != len) break;
            if (len >= 16) { channels = fm[2] | (fm[3] << 8); bits = fm[14] | (fm[15] << 8); }
        } else if (!std::memcmp(ck, "data", 4)) {
            if (bits != 16 || channels < 1) break;
            std::vector<int16_t> raw(len / 2);
            const size_t got = std::fread(raw.data(), 2, raw.size(), f);
            raw.resize(got);
            for (size_t i = 0; i + (size_t)channels <= raw.size(); i += (size_t)channels) out.push_back(raw[i]);   // first channel
            break;
        } else {
            std::fseek(f, (long)len + (len & 1), SEEK_CUR);
        }
    }
    std::fclose(f);
    return out;
}

Pcm drumSample(const std::string& id) {
    static std::mutex mu;
    static std::map<std::string, Pcm> cache;
    std::lock_guard<std::mutex> lk(mu);
    auto it = cache.find(id);
    if (it != cache.end()) return it->second;
    auto p = std::make_shared<std::vector<int16_t>>(readWav(tr808::samplesDir() + "/" + id + ".wav"));
    cache[id] = p;
    return p;
}

// ── Commands ─────────────────────────────────────────────────────────────────

std::string streamCommand(const std::string& action, double captureMs) {
    std::ostringstream o;
    const Mixer::Stats st = mixer().stats();
    if (action.empty() || action == "state") {
        o << "{\"ok\":true,\"out\":\"" << streamOut() << "\",\"running\":" << (streamRunning() ? "true" : "false")
          << ",\"rate\":" << kRate << ",\"block\":" << kBlock << ",\"buffers\":" << (g_kind == 2 ? kBuffers : 0)
          << ",\"lead_ms\":" << leadMs() << ",\"clock_ms\":" << (long long)clockMs()
          << ",\"samples\":" << st.clock << ",\"notes\":" << st.notes << ",\"hits\":" << st.hits << ",\"late\":" << st.late
          << ",\"fm_busy\":" << st.fmBusy << ",\"speaker\":" << (st.speaker ? "true" : "false") << ",\"ringing\":" << st.samples
          << ",\"note\":\"" << jsonEscape(g_note) << "\"}";
        return o.str();
    }
    if (action == "capture") {
        int64_t end = 0;
        const size_t n = (size_t)std::max<int64_t>(1, std::min<int64_t>(msToSample(captureMs), (int64_t)kRate * 10));
        auto s = mixer().capture(n, end);
        const int64_t first = end - (int64_t)s.size();
        // Onsets: 64-sample frames whose mean level jumps above four times the frames before.
        std::vector<double> fr;
        for (size_t i = 0; i + 64 <= s.size(); i += 64) {
            double a = 0;
            for (size_t k = 0; k < 64; ++k) a += std::abs((int)s[i + k]);
            fr.push_back(a / 64);
        }
        std::vector<double> onsets;
        double lastOn = -1e9;
        for (size_t i = 3; i < fr.size(); ++i) {
            const double before = std::max(30.0, (fr[i - 1] + fr[i - 2] + fr[i - 3]) / 3);
            const double at = g_offsetMs + sampleToMs(first + (int64_t)i * 64);
            if (fr[i] > 300 && fr[i] > 4 * before && at - lastOn > 30) {
                // the first sample in the frame that is not silent
                size_t k = 0;
                while (k < 64 && std::abs((int)s[i * 64 + k]) < 64) ++k;
                const double exact = g_offsetMs + sampleToMs(first + (int64_t)(i * 64 + std::min<size_t>(k, 63)));
                onsets.push_back(exact);
                lastOn = at;
            }
        }
        o << "{\"ok\":true,\"standing\":\"data\",\"out\":\"" << streamOut() << "\",\"rate\":" << kRate
          << ",\"samples\":" << s.size() << ",\"from_ms\":" << g_offsetMs + sampleToMs(first)
          << ",\"to_ms\":" << g_offsetMs + sampleToMs(end) << ",\"peak\":" << peak(s) << ",\"rms\":" << rms(s)
          << ",\"onsets_ms\":[";
        for (size_t i = 0; i < onsets.size(); ++i) o << (i ? "," : "") << onsets[i];
        o << "]}";
        return o.str();
    }
    return "err action is state|capture";
}

}  // namespace sound
