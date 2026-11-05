#include "tr808_view.h"

#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TProgram
#define Uses_TDeskTop
#include <tvision/tv.h>

#include "tr808_audio.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <sstream>
#include <vector>

using namespace tr808;

// ── Event sink ────────────────────────────────────────────────────────────

static Tr808EventSink gSink = nullptr;
void tr808SetEventSink(Tr808EventSink sink) { gSink = sink; }

static double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

static long long epochMs() {
    using namespace std::chrono;
    return (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// ── Glyphs (all one cell wide) ────────────────────────────────────────────

static const char* kGroupOn[4] = {"██", "▓▓", "▓▓", "▒▒"};  // ██ ▓▓ ▓▓ ▒▒
static const char* kOff = "░░";    // ░░
static const char* kEditOff = "▒▒";  // ▒▒
static const char* kAccOn = "▲ ";      // ▲
static const char* kAccOff = "▽ ";     // ▽
static const char* kKnob[5] = {"○", "◔", "◑", "◕", "●"};  // ○ ◔ ◑ ◕ ●
static const char* kBarOn = "▮";   // ▮
static const char* kBarOff = "▯";  // ▯

static const char* knob(int v, int mx) {
    int i = (int)((double)v / std::max(1, mx) * 5);
    return kKnob[std::max(0, std::min(4, i))];
}

static std::string bar(int v, int mx, int w) {
    int f = (int)((double)v / std::max(1, mx) * w + 0.5);
    std::string s;
    for (int i = 0; i < w; ++i) s += (i < f) ? kBarOn : kBarOff;
    return s;
}

// Display columns of a UTF-8 string whose glyphs are all one cell wide.
static int cols(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

// ── Palette ───────────────────────────────────────────────────────────────

struct TTR808View::Pal {
    TColorAttr base, dim, bright, off, sel, cursor, head, play, mute, solo, grp[4];
};

TTR808View::Pal TTR808View::palette() const {
    Pal p;
    TColorAttr themed;
    if (ww::contentAttr(this, 15, themed)) {
        // EGA theme or an explicit window colour: EGA colours on the window background.
        const TColorDesired bg = ::getBack(themed);
        auto on = [&](int idx) { return TColorAttr(ww::egaRGB(idx), bg); };
        p.base = themed;
        p.dim = on(7);
        p.bright = on(15);
        p.off = on(9);
        p.sel = on(11);
        p.cursor = TColorAttr(ww::egaRGB(0), ww::egaRGB(3));
        p.head = TColorAttr(ww::egaRGB(0), ww::egaRGB(7));
        p.play = on(10);
        p.mute = on(12);
        p.solo = on(14);
        p.grp[0] = on(12);
        p.grp[1] = on(14);
        p.grp[2] = on(14);
        p.grp[3] = on(15);
    } else {
        // Monochrome: greys on black; the step groups differ by glyph (█ ▓ ▓ ▒).
        const TColorRGB bk(0, 0, 0);
        auto g = [&](int v) { return TColorAttr(TColorRGB(v, v, v), bk); };
        p.base = g(0xAA);
        p.dim = g(0x77);
        p.bright = g(0xFF);
        p.off = g(0x44);
        p.sel = g(0xFF);
        p.cursor = TColorAttr(bk, TColorRGB(0xAA, 0xAA, 0xAA));
        p.head = TColorAttr(bk, TColorRGB(0xFF, 0xFF, 0xFF));
        p.play = g(0xFF);
        p.mute = g(0xFF);
        p.solo = g(0xFF);
        for (auto& a : p.grp) a = g(0xFF);
    }
    return p;
}

// ── View ──────────────────────────────────────────────────────────────────

TTR808View::TTR808View(const TRect& bounds) : TView(bounds) {
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable | ofFirstClick;
    eventMask |= evBroadcast | evKeyDown;
}

TTR808View::~TTR808View() {
    if (timer_) killTimer(timer_);
}

void TTR808View::publish(const char* type, const std::string& body) {
    if (gSink) gSink(dynamic_cast<TWindow*>(owner), type, body);
}

std::string TTR808View::transportBody(const char* change) const {
    std::ostringstream o;
    o << "\"state\":\"" << (eng_.playing() ? "playing" : "stopped") << "\""
      << ",\"change\":\"" << change << "\""
      << ",\"tempo\":" << eng_.tempo() << ",\"swing\":" << eng_.swing()
      << ",\"scale\":\"" << scaleId(eng_.scale()) << "\""
      << ",\"pattern\":\"" << eng_.slotLabel() << "\""
      << ",\"step_ms\":" << eng_.baseStepMs()
      << ",\"at\":" << epochMs();
    return o.str();
}

void TTR808View::fire(const StepEvent& ev) {
    const Pattern& p = eng_.pattern();
    std::ostringstream voices, levels;
    for (size_t i = 0; i < ev.voices.size(); ++i) {
        const int v = ev.voices[i];
        const int lvl = eng_.getParam(v, "level");
        if (i) { voices << ","; levels << ","; }
        voices << "\"" << voiceDef(v).id << "\"";
        levels << "\"" << voiceDef(v).id << "\":" << lvl;
        double vol = 0.9 * (ev.accent ? 1.0 + eng_.accentLevel() / 240.0 : 1.0)
                   * (lvl / 80.0) * (eng_.master() / 100.0);
        if (vol > 0.0) playVoice(voiceDef(v).id, vol);
    }
    // Only steps with hits (a voice or an accent) are published.
    if (ev.voices.empty() && !ev.accent) return;
    std::ostringstream o;
    o << "\"step\":" << ev.step << ",\"voices\":[" << voices.str() << "]"
      << ",\"levels\":{" << levels.str() << "}"
      << ",\"accent\":" << (ev.accent ? "true" : "false")
      << ",\"accent_level\":" << eng_.accentLevel() << ",\"master\":" << eng_.master()
      << ",\"tempo\":" << eng_.tempo() << ",\"swing\":" << eng_.swing()
      << ",\"scale\":\"" << scaleId(eng_.scale()) << "\""
      << ",\"gap_ms\":" << ev.gapMs << ",\"last_step\":" << p.lastStep
      << ",\"pattern\":\"" << eng_.slotLabel() << "\""
      << ",\"at\":" << epochMs();
    publish("tr808_step", o.str());
}

void TTR808View::schedule() {
    if (timer_) { killTimer(timer_); timer_ = nullptr; }
    if (!eng_.playing()) return;
    double ms = eng_.msUntilNext(nowMs());
    timer_ = setTimer((uint)std::max(1.0, ms + 0.5), -1);
}

void TTR808View::onTimer() {
    timer_ = nullptr;
    for (const auto& ev : eng_.poll(nowMs())) fire(ev);
    schedule();
    drawView();
}

void TTR808View::play() {
    if (eng_.playing()) return;
    StepEvent ev = eng_.start(nowMs());
    publish("tr808_transport", transportBody("play"));
    fire(ev);
    schedule();
    drawView();
}

void TTR808View::stop() {
    if (!eng_.playing()) return;
    eng_.stop();
    schedule();
    publish("tr808_transport", transportBody("stop"));
    drawView();
}

void TTR808View::setTempo(int bpm) {
    eng_.setTempo(bpm, nowMs());
    schedule();
    publish("tr808_transport", transportBody("tempo"));
}

// ── Draw ──────────────────────────────────────────────────────────────────

void TTR808View::draw() {
    const Pal P = palette();
    const int W = size.x, H = size.y;
    const int w = std::max(80, W);
    const int lblW = 5;
    const int stepW = kSteps * 3;
    const int paramW = std::min(45, std::max(16, w - lblW - stepW - 8));
    const int divX = lblW + 1 + paramW + 1;  // the │ column
    const int S = divX + 2;                  // first step cell
    const bool playing = eng_.playing();
    const int cur = eng_.step();
    const int sel = eng_.selected();

    std::vector<TDrawBuffer> rows(std::max(0, H));
    auto put = [&](int y, int x, const std::string& s, TColorAttr a) -> int {
        if (y < 0 || y >= H || x >= W || x < 0) return x + cols(s);
        rows[y].moveStr((ushort)x, s, a, (ushort)(W - x));
        return x + cols(s);
    };
    for (auto& b : rows) b.moveChar(0, ' ', P.base, (ushort)W);

    // Title
    {
        std::string t = "T R - 8 0 8   R H Y T H M   C O M P O S E R";
        put(0, std::max(0, (w - cols(t)) / 2), t, P.bright);
    }

    // Transport bar
    {
        int x = 2;
        x = put(2, x, playing ? "▶ PLAY" : "■ STOP", playing ? P.play : P.dim);
        x = put(2, x + 2, std::to_string(eng_.tempo()), P.bright);
        x = put(2, x + 1, "BPM", P.dim);
        x = put(2, x + 2, eng_.slotLabel(), P.sel);
        x = put(2, x + 2, scaleLabel(eng_.scale()), P.dim);
        if (eng_.swing() != 50) {
            x = put(2, x + 2, "SWG:", P.base);
            x = put(2, x, std::to_string(eng_.swing()) + "%", P.bright);
        }
        if (eng_.pattern().lastStep != kSteps) {
            x = put(2, x + 2, "LEN:", P.base);
            x = put(2, x, std::to_string(eng_.pattern().lastStep), P.bright);
        }
        x = put(2, x + 2, audioEnabled() ? "♫ ON" : "♫ --", audioEnabled() ? P.play : P.dim);
        x = put(2, x + 4, "VOL:", P.base);
        x = put(2, x, bar(eng_.master(), 100, 8), P.base);
        x = put(2, x + 2, "ACC:", P.base);
        put(2, x, bar(eng_.accentLevel(), 100, 8), P.base);
    }

    // Step numbers + separator
    put(4, divX, "│", P.dim);
    put(5, divX, "│", P.dim);
    for (int i = 0; i < kSteps; ++i) {
        char num[4];
        std::snprintf(num, sizeof(num), "%2d", i + 1);
        put(4, S + i * 3, num, P.grp[i / 4]);
    }
    {
        std::string sep;
        for (int i = 0; i < stepW; ++i) sep += "─";
        put(5, S, sep, P.dim);
    }

    auto stepCell = [&](int y, int i, bool on, const char* onGlyph, const char* offGlyph,
                        const char* editOffGlyph) {
        const bool isHead = playing && i == cur;
        const bool isEdit = !playing && i == cursor_;
        const int x = S + i * 3;
        if (isHead) put(y, x, on ? onGlyph : offGlyph, P.head);
        else if (isEdit && on) put(y, x, onGlyph, P.cursor);
        else if (isEdit) put(y, x, editOffGlyph, P.sel);
        else if (on) put(y, x, onGlyph, P.grp[i / 4]);
        else put(y, x, offGlyph, P.off);
    };

    // Voice rows
    for (int v = 0; v < kVoices; ++v) {
        const int y = 6 + v;
        const VoiceDef& d = voiceDef(v);
        const bool isSel = sel == v;
        if (isSel) put(y, 0, "▶", P.sel);
        put(y, 1, d.shortLabel, isSel ? P.sel : P.bright);
        if (eng_.isSoloed(v)) put(y, 3, "S", P.solo);
        else if (eng_.isMuted(v)) put(y, 3, "M", P.mute);
        int x = lblW + 1;
        for (int i = 0; i < d.nParams; ++i) {
            if (x + 4 > lblW + 1 + paramW) break;
            x = put(y, x, d.params[i].label, P.dim);
            x = put(y, x, knob(eng_.pattern().params[v][i], d.params[i].max), P.base);
            x += 1;
        }
        put(y, divX, "│", P.dim);
        for (int i = 0; i < kSteps; ++i)
            stepCell(y, i, eng_.pattern().steps[v][i], kGroupOn[i / 4], kOff, kEditOff);
    }

    // Accent row
    {
        const int y = 6 + kVoices;
        const bool isSel = sel == kAccentSel;
        if (isSel) put(y, 0, "▶", P.sel);
        put(y, 1, "AC", isSel ? P.sel : P.bright);
        int x = put(y, lblW + 1, "LVL", P.dim);
        x = put(y, x, bar(eng_.accentLevel(), 100, 8), P.base);
        put(y, x + 1, std::to_string(eng_.accentLevel()) + "%", P.base);
        put(y, divX, "│", P.dim);
        for (int i = 0; i < kSteps; ++i)
            stepCell(y, i, eng_.pattern().accent[i], kAccOn, kAccOff, kAccOff);
    }

    // Playhead / cursor marker
    {
        const int y = 8 + kVoices;
        if (playing && cur >= 0) put(y, S + cur * 3, "▲▲", P.bright);
        else if (cursor_ >= 0) put(y, S + cursor_ * 3, "▲▲", P.sel);
    }

    // Group labels: centred under each 4-step group, │ in the gaps between groups
    {
        const int y = 9 + kVoices;
        for (int g = 0; g < 4; ++g) {
            char lab[8];
            std::snprintf(lab, sizeof(lab), "%d-%d", g * 4 + 1, g * 4 + 4);
            const int gx = S + g * 12;
            put(y, gx + (11 - (int)std::strlen(lab)) / 2, lab, P.grp[g]);
            if (g > 0) put(y, gx - 1, "│", P.dim);
        }
    }

    // Keyboard help
    put(11 + kVoices, 2,
        "SPC:play/stop  ENTER:toggle  ←→:cursor  1-0,-,=,BKSP:inst  `:accent  "
        "a/z:tempo  v:var  b:bank  p:preset  m:audio", P.dim);
    put(12 + kVoices, 2,
        "F1-F4 F7 or !@#$%^&*:pattern  c/C:clear  r:random  s:scale  w:swing  "
        "x:mute  o:solo  ^C/^V:copy/paste  q:close", P.dim);

    for (int y = 0; y < H; ++y) writeLine(0, y, W, 1, rows[y]);
}

// ── Keyboard ──────────────────────────────────────────────────────────────

static int keyToSel(char c) {
    switch (c) {
        case '1': return BD; case '2': return SD; case '3': return LT; case '4': return MT;
        case '5': return HT; case '6': return RS; case '7': return CB; case '8': return CP;
        case '9': return MA; case '0': return CL; case '-': return CY; case '=': return OH;
        case '`': return kAccentSel;
    }
    return -1;
}

static int shiftDigitToPattern(char c) {
    const char* s = "!@#$%^&*";
    const char* p = std::strchr(s, c);
    return (p && c) ? (int)(p - s) + 1 : 0;
}

void TTR808View::handleEvent(TEvent& ev) {
    TView::handleEvent(ev);

    if (ev.what == evBroadcast && ev.message.command == cmTimerExpired) {
        if (timer_ && ev.message.infoPtr == timer_) {
            onTimer();
            clearEvent(ev);
        }
        return;
    }
    if (ev.what != evKeyDown) return;

    const ushort key = ev.keyDown.keyCode;
    const char ch = (char)ev.keyDown.charScan.charCode;
    const bool hasAlt = (ev.keyDown.controlKeyState & kbAltShift) != 0;
    if (hasAlt) return;  // Alt+key belongs to the menu bar
    bool handled = true;

    switch (key) {
        case kbEnter: eng_.toggleStep(cursor_); break;
        case kbLeft: cursor_ = (cursor_ - 1 + kSteps) % kSteps; break;
        case kbRight: cursor_ = (cursor_ + 1) % kSteps; break;
        case kbBack: eng_.select(CH); break;
        case kbCtrlC: eng_.copyPattern(); break;
        case kbCtrlV: eng_.pastePattern(); break;
        case kbEsc: {
            TEvent e;
            e.what = evCommand;
            e.message.command = cmClose;
            e.message.infoPtr = owner;
            putEvent(e);
            break;
        }
        case kbF1: eng_.setSlot(0, 1, 0); break;
        case kbF2: eng_.setSlot(0, 2, 0); break;
        case kbF3: eng_.setSlot(0, 3, 0); break;
        case kbF4: eng_.setSlot(0, 4, 0); break;
        case kbF5: eng_.setSlot(0, 5, 0); break;
        case kbF6: eng_.setSlot(0, 6, 0); break;
        case kbF7: eng_.setSlot(0, 7, 0); break;
        case kbF8: eng_.setSlot(0, 8, 0); break;
        default: handled = false;
    }

    if (!handled && ch) {
        handled = true;
        int s = keyToSel(ch);
        int pat = shiftDigitToPattern(ch);
        if (s >= 0) eng_.select(s);
        else if (pat > 0) eng_.setSlot(0, pat, 0);
        else switch (ch) {
            case ' ': if (eng_.playing()) stop(); else play(); break;
            case 'a': setTempo(eng_.tempo() + 5); break;
            case 'z': setTempo(eng_.tempo() - 5); break;
            case 'A': setTempo(eng_.tempo() + 1); break;
            case 'Z': setTempo(eng_.tempo() - 1); break;
            case 'v': eng_.setSlot(0, 0, eng_.variation() == 'A' ? 'B' : 'A'); break;
            case 'b': eng_.setSlot(eng_.bank() == 'A' ? 'B' : 'A', 0, 0); break;
            case 'c': eng_.clearInstrument(); break;
            case 'C': eng_.clearPattern(); break;
            case 'r': eng_.randomize(); break;
            case 's': {
                Scale next = eng_.scale() == Scale::S16 ? Scale::S32
                           : eng_.scale() == Scale::S32 ? Scale::T8 : Scale::S16;
                eng_.setScale(next);
                break;
            }
            case 'w': {
                static const int sw[] = {50, 55, 60, 66, 75};
                int idx = 0;
                for (int i = 0; i < 5; ++i) if (sw[i] == eng_.swing()) idx = i;
                eng_.setSwing(sw[(idx + 1) % 5]);
                break;
            }
            case 'p': {
                const auto& names = presetNames();
                eng_.loadPreset(names[presetIdx_ % names.size()]);
                ++presetIdx_;
                break;
            }
            case 'x': if (eng_.selected() < kVoices) eng_.toggleMute(eng_.selected()); break;
            case 'o': if (eng_.selected() < kVoices) eng_.toggleSolo(eng_.selected()); break;
            case 'm': setAudioEnabled(!audioEnabled()); break;
            case 'q': {
                TEvent e;
                e.what = evCommand;
                e.message.command = cmClose;
                e.message.infoPtr = owner;
                putEvent(e);
                break;
            }
            default: handled = false;
        }
    }

    if (handled) {
        drawView();
        clearEvent(ev);
    }
}

// ── API ───────────────────────────────────────────────────────────────────

static std::string get(const std::map<std::string, std::string>& kv, const char* k,
                       const std::string& dflt = "") {
    auto it = kv.find(k);
    return it == kv.end() ? dflt : it->second;
}

static bool has(const std::map<std::string, std::string>& kv, const char* k) {
    auto it = kv.find(k);
    return it != kv.end() && !it->second.empty();
}

static bool truthy(const std::string& s) {
    return s == "1" || s == "true" || s == "on" || s == "yes";
}

std::string TTR808View::stateJson() const {
    std::ostringstream o;
    o << "{\"ok\":true"
      << ",\"transport\":\"" << (eng_.playing() ? "playing" : "stopped") << "\""
      << ",\"step\":" << eng_.step()
      << ",\"tempo\":" << eng_.tempo()
      << ",\"bank\":\"" << eng_.bank() << "\",\"number\":" << eng_.number()
      << ",\"variation\":\"" << eng_.variation() << "\""
      << ",\"pattern\":\"" << eng_.slotLabel() << "\""
      << ",\"scale\":\"" << scaleId(eng_.scale()) << "\""
      << ",\"swing\":" << eng_.swing()
      << ",\"accent_level\":" << eng_.accentLevel() << ",\"master\":" << eng_.master()
      << ",\"last_step\":" << eng_.pattern().lastStep
      << ",\"selected\":\"" << (eng_.selected() == kAccentSel ? "accent" : voiceDef(eng_.selected()).id) << "\""
      << ",\"cursor\":" << cursor_
      << ",\"audio\":" << (audioEnabled() ? "true" : "false")
      << ",\"audio_available\":" << (audioAvailable() ? "true" : "false")
      << ",\"presets\":[";
    const auto& names = presetNames();
    for (size_t i = 0; i < names.size(); ++i) o << (i ? "," : "") << "\"" << names[i] << "\"";
    o << "],\"instruments\":[";
    for (int v = 0; v < kVoices; ++v) {
        const VoiceDef& d = voiceDef(v);
        std::string steps;
        for (int i = 0; i < kSteps; ++i) steps += eng_.pattern().steps[v][i] ? 'x' : '.';
        o << (v ? "," : "") << "{\"id\":\"" << d.id << "\",\"steps\":\"" << steps << "\""
          << ",\"muted\":" << (eng_.isMuted(v) ? "true" : "false")
          << ",\"soloed\":" << (eng_.isSoloed(v) ? "true" : "false") << ",\"params\":{";
        for (int i = 0; i < d.nParams; ++i)
            o << (i ? "," : "") << "\"" << d.params[i].id << "\":" << eng_.pattern().params[v][i];
        o << "}}";
    }
    std::string acc;
    for (int i = 0; i < kSteps; ++i) acc += eng_.pattern().accent[i] ? 'x' : '.';
    o << "],\"accent\":\"" << acc << "\"}";
    return o.str();
}

static int toInt(const std::string& s, int dflt) {
    if (s.empty()) return dflt;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    return (end && end != s.c_str()) ? (int)v : dflt;
}

std::string TTR808View::apiAction(const std::string& action,
                                  const std::map<std::string, std::string>& kv) {
    auto selArg = [&](const char* k, int dflt, bool& bad) {
        if (!has(kv, k)) return dflt;
        int s = voiceIndex(get(kv, k));
        if (s < 0) bad = true;
        return s;
    };
    bool bad = false;
    std::string err;

    if (action.empty() || action == "state" || action == "get_state" || action == "open") {
        // open: the caller already has (or just made) the window
    } else if (action == "play" || action == "start") {
        play();
    } else if (action == "stop") {
        stop();
    } else if (action == "toggle") {
        if (eng_.playing()) stop(); else play();
    } else if (action == "tempo") {
        if (!has(kv, "bpm")) return "err missing bpm";
        setTempo(toInt(get(kv, "bpm"), eng_.tempo()));
    } else if (action == "select") {
        int s = selArg("instrument", -1, bad);
        if (bad || s < 0) return "err unknown instrument";
        eng_.select(s);
    } else if (action == "toggle_step" || action == "set_step") {
        if (!has(kv, "step")) return "err missing step";
        int step = toInt(get(kv, "step"), -1);
        if (step < 0 || step >= kSteps) return "err step out of range (0-15)";
        int s = selArg("instrument", -1, bad);
        if (bad) return "err unknown instrument";
        if (action == "toggle_step") eng_.toggleStep(step, s);
        else {
            if (!has(kv, "active")) return "err missing active";
            eng_.setStep(step, truthy(get(kv, "active")), s);
        }
    } else if (action == "set_param") {
        int s = selArg("instrument", -1, bad);
        if (bad || s < 0 || s >= kVoices) return "err unknown instrument";
        if (!eng_.setParam(s, get(kv, "param"), toInt(get(kv, "value"), 0)))
            return "err unknown param for that instrument";
    } else if (action == "load_preset" || action == "preset") {
        if (!eng_.loadPreset(get(kv, "preset", get(kv, "name")))) return "err unknown preset";
    } else if (action == "clear") {
        if (truthy(get(kv, "all"))) eng_.clearPattern();
        else {
            int s = selArg("instrument", -1, bad);
            if (bad) return "err unknown instrument";
            eng_.clearInstrument(s);
        }
    } else if (action == "set_pattern") {
        std::string b = get(kv, "bank"), va = get(kv, "variation");
        eng_.setSlot(b.empty() ? 0 : b[0], toInt(get(kv, "number"), 0), va.empty() ? 0 : va[0]);
    } else if (action == "swing") {
        eng_.setSwing(toInt(get(kv, "value", get(kv, "swing")), eng_.swing()));
    } else if (action == "scale") {
        Scale sc;
        if (!parseScale(get(kv, "value", get(kv, "scale")), sc)) return "err scale is 16th|32nd|8th-triplet";
        eng_.setScale(sc);
    } else if (action == "accent") {
        eng_.setAccentLevel(toInt(get(kv, "value", get(kv, "level")), eng_.accentLevel()));
    } else if (action == "master") {
        eng_.setMaster(toInt(get(kv, "value", get(kv, "level")), eng_.master()));
    } else if (action == "last_step") {
        eng_.setLastStep(toInt(get(kv, "value", get(kv, "step")), eng_.pattern().lastStep));
    } else if (action == "mute" || action == "solo") {
        int s = selArg("instrument", -1, bad);
        if (bad || s < 0 || s >= kVoices) return "err unknown instrument";
        if (action == "mute") eng_.toggleMute(s); else eng_.toggleSolo(s);
    } else if (action == "audio") {
        std::string on = get(kv, "on");
        setAudioEnabled(on.empty() ? !audioEnabled() : truthy(on));
    } else if (action == "random" || action == "randomize") {
        double d = has(kv, "density") ? std::atof(get(kv, "density").c_str()) : 0.3;
        eng_.randomize(d);
    } else if (action == "copy") {
        eng_.copyPattern();
    } else if (action == "paste") {
        if (!eng_.pastePattern()) return "err clipboard empty";
    } else if (action == "cursor") {
        int c = toInt(get(kv, "step"), cursor_);
        if (c < 0 || c >= kSteps) return "err step out of range (0-15)";
        cursor_ = c;
    } else if (action == "text") {
        err = textCommand(get(kv, "text"));
        if (!err.empty()) return err;
    } else {
        return "err unknown action";
    }
    drawView();
    return stateJson();
}

// The text grammar of the TS microapp's input handler ("tempo 140", "toggle bd 4", ...).
std::string TTR808View::textCommand(const std::string& line) {
    std::istringstream is(line);
    std::vector<std::string> t;
    for (std::string w; is >> w;) {
        for (auto& c : w) c = (char)std::tolower((unsigned char)c);
        t.push_back(w);
    }
    if (t.empty()) return "err empty text";
    std::map<std::string, std::string> kv;
    const std::string& c = t[0];
    auto run = [&](const char* action) {
        std::string r = apiAction(action, kv);
        return r.rfind("err", 0) == 0 ? r : std::string();
    };
    auto isNum = [](const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), [](char ch) { return std::isdigit((unsigned char)ch); });
    };
    if (c == "play" || c == "start" || c == "stop" || c == "toggle") {
        if (c != "toggle" || t.size() == 1) return run(c.c_str());
    }
    if (c == "toggle" && t.size() >= 2) {
        if (t.size() == 2 && isNum(t[1])) { kv["step"] = t[1]; return run("toggle_step"); }
        if (t.size() == 3) { kv["instrument"] = t[1]; kv["step"] = t[2]; return run("toggle_step"); }
    }
    if (c == "tempo" && t.size() == 2) { kv["bpm"] = t[1]; return run("tempo"); }
    if (c == "select" && t.size() == 2) { kv["instrument"] = t[1]; return run("select"); }
    if (c == "set" && t.size() == 4) {
        kv["instrument"] = t[1]; kv["step"] = t[2]; kv["active"] = (t[3] == "on") ? "1" : "0";
        return run("set_step");
    }
    if (c == "param" && t.size() == 4) {
        kv["instrument"] = t[1]; kv["param"] = t[2]; kv["value"] = t[3];
        return run("set_param");
    }
    if (c == "bank" && t.size() == 2) { kv["bank"] = t[1]; return run("set_pattern"); }
    if (c == "variation" && t.size() == 2) { kv["variation"] = t[1]; return run("set_pattern"); }
    if (c == "pattern" && t.size() == 2) { kv["number"] = t[1]; return run("set_pattern"); }
    if (c == "preset" && t.size() == 2) { kv["preset"] = t[1]; return run("load_preset"); }
    if (c == "clear") { if (t.size() == 2 && t[1] == "all") kv["all"] = "1"; return run("clear"); }
    if (c == "scale" && t.size() == 2) { kv["value"] = t[1]; return run("scale"); }
    if (c == "accent" && t.size() == 2) { kv["value"] = t[1]; return run("accent"); }
    if (c == "master" && t.size() == 2) { kv["value"] = t[1]; return run("master"); }
    if (c == "laststep" && t.size() == 2) { kv["value"] = t[1]; return run("last_step"); }
    if (c == "swing" && t.size() == 2) { kv["value"] = t[1]; return run("swing"); }
    if (c == "mute" && t.size() == 2) { kv["instrument"] = t[1]; return run("mute"); }
    if (c == "solo" && t.size() == 2) { kv["instrument"] = t[1]; return run("solo"); }
    if (c == "mute" && t.size() == 1) { kv["on"] = "0"; return run("audio"); }
    if (c == "unmute") { kv["on"] = "1"; return run("audio"); }
    if (c == "audio" && t.size() == 2 && t[1] == "toggle") return run("audio");
    if (c == "copy" || c == "paste") return run(c.c_str());
    if (c == "random" || c == "randomize") { if (t.size() == 2) kv["density"] = t[1]; return run("random"); }
    return "err unknown text command";
}

// ── Window ────────────────────────────────────────────────────────────────

TTR808Window::TTR808Window(const TRect& bounds)
    : TWindowInit(&TTR808Window::initFrame),
      TWindow(bounds, "TR-808 Rhythm Composer", wnNoNumber) {
    options |= ofTileable;
    TRect c = getExtent();
    c.grow(-1, -1);
    view_ = new TTR808View(c);
    insert(view_);
}

TWindow* createTR808Window(const TRect& bounds, const std::map<std::string, std::string>& kv) {
    auto* w = new TTR808Window(bounds);
    TTR808View* v = w->view();
    if (has(kv, "audio")) setAudioEnabled(truthy(get(kv, "audio")));
    if (has(kv, "preset")) v->engine().loadPreset(get(kv, "preset"));
    if (has(kv, "tempo")) v->engine().setTempo(toInt(get(kv, "tempo"), 120), 0);
    return w;
}
