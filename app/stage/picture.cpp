// See picture.h and docs/stage.md ("Colour pictures").
#include "picture.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <sys/stat.h>

namespace pic {

static int idx(unsigned char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'z' ? c - 'a' + 10 : c >= 'A' && c <= 'Z' ? c - 'A' + 36
         : c == '.' ? NONE : -1;
}

// One UTF-8 character at s[i], packed first byte lowest; i moves past it.
static uint32_t glyph(const std::string& s, size_t& i)
{
    unsigned char c = s[i];
    size_t n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
    uint32_t v = 0;
    for (size_t k = 0; k < n && i + k < s.size(); ++k) v |= uint32_t((unsigned char) s[i + k]) << (8 * k);
    i += n;
    return v;
}

static void palette(const std::string& line, std::vector<TColorRGB>& pal)
{
    std::istringstream ps(line); std::string c;
    while (ps >> c)
        if (c.size() == 7 && c[0] == '#') {
            unsigned v = (unsigned) strtoul(c.c_str() + 1, nullptr, 16);
            pal.push_back(TColorRGB((v >> 16) & 255, (v >> 8) & 255, v & 255));
        }
}

static size_t lineEnd(const std::string& s, size_t at) { size_t e = s.find('\n', at); return e == std::string::npos ? s.size() : e; }

bool Picture::parse(const std::string& t)
{
    if (t.compare(0, 6, "PAINT ") == 0) {
        size_t e = lineEnd(t, 0);
        palette(t.substr(6, e - 6), pal);
        std::vector<Cell> fr; std::vector<std::vector<Cell>> rows;
        auto flush = [&]() {
            if (rows.empty()) return;
            int hh = (int) rows.size(), ww = 0;
            for (auto& r : rows) ww = std::max(ww, (int) r.size());
            if (paintFrames.empty()) { w = ww; h = hh; }
            std::vector<Cell> f((size_t) w * h);
            for (int y = 0; y < std::min(h, hh); ++y)
                for (int x = 0; x < std::min(w, (int) rows[y].size()); ++x) f[(size_t) y * w + x] = rows[y][x];
            paintFrames.push_back(std::move(f));
            rows.clear();
        };
        for (size_t at = e + 1; at < t.size();) {
            size_t le = lineEnd(t, at);
            std::string l = t.substr(at, le - at);
            at = le + 1;
            if (!l.empty() && l.back() == '\r') l.pop_back();
            if (l == "----") { flush(); continue; }
            if (l.compare(0, 4, "FPS=") == 0) continue;
            std::vector<Cell> r;
            size_t i = 0;
            while (i + 2 < l.size()) {
                int fi = idx(l[i]), bi = idx(l[i + 1]); i += 2;
                Cell c; c.ch = glyph(l, i);
                if (fi < 0 || bi < 0 || (fi != NONE && fi >= (int) pal.size()) || (bi != NONE && bi >= (int) pal.size())) fi = NONE;
                c.ink = fi == NONE ? NONE : (uint8_t) fi;
                c.paper = c.ink == NONE ? NONE : bi == NONE ? NONE : (uint8_t) bi;
                r.push_back(c);
            }
            rows.push_back(std::move(r));
        }
        flush();
        return !paintFrames.empty();
    }
    if (t.compare(0, 6, "PFILM ") == 0) {
        film = true;
        text = t;
        size_t e = lineEnd(t, 0);
        if (sscanf(t.c_str() + 6, "%d %d %lf", &w, &h, &fps) < 2 || w <= 0 || h <= 0) return false;
        size_t e2 = lineEnd(t, e + 1);
        palette(t.substr(e + 1, e2 - e - 1), pal);
        int key = -1;
        for (size_t at = e2 + 1; at < t.size();) {
            size_t le = lineEnd(t, at);
            char k = t[at];
            int rowsN = 0;
            if (k == 'K') { key = (int) refs.size(); rowsN = h; }
            else if (k == 'D') { std::istringstream rs(t.substr(at + 1, le - at - 1)); int r; while (rs >> r) ++rowsN; }
            else if (k != '=') { at = le + 1; continue; }   // not a frame line: skipped (blank lines at the end)
            if (key < 0) return false;                       // the first frame must be a key frame
            refs.push_back({at, k, key});
            at = le + 1;
            for (int r = 0; r < rowsN && at <= t.size(); ++r) at = lineEnd(t, at) + 1;
        }
        return !refs.empty();
    }
    return false;
}

// Frame k written over cur (which holds frame k-1, or anything when k is a key frame).
void Picture::apply(int k)
{
    const Ref& rf = refs[k];
    if (rf.kind == '=') { curK = k; return; }
    size_t at = lineEnd(text, rf.off) + 1;
    std::vector<int> rows;
    if (rf.kind == 'K') { for (int y = 0; y < h; ++y) rows.push_back(y); std::fill(cur.begin(), cur.end(), Cell()); }
    else { std::istringstream rs(text.substr(rf.off + 1, at - 1 - rf.off - 1)); int r; while (rs >> r) rows.push_back(r); }
    for (int y : rows) {
        size_t le = lineEnd(text, at);
        if (y >= 0 && y < h) {
            Cell* row = &cur[(size_t) y * w];
            int x = 0; size_t i = at;
            while (i < le && x < w) {
                int rep = 1;
                if (text[i] == '>') { if (i + 1 >= le) break; x += idx(text[i + 1]) + 1; i += 2; continue; }
                if (text[i] == '*') { if (i + 1 >= le) break; rep = idx(text[i + 1]) + 2; i += 2; }
                if (i >= le) break;
                Cell c;
                if (text[i] == '\'') {
                    ++i;
                    if (x > 0) { c.ink = row[x - 1].ink; c.paper = row[x - 1].paper; }
                } else {
                    if (i + 2 >= le) break;
                    int fi = idx(text[i]), bi = idx(text[i + 1]); i += 2;
                    c.ink = fi < 0 || (fi != NONE && fi >= (int) pal.size()) ? NONE : (uint8_t) fi;
                    c.paper = c.ink == NONE || bi < 0 || (bi != NONE && bi >= (int) pal.size()) ? NONE : (uint8_t) bi;
                }
                if (i >= le) break;
                c.ch = glyph(text, i);
                for (int n = 0; n < rep && x < w; ++n) row[x++] = c;
            }
        }
        at = le + 1;
    }
    curK = k;
}

const std::vector<Cell>& Picture::frame(int k)
{
    static const std::vector<Cell> empty;
    int n = frames();
    if (n == 0) return empty;
    k = std::max(0, std::min(k, n - 1));
    if (!film) return paintFrames[k];
    if (k == curK) return cur;
    if ((int) cur.size() != w * h) cur.assign((size_t) w * h, Cell());
    int from = refs[k].key;
    if (curK >= from && curK < k) from = curK + 1;   // play on from the frame shown, else from the key frame
    for (int i = from; i <= k; ++i) apply(i);
    return cur;
}

bool isPicture(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    char b[6] = {0};
    f.read(b, 6);
    return f.gcount() == 6 && (std::string(b, 6) == "PAINT " || std::string(b, 6) == "PFILM ");
}

std::shared_ptr<Picture> get(const std::string& path)
{
    struct Entry { long long size; long long mtime; std::shared_ptr<Picture> p; };
    static std::map<std::string, Entry> cache;
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return nullptr;
    auto it = cache.find(path);
    if (it != cache.end() && it->second.size == (long long) st.st_size && it->second.mtime == (long long) st.st_mtime) return it->second.p;
    std::ifstream f(path, std::ios::binary);
    std::string t((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto p = std::make_shared<Picture>();
    if (!p->parse(t)) p = nullptr;
    cache[path] = {(long long) st.st_size, (long long) st.st_mtime, p};
    return p;
}

TColorRGB apply(const Fx& fx, TColorRGB c)
{
    int r = c.getRed() * fx.tint.getRed() / 255, g = c.getGreen() * fx.tint.getGreen() / 255, b = c.getBlue() * fx.tint.getBlue() / 255;
    if (fx.fade > 0) {
        r += (fx.fadeTo.getRed() - r) * fx.fade / 255; g += (fx.fadeTo.getGreen() - g) * fx.fade / 255; b += (fx.fadeTo.getBlue() - b) * fx.fade / 255;
    }
    return TColorRGB((uint8_t) r, (uint8_t) g, (uint8_t) b);
}

void drawRow(const Picture& p, const std::vector<Cell>& fr, int y, TScreenCell* out, int outW, int x0, const TColorRGB* pal)
{
    if (y < 0 || y >= p.h || fr.empty()) return;
    const Cell* row = &fr[(size_t) y * p.w];
    int a = std::max(0, -x0), b = std::min(p.w, outW - x0);
    for (int x = a; x < b; ++x) {
        const Cell& c = row[x];
        if (c.ink == NONE) continue;
        TScreenCell& o = out[x0 + x];
        TColor back = c.paper == NONE ? o.attribute.getBackground() : TColor(pal[c.paper]);
        o.character.initWithMultiByteChar(c.ch);
        o.attribute = TColorAttr(pal[c.ink], back);
    }
}

}
