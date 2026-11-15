#include "tracker_view.h"

#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TProgram
#include <tvision/tv.h>

#include "ears.h"
#include "sound_host.h"
#include "stream.h"
#include "tr808_audio.h"
#include "midi.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

using namespace sound;

static const char* kLower = "zsxdcvgbhnjm,";
static const char* kUpper = "q2w3er5t6y7ui";

static int semitoneOf(char ch) {
    ch = (char)std::tolower((unsigned char)ch);
    for (int i = 0; kLower[i]; ++i) if (kLower[i] == ch) return i;
    for (int i = 0; kUpper[i]; ++i) if (kUpper[i] == ch) return 12 + i;
    return -1;
}

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)std::tolower((unsigned char)c);
    return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
}

TTrackerView::TTrackerView(const TRect& bounds, const Song& song, const std::string& origin)
    : TView(bounds), song_(song), origin_(origin) {
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable | ofFirstClick;
    eventMask |= evBroadcast | evKeyDown;
}

TTrackerView::~TTrackerView() {
    if (timer_) killTimer(timer_);
}

std::string TTrackerView::windowId() const {
    TWindow* w = dynamic_cast<TWindow*>(owner);
    return (w && host().idOf) ? host().idOf(w) : "";
}

void TTrackerView::play(int fromRow) {
    if (timer_) { killTimer(timer_); timer_ = nullptr; }
    playing_ = true;
    startedBy_ = currentActor();
    fromRow = std::max(0, std::min(song_.rows() - 1, fromRow));
    // Row fromRow plays now; startMs_ is where row 0 of this pass would have been.
    startMs_ = clockMs() - fromRow * rowMs(song_);
    nextRow_ = fromRow;
    if (midi::clockOut()) midi::send({0xFA}, clockMs(), "tracker");
    timer_ = setTimer(5, 5);
    tick();
}

void TTrackerView::stop() {
    if (playing_ && midi::clockOut()) midi::send({0xFC}, clockMs(), "tracker");
    playing_ = false;
    playRow_ = -1;
    if (timer_) { killTimer(timer_); timer_ = nullptr; }
    drawView();
}

void TTrackerView::playRow(int row, double atMs) {
    const std::string wid = windowId();
    const int chans = (int)song_.chans.size();
    // MIDI out: the row's clock ticks (24 a beat) and its notes: channel n+1 for tracker channel n, drums on 10.
    if (midi::clockOut()) {
        const int ticks = std::max(1, 24 / std::max(1, song_.rpb));
        for (int k = 0; k < ticks; ++k) midi::send({0xF8}, atMs + k * rowMs(song_) / ticks, "tracker");
    }
    for (const RowSound& r : rowSounds(song_, row)) {
        if (r.drum) {
            const uint8_t key = (uint8_t)midi::gmDrum(r.drum_voice);
            midi::send({0x99, key, (uint8_t)std::max(1, (int)std::lround(r.gain * 127))}, atMs + r.atMs, "tracker");
            midi::send({0x89, key, 0}, atMs + r.atMs + 50, "tracker");
        } else {
            const uint8_t ch = (uint8_t)std::min(15, r.chan == 9 ? 10 : r.chan);
            midi::send({(uint8_t)(0x90 | ch), (uint8_t)r.note.midi, (uint8_t)r.note.vel}, atMs + r.atMs, "tracker");
            midi::send({(uint8_t)(0x80 | ch), (uint8_t)r.note.midi, 0}, atMs + r.atMs + r.note.durMs, "tracker");
        }
    }
    for (const RowSound& r : rowSounds(song_, row)) {
        Heard h;
        h.window = wid;
        h.channel = r.chan;
        h.channels = chans;
        h.cell = r.cell;
        h.who = startedBy_;
        h.startMs = atMs + leadMs() + r.atMs;
        if (r.drum) {
            const std::string id = drumId(r.drum_voice);
            if (streamRunning() && tr808::audioEnabled())
                mixer().scheduleSample(drumSample(id), r.gain * 0.7, sampleForClock(atMs) + msToSample(r.atMs));
            h.source = "tr808";
            h.voice = id;
            h.endMs = h.startMs + drumRingMs(id);
            h.level = r.gain;
            h.tempo = song_.tempo;
            h.step = row;
        } else {
            if (streamRunning() && synthAudioEnabled())
                mixer().schedule(r.note, sampleForClock(atMs) + msToSample(r.atMs));
            h.source = "synth";
            h.voice = voiceId(r.note.voice);
            h.patch = r.note.voice == Voice::Fm ? r.note.patch.name : "square";
            h.midi = r.note.midi;
            h.durMs = r.note.durMs;
            h.endMs = h.startMs + soundingMs(r.note);
            h.level = r.note.voice == Voice::Fm ? (r.note.vel / 127.0) * r.note.patch.level : speakerPatch().level;
            h.hasEnvelope = r.note.voice == Voice::Fm;
            h.envPatch = r.note.patch;
        }
        ears().hear(h);
    }
}

