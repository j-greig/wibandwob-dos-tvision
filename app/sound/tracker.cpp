#include "tracker.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace sound {

static const char* kDrums[13] = {"bd", "sd", "lt", "mt", "ht", "rs", "cb", "cp", "ma", "cl", "cy", "oh", "ch"};

const char* drumId(int v) { return (v >= 0 && v < 13) ? kDrums[v] : ""; }

int drumIndex(const std::string& s0) {
    std::string s;
    for (char c : s0) if (c != '-') s += (char)std::tolower((unsigned char)c);
    for (int i = 0; i < 13; ++i) if (s == kDrums[i]) return i;
    return -1;
}

std::string chanName(const TrackChannel& c) {
    if (c.kind == ChanKind::Speaker) return "speaker";
    if (c.kind == ChanKind::Drums) return "drums";
    return "fm:" + fmPatches()[c.patch].name;
}

bool parseChan(const std::string& s0, TrackChannel& out) {
    std::string s;
    for (char c : s0) s += (char)std::tolower((unsigned char)c);
    if (s == "speaker" || s == "pcspk" || s == "s") { out = {ChanKind::Speaker, 0}; return true; }
    if (s == "drums" || s == "tr808" || s == "d") { out = {ChanKind::Drums, 0}; return true; }
    if (s == "fm" || s == "f") { out = {ChanKind::Fm, 0}; return true; }
    if (s.rfind("fm:", 0) == 0 || s.rfind("f:", 0) == 0) {
        const int p = patchIndex(s.substr(s.find(':') + 1));
        if (p < 0) return false;
        out = {ChanKind::Fm, p};
        return true;
    }
    return false;
}

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)std::tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

std::string cellText(const TrackChannel& c, const TrackCell& cell) {
    std::string n;
    if (cell.note == -1) n = "---";
    else if (cell.note == -2) n = "===";
    else if (c.kind == ChanKind::Drums) {
        n = drumId(cell.note);
        for (auto& ch : n) ch = (char)std::toupper((unsigned char)ch);
        n += "-";
    } else n = noteName(cell.note);
    char in[4], fx[5];
    if (cell.instr < 0) std::snprintf(in, sizeof(in), "..");
    else std::snprintf(in, sizeof(in), "%02X", cell.instr & 0xFF);
    if (!cell.fx) std::snprintf(fx, sizeof(fx), "...");
    else std::snprintf(fx, sizeof(fx), "%c%02X", cell.fx, cell.fxv & 0xFF);
    return n + " " + in + " " + fx;
}

bool parseCell(const TrackChannel& c, const std::string& text, TrackCell& out, std::string& err) {
    std::istringstream is(text);
    std::vector<std::string> t;
    for (std::string w; is >> w;) t.push_back(w);
    TrackCell cell;
    if (t.empty() || t.size() > 3) { err = "a cell is: note [instr] [effect], as C-4 01 A37"; return false; }
    const std::string& n = t[0];
    if (n == "---" || n == "..." || n == ".") cell.note = -1;
    else if (n == "===" || n == "off" || n == "OFF") cell.note = -2;
    else if (c.kind == ChanKind::Drums) {
        cell.note = drumIndex(n);
        if (cell.note < 0) { err = "not a drum voice: " + n + " (BD- SD- LT- MT- HT- RS- CB- CP- MA- CL- CY- OH- CH-)"; return false; }
    } else {
        cell.note = parseNote(n);
        if (cell.note < 0) { err = "not a note: " + n; return false; }
    }
    if (t.size() > 1 && t[1] != ".." && t[1] != ".") {
        if (t[1].size() > 2 || hexv(t[1][0]) < 0 || (t[1].size() == 2 && hexv(t[1][1]) < 0)) { err = "instrument is two hex digits: " + t[1]; return false; }
        cell.instr = std::strtol(t[1].c_str(), nullptr, 16);
        if (c.kind == ChanKind::Fm && (cell.instr < 1 || cell.instr > (int)fmPatches().size())) {
            err = "FM instrument is 01-" + std::to_string(fmPatches().size()) + " (the patches in order)";
            return false;
        }
    }
    if (t.size() > 2 && t[2] != "..." && t[2] != ".") {
        const std::string& f = t[2];
        const char k = (char)std::toupper((unsigned char)f[0]);
        if (f.size() != 3 || (k != 'A' && k != 'V' && k != 'G') || hexv(f[1]) < 0 || hexv(f[2]) < 0) {
            err = "effect is A, V or G and two hex digits: " + f;
            return false;
        }
        cell.fx = k;
        cell.fxv = hexv(f[1]) * 16 + hexv(f[2]);
    }
    out = cell;
    return true;
}

std::string rowText(const Song& s, int row) {
    char head[8];
    std::snprintf(head, sizeof(head), "%02d ", row);
    std::string out = head;
    for (size_t c = 0; c < s.chans.size(); ++c) {
        if (c) out += " | ";
        out += cellText(s.chans[c], s.grid[row][c]);
    }
    return out;
}

Song newSong(int rows, const std::vector<TrackChannel>& chans) {
    Song s;
    s.chans = chans;
    s.grid.assign((size_t)std::max(1, std::min(128, rows)), std::vector<TrackCell>(chans.size()));
    return s;
}

std::string songText(const Song& s) {
    std::ostringstream o;
    o << "# wwdos tracker\n"
      << "title " << s.title << "\n"
      << "tempo " << s.tempo << "\n"
      << "rpb " << s.rpb << "\n"
      << "channels";
    for (const auto& c : s.chans) o << " " << chanName(c);
    o << "\n";
    for (int r = 0; r < s.rows(); ++r) o << rowText(s, r) << "\n";
    return o.str();
}

