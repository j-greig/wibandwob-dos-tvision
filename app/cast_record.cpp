// Cast recording and reading (asciinema v2). See cast_record.h.
#include "cast_record.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>

namespace ww {

std::string castEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20 || c == 0x7f) {
                char b[8];
                snprintf(b, sizeof b, "\\u%04x", c);
                o += b;
            } else {
                o += (char) c;
            }
        }
    }
    return o;
}

CastRecorder::~CastRecorder() { stop(); }

double CastRecorder::clockLocked() const {
    auto now = held_ ? heldAt_ : std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - t0_).count() - heldTotal_;
}

void CastRecorder::flushLocked() {
    if (!f_ || out_.empty()) return;
    fprintf(f_, "[%.6f, \"o\", \"%s\"]\n", outT_ < 0 ? 0 : outT_, castEscape(out_).c_str());
    fflush(f_);
    ++events_;
    out_.clear();
}

void CastRecorder::writeLocked(double t, const std::string& data) {
    if (!f_ || data.empty()) return;
    // Output read within 10 ms of the last read joins one event: a shell's printf can reach the
    // pty a byte at a time, and one line per byte only makes the file long.
    if (!out_.empty() && t - outLast_ < 0.010) { out_ += data; outLast_ = t; return; }
    flushLocked();
    out_ = data;
    outT_ = outLast_ = t;
}

void CastRecorder::writeRawLocked(double t, const std::string& data) {
    if (!f_ || data.empty()) return;
    fprintf(f_, "[%.6f, \"o\", \"%s\"]\n", t < 0 ? 0 : t, castEscape(data).c_str());
    fflush(f_);
    ++events_;
}

bool CastRecorder::start(const std::string& path, int width, int height, const std::string& title,
                         const std::vector<std::string>& screen, std::string& err) {
    std::lock_guard<std::mutex> g(m_);
    if (f_) { err = "already recording to " + path_; return false; }
    FILE* f = fopen(path.c_str(), "w");
    if (!f) { err = "cannot write " + path + ": " + strerror(errno); return false; }
    f_ = f;
    path_ = path;
    pending_.clear();
    events_ = 0;
    heldTotal_ = 0;
    held_ = false;
    t0_ = std::chrono::steady_clock::now();
    fprintf(f_, "{\"version\": 2, \"width\": %d, \"height\": %d, \"timestamp\": %ld, \"title\": \"%s\", "
                "\"env\": {\"TERM\": \"xterm-256color\"}}\n",
            width, height, (long) time(nullptr), castEscape(title).c_str());
    // What the window shows now, so the cast opens on the same screen.
    std::string first = "\x1b[0m\x1b[2J\x1b[H";
    for (size_t i = 0; i < screen.size(); ++i) {
        if (i) first += "\r\n";
        first += screen[i];
    }
    out_.clear();
    writeRawLocked(0, first);
    return true;
}

long CastRecorder::stop() {
    std::lock_guard<std::mutex> g(m_);
    if (!f_) return -1;
    if (!pending_.empty()) writeLocked(clockLocked(), pending_);
    pending_.clear();
    flushLocked();
    fclose(f_);
    f_ = nullptr;
    return events_;
}

bool CastRecorder::active() {
    std::lock_guard<std::mutex> g(m_);
    return f_ != nullptr;
}

std::string CastRecorder::path() {
    std::lock_guard<std::mutex> g(m_);
    return f_ ? path_ : std::string();
}

double CastRecorder::seconds() {
    std::lock_guard<std::mutex> g(m_);
    return f_ ? clockLocked() : 0;
}

void CastRecorder::output(const char* data, size_t n) {
    std::lock_guard<std::mutex> g(m_);
    if (!f_ || n == 0) return;
    std::string s = pending_;
    s.append(data, n);
    pending_.clear();
    // Keep an incomplete UTF-8 sequence at the end for the next read: each line holds whole characters.
    size_t end = s.size(), i = end;
    int back = 0;
    while (i > 0 && back < 4) {
        unsigned char c = (unsigned char) s[i - 1];
        if ((c & 0xC0) == 0x80) { --i; ++back; continue; }
        if (c >= 0xC0) {
            size_t need = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
            if (end - (i - 1) < need) { pending_ = s.substr(i - 1); s.resize(i - 1); }
        }
        break;
    }
    writeLocked(clockLocked(), s);
}