void TTrackerView::tick() {
    if (!playing_) return;
    const double rm = rowMs(song_);
    const double now = clockMs();
    while (playing_ && startMs_ + nextRow_ * rm <= now + 0.5) {
        if (!loop_ && nextRow_ >= song_.rows()) { stop(); return; }
        const int row = (int)(nextRow_ % song_.rows());
        playRow(row, startMs_ + nextRow_ * rm);
        playRow_ = row;
        ++nextRow_;
    }
    drawView();
}

void TTrackerView::moveCursor(int dr, int dc) {
    curRow_ = (curRow_ + dr + song_.rows() * 4) % song_.rows();
    const int n = (int)song_.chans.size();
    curChan_ = (curChan_ + dc + n * 4) % n;
}

void TTrackerView::handleEvent(TEvent& ev) {
    TView::handleEvent(ev);
    if (ev.what == evBroadcast && ev.message.command == cmTimerExpired) {
        if (timer_ && ev.message.infoPtr == timer_) { tick(); clearEvent(ev); }
        return;
    }
    if (ev.what != evKeyDown) return;
    if (ev.keyDown.controlKeyState & kbAltShift) return;
    const ushort key = ev.keyDown.keyCode;
    const char ch = (char)ev.keyDown.charScan.charCode;
    TrackCell& cell = song_.grid[curRow_][curChan_];
    const TrackChannel& chan = song_.chans[curChan_];
    bool handled = true;
    switch (key) {
        case kbUp: moveCursor(-1, 0); break;
        case kbDown: moveCursor(1, 0); break;
        case kbPgUp: moveCursor(-song_.rpb, 0); break;
        case kbPgDn: moveCursor(song_.rpb, 0); break;
        case kbLeft:
            if (curField_ > 0) --curField_; else { curField_ = 2; moveCursor(0, -1); }
            break;
        case kbRight:
            if (curField_ < 2) ++curField_; else { curField_ = 0; moveCursor(0, 1); }
            break;
        case kbTab: curField_ = 0; moveCursor(0, 1); break;
        case kbShiftTab: curField_ = 0; moveCursor(0, -1); break;
        case kbEnter: if (playing_) stop(); else play(curRow_); break;
        case kbCtrlL: loop_ = !loop_; break;
        case kbBack: case kbDel:
            if (curField_ == 0) cell = TrackCell();
            else if (curField_ == 1) cell.instr = -1;
            else { cell.fx = 0; cell.fxv = 0; }
            break;
        default: handled = false;
    }
    if (!handled && ch) {
        handled = true;
        if (ch == ' ') { if (playing_) stop(); else play(0); }
        else if (ch == '[') octave_ = std::max(0, octave_ - 1);
        else if (ch == ']') octave_ = std::min(8, octave_ + 1);
        else if (ch == '.') {
            if (curField_ == 0) cell = TrackCell();
            else if (curField_ == 1) cell.instr = -1;
            else { cell.fx = 0; cell.fxv = 0; }
        } else if (curField_ == 0) {
            if (ch == '1') { cell.note = -2; moveCursor(1, 0); }
            else {
                const int s = semitoneOf(ch);
                if (s < 0) handled = false;
                else if (chan.kind == ChanKind::Drums) {
                    if (s < 13) { cell.note = s; moveCursor(1, 0); }
                } else {
                    const int m = (octave_ + 1) * 12 + s;
                    if (m <= 127) {
                        cell.note = m;
                        if (instr_ >= 0) cell.instr = instr_;
                        moveCursor(1, 0);
                    }
                }
            }
        } else if (curField_ == 1) {
            const int d = hexDigit(ch);
            if (d < 0) handled = false;
            else cell.instr = (((cell.instr < 0 ? 0 : cell.instr) << 4) | d) & 0xFF;
        } else {
            const char up = (char)std::toupper((unsigned char)ch);
            const int d = hexDigit(ch);
            if (!cell.fx && (up == 'A' || up == 'V' || up == 'G')) { cell.fx = up; cell.fxv = 0; }
            else if (cell.fx && d >= 0) cell.fxv = ((cell.fxv << 4) | d) & 0xFF;
            else handled = false;
        }
    }
    if (handled) { drawView(); clearEvent(ev); }
}

