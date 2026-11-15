#include "synth_view.h"

#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TProgram
#include <tvision/tv.h>

#include "ears.h"
#include "sound_host.h"
#include "stream.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

using namespace sound;

// White keys of two octaves and the top C, with the key that plays each.
static const char* kWhiteLabels = "zxcvbnmqwertyui";
static const int kWhiteSemis[15] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24};

TSynthView::TSynthView(const TRect& bounds) : TView(bounds) {
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable | ofFirstClick;
    eventMask |= evBroadcast | evKeyDown;
}

TSynthView::~TSynthView() {
    if (timer_) killTimer(timer_);
}

const Patch& TSynthView::patch() const {
    return voice_ == Voice::PcSpeaker ? speakerPatch() : fmPatches()[patch_];
}

Note TSynthView::makeNote(int midi, double atMs, double durMs, int vel) const {
    Note n;
    n.atMs = atMs;
    n.voice = voice_;
    n.midi = midi;
    n.vel = vel;
    n.durMs = durMs;
    n.patch = fmPatches()[patch_];
    return n;
}

int TSynthView::keyToSemitone(char ch) const {
    static const char* lower = "zsxdcvgbhnjm,";
    static const char* upper = "q2w3er5t6y7ui";
    ch = (char)std::tolower((unsigned char)ch);
    for (int i = 0; lower[i]; ++i) if (lower[i] == ch) return i;
    for (int i = 0; upper[i]; ++i) if (upper[i] == ch) return 12 + i;
    return -1;
}

void TSynthView::playNotes(const std::vector<Note>& notes, const std::string& who) {
    if (notes.empty()) return;
    const double asked = clockMs();
    const double now = asked + leadMs();   // where the stream places a sound asked for now
    TWindow* win = dynamic_cast<TWindow*>(owner);
    const std::string wid = (win && host().idOf) ? host().idOf(win) : "";
    for (const auto& n : notes) {
        Played p{n.midi, n.voice, n.voice == Voice::Fm ? n.patch.name : std::string("square"), n.vel,
                 now + n.atMs, n.durMs, now + n.atMs + soundingMs(n), who};
        recent_.push_back(p);
        while (recent_.size() > 32) recent_.pop_front();
        Heard h;
        h.startMs = p.startMs;
        h.endMs = p.endMs;
        h.source = "synth";
        h.window = wid;
        h.voice = voiceId(n.voice);
        h.patch = p.patch;
        h.midi = n.midi;
        h.level = n.voice == Voice::Fm ? (n.vel / 127.0) * n.patch.level : speakerPatch().level;
        h.who = who;
        h.hasEnvelope = n.voice == Voice::Fm;
        h.envPatch = n.patch;
        h.durMs = n.durMs;
        ears().hear(h);
    }
    played_ += (long)notes.size();
    if (sendToSpeakers(notes, asked)) sent_ += (long)notes.size();
    else silenced_ += (long)notes.size();
    if (!timer_) timer_ = setTimer(50, 50);
    drawView();
}