void CastRecorder::hold(bool on) {
    std::lock_guard<std::mutex> g(m_);
    if (on == held_) return;
    auto now = std::chrono::steady_clock::now();
    if (on) heldAt_ = now;
    else heldTotal_ += std::chrono::duration<double>(now - heldAt_).count();
    held_ = on;
}

// ---------------------------------------------------------------- reading

namespace {

void putUtf8(std::string& o, unsigned cp) {
    if (cp < 0x80) o += (char) cp;
    else if (cp < 0x800) { o += (char) (0xC0 | (cp >> 6)); o += (char) (0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        o += (char) (0xE0 | (cp >> 12)); o += (char) (0x80 | ((cp >> 6) & 0x3F)); o += (char) (0x80 | (cp & 0x3F));
    } else {
        o += (char) (0xF0 | (cp >> 18)); o += (char) (0x80 | ((cp >> 12) & 0x3F));
        o += (char) (0x80 | ((cp >> 6) & 0x3F)); o += (char) (0x80 | (cp & 0x3F));
    }
}

// Reads a JSON string starting at s[i] == '"'; i ends after the closing quote.
bool readString(const std::string& s, size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    ++i;
    out.clear();
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') return true;
        if (c != '\\') { out += c; continue; }
        if (i >= s.size()) return false;
        char e = s[i++];
        switch (e) {
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'u': {
            if (i + 4 > s.size()) return false;
            unsigned cp = (unsigned) strtoul(s.substr(i, 4).c_str(), nullptr, 16);
            i += 4;
            if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                unsigned lo = (unsigned) strtoul(s.substr(i + 2, 4).c_str(), nullptr, 16);
                if (lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); i += 6; }
            }
            putUtf8(out, cp);
            break;
        }
        default: out += e;
        }
    }
    return false;
}

int headerInt(const std::string& h, const char* key, int def) {
    size_t p = h.find(std::string("\"") + key + "\"");
    if (p == std::string::npos) return def;
    p = h.find(':', p);
    return p == std::string::npos ? def : atoi(h.c_str() + p + 1);
}

}  // namespace

bool loadCast(const std::string& path, Cast& out, std::string& err) {
    std::ifstream in(path);
    if (!in) { err = "cannot read " + path; return false; }
    std::string line;
    if (!std::getline(in, line) || line.find("\"version\"") == std::string::npos) {
        err = path + " is not an asciinema v2 cast (no header line)";
        return false;
    }
    if (headerInt(line, "version", 0) != 2) { err = "only asciinema version 2 casts are read"; return false; }
    out = Cast{};
    out.width = headerInt(line, "width", 80);
    out.height = headerInt(line, "height", 24);
    size_t tp = line.find("\"title\"");
    if (tp != std::string::npos) {
        size_t q = line.find('"', line.find(':', tp));
        if (q != std::string::npos) readString(line, q, out.title);
    }
    while (std::getline(in, line)) {
        size_t i = line.find('[');
        if (i == std::string::npos) continue;
        char* endp = nullptr;
        double t = strtod(line.c_str() + i + 1, &endp);
        if (!endp) continue;
        i = endp - line.c_str();
        size_t q = line.find('"', i);
        std::string type, data;
        if (q == std::string::npos || !readString(line, q, type)) continue;
        q = line.find('"', q);
        if (q == std::string::npos || !readString(line, q, data)) continue;
        if (type == "o") out.events.push_back({t, data});
    }
    if (out.width < 1 || out.height < 1 || out.width > 400 || out.height > 200) {
        err = "cast size out of range";
        return false;
    }
    return true;
}

}  // namespace ww