void TTrackerView::draw() {
    TColorAttr base, dim, bright, cur, playA;
    TColorAttr themed;
    if (ww::contentAttr(this, 15, themed)) {
        const TColor bg = themed.getBackground();
        base = themed;
        dim = TColorAttr(ww::egaRGB(7), bg);
        bright = TColorAttr(ww::egaRGB(15), bg);
        cur = TColorAttr(ww::egaRGB(0), ww::egaRGB(14));
        playA = TColorAttr(ww::egaRGB(0), ww::egaRGB(11));
    } else {
        const TColorRGB bk(0, 0, 0);
        base = TColorAttr(TColorRGB(0xAA, 0xAA, 0xAA), bk);
        dim = TColorAttr(TColorRGB(0x66, 0x66, 0x66), bk);
        bright = TColorAttr(TColorRGB(0xFF, 0xFF, 0xFF), bk);
        cur = TColorAttr(bk, TColorRGB(0xFF, 0xFF, 0xFF));
        playA = TColorAttr(bk, TColorRGB(0xAA, 0xAA, 0xAA));
    }
    const int W = size.x, H = size.y;
    const int vis = std::max(1, H - 3);
    const int focus = playing_ && playRow_ >= 0 ? playRow_ : curRow_;
    const int top = std::max(0, std::min(focus - vis / 2, song_.rows() - vis));
    for (int y = 0; y < H; ++y) {
        TDrawBuffer b;
        b.moveChar(0, ' ', base, (ushort)W);
        if (y == 0) {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "TRACKER  %s  %d bpm  %d rows/beat  %s%s  oct %d  instr %s",
                          song_.title.c_str(), song_.tempo, song_.rpb, playing_ ? "\u25b6 row " : "\u25a0 stopped",
                          playing_ ? (std::to_string(playRow_) + "/" + std::to_string(song_.rows())).c_str() : "",
                          octave_, instr_ < 0 ? ".." : std::to_string(instr_).c_str());
            b.moveStr(1, std::string(buf) + (loop_ ? "  loop" : ""), bright);
        } else if (y == 1) {
            std::string h = "   ";
            for (size_t c = 0; c < song_.chans.size(); ++c) {
                std::string n = chanName(song_.chans[c]);
                n = n.substr(0, 10);
                n.resize(10, ' ');
                h += (c ? " | " : "") + n;
            }
            b.moveStr(1, h, dim);
        } else if (y >= 2 && y < 2 + vis) {
            const int r = top + (y - 2);
            if (r < song_.rows()) {
                char head[8];
                std::snprintf(head, sizeof(head), "%02d ", r);
                const bool isPlay = playing_ && r == playRow_;
                b.moveStr(1, head, isPlay ? playA : (r % song_.rpb == 0 ? bright : dim));
                int x = 4;
                for (size_t c = 0; c < song_.chans.size(); ++c) {
                    if (c) { b.moveStr((ushort)x, " | ", dim); x += 3; }
                    const std::string t = cellText(song_.chans[c], song_.grid[r][c]);   // "NNN II FFF"
                    const int fs[3] = {0, 4, 7}, fl[3] = {3, 2, 3};
                    for (int f = 0; f < 3; ++f) {
                        TColorAttr a = isPlay ? playA : (song_.grid[r][c].empty() ? dim : base);
                        if (r == curRow_ && (int)c == curChan_ && f == curField_) a = cur;
                        b.moveStr((ushort)(x + fs[f]), t.substr(fs[f], fl[f]), a);
                    }
                    x += 10;
                }
            }
        } else if (y == H - 1) {
            b.moveStr(1, "z-m q-i notes  1 off  . clear  arrows move  [ ] octave  Space play  Enter from here  ^L loop", dim);
        }
        writeLine(0, (short)y, (short)W, 1, b);
    }
}