void TSynthView::midiNote(int midi, int vel, bool on, const std::string& who) {
    if (midi < 0 || midi > 127) return;
    const double asked = clockMs();
    const double now = asked + leadMs();
    TWindow* win = dynamic_cast<TWindow*>(owner);
    const std::string wid = (win && host().idOf) ? host().idOf(win) : "";
    const double kHeldMs = 60000;   // a key held this long is let go by the mixer
    if (on) {
        if (held_.count(midi)) midiNote(midi, 0, false, who);   // the same key again: let the first go
        Note n = makeNote(midi, 0, kHeldMs, std::max(1, std::min(127, vel)));
        uint64_t id = 0;
        const bool audible = synthAudioEnabled() && streamRunning();
        if (audible) id = mixer().schedule(n, sampleForClock(asked));
        held_[midi] = {id, now};
        Played p{midi, voice_, voice_ == Voice::Fm ? n.patch.name : std::string("square"), n.vel, now, kHeldMs, now + 1e9, who};
        recent_.push_back(p);
        while (recent_.size() > 32) recent_.pop_front();
        ++played_;
        if (audible) ++sent_; else ++silenced_;
        Heard h;
        h.startMs = now;
        h.endMs = now + 1e9;
        h.held = true;
        h.source = "synth";
        h.window = wid;
        h.voice = voiceId(voice_);
        h.patch = p.patch;
        h.midi = midi;
        h.level = voice_ == Voice::Fm ? (n.vel / 127.0) * n.patch.level : speakerPatch().level;
        h.who = who;
        h.hasEnvelope = voice_ == Voice::Fm;
        h.envPatch = n.patch;
        h.durMs = kHeldMs;
        ears().hear(h);
    } else {
        auto it = held_.find(midi);
        if (it == held_.end()) return;
        if (it->second.id) mixer().release(it->second.id, sampleForClock(asked));
        const double dur = std::max(1.0, now - it->second.startMs);
        for (auto r = recent_.rbegin(); r != recent_.rend(); ++r)
            if (r->midi == midi && r->endMs > now + 1e8) {
                r->durMs = dur;
                r->endMs = now + (r->voice == Voice::Fm ? fmPatches()[patch_].releaseMs : 0);
                break;
            }
        ears().release(wid, midi, now);
        held_.erase(it);
    }
    if (!timer_) timer_ = setTimer(50, 50);
    drawView();
}

void TSynthView::tick() {
    const double now = clockMs();
    bool any = false;
    for (const auto& p : recent_) if (p.endMs > now) { any = true; break; }
    if (ears().lastStartMs() > now - 4000) any = true;   // the ears line still moving
    if (!any && timer_) { killTimer(timer_); timer_ = nullptr; }
    drawView();
}

void TSynthView::handleEvent(TEvent& ev) {
    TView::handleEvent(ev);
    if (ev.what == evBroadcast && ev.message.command == cmTimerExpired) {
        if (timer_ && ev.message.infoPtr == timer_) {
            tick();
            clearEvent(ev);
        }
        return;
    }
    if (ev.what != evKeyDown) return;
    if (ev.keyDown.controlKeyState & kbAltShift) return;  // Alt+key belongs to the menu bar
    const ushort key = ev.keyDown.keyCode;
    const char ch = (char)ev.keyDown.charScan.charCode;
    bool handled = true;
    if (key == kbTab) {
        voice_ = voice_ == Voice::Fm ? Voice::PcSpeaker : Voice::Fm;
    } else if (ch == '[') {
        octave_ = std::max(0, octave_ - 1);
    } else if (ch == ']') {
        octave_ = std::min(7, octave_ + 1);
    } else if (ch == '-') {
        patch_ = (patch_ + (int)fmPatches().size() - 1) % (int)fmPatches().size();
    } else if (ch == '=') {
        patch_ = (patch_ + 1) % (int)fmPatches().size();
    } else if (ch == '/') {
        static const int gates[] = {60, 125, 250, 500, 1000};
        int i = 0;
        for (int k = 0; k < 5; ++k) if (gates[k] == gateMs_) i = k;
        gateMs_ = gates[(i + 1) % 5];
    } else if (ch == '0') {
        setSynthAudioEnabled(!synthAudioEnabled());
    } else {
        const int semi = ch ? keyToSemitone(ch) : -1;
        if (semi >= 0) {
            const int midi = (octave_ + 1) * 12 + semi;
            if (midi >= 0 && midi <= 127) playNotes({makeNote(midi, 0, gateMs_, 100)}, currentActor());
        } else {
            handled = false;
        }
    }
    if (handled) {
        drawView();
        clearEvent(ev);
    }
}

// ── Draw ──────────────────────────────────────────────────────────────────

