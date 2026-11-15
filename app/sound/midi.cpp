#include "midi.h"

#include "synth.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <sstream>

#ifdef __APPLE__
#include <CoreMIDI/CoreMIDI.h>
#include <mach/mach_time.h>
#endif

namespace sound {

double clockMs();   // sound_host.cpp
double leadMs();    // stream.cpp

namespace midi {

static std::mutex g_mu;
static std::deque<Msg> g_in, g_log;
static long g_inCount = 0, g_outCount = 0;
static std::atomic<int> g_kind{0};   // 0 off, 1 null, 2 coremidi
static std::atomic<int> g_sources{0};
static std::atomic<bool> g_clockOut{true};
static void (*g_wake)() = nullptr;

void setWake(void (*fn)()) { g_wake = fn; }
const char* device() { return g_kind == 2 ? "coremidi" : g_kind == 1 ? "null" : "off"; }
int sourcesConnected() { return g_sources.load(); }
bool clockOut() { return g_clockOut.load(); }
void setClockOut(bool on) { g_clockOut.store(on); }
long inCount() { std::lock_guard<std::mutex> lk(g_mu); return g_inCount; }
long outCount() { std::lock_guard<std::mutex> lk(g_mu); return g_outCount; }

int gmDrum(int v) {
    static const int map[13] = {36, 38, 41, 45, 48, 37, 56, 39, 70, 75, 49, 46, 42};
    return (v >= 0 && v < 13) ? map[v] : 36;
}

std::string hex(const std::vector<uint8_t>& b) {
    std::string s;
    char t[4];
    for (size_t i = 0; i < b.size(); ++i) { std::snprintf(t, sizeof(t), "%02X", b[i]); s += (i ? " " : "") + std::string(t); }
    return s;
}

bool parseHex(const std::string& s, std::vector<uint8_t>& out) {
    out.clear();
    std::istringstream is(s);
    for (std::string w; is >> w;) {
        if (w.size() > 2) return false;
        char* end = nullptr;
        long v = std::strtol(w.c_str(), &end, 16);
        if (!end || *end || v < 0 || v > 255) return false;
        out.push_back((uint8_t)v);
    }
    return !out.empty() && out.size() <= 64;
}

std::string describe(const std::vector<uint8_t>& b) {
    if (b.empty()) return "";
    const uint8_t st = b[0];
    if (st == 0xF8) return "clock";
    if (st == 0xFA) return "start";
    if (st == 0xFB) return "continue";
    if (st == 0xFC) return "stop";
    const int ch = (st & 0x0F) + 1;
    const int kind = st & 0xF0;
    char t[64];
    if ((kind == 0x90 || kind == 0x80) && b.size() >= 3) {
        const bool on = kind == 0x90 && b[2] > 0;
        if (on) std::snprintf(t, sizeof(t), "note on %s v%d ch%d", noteName(b[1]).c_str(), b[2], ch);
        else std::snprintf(t, sizeof(t), "note off %s ch%d", noteName(b[1]).c_str(), ch);
        return t;
    }
    if (kind == 0xB0 && b.size() >= 3) { std::snprintf(t, sizeof(t), "cc %d=%d ch%d", b[1], b[2], ch); return t; }
    if (kind == 0xC0 && b.size() >= 2) { std::snprintf(t, sizeof(t), "program %d ch%d", b[1], ch); return t; }
    if (kind == 0xE0 && b.size() >= 3) { std::snprintf(t, sizeof(t), "pitch bend %d ch%d", (b[2] << 7 | b[1]) - 8192, ch); return t; }
    return hex(b);
}

static void logMsg(const Msg& m) {
    g_log.push_back(m);
    while (g_log.size() > 1024) g_log.pop_front();
}

void inject(const std::vector<uint8_t>& bytes, const std::string& from) {
    {
        std::lock_guard<std::mutex> lk(g_mu);
        Msg m;
        m.ms = clockMs();
        m.out = false;
        m.bytes = bytes;
        m.from = from;
        g_in.push_back(m);
        while (g_in.size() > 1024) g_in.pop_front();
        logMsg(m);
        ++g_inCount;
    }
    if (g_wake) g_wake();
}

std::vector<Msg> takeIn() {
    std::lock_guard<std::mutex> lk(g_mu);
    std::vector<Msg> out(g_in.begin(), g_in.end());
    g_in.clear();
    return out;
}

std::vector<Msg> recent(size_t n) {
    std::lock_guard<std::mutex> lk(g_mu);
    const size_t k = std::min(n, g_log.size());
    std::vector<Msg> out(g_log.end() - (long)k, g_log.end());
    std::stable_sort(out.begin(), out.end(), [](const Msg& a, const Msg& b) { return a.ms < b.ms; });
    return out;
}

#ifdef __APPLE__
static MIDIClientRef g_client = 0;
static MIDIPortRef g_inPort = 0;
static MIDIEndpointRef g_src = 0, g_dst = 0;
static double g_nsPerTick = 1.0;

// Splits a packet into messages (running status kept within the packet).
static void readPackets(const MIDIPacketList* list, const std::string& from) {
    const MIDIPacket* p = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; ++i) {
        uint8_t status = 0;
        size_t k = 0;
        while (k < p->length) {
            uint8_t b = p->data[k];
            if (b >= 0xF8) { inject({b}, from); ++k; continue; }   // real time
            if (b == 0xF0) { while (k < p->length && p->data[k] != 0xF7) ++k; ++k; continue; }   // sysex: skipped
            if (b & 0x80) { status = b; ++k; }
            if (!status) { ++k; continue; }
            const int kind = status & 0xF0;
            const size_t need = (kind == 0xC0 || kind == 0xD0) ? 1 : 2;
            if (k + need > p->length) break;
            std::vector<uint8_t> m{status};
            for (size_t j = 0; j < need; ++j) m.push_back(p->data[k + j]);
            k += need;
            inject(m, from);
        }
        p = MIDIPacketNext(p);
    }
}

static void readProc(const MIDIPacketList* list, void*, void* srcConn) {
    const char* name = static_cast<const char*>(srcConn);
    readPackets(list, name ? name : "midi");
}

// Connects the input port to every source (a keyboard plugged in later too: called on setup changes).
static void connectAll() {
    if (!g_inPort) return;
    const ItemCount n = MIDIGetNumberOfSources();
    int connected = 0;
    for (ItemCount i = 0; i < n; ++i) {
        MIDIEndpointRef e = MIDIGetSource(i);
        if (e == g_src) continue;
        MIDIPortDisconnectSource(g_inPort, e);
        if (MIDIPortConnectSource(g_inPort, e, (void*)"keyboard") == noErr) ++connected;
    }
    g_sources = connected;
}

static void notifyProc(const MIDINotification* n, void*) {
    if (n && n->messageID == kMIDIMsgSetupChanged) connectAll();
}

static bool startCoreMidi() {
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    g_nsPerTick = (double)tb.numer / tb.denom;
    if (MIDIClientCreate(CFSTR("WibWob-DOS"), notifyProc, nullptr, &g_client) != noErr) return false;
    MIDISourceCreate(g_client, CFSTR("WibWob-DOS"), &g_src);
    MIDIDestinationCreate(g_client, CFSTR("WibWob-DOS"), readProc, (void*)"virtual", &g_dst);
    if (MIDIInputPortCreate(g_client, CFSTR("in"), readProc, nullptr, &g_inPort) != noErr) return true;
    connectAll();
    return true;
}
#endif

void start() {
    static std::once_flag once;
    std::call_once(once, [] {
        std::string want = "coremidi";
        if (const char* e = std::getenv("WWDOS_MIDI")) if (*e) want = e;
#ifndef __APPLE__
        if (want == "coremidi") want = "null";
#endif
        if (want == "off") return;
#ifdef __APPLE__
        if (want == "coremidi" && startCoreMidi()) { g_kind = 2; return; }
#endif
        g_kind = 1;
    });
}

void send(const std::vector<uint8_t>& bytes, double atClockMs, const std::string& from) {
    if (g_kind == 0 || bytes.empty()) return;
    const double due = atClockMs + leadMs();
    {
        std::lock_guard<std::mutex> lk(g_mu);
        Msg m;
        m.ms = due;
        m.out = true;
        m.bytes = bytes;
        m.from = from;
        logMsg(m);
        ++g_outCount;
    }
#ifdef __APPLE__
    if (g_kind == 2 && g_src) {
        const double aheadMs = std::max(0.0, due - clockMs());
        const MIDITimeStamp ts = mach_absolute_time() + (MIDITimeStamp)(aheadMs * 1e6 / g_nsPerTick);
        Byte buf[256];
        MIDIPacketList* list = reinterpret_cast<MIDIPacketList*>(buf);
        MIDIPacket* pk = MIDIPacketListInit(list);
        pk = MIDIPacketListAdd(list, sizeof(buf), pk, ts, bytes.size(), bytes.data());
        if (pk) MIDIReceived(g_src, list);
    }
#endif
}

}  // namespace midi
}  // namespace sound
