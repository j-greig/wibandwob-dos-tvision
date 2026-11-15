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
#include "voice.h"

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
    : TView(bounds), song_(song), origin_(origin), mute_(song.chans.size(), false), solo_(song.chans.size(), false) {
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

// The block clipboard, shared by every tracker window: rows down, channels across.
static std::vector<std::vector<TrackCell>> g_clip;
static std::vector<ChanKind> g_clipKinds;   // the kind of channel each clipboard column came from

bool TTrackerView::audible(int ch) const {
    bool anySolo = false;
    for (bool s : solo_) anySolo = anySolo || s;
    if (anySolo) return solo_[ch];
    return !mute_[ch];
}

void TTrackerView::play(int pos, int row, bool patternOnly) {
    if (timer_) { killTimer(timer_); timer_ = nullptr; }
    playing_ = true;
    patternOnly_ = patternOnly;
    startedBy_ = currentActor();
    tempoNow_ = song_.tempo;
    speedNow_ = speedOf(song_.rpb);
    nextPos_ = std::max(0, std::min((int)song_.order.size() - 1, pos));
    const int pat = patternOnly ? editPat_ : song_.order[nextPos_];
    nextRowIdx_ = std::max(0, std::min(song_.rows(pat) - 1, row));
    // Render every sung word before the first row, so 'say' never runs while the song plays.
    for (const auto& pt : song_.pats)
        for (const auto& line : pt.grid)
            for (size_t c = 0; c < song_.chans.size(); ++c)
                if (song_.chans[c].kind == ChanKind::Voice && !line[c].word.empty()) voice::spoken(song_.chans[c].singer, line[c].word);
    startMs_ = nextAt_ = clockMs();
    if (midi::clockOut()) midi::send({0xFA}, nextAt_, "tracker");
    timer_ = setTimer(5, 5);
    tick();
}

void TTrackerView::stop() {
    if (playing_ && midi::clockOut()) midi::send({0xFC}, std::max(clockMs(), nextAt_), "tracker");
    playing_ = false;
    playRow_ = -1;
    if (timer_) { killTimer(timer_); timer_ = nullptr; }
    drawView();
}

void TTrackerView::playRow(int pat, int row, double atMs, double rm) {
    const std::string wid = windowId();
    const int chans = (int)song_.chans.size();
    if (midi::clockOut()) {
        const int ticks = std::max(1, speedNow_);
        for (int k = 0; k < ticks; ++k) midi::send({0xF8}, atMs + k * rm / ticks, "tracker");
    }
    for (const RowSound& r : rowSounds(song_, pat, row, rm)) {
        if (!audible(r.chan)) continue;   // muted, or another channel soloed
        if (r.sing) {
            Pcm p = voice::sung(r.singer, r.word, r.note.midi, r.note.durMs);
            if (!p) continue;
            const voice::Singer sg = voice::singer(r.singer);
            if (streamRunning() && synthAudioEnabled())
                mixer().scheduleSample(p, 0.9 * sg.gain * r.note.vel / 100.0, sampleForClock(atMs) + msToSample(r.atMs));
            Heard h;
            h.window = wid;
            h.channel = r.chan;
            h.channels = chans;
            h.cell = r.cell;
            h.row = row;
            h.rowAtMs = atMs + leadMs();
            h.startMs = atMs + leadMs() + r.atMs;
            h.durMs = (double)p->size() * 1000.0 / kRate;
            h.endMs = h.startMs + h.durMs;
            h.source = "voice";
            h.voice = r.singer;
            h.word = r.word;
            h.midi = r.note.midi;
            h.level = 0.9 * sg.gain;
            h.who = r.singer;   // the one who sang
            ears().hear(h);
            continue;
        }
        if (r.drum) {
            const uint8_t key = (uint8_t)midi::gmDrum(r.drum_voice);
            midi::send({0x99, key, (uint8_t)std::max(1, (int)std::lround(r.gain * 127))}, atMs + r.atMs, "tracker");
            midi::send({0x89, key, 0}, atMs + r.atMs + 50, "tracker");
        } else {
            const uint8_t ch = (uint8_t)std::min(15, r.chan == 9 ? 10 : r.chan);
            midi::send({(uint8_t)(0x90 | ch), (uint8_t)r.note.midi, (uint8_t)r.note.vel}, atMs + r.atMs, "tracker");
            midi::send({(uint8_t)(0x80 | ch), (uint8_t)r.note.midi, 0}, atMs + r.atMs + r.note.durMs, "tracker");
        }
        Heard h;
        h.window = wid;
        h.channel = r.chan;
        h.channels = chans;
        h.cell = r.cell;
        h.row = row;
        h.rowAtMs = atMs + leadMs();
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
            h.tempo = tempoNow_;
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
    const double now = clockMs();
    int guard = 0;
    while (playing_ && nextAt_ <= now + 0.5 && ++guard < 512) {
        int pat = patternOnly_ ? editPat_ : song_.order[nextPos_];
        if (nextRowIdx_ >= song_.rows(pat)) {   // the pattern is done: the next order position
            nextRowIdx_ = 0;
            if (!patternOnly_) {
                if (nextPos_ + 1 >= (int)song_.order.size()) {
                    if (!loop_) { stop(); return; }
                    nextPos_ = 0;
                } else ++nextPos_;
                pat = song_.order[nextPos_];
            } else if (!loop_) { stop(); return; }
        }
        // Tempo and speed effects take hold at their row.
        const RowFx fx = rowEffects(song_, pat, nextRowIdx_);
        if (fx.tempo > 0) tempoNow_ = std::max(30, fx.tempo);
        if (fx.speed > 0) speedNow_ = fx.speed;
        const double rm = rowMs(tempoNow_, speedNow_);
        playRow(pat, nextRowIdx_, nextAt_, rm);
        playPos_ = nextPos_;
        playPat_ = pat;
        playRow_ = nextRowIdx_;
        ++nextRowIdx_;
        nextAt_ += rm;
    }
    drawView();
}

void TTrackerView::moveCursor(int dr, int dc) {
    curRow_ = (curRow_ + dr + rows() * 64) % rows();
    const int n = (int)song_.chans.size();
    curChan_ = (curChan_ + dc + n * 4) % n;
}

static void copyBlock(const Song& s, int pat, int row, int nrows, int ch, int nch) {
    g_clip.clear();
    g_clipKinds.clear();
    for (int c = ch; c < std::min((int)s.chans.size(), ch + nch); ++c) g_clipKinds.push_back(s.chans[c].kind);
    for (int r = row; r < std::min(s.rows(pat), row + nrows); ++r) {
        std::vector<TrackCell> line;
        for (int c = ch; c < std::min((int)s.chans.size(), ch + nch); ++c) line.push_back(s.cell(pat, r, c));
        g_clip.push_back(line);
    }
}

// Pastes the clipboard with its top left at (row, ch); cells past the pattern's edge are dropped.
// A column copied from a drum channel lands only on drum channels, and a note column only on
// FM and speaker channels; a cell that does not fit is left as it was.
static int pasteBlock(Song& s, int pat, int row, int ch) {
    int n = 0;
    for (size_t i = 0; i < g_clip.size() && row + (int)i < s.rows(pat); ++i)
        for (size_t j = 0; j < g_clip[i].size() && ch + (int)j < (int)s.chans.size(); ++j) {
            const bool toDrum = s.chans[ch + j].kind == ChanKind::Drums;
            const bool fromDrum = j < g_clipKinds.size() && g_clipKinds[j] == ChanKind::Drums;
            if (toDrum != fromDrum) continue;
            s.cell(pat, row + (int)i, ch + (int)j) = g_clip[i][j];
            ++n;
        }
    return n;
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
    TrackCell& cell = at(curRow_, curChan_);
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
        case kbEnter: if (playing_) stop(); else play(0, curRow_, true); break;
        case kbCtrlL: loop_ = !loop_; break;
        // (Ctrl+B, N, O, S belong to the app's menus: mute, solo and new pattern take E, T and K.)
        case kbCtrlE: mute_[curChan_] = !mute_[curChan_]; break;
        case kbCtrlT: solo_[curChan_] = !solo_[curChan_]; break;
        case kbCtrlC: copyBlock(song_, editPat_, curRow_, rows() - curRow_, curChan_, 1); break;   // the channel from here down
        case kbCtrlV: pasteBlock(song_, editPat_, curRow_, curChan_); break;
        case kbCtrlK: {   // a new pattern of the same length, edited at once
            if (song_.pats.size() < 64) {
                song_.pats.push_back(emptyPattern(song_, rows()));
                editPat_ = (int)song_.pats.size() - 1;
                curRow_ = 0;
            }
            break;
        }
        case kbBack: case kbDel:
            if (curField_ == 0) cell = TrackCell();
            else if (curField_ == 1 && chan.kind == ChanKind::Voice) { if (!cell.word.empty()) cell.word.pop_back(); }
            else if (curField_ == 1) cell.instr = -1;
            else { cell.fx = 0; cell.fxv = 0; }
            break;
        default: handled = false;
    }
    if (!handled && ch) {
        handled = true;
        if (ch == ' ') { if (playing_) stop(); else play(0, 0, false); }
        else if (ch == '[') octave_ = std::max(0, octave_ - 1);
        else if (ch == ']') octave_ = std::min(8, octave_ + 1);
        else if (ch == '<' || ch == '>') {
            const int n = (int)song_.pats.size();
            editPat_ = (editPat_ + (ch == '<' ? n - 1 : 1)) % n;
            curRow_ = std::min(curRow_, rows() - 1);
        } else if (ch == '.') {
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
        } else if (curField_ == 1 && chan.kind == ChanKind::Voice) {
            // a voice channel's word: letters, ' and -
            if ((std::isalpha((unsigned char)ch) || ch == '\'' || ch == '-') && cell.word.size() < 12) cell.word += ch;
            else handled = false;
        } else if (curField_ == 1) {
            const int d = hexDigit(ch);
            if (d < 0) handled = false;
            else cell.instr = (((cell.instr < 0 ? 0 : cell.instr) << 4) | d) & 0xFF;
        } else {
            const char up = (char)std::toupper((unsigned char)ch);
            const int d = hexDigit(ch);
            if (!cell.fx && (up == 'A' || up == 'V' || up == 'G' || up == 'F')) { cell.fx = up; cell.fxv = 0; }
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
    const int vis = std::max(1, H - 4);
    const bool showPlay = playing_ && playPat_ == editPat_ && playRow_ >= 0;
    const int focus = showPlay ? playRow_ : curRow_;
    const int top = std::max(0, std::min(focus - vis / 2, rows() - vis));
    for (int y = 0; y < H; ++y) {
        TDrawBuffer b;
        b.moveChar(0, ' ', base, (ushort)W);
        if (y == 0) {
            std::ostringstream o;
            o << "TRACKER  " << song_.title << "  " << (playing_ ? tempoNow_ : song_.tempo) << " bpm  speed "
              << (playing_ ? speedNow_ : speedOf(song_.rpb)) << "  "
              << (playing_ ? "\u25b6 pos " + std::to_string(playPos_) + " pat " + std::to_string(playPat_) + " row " + std::to_string(playRow_)
                           : std::string("\u25a0 stopped"))
              << (loop_ ? "  loop" : "") << "  oct " << octave_;
            b.moveStr(1, o.str(), bright);
        } else if (y == 1) {
            std::ostringstream o;
            o << "pattern " << editPat_ << "/" << song_.pats.size() << " (" << rows() << " rows)  order";
            for (size_t i = 0; i < song_.order.size() && i < 24; ++i)
                o << ((playing_ && (int)i == playPos_) ? " [" : " ") << song_.order[i] << ((playing_ && (int)i == playPos_) ? "]" : "");
            b.moveStr(1, o.str(), dim);
        } else if (y == 2) {
            std::string h = "   ";
            for (size_t c = 0; c < song_.chans.size(); ++c) {
                std::string n = chanName(song_.chans[c]);
                if (mute_[c]) n = "(muted) " + n;
                if (solo_[c]) n = "(solo) " + n;
                n = n.substr(0, 10);
                n.resize(10, ' ');
                h += (c ? " | " : "") + n;
            }
            b.moveStr(1, h, dim);
        } else if (y >= 3 && y < 3 + vis) {
            const int r = top + (y - 3);
            if (r < rows()) {
                char head[8];
                std::snprintf(head, sizeof(head), "%02d ", r);
                const bool isPlay = showPlay && r == playRow_;
                b.moveStr(1, head, isPlay ? playA : (r % song_.rpb == 0 ? bright : dim));
                int x = 4;
                for (size_t c = 0; c < song_.chans.size(); ++c) {
                    if (c) { b.moveStr((ushort)x, " | ", dim); x += 3; }
                    const TrackCell& tc = song_.cell(editPat_, r, (int)c);
                    const std::string t = cellText(song_.chans[c], tc);
                    const int tl = (int)t.size();
                    const int fs[3] = {0, 4, tl - 3}, fl[3] = {3, tl - 8, 3};   // note, instrument or word, effect
                    for (int f = 0; f < 3; ++f) {
                        TColorAttr a = isPlay ? playA : ((tc.empty() || !audible((int)c)) ? dim : base);
                        if (r == curRow_ && (int)c == curChan_ && f == curField_) a = cur;
                        b.moveStr((ushort)(x + fs[f]), t.substr(fs[f], fl[f]), a);
                    }
                    x += tl;
                }
            }
        } else if (y == H - 1) {
            b.moveStr(1, "z-m q-i notes  1 off  . clear  < > pattern  ^K new  ^C ^V copy paste  ^E mute ^T solo  Space song  Enter pattern  ^L loop", dim);
        }
        writeLine(0, (short)y, (short)W, 1, b);
    }
}

std::string TTrackerView::describeState() const {
    int filled = 0;
    for (const auto& p : song_.pats) for (const auto& row : p.grid) for (const auto& c : row) if (!c.empty()) ++filled;
    std::ostringstream o;
    o << "tracker \"" << song_.title << "\", " << song_.chans.size() << " channels, " << song_.pats.size() << " pattern"
      << (song_.pats.size() == 1 ? "" : "s") << " in an order of " << song_.order.size() << " at " << song_.tempo << " bpm, "
      << filled << " cells written; editing pattern " << editPat_ << " (" << rows() << " rows); "
      << (playing_ ? "playing pattern " + std::to_string(playPat_) + " row " + std::to_string(playRow_) + (loop_ ? ", looping" : "")
                   : std::string("stopped"));
    return o.str();
}

bool TTrackerView::describeText(std::vector<std::string>& out) const {
    for (int r = 0; r < song_.rows(editPat_); ++r) out.push_back(rowText(song_, editPat_, r));
    return true;
}

std::string TTrackerView::stateJson() const {
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(3);
    o << "{\"ok\":true,\"standing\":\"data\",\"title\":\"" << jsonEscape(song_.title) << "\",\"from\":\"" << jsonEscape(origin_)
      << "\",\"tempo\":" << song_.tempo << ",\"rpb\":" << song_.rpb << ",\"rows\":" << rows()
      << ",\"row_ms\":" << rowMs(song_) << ",\"playing\":" << (playing_ ? "true" : "false")
      << ",\"loop\":" << (loop_ ? "true" : "false") << ",\"pattern_only\":" << (patternOnly_ ? "true" : "false")
      << ",\"play_pos\":" << playPos_ << ",\"play_pattern\":" << playPat_ << ",\"play_row\":" << playRow_
      << ",\"tempo_now\":" << tempoNow_ << ",\"speed_now\":" << speedNow_
      << ",\"started_ms\":" << startMs_ << ",\"lead_ms\":" << leadMs() << ",\"started_by\":\"" << jsonEscape(startedBy_) << "\""
      << ",\"edit_pattern\":" << editPat_ << ",\"cursor\":{\"row\":" << curRow_ << ",\"ch\":" << curChan_ << ",\"field\":\""
      << (curField_ == 0 ? "note" : curField_ == 1 ? "instr" : "fx") << "\"},\"octave\":" << octave_
      << ",\"clipboard\":[" << g_clip.size() << "," << (g_clip.empty() ? 0 : g_clip[0].size()) << "]";
    o << ",\"order\":[";
    for (size_t i = 0; i < song_.order.size(); ++i) o << (i ? "," : "") << song_.order[i];
    o << "],\"patterns\":[";
    for (size_t p = 0; p < song_.pats.size(); ++p) o << (p ? "," : "") << song_.pats[p].rows();
    o << "],\"channels\":[";
    for (size_t c = 0; c < song_.chans.size(); ++c) o << (c ? "," : "") << "\"" << chanName(song_.chans[c]) << "\"";
    o << "],\"mute\":[";
    for (size_t c = 0; c < mute_.size(); ++c) o << (c ? "," : "") << (mute_[c] ? "true" : "false");
    o << "],\"solo\":[";
    for (size_t c = 0; c < solo_.size(); ++c) o << (c ? "," : "") << (solo_[c] ? "true" : "false");
    o << "],\"lines\":[";
    for (int r = 0; r < rows(); ++r) o << (r ? "," : "") << "\"" << rowText(song_, editPat_, r) << "\"";
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
    // pat= names a pattern (default: the one being edited)
    int pat = toInt(kvGet(kv, "pat"), editPat_);
    if (pat < 0 || pat >= (int)song_.pats.size()) return "err pat out of range (0-" + std::to_string(song_.pats.size() - 1) + ")";
    auto chanArg = [&](int& c) {
        c = toInt(kvGet(kv, "ch"), curChan_);
        return c >= 0 && c < (int)song_.chans.size();
    };
    auto rowArg = [&](int& r) {
        r = toInt(kvGet(kv, "row"), curRow_);
        return r >= 0 && r < song_.rows(pat);
    };
    if (action.empty() || action == "state" || action == "open") {
    } else if (action == "play") {
        play(toInt(kvGet(kv, "pos"), 0), toInt(kvGet(kv, "row"), 0), false);
    } else if (action == "play_pattern") {
        editPat_ = pat;
        play(0, toInt(kvGet(kv, "row"), 0), true);
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
        song_.cell(pat, r, c) = cell;
    } else if (action == "write") {
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
            if (row >= song_.rows(pat)) return "err notes run past the last row";
            if (t[i] == ".") continue;
            TrackCell cell;
            std::string err;
            if (!parseCell(song_.chans[c], t[i] + (t[i] == "===" || t[i] == "---" ? "" : tail), cell, err)) return "err " + err;
            cells.push_back({row, cell});
        }
        for (const auto& p : cells) song_.cell(pat, p.first, c) = p.second;
    } else if (action == "clear") {
        if (truthy(kvGet(kv, "all"))) {
            for (auto& row : song_.pats[pat].grid) for (auto& cell : row) cell = TrackCell();
        } else {
            int r, c;
            if (!rowArg(r) || !chanArg(c)) return "err row or ch out of range";
            song_.cell(pat, r, c) = TrackCell();
        }
    } else if (action == "tempo") {
        const int t = toInt(kvGet(kv, "value"), song_.tempo);
        if (t < 30 || t > 300) return "err tempo is 30 to 300";
        song_.tempo = t;
        tempoNow_ = t;
    } else if (action == "rows" || action == "pattern_rows") {
        const int n = toInt(kvGet(kv, "value"), song_.rows(pat));
        if (n < 1 || n > 128) return "err rows is 1 to 128";
        song_.pats[pat].grid.resize((size_t)n, std::vector<TrackCell>(song_.chans.size()));
        if (pat == editPat_) curRow_ = std::min(curRow_, n - 1);
    } else if (action == "pattern") {
        const int p = toInt(kvGet(kv, "value"), editPat_);
        if (p < 0 || p >= (int)song_.pats.size()) return "err no pattern " + std::to_string(p);
        editPat_ = p;
        curRow_ = std::min(curRow_, rows() - 1);
    } else if (action == "new_pattern") {
        if (song_.pats.size() >= 64) return "err at most 64 patterns";
        if (kvHas(kv, "copy")) {
            const int from = toInt(kvGet(kv, "copy"), -1);
            if (from < 0 || from >= (int)song_.pats.size()) return "err no pattern " + kvGet(kv, "copy") + " to copy";
            song_.pats.push_back(song_.pats[from]);
        } else {
            song_.pats.push_back(emptyPattern(song_, toInt(kvGet(kv, "rows"), rows())));
        }
        editPat_ = (int)song_.pats.size() - 1;
        curRow_ = 0;
    } else if (action == "order") {
        std::vector<int> ord;
        for (const auto& w : words(kvGet(kv, "value"))) {
            const int p = std::atoi(w.c_str());
            if (w.empty() || !std::isdigit((unsigned char)w[0]) || p >= (int)song_.pats.size()) return "err order names pattern " + w + ", which is not in the song";
            ord.push_back(p);
        }
        if (ord.empty() || ord.size() > 256) return "err order holds 1 to 256 pattern numbers";
        song_.order = ord;
        if (nextPos_ >= (int)ord.size()) nextPos_ = 0;
    } else if (action == "mute" || action == "solo") {
        int c;
        if (!chanArg(c)) return "err ch out of range";
        auto& v = action == "mute" ? mute_ : solo_;
        const std::string on = kvGet(kv, "on");
        v[c] = on.empty() ? !v[c] : truthy(on);
    } else if (action == "copy") {
        int r, c;
        if (!rowArg(r) || !chanArg(c)) return "err row or ch out of range";
        copyBlock(song_, pat, r, std::max(1, toInt(kvGet(kv, "rows"), 1)), c, std::max(1, toInt(kvGet(kv, "chs"), 1)));
    } else if (action == "paste") {
        int r, c;
        if (!rowArg(r) || !chanArg(c)) return "err row or ch out of range";
        if (g_clip.empty()) return "err the clipboard is empty";
        pasteBlock(song_, pat, r, c);
    } else if (action == "cursor") {
        int r, c;
        if (!rowArg(r) || !chanArg(c)) return "err row or ch out of range";
        editPat_ = pat;
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
        mute_.assign(s.chans.size(), false);
        solo_.assign(s.chans.size(), false);
        origin_ = kvGet(kv, "from", "text");
        editPat_ = curRow_ = curChan_ = curField_ = 0;
        if (was) play(0, 0, false);
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