void TSynthView::draw() {
    TColorAttr base, dim, bright, lit;
    TColorAttr themed;
    if (ww::contentAttr(this, 15, themed)) {
        const TColor bg = themed.getBackground();
        base = themed;
        dim = TColorAttr(ww::egaRGB(7), bg);
        bright = TColorAttr(ww::egaRGB(15), bg);
        lit = TColorAttr(ww::egaRGB(0), ww::egaRGB(14));
    } else {
        const TColorRGB bk(0, 0, 0);
        base = TColorAttr(TColorRGB(0xAA, 0xAA, 0xAA), bk);
        dim = TColorAttr(TColorRGB(0x77, 0x77, 0x77), bk);
        bright = TColorAttr(TColorRGB(0xFF, 0xFF, 0xFF), bk);
        lit = TColorAttr(bk, TColorRGB(0xFF, 0xFF, 0xFF));
    }
    const int W = size.x, H = size.y;
    const double now = clockMs();
    const int baseMidi = (octave_ + 1) * 12;
    bool on[25] = {};
    for (const auto& p : recent_)
        if (p.startMs <= now && p.endMs > now && p.midi >= baseMidi && p.midi <= baseMidi + 24) on[p.midi - baseMidi] = true;

    for (int y = 0; y < H; ++y) {
        TDrawBuffer b;
        b.moveChar(0, ' ', base, (ushort)W);
        std::string line;
        TColorAttr a = base;
        if (y == 0) {
            std::ostringstream o;
            o << "SYNTH  voice " << (voice_ == Voice::Fm ? "FM (" + fmPatches()[patch_].name + ")" : std::string("PC SPEAKER"))
              << "  oct " << octave_ << "  gate " << gateMs_ << " ms  " << (synthAudioEnabled() ? "\u266b ON" : "\u266b --");
            line = o.str();
            a = bright;
            b.moveStr(1, line, a, (ushort)std::max(0, W - 1));
        } else if (y >= 2 && y <= 5) {
            // The keyboard: 15 white keys, 3 cells each, black keys over the borders.
            for (int x = 0; x <= 45 && x < W - 1; ++x) {
                const int wk = x / 3;
                const char* g = " ";
                TColorAttr ca = base;
                bool black = false;
                int semi = -1;
                if (y <= 4) {
                    // black key spans: after C D F G A of each octave, cols 3i+2 and 3i+3
                    for (int i = 0; i < 14; ++i) {
                        const int s = kWhiteSemis[i] % 12;
                        if ((s == 0 || s == 2 || s == 5 || s == 7 || s == 9) && (x == 3 * i + 2 || x == 3 * i + 3) && y <= 3) {
                            black = true;
                            semi = kWhiteSemis[i] + 1;
                        }
                    }
                }
                if (black) {
                    g = "\u2588";
                    ca = (semi >= 0 && semi <= 24 && on[semi]) ? lit : dim;
                    if (semi >= 0 && semi <= 24 && on[semi]) g = " ";
                } else if (y <= 4) {
                    if (x % 3 == 0) g = "\u2502";
                    else if (wk < 15 && on[kWhiteSemis[wk]]) ca = lit;
                } else {
                    if (x % 3 == 1 && wk < 15) {
                        char lab[2] = {kWhiteLabels[wk], 0};
                        b.moveStr((ushort)(x + 1), lab, dim);
                        continue;
                    }
                    continue;
                }
                b.moveStr((ushort)(x + 1), g, ca);
            }
        } else if (y == 6) {
            // What the ears hear from every source over the last 4 s, oldest on the left.
            b.moveStr(1, "ears ", dim);
            b.moveStr(6, ears().meter(now, 4000, 125), bright);
        } else if (y >= 7 && y < H - 1) {
            // The last notes, newest first.
            const int k = y - 7;
            if (k < (int)recent_.size()) {
                const Played& p = recent_[recent_.size() - 1 - k];
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s  %-5s %-6s v%-3d %4.0f ms  %s", noteName(p.midi).c_str(),
                              voiceId(p.voice), p.patch.c_str(), p.vel, p.durMs, p.who.c_str());
                b.moveStr(1, buf, (p.startMs <= now && p.endMs > now) ? bright : base);
            }
        } else if (y == H - 1 && H > 8) {
            b.moveStr(1, "z-m q-i play  Tab voice  [ ] octave  - = patch  / gate  0 sound", dim);
        }
        writeLine(0, (short)y, (short)W, 1, b);
    }
}

// ── API ───────────────────────────────────────────────────────────────────