std::string TTrackerView::describeState() const {
    int filled = 0;
    for (const auto& row : song_.grid) for (const auto& c : row) if (!c.empty()) ++filled;
    std::ostringstream o;
    o << "tracker \"" << song_.title << "\", " << song_.chans.size() << " channels, " << song_.rows() << " rows at "
      << song_.tempo << " bpm, " << filled << " cells written; "
      << (playing_ ? "playing row " + std::to_string(playRow_) + (loop_ ? ", looping" : "") : std::string("stopped"));
    return o.str();
}

bool TTrackerView::describeText(std::vector<std::string>& out) const {
    for (int r = 0; r < song_.rows(); ++r) out.push_back(rowText(song_, r));
    return true;
}

std::string TTrackerView::stateJson() const {
    std::ostringstream o;
    o << "{\"ok\":true,\"standing\":\"data\",\"title\":\"" << jsonEscape(song_.title) << "\",\"from\":\"" << jsonEscape(origin_)
      << "\",\"tempo\":" << song_.tempo << ",\"rpb\":" << song_.rpb << ",\"rows\":" << song_.rows()
      << ",\"row_ms\":" << rowMs(song_) << ",\"playing\":" << (playing_ ? "true" : "false")
      << ",\"loop\":" << (loop_ ? "true" : "false") << ",\"play_row\":" << playRow_
      << ",\"started_ms\":" << startMs_ << ",\"lead_ms\":" << leadMs() << ",\"started_by\":\"" << jsonEscape(startedBy_) << "\""
      << ",\"cursor\":{\"row\":" << curRow_ << ",\"ch\":" << curChan_ << ",\"field\":\""
      << (curField_ == 0 ? "note" : curField_ == 1 ? "instr" : "fx") << "\"},\"octave\":" << octave_
      << ",\"channels\":[";
    for (size_t c = 0; c < song_.chans.size(); ++c) o << (c ? "," : "") << "\"" << chanName(song_.chans[c]) << "\"";
    o << "],\"lines\":[";
    for (int r = 0; r < song_.rows(); ++r) o << (r ? "," : "") << "\"" << rowText(song_, r) << "\"";
    o << "]}";
    return o.str();
}

static std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> t;
    std::string cur;
    for (char c : s) {
        if (c == ' ' || c == ',' || c == '\t') { if (!cur.empty()) t.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) t.push_back(cur);
    return t;
}