bool parseSong(const std::string& text, Song& out, std::string& err) {
    Song s;
    s.chans.clear();
    std::istringstream is(text);
    std::string line;
    int n = 0;
    bool haveChans = false;
    auto fail = [&](const std::string& why) { err = "line " + std::to_string(n) + ": " + why; return false; };
    while (std::getline(is, line)) {
        ++n;
        if (n > 300) return fail("a song has at most 300 lines");
        size_t a = line.find_first_not_of(" \t\r");
        if (a == std::string::npos || line[a] == '#') continue;
        line = line.substr(a);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        std::istringstream ls(line);
        std::string k;
        ls >> k;
        if (k == "title") {
            std::string rest;
            std::getline(ls, rest);
            size_t b = rest.find_first_not_of(' ');
            s.title = b == std::string::npos ? "untitled" : rest.substr(b, 60);
        } else if (k == "tempo") {
            if (!(ls >> s.tempo) || s.tempo < 30 || s.tempo > 300) return fail("tempo is 30 to 300");
        } else if (k == "rpb") {
            if (!(ls >> s.rpb) || s.rpb < 1 || s.rpb > 8) return fail("rpb (rows per beat) is 1 to 8");
        } else if (k == "channels") {
            for (std::string c; ls >> c;) {
                TrackChannel ch;
                if (!parseChan(c, ch)) return fail("a channel is fm:<patch>, speaker or drums: " + c);
                s.chans.push_back(ch);
            }
            if (s.chans.empty() || s.chans.size() > 12) return fail("1 to 12 channels");
            haveChans = true;
        } else if (std::isdigit((unsigned char)k[0])) {
            if (!haveChans) return fail("a row comes after the channels line");
            if (s.rows() >= 128) return fail("at most 128 rows");
            std::string rest;
            std::getline(ls, rest);
            std::vector<std::string> cells;
            size_t p = 0;
            while (true) {
                size_t q = rest.find('|', p);
                cells.push_back(rest.substr(p, q == std::string::npos ? std::string::npos : q - p));
                if (q == std::string::npos) break;
                p = q + 1;
            }
            if (cells.size() != s.chans.size())
                return fail("row has " + std::to_string(cells.size()) + " cells for " + std::to_string(s.chans.size()) + " channels");
            std::vector<TrackCell> row(s.chans.size());
            for (size_t c = 0; c < cells.size(); ++c) {
                std::string e;
                if (!parseCell(s.chans[c], cells[c], row[c], e)) return fail(e);
            }
            s.grid.push_back(row);
        } else {
            return fail("unknown line (title, tempo, rpb, channels, or a row starting with its number)");
        }
    }
    if (!haveChans) { n = 0; return fail("a song needs a channels line"); }
    if (s.grid.empty()) { n = 0; return fail("a song needs at least one row"); }
    out = s;
    err.clear();
    return true;
}

double rowMs(const Song& s) { return 60000.0 / (s.tempo * s.rpb); }

std::vector<RowSound> rowSounds(const Song& s, int row) {
    std::vector<RowSound> out;
    if (row < 0 || row >= s.rows()) return out;
    const double rm = rowMs(s);
    for (size_t c = 0; c < s.chans.size(); ++c) {
        const TrackChannel& ch = s.chans[c];
        const TrackCell& cell = s.grid[row][c];
        if (cell.note < 0) continue;
        const std::string text = cellText(ch, cell);
        if (ch.kind == ChanKind::Drums) {
            RowSound r;
            r.chan = (int)c;
            r.drum = true;
            r.drum_voice = cell.note;
            double lv = cell.instr >= 0 ? std::min(127, cell.instr) / 127.0 : 0.8;
            if (cell.fx == 'V') lv = std::min(127, cell.fxv) / 127.0;
            r.gain = lv;
            r.cell = text;
            out.push_back(r);
            continue;
        }
        // Gate: G in sixteenths of a row; else until the next note or note off in the channel (at most 16 rows).
        double gate;
        if (cell.fx == 'G') gate = std::max(1, cell.fxv) * rm / 16.0;
        else {
            int k = 1;
            while (k < 16 && row + k < s.rows() && s.grid[row + k][c].note == -1) ++k;
            gate = k * rm * 0.95;
        }
        Note n;
        n.voice = ch.kind == ChanKind::Speaker ? Voice::PcSpeaker : Voice::Fm;
        n.midi = cell.note;
        n.vel = cell.fx == 'V' ? std::max(1, std::min(127, cell.fxv)) : 100;
        n.patch = fmPatches()[cell.instr >= 1 && cell.instr <= (int)fmPatches().size() ? cell.instr - 1 : ch.patch];
        if (cell.fx == 'A') {
            // Arpeggio: the row split in three, the note, then up x semitones, then up y.
            const int offs[3] = {0, cell.fxv >> 4, cell.fxv & 0xF};
            for (int k = 0; k < 3; ++k) {
                RowSound r;
                r.chan = (int)c;
                r.note = n;
                r.note.midi = std::min(127, n.midi + offs[k]);
                r.note.durMs = rm / 3.0 * 0.9;
                r.atMs = k * rm / 3.0;
                r.cell = text;
                out.push_back(r);
            }
            continue;
        }
        RowSound r;
        r.chan = (int)c;
        r.note = n;
        r.note.durMs = gate;
        r.cell = text;
        out.push_back(r);
    }
    return out;
}

}  // namespace sound