static std::vector<std::string> tokens(const std::string& s) {
    std::vector<std::string> t;
    std::string cur;
    for (char c : s) {
        if (c == ' ' || c == ',' || c == '\t') { if (!cur.empty()) t.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) t.push_back(cur);
    return t;
}

std::string TSynthView::stateJson() const {
    std::ostringstream o;
    const double now = clockMs();
    o << "{\"ok\":true,\"voice\":\"" << voiceId(voice_) << "\",\"patch\":\"" << fmPatches()[patch_].name << "\""
      << ",\"octave\":" << octave_ << ",\"gate_ms\":" << gateMs_
      << ",\"audio\":" << (synthAudioEnabled() ? "true" : "false")
      << ",\"audio_available\":" << (synthAudioAvailable() ? "true" : "false")
      << ",\"played\":" << played_ << ",\"sent\":" << sent_ << ",\"silenced\":" << silenced_
      << ",\"patches\":[";
    for (size_t i = 0; i < fmPatches().size(); ++i) o << (i ? "," : "") << "\"" << fmPatches()[i].name << "\"";
    o << "],\"recent\":[";
    for (size_t i = 0; i < recent_.size(); ++i) {
        const Played& p = recent_[recent_.size() - 1 - i];
        o << (i ? "," : "") << "{\"note\":\"" << noteName(p.midi) << "\",\"midi\":" << p.midi
          << ",\"voice\":\"" << voiceId(p.voice) << "\",\"patch\":\"" << p.patch << "\",\"vel\":" << p.vel
          << ",\"dur_ms\":" << (long)p.durMs << ",\"ago_ms\":" << (long)(now - p.startMs)
          << ",\"who\":\"" << jsonEscape(p.who) << "\"}";
    }
    o << "]}";
    return o.str();
}

bool TSynthView::describeText(std::vector<std::string>& out) const {
    for (size_t i = 0; i < recent_.size(); ++i) {
        const Played& p = recent_[recent_.size() - 1 - i];
        out.push_back(noteName(p.midi) + " " + voiceId(p.voice) + " " + p.patch + " v" + std::to_string(p.vel) + " " + p.who);
    }
    return true;
}

std::string TSynthView::sentence() const {
    std::ostringstream o;
    o << "synth, " << (voice_ == Voice::Fm ? "FM voice with patch " + fmPatches()[patch_].name : std::string("PC speaker voice"))
      << ", octave " << octave_ << "; " << played_ << " notes played";
    if (!recent_.empty()) {
        const Played& p = recent_.back();
        char ago[32];
        std::snprintf(ago, sizeof(ago), "%.1f", std::max(0.0, clockMs() - p.startMs) / 1000.0);
        o << ", the last " << noteName(p.midi) << " by " << p.who << " " << ago << " s ago";
    }
    o << "; sound " << (synthAudioEnabled() ? "on" : "off");
    return o.str();
}

std::string TSynthView::apiAction(const std::string& action, const std::map<std::string, std::string>& kv) {
    Voice v = voice_;
    if (kvHas(kv, "voice") && !parseVoice(kvGet(kv, "voice"), v)) return "err voice is pcspk|fm";
    int pi = patch_;
    if (kvHas(kv, "patch") && action != "patch") {
        pi = patchIndex(kvGet(kv, "patch"));
        if (pi < 0) return "err unknown patch";
    }
    auto phrase = [&](std::vector<Note>& out, std::string& err) {
        const auto t = tokens(kvGet(kv, "notes", kvGet(kv, "note")));
        if (t.empty()) { err = "err missing notes"; return; }
        const int step = std::max(10, std::min(4000, toInt(kvGet(kv, "step"), 125)));
        const int dur = std::max(10, std::min(8000, toInt(kvGet(kv, "dur"), action == "note" ? gateMs_ : (int)(step * 0.9))));
        const int vel = std::max(1, std::min(127, toInt(kvGet(kv, "vel"), 100)));
        if (t.size() > 256) { err = "err at most 256 notes"; return; }
        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i] == "." || t[i] == "-" || t[i] == "r" || t[i] == "...") continue;
            const int m = parseNote(t[i]);
            if (m < 0) { err = "err not a note: " + t[i]; return; }
            Note n;
            n.atMs = (double)i * step;
            n.voice = v;
            n.midi = m;
            n.vel = vel;
            n.durMs = dur;
            n.patch = fmPatches()[pi];
            out.push_back(n);
        }
    };

    if (action.empty() || action == "state" || action == "open") {
        if (kvHas(kv, "voice")) voice_ = v;
        if (kvHas(kv, "patch")) patch_ = pi;
    } else if (action == "note" || action == "notes" || action == "play") {
        std::vector<Note> notes;
        std::string err;
        phrase(notes, err);
        if (!err.empty()) return err;
        playNotes(notes, currentActor());
    } else if (action == "render") {
        std::vector<Note> notes;
        std::string err;
        phrase(notes, err);
        if (!err.empty()) return err;
        const double ms = kvHas(kv, "ms") ? (double)toInt(kvGet(kv, "ms"), 0) : renderLengthMs(notes);
        if (ms > 60000) return "err at most 60000 ms";
        auto s = render(notes, ms);
        const size_t from = (size_t)std::max(0, toInt(kvGet(kv, "from_ms"), 0)) * kRate / 1000;
        const size_t to = kvHas(kv, "to_ms") ? (size_t)toInt(kvGet(kv, "to_ms"), 0) * kRate / 1000 : s.size();
        char h[24];
        std::snprintf(h, sizeof(h), "%016llx", (unsigned long long)fnv1a(s));
        std::ostringstream o;
        o << "{\"ok\":true,\"rate\":" << kRate << ",\"samples\":" << s.size() << ",\"ms\":" << (long)ms
          << ",\"fnv\":\"" << h << "\",\"peak\":" << peak(s) << ",\"rms\":" << rms(s, from, to)
          << ",\"up_crossings\":" << upCrossings(s, from, to) << ",\"notes\":" << notes.size() << "}";
        return o.str();
    } else if (action == "voice") {
        if (!parseVoice(kvGet(kv, "value", kvGet(kv, "voice")), v)) return "err voice is pcspk|fm";
        voice_ = v;
    } else if (action == "patch") {
        const int p = patchIndex(kvGet(kv, "value", kvGet(kv, "patch")));
        if (p < 0) return "err unknown patch";
        patch_ = p;
    } else if (action == "octave") {
        octave_ = std::max(0, std::min(7, toInt(kvGet(kv, "value"), octave_)));
    } else if (action == "gate") {
        gateMs_ = std::max(10, std::min(8000, toInt(kvGet(kv, "value"), gateMs_)));
    } else if (action == "audio") {
        const std::string on = kvGet(kv, "on");
        setSynthAudioEnabled(on.empty() ? !synthAudioEnabled() : truthy(on));
    } else {
        return "err unknown action";
    }
    drawView();
    return stateJson();
}

// ── Window ────────────────────────────────────────────────────────────────

TSynthWindow::TSynthWindow(const TRect& bounds)
    : TWindowInit(&TSynthWindow::initFrame), TWindow(bounds, "Synth", wnNoNumber) {
    options |= ofTileable;
    TRect c = getExtent();
    c.grow(-1, -1);
    view_ = new TSynthView(c);
    insert(view_);
}

TWindow* createSynthWindow(const TRect& bounds, const std::map<std::string, std::string>& kv) {
    auto* w = new TSynthWindow(bounds);
    if (kvHas(kv, "audio")) setSynthAudioEnabled(truthy(kvGet(kv, "audio")));
    std::map<std::string, std::string> init;
    if (kvHas(kv, "voice")) init["voice"] = kvGet(kv, "voice");
    if (kvHas(kv, "patch")) init["patch"] = kvGet(kv, "patch");
    w->view()->apiAction("state", init);
    if (kvHas(kv, "octave")) w->view()->apiAction("octave", {{"value", kvGet(kv, "octave")}});
    if (kvHas(kv, "gate")) w->view()->apiAction("gate", {{"value", kvGet(kv, "gate")}});
    return w;
}