std::string TTrackerView::apiAction(const std::string& action, const std::map<std::string, std::string>& kv) {
    auto chanArg = [&](int& c) {
        c = toInt(kvGet(kv, "ch"), curChan_);
        return c >= 0 && c < (int)song_.chans.size();
    };
    auto rowArg = [&](int& r) {
        r = toInt(kvGet(kv, "row"), curRow_);
        return r >= 0 && r < song_.rows();
    };
    if (action.empty() || action == "state" || action == "open") {
    } else if (action == "play") {
        play(toInt(kvGet(kv, "row"), 0));
    } else if (action == "stop") {
        stop();
    } else if (action == "loop") {
        const std::string on = kvGet(kv, "on");
        loop_ = on.empty() ? !loop_ : truthy(on);
    } else if (action == "set") {
        int r, c;
        if (!rowArg(r)) return "err row out of range";
        if (!chanArg(c)) return "err ch out of range";
        TrackCell cell;
        std::string err;
        if (!parseCell(song_.chans[c], kvGet(kv, "cell", "---"), cell, err)) return "err " + err;
        song_.grid[r][c] = cell;
    } else if (action == "write") {
        // A column of notes from row= down: notes="C-2 . E-2 ===", every step= rows (default 1).
        int r, c;
        if (!rowArg(r)) return "err row out of range";
        if (!chanArg(c)) return "err ch out of range";
        const int step = std::max(1, toInt(kvGet(kv, "step"), 1));
        const std::string tail = std::string(kvHas(kv, "instr") ? " " + kvGet(kv, "instr") : " ..") +
                                 (kvHas(kv, "fx") ? " " + kvGet(kv, "fx") : "");
        const auto t = words(kvGet(kv, "notes"));
        if (t.empty()) return "err missing notes";
        std::vector<std::pair<int, TrackCell>> cells;
        for (size_t i = 0; i < t.size(); ++i) {
            const int row = r + (int)i * step;
            if (row >= song_.rows()) return "err notes run past the last row";
            if (t[i] == ".") continue;
            TrackCell cell;
            std::string err;
            if (!parseCell(song_.chans[c], t[i] + (t[i] == "===" || t[i] == "---" ? "" : tail), cell, err)) return "err " + err;
            cells.push_back({row, cell});
        }
        for (const auto& p : cells) song_.grid[p.first][c] = p.second;
    } else if (action == "clear") {
        if (truthy(kvGet(kv, "all"))) {
            for (auto& row : song_.grid) for (auto& cell : row) cell = TrackCell();
        } else {
            int r, c;
            if (!rowArg(r) || !chanArg(c)) return "err row or ch out of range";
            song_.grid[r][c] = TrackCell();
        }
    } else if (action == "tempo") {
        const int t = toInt(kvGet(kv, "value"), song_.tempo);
        if (t < 30 || t > 300) return "err tempo is 30 to 300";
        song_.tempo = t;
        if (playing_) play(playRow_ < 0 ? 0 : playRow_);
    } else if (action == "rows") {
        const int n = toInt(kvGet(kv, "value"), song_.rows());
        if (n < 1 || n > 128) return "err rows is 1 to 128";
        song_.grid.resize((size_t)n, std::vector<TrackCell>(song_.chans.size()));
        curRow_ = std::min(curRow_, n - 1);
    } else if (action == "cursor") {
        int r, c;
        if (!rowArg(r) || !chanArg(c)) return "err row or ch out of range";
        curRow_ = r;
        curChan_ = c;
        const std::string f = kvGet(kv, "field", "note");
        curField_ = f == "instr" ? 1 : f == "fx" ? 2 : 0;
        if (kvHas(kv, "octave")) octave_ = std::max(0, std::min(8, toInt(kvGet(kv, "octave"), octave_)));
    } else if (action == "text") {
        return "{\"ok\":true,\"standing\":\"data\",\"text\":\"" + jsonEscape(songText(song_)) + "\"}";
    } else if (action == "load_text") {
        Song s;
        std::string err;
        if (!parseSong(kvGet(kv, "text"), s, err)) return "err song " + err;
        const bool was = playing_;
        stop();
        song_ = s;
        origin_ = kvGet(kv, "from", "text");
        curRow_ = curChan_ = curField_ = 0;
        if (was) play(0);
    } else {
        return "err unknown action";
    }
    drawView();
    return stateJson();
}

TTrackerWindow::TTrackerWindow(const TRect& bounds, const Song& song, const std::string& origin)
    : TWindowInit(&TTrackerWindow::initFrame), TWindow(bounds, "Tracker", wnNoNumber) {
    options |= ofTileable;
    TRect c = getExtent();
    c.grow(-1, -1);
    view_ = new TTrackerView(c, song, origin);
    insert(view_);
}
