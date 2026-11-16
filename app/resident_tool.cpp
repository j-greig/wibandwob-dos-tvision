// Residents' tools: the language, the window, the commands (resident_tool.h).
#include "resident_tool.h"

#include "core/json_utils.h"
#include "describe.h"
#include "resident_life.h"
#include "resident_senses.h"

#define Uses_TWindow
#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TDeskTop
#define Uses_TProgram
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TRect
#include <tvision/tv.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <vector>

namespace tools {
namespace {

using resident::KV;
using resident::Source;
using resident::Who;

long long nowMs() {
    using namespace std::chrono;
    static const steady_clock::time_point t0 = steady_clock::now();
    return duration_cast<milliseconds>(steady_clock::now() - t0).count();
}
std::string q(const std::string& s) { return "\"" + json_escape(s) + "\""; }
std::string str(const KV& kv, const char* k, const std::string& d = "") {
    auto it = kv.find(k);
    return it == kv.end() ? d : it->second;
}
std::string upper(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
std::string dir() { return life::home() + "/TOOLS"; }

// ---------------------------------------------------------------- the language
//
// A program is sections of statements:
//   ON START ... END          once, when the window opens
//   ON TICK <ms> ... END      every <ms> (at least 50)
//   ON KEY ... END            on each key to its window (KEY$ holds it)
// Statements: REM, LET v = e (or v = e), CLS, AT x, y PRINT e, CELL x, y, e, COLOR fg, bg,
//   IF e THEN <statement>, GOTO label, label:, STOP, CALL "command", "k=v k=v".
// Values are whole numbers and strings. Functions: CLOCK HOUR MIN SEC W H KEY$ RND(n) LEN(s)
//   STR$(n) VAL(s) MID$(s,i,n) CHR$(n) PAD$(n) PEEK$(x,y). Operators: + - * / MOD = <> < > <= >= AND OR.
// A section runs at most 5000 statements; a program at most 400 lines.

struct Tok {
    enum K { Num, Str, Id, Op, End } k = End;
    long long n = 0;
    std::string s;
};

struct Val {
    bool isStr = false;
    long long n = 0;
    std::string s;
    std::string text() const { return isStr ? s : std::to_string(n); }
    long long num() const { return isStr ? std::atoll(s.c_str()) : n; }
};
Val num(long long n) { Val v; v.n = n; return v; }
Val strv(const std::string& s) { Val v; v.isStr = true; v.s = s; return v; }

std::vector<Tok> lex(const std::string& line, std::string& err) {
    std::vector<Tok> out;
    size_t i = 0;
    while (i < line.size()) {
        char c = line[i];
        if (std::isspace((unsigned char)c)) { ++i; continue; }
        Tok t;
        if (std::isdigit((unsigned char)c)) {
            size_t j = i;
            while (j < line.size() && std::isdigit((unsigned char)line[j])) ++j;
            t.k = Tok::Num;
            t.n = std::atoll(line.substr(i, j - i).c_str());
            i = j;
        } else if (c == '"') {
            size_t j = line.find('"', i + 1);
            if (j == std::string::npos) { err = "a string is not closed"; return out; }
            t.k = Tok::Str;
            t.s = line.substr(i + 1, j - i - 1);
            i = j + 1;
        } else if (std::isalpha((unsigned char)c) || c == '_') {
            size_t j = i;
            while (j < line.size() && (std::isalnum((unsigned char)line[j]) || line[j] == '_')) ++j;
            if (j < line.size() && line[j] == '$') ++j;
            t.k = Tok::Id;
            t.s = upper(line.substr(i, j - i));
            i = j;
        } else {
            std::string two = line.substr(i, 2);
            t.k = Tok::Op;
            if (two == "<>" || two == "<=" || two == ">=") { t.s = two; i += 2; }
            else if (std::string("+-*/=<>(),:").find(c) != std::string::npos) { t.s = std::string(1, c); ++i; }
            else { err = std::string("the character '") + c + "' means nothing here"; return out; }
        }
        out.push_back(t);
    }
    return out;
}

// Commands a tool may call: the ones a resident could call with its own hands, none that reads or
// writes files, starts programs or reaches outside the desktop.
const std::set<std::string>& callable() {
    static const std::set<std::string> s = {"synth", "tr808", "hear", "look_glance", "logbook_write", "send_text",
                                            "paint_cell", "paint_text", "paint_line", "paint_rect", "move_window",
                                            "raise_window", "lower_window", "resize_window"};
    return s;
}

struct Program {
    std::vector<std::vector<Tok>> lines;   // tokens per source line
    std::vector<int> srcLine;              // 1-based source line numbers
    int start = -1, key = -1, tick = -1;   // first statement index of each section
    long tickMs = 0;
    std::map<std::string, int> labels;
    std::string error;
};

bool parseProgram(const std::vector<std::string>& src, Program& p) {
    if (src.size() > 400) { p.error = "a tool has at most 400 lines"; return false; }
    int* open = nullptr;
    for (size_t i = 0; i < src.size(); ++i) {
        std::string line = src[i];
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        line = line.substr(a);
        if (upper(line.substr(0, 3)) == "REM" || line[0] == '\'') continue;
        std::string err;
        auto toks = lex(line, err);
        if (!err.empty()) { p.error = "line " + std::to_string(i + 1) + ": " + err; return false; }
        if (toks.empty()) continue;
        if (toks[0].k == Tok::Id && toks[0].s == "ON" && toks.size() >= 2) {
            if (open) { p.error = "line " + std::to_string(i + 1) + ": ON inside a section (END the last one first)"; return false; }
            std::string what = toks[1].s;
            int idx = (int)p.lines.size();
            if (what == "START") open = &p.start;
            else if (what == "KEY") open = &p.key;
            else if (what == "TICK") {
                open = &p.tick;
                p.tickMs = toks.size() >= 3 && toks[2].k == Tok::Num ? std::max(50LL, toks[2].n) : 1000;
            } else { p.error = "line " + std::to_string(i + 1) + ": ON START, ON TICK <ms> or ON KEY"; return false; }
            *open = idx;
            continue;
        }
        if (toks[0].k == Tok::Id && toks[0].s == "END" && toks.size() == 1) {
            if (!open) { p.error = "line " + std::to_string(i + 1) + ": END with no section"; return false; }
            Tok e;
            e.k = Tok::Id;
            e.s = "END";
            p.lines.push_back({e});
            p.srcLine.push_back((int)i + 1);
            open = nullptr;
            continue;
        }
        if (!open) { p.error = "line " + std::to_string(i + 1) + ": a statement outside ON ... END"; return false; }
        if (toks.size() == 2 && toks[0].k == Tok::Id && toks[1].k == Tok::Op && toks[1].s == ":") {
            p.labels[toks[0].s] = (int)p.lines.size();
            continue;
        }
        p.lines.push_back(toks);
        p.srcLine.push_back((int)i + 1);
    }
    if (open) { p.error = "the last section has no END"; return false; }
    return true;
}

class TToolView;

struct Machine {
    Program prog;
    std::map<std::string, Val> vars;      // arrays as NAME(i)
    std::map<std::string, long long> dims; // DIM'd arrays and their size
    std::vector<int> stack;                // GOSUB return points
    int pc = 0;
    bool restored = false;                 // its state came back from NAME.STATE
    std::string key;
    long long startMs = 0;
    TToolView* view = nullptr;
    Who author{"", Source::Visitor};
    std::string error;
    int calls = 0;
};

// ---------------------------------------------------------------- the window

class TToolView : public TView {
public:
    TToolView(const TRect& r) : TView(r) {
        growMode = gfGrowHiX | gfGrowHiY;
        options |= ofSelectable;
        eventMask |= evKeyDown;
        clear();
    }
    std::vector<std::string> rows;               // one char per cell (ASCII; others shown as given)
    std::vector<std::vector<unsigned char>> ink; // fg per cell
    unsigned char fg = 7, bg = 0;
    Machine* m = nullptr;
    void clear() {
        rows.assign(std::max(1, (int)size.y), std::string(std::max(1, (int)size.x), ' '));
        ink.assign(rows.size(), std::vector<unsigned char>(rows[0].size(), 7));
    }
    void put(long long x, long long y, const std::string& s) {
        if (y < 0 || y >= (long long)rows.size()) return;
        for (size_t i = 0; i < s.size(); ++i) {
            long long cx = x + (long long)i;
            if (cx < 0 || cx >= (long long)rows[y].size()) continue;
            rows[y][cx] = (unsigned char)s[i] >= 0x20 && (unsigned char)s[i] < 0x7f ? s[i] : '?';
            ink[y][cx] = fg;
        }
    }
    std::string peek(long long x, long long y) const {
        if (y < 0 || y >= (long long)rows.size() || x < 0 || x >= (long long)rows[y].size()) return "";
        return std::string(1, rows[y][x]);
    }
    void draw() override {
        for (int y = 0; y < size.y; ++y) {
            TDrawBuffer b;
            b.moveChar(0, ' ', TColorAttr(TColorBIOS(7), TColorBIOS(bg)), size.x);
            if (y < (int)rows.size())
                for (int x = 0; x < size.x && x < (int)rows[y].size(); ++x)
                    b.moveChar(x, rows[y][x], TColorAttr(TColorBIOS(ink[y][x]), TColorBIOS(bg)), 1);
            writeLine(0, y, size.x, 1, b);
        }
    }
    void handleEvent(TEvent& ev) override;
};

// ---------------------------------------------------------------- running a section

struct Run {
    Machine& m;
    const std::vector<Tok>* t = nullptr;
    size_t i = 0;
    std::string err;

    const Tok& peek() {
        static Tok end;
        return i < t->size() ? (*t)[i] : end;
    }
    bool isOp(const char* s) { return peek().k == Tok::Op && peek().s == s; }
    bool isId(const char* s) { return peek().k == Tok::Id && peek().s == s; }
    bool expect(const char* s) {
        if (isOp(s)) { ++i; return true; }
        err = std::string("expected ") + s;
        return false;
    }

    Val call(const std::string& f) {
        if (m.dims.count(f) && isOp("(")) {   // an array element
            ++i;
            long long ix = expr().num();
            if (!err.empty() || !expect(")")) return Val();
            if (ix < 0 || ix >= m.dims[f]) { err = f + "(" + std::to_string(ix) + ") is outside DIM " + std::to_string(m.dims[f]); return Val(); }
            auto it = m.vars.find(f + "(" + std::to_string(ix) + ")");
            if (it != m.vars.end()) return it->second;
            return f.back() == '$' ? strv("") : num(0);
        }
        if (f == "RESTORED") return num(m.restored ? 1 : 0);
        std::vector<Val> a;
        if (isOp("(")) {
            ++i;
            while (!isOp(")")) {
                a.push_back(expr());
                if (!err.empty()) return Val();
                if (isOp(",")) ++i;
                else if (!isOp(")")) { err = "expected , or )"; return Val(); }
            }
            ++i;
        }
        auto need = [&](size_t n) { if (a.size() < n) err = f + " needs " + std::to_string(n) + " arguments"; return a.size() >= n; };
        std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_r(&now, &tm);
        int w = m.view ? m.view->size.x : 0, h = m.view ? m.view->size.y : 0;
        if (f == "CLOCK") return num(nowMs() - m.startMs);
        if (f == "HOUR") return num(tm.tm_hour);
        if (f == "MIN") return num(tm.tm_min);
        if (f == "SEC") return num(tm.tm_sec);
        if (f == "W") return num(w);
        if (f == "H") return num(h);
        if (f == "KEY$") return strv(m.key);
        if (f == "RND") { if (!need(1)) return Val(); long long n = std::max(1LL, a[0].num()); return num(std::rand() % n); }
        if (f == "LEN") { if (!need(1)) return Val(); return num((long long)a[0].text().size()); }
        if (f == "STR$") { if (!need(1)) return Val(); return strv(a[0].text()); }
        if (f == "VAL") { if (!need(1)) return Val(); return num(a[0].num()); }
        if (f == "CHR$") { if (!need(1)) return Val(); long long c = a[0].num(); return strv(c >= 32 && c < 127 ? std::string(1, (char)c) : "?"); }
        if (f == "PAD$") { if (!need(1)) return Val(); long long n = a[0].num(); return strv((n < 10 && n >= 0 ? "0" : "") + std::to_string(n)); }
        if (f == "MID$") {
            if (!need(3)) return Val();
            std::string s = a[0].text();
            long long st = std::max(1LL, a[1].num()) - 1, n = std::max(0LL, a[2].num());
            return strv(st < (long long)s.size() ? s.substr(st, n) : "");
        }
        if (f == "PEEK$") { if (!need(2)) return Val(); return strv(m.view ? m.view->peek(a[0].num(), a[1].num()) : ""); }
        auto it = m.vars.find(f);
        if (it != m.vars.end()) return it->second;
        return f.back() == '$' ? strv("") : num(0);
    }
    Val atom() {
        const Tok& k = peek();
        if (k.k == Tok::Num) { ++i; return num(k.n); }
        if (k.k == Tok::Str) { ++i; return strv(k.s); }
        if (k.k == Tok::Op && k.s == "(") { ++i; Val v = expr(); expect(")"); return v; }
        if (k.k == Tok::Op && k.s == "-") { ++i; Val v = atom(); return num(-v.num()); }
        if (k.k == Tok::Id) { std::string f = k.s; ++i; return call(f); }
        err = "expected a value";
        return Val();
    }
    Val term() {
        Val v = atom();
        while (err.empty() && (isOp("*") || isOp("/") || isId("MOD"))) {
            std::string op = peek().s;
            ++i;
            Val r = atom();
            long long b = r.num();
            if (op == "*") v = num(v.num() * b);
            else if (b == 0) { err = "division by zero"; return Val(); }
            else v = num(op == "/" ? v.num() / b : v.num() % b);
        }
        return v;
    }
    Val sum() {
        Val v = term();
        while (err.empty() && (isOp("+") || isOp("-"))) {
            std::string op = peek().s;
            ++i;
            Val r = term();
            if (op == "+" && (v.isStr || r.isStr)) v = strv(v.text() + r.text());
            else v = num(op == "+" ? v.num() + r.num() : v.num() - r.num());
        }
        return v;
    }
    Val cmp() {
        Val v = sum();
        static const char* ops[] = {"=", "<>", "<", ">", "<=", ">="};
        for (auto o : ops) {
            if (!isOp(o)) continue;
            ++i;
            Val r = sum();
            int c = (v.isStr || r.isStr) ? v.text().compare(r.text()) : (v.num() < r.num() ? -1 : v.num() > r.num());
            std::string s = o;
            bool b = s == "=" ? c == 0 : s == "<>" ? c != 0 : s == "<" ? c < 0 : s == ">" ? c > 0 : s == "<=" ? c <= 0 : c >= 0;
            return num(b);
        }
        return v;
    }
    Val expr() {
        Val v = cmp();
        while (err.empty() && (isId("AND") || isId("OR"))) {
            bool isAnd = peek().s == "AND";
            ++i;
            Val r = cmp();
            v = num(isAnd ? (v.num() && r.num()) : (v.num() || r.num()));
        }
        return v;
    }

    // One statement from position i; next is set to a jump target, -2 for STOP / END.
    void statement(int& next) {
        const Tok& k = peek();
        if (k.k != Tok::Id) { err = "a statement starts with a word"; return; }
        std::string w = k.s;
        ++i;
        bool letArray = w == "LET" && peek().k == Tok::Id && i + 1 < t->size() && (*t)[i + 1].k == Tok::Op && (*t)[i + 1].s == "(";
        if (letArray || (m.dims.count(w) && isOp("("))) {   // A(i) = value
            std::string name = w;
            if (letArray) { name = peek().s; ++i; }
            if (!m.dims.count(name)) { err = name + " is not an array (DIM it first)"; return; }
            if (!expect("(")) return;
            long long ix = expr().num();
            if (!err.empty() || !expect(")") || !expect("=")) return;
            if (ix < 0 || ix >= m.dims[name]) { err = name + "(" + std::to_string(ix) + ") is outside DIM " + std::to_string(m.dims[name]); return; }
            Val v = expr();
            if (!err.empty()) return;
            if (name.back() == '$') v = strv(v.text());
            m.vars[name + "(" + std::to_string(ix) + ")"] = v;
            return;
        }
        if (w == "DIM") {
            while (true) {
                if (peek().k != Tok::Id) { err = "DIM needs a name"; return; }
                std::string name = peek().s;
                ++i;
                if (!expect("(")) return;
                long long n = expr().num();
                if (!err.empty() || !expect(")")) return;
                if (n < 1 || n > 1000) { err = "DIM sizes are 1 to 1000"; return; }
                if (!m.dims.count(name)) m.dims[name] = n;   // kept state keeps its elements
                if (!isOp(",")) break;
                ++i;
            }
            return;
        }
        if (w == "GOSUB") {
            if (peek().k != Tok::Id) { err = "GOSUB needs a label"; return; }
            auto it = m.prog.labels.find(peek().s);
            if (it == m.prog.labels.end()) { err = "no label " + peek().s; return; }
            ++i;
            if (m.stack.size() >= 64) { err = "GOSUB deeper than 64"; return; }
            m.stack.push_back(m.pc + 1);
            next = it->second;
            return;
        }
        if (w == "RETURN") {
            if (m.stack.empty()) { err = "RETURN without GOSUB"; return; }
            next = m.stack.back();
            m.stack.pop_back();
            return;
        }
        if (w == "LET" || isOp("=")) {
            std::string name = w;
            if (w == "LET") {
                if (peek().k != Tok::Id) { err = "LET needs a name"; return; }
                name = peek().s;
                ++i;
            }
            if (!expect("=")) return;
            Val v = expr();
            if (!err.empty()) return;
            if (name.back() == '$') v = strv(v.text());
            m.vars[name] = v;
        } else if (w == "CLS") {
            if (m.view) m.view->clear();
        } else if (w == "AT" || w == "CELL") {
            Val x = expr();
            if (!err.empty() || !expect(",")) return;
            Val y = expr();
            if (!err.empty()) return;
            if (w == "AT") { if (!isId("PRINT")) { err = "AT x, y PRINT value"; return; } ++i; }
            else if (!expect(",")) return;
            Val v = expr();
            if (!err.empty()) return;
            std::string s = v.text();
            if (w == "CELL") s = s.substr(0, 1);
            if (m.view) m.view->put(x.num(), y.num(), s);
        } else if (w == "COLOR") {
            Val f = expr();
            if (!err.empty() || !expect(",")) return;
            Val b = expr();
            if (!err.empty()) return;
            if (m.view) { m.view->fg = (unsigned char)(f.num() & 15); m.view->bg = (unsigned char)(b.num() & 7); }
        } else if (w == "IF") {
            Val c = expr();
            if (!err.empty()) return;
            if (!isId("THEN")) { err = "IF needs THEN"; return; }
            ++i;
            if (c.num()) statement(next);
            else i = t->size();
        } else if (w == "GOTO") {
            if (peek().k != Tok::Id) { err = "GOTO needs a label"; return; }
            auto it = m.prog.labels.find(peek().s);
            if (it == m.prog.labels.end()) { err = "no label " + peek().s; return; }
            ++i;
            next = it->second;
        } else if (w == "STOP" || w == "END") {
            next = -2;
        } else if (w == "CALL") {
            Val name = expr();
            if (!err.empty()) return;
            std::string args;
            if (isOp(",")) { ++i; args = expr().text(); if (!err.empty()) return; }
            std::string n = name.text();
            if (!callable().count(n)) { err = "CALL " + n + " is not one a tool may call"; return; }
            if (++m.calls > 200) { err = "more than 200 calls"; return; }
            KV kv;
            std::istringstream is(args);
            std::string pair;
            while (is >> pair) {
                size_t e = pair.find('=');
                if (e != std::string::npos) kv[pair.substr(0, e)] = pair.substr(e + 1);
            }
            kv["actor"] = m.author.actor;
            kv["source"] = resident::sourceName(m.author.source == Source::Console ? Source::Visitor : m.author.source);
            std::string r = resident::hooks().exec ? resident::hooks().exec(n, kv) : "err no exec";
            m.vars["RESULT$"] = strv(r.substr(0, 200));
        } else {
            err = "unknown statement " + w;
        }
    }
};

// Runs the section starting at statement index s; returns false on an error (kept in m.error).
bool runSection(Machine& m, int s) {
    if (s < 0 || !m.error.empty()) return m.error.empty();
    int pc = s;
    m.stack.clear();
    for (int steps = 0; steps < 5000; ++steps) {
        if (pc < 0 || pc >= (int)m.prog.lines.size()) return true;
        m.pc = pc;
        Run r{m};
        r.t = &m.prog.lines[pc];
        int next = -1;
        r.statement(next);
        if (r.err.empty() && r.i < r.t->size()) r.err = "unexpected " + r.peek().s;
        if (!r.err.empty()) {
            m.error = "line " + std::to_string(m.prog.srcLine[pc]) + ": " + r.err;
            return false;
        }
        if (next == -2) return true;
        pc = next >= 0 ? next : pc + 1;
    }
    m.error = "a section ran more than 5000 statements";
    return false;
}

// ---------------------------------------------------------------- files

struct ToolFile {
    std::string name, author, source, about;
    std::vector<std::string> lines;
    bool found = false;
};

bool validName(const std::string& n) {
    if (n.empty() || n.size() > 8) return false;
    for (char c : n) if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-')) return false;
    return true;
}

ToolFile readTool(const std::string& name) {
    ToolFile t;
    t.name = upper(name);
    std::ifstream f(dir() + "/" + t.name + ".BAS");
    if (!f) return t;
    t.found = true;
    std::string l;
    while (std::getline(f, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        t.lines.push_back(l);
    }
    for (auto& x : t.lines) {
        if (x.rfind("REM AUTHOR: ", 0) == 0) {
            std::string a = x.substr(12);
            size_t p = a.find(" (");
            t.author = a.substr(0, p);
            if (p != std::string::npos) {
                t.source = a.substr(p + 2);
                size_t e = t.source.find_first_of(",)");
                t.source = t.source.substr(0, e);
            }
        } else if (x.rfind("REM ", 0) == 0 && t.about.empty() && x.rfind("REM SAVED", 0) != 0) {
            t.about = x.substr(4);
        }
    }
    return t;
}

std::vector<std::string> toolNames() {
    std::vector<std::string> out;
    if (DIR* d = opendir(dir().c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n.substr(n.size() - 4) == ".BAS") out.push_back(n.substr(0, n.size() - 4));
        }
        closedir(d);
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool writeTool(const std::string& name, const Who& by, const std::string& extra, const std::vector<std::string>& body) {
    ::mkdir(dir().c_str(), 0755);
    std::time_t now = std::time(nullptr);
    char stamp[32];
    std::tm tm{};
    localtime_r(&now, &tm);
    std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M", &tm);
    std::ofstream f(dir() + "/" + name + ".BAS", std::ios::trunc);
    f << "REM AUTHOR: " << by.actor << " (" << resident::sourceName(by.source) << extra << ")\n"
      << "REM SAVED: " << stamp << "\n";
    for (auto& l : body) if (l.rfind("REM AUTHOR:", 0) != 0 && l.rfind("REM SAVED:", 0) != 0) f << l << "\n";
    return (bool)f;
}

// ---------------------------------------------------------------- state between openings

// NAME.STATE beside the tool: one variable a line (NAME=12, NAME$="text", A(3)=5, DIM A=10),
// written when its window closes, every 5 s while it changes, and at the app's close.
std::string stateFile(const std::string& name) { return dir() + "/" + name + ".STATE"; }

std::string encodeState(const Machine& m) {
    std::ostringstream os;
    for (auto& d : m.dims) os << "DIM " << d.first << "=" << d.second << "\n";
    for (auto& v : m.vars) {
        if (v.first == "RESULT$") continue;
        os << v.first << "=";
        if (v.second.isStr) {
            os << '"';
            for (char c : v.second.s) { if (c == '"' || c == '\\') os << '\\'; os << (c == '\n' ? ' ' : c); }
            os << '"';
        } else os << v.second.n;
        os << "\n";
    }
    return os.str();
}

void loadState(Machine& m, const std::string& name) {
    std::ifstream f(stateFile(name));
    if (!f) return;
    std::string l;
    while (std::getline(f, l)) {
        size_t e = l.find('=');
        if (e == std::string::npos) continue;
        std::string k = l.substr(0, e), v = l.substr(e + 1);
        if (k.rfind("DIM ", 0) == 0) { m.dims[k.substr(4)] = std::max(1LL, std::min(1000LL, std::atoll(v.c_str()))); continue; }
        if (!v.empty() && v[0] == '"') {
            std::string s;
            for (size_t i = 1; i + 1 < v.size(); ++i) { if (v[i] == '\\' && i + 2 < v.size()) ++i; s += v[i]; }
            m.vars[k] = strv(s);
        } else m.vars[k] = num(std::atoll(v.c_str()));
    }
    m.restored = true;
}

// ---------------------------------------------------------------- tool windows

class TToolWindow : public TWindow, public ww::Describable {
public:
    TToolWindow(const TRect& r, const ToolFile& tf)
        : TWindowInit(&TToolWindow::initFrame), TWindow(r, (tf.name + ".BAS").c_str(), wnNoNumber) {
        TRect in = getExtent();
        in.grow(-1, -1);
        view = new TToolView(in);
        insert(view);
        name = tf.name;
        m.author = Who{tf.author.empty() ? "unknown" : tf.author,
                       tf.source == "resident" ? Source::Resident : tf.source == "console" ? Source::Console : Source::Visitor};
        m.view = view;
        view->m = &m;
        m.startMs = nowMs();
        if (!parseProgram(tf.lines, m.prog)) m.error = m.prog.error;
        loadState(m, name);
    }
    void saveState() {
        std::string s = encodeState(m);
        size_t h = std::hash<std::string>()(s);
        if (h == savedHash) return;
        savedHash = h;
        std::ofstream f(stateFile(name), std::ios::trunc);
        f << s;
    }
    void close() override {
        saveState();
        TWindow::close();
    }
    size_t savedHash = 0;
    long long nextSave = 0;
    void start() {
        runSection(m, m.prog.start);
        nextTick = nowMs() + m.prog.tickMs;
        show();
    }
    void show() {
        if (!m.error.empty() && !view->rows.empty()) {
            std::string e = "ERROR " + m.error;
            view->fg = 12;
            view->put(0, (long long)view->rows.size() - 1, e + std::string(view->rows[0].size(), ' '));
        }
        view->drawView();
    }
    void tick() {
        if (nowMs() >= nextSave) { nextSave = nowMs() + 5000; saveState(); }
        if (m.prog.tick < 0 || !m.error.empty() || nowMs() < nextTick) return;
        nextTick = nowMs() + m.prog.tickMs;
        ++ticks;
        runSection(m, m.prog.tick);
        show();
    }
    void key(const std::string& k) {
        m.key = k;
        ++keys;
        runSection(m, m.prog.key);
        show();
    }
    std::string describeState() const override {
        return "tool " + name + ".BAS by " + m.author.actor + (m.restored ? ", its state kept from before" : "") + ", " +
               (m.error.empty() ? "running" : "stopped at an error: " + m.error) + "; " + std::to_string(ticks) +
               " ticks, " + std::to_string(keys) + " keys";
    }
    bool describeText(std::vector<std::string>& out) const override {
        out = view->rows;
        for (auto& r : out) while (!r.empty() && r.back() == ' ') r.pop_back();
        return true;
    }
    TToolView* view;
    Machine m;
    std::string name;
    long long nextTick = 0;
    int ticks = 0, keys = 0;
};

std::string keyName(const TEvent& ev) {
    switch (ev.keyDown.keyCode) {
        case kbUp: return "UP";
        case kbDown: return "DOWN";
        case kbLeft: return "LEFT";
        case kbRight: return "RIGHT";
        case kbEnter: return "ENTER";
        case kbBack: return "BACK";
        default: break;
    }
    char c = ev.keyDown.charScan.charCode;
    if (c >= 32 && c < 127) return std::string(1, c);
    return "";
}

void TToolView::handleEvent(TEvent& ev) {
    if (ev.what == evKeyDown) {
        std::string k = keyName(ev);
        if (!k.empty()) {
            if (auto* w = dynamic_cast<TToolWindow*>(owner)) w->key(k);
            clearEvent(ev);
            return;
        }
    }
    TView::handleEvent(ev);
}

std::vector<TToolWindow*> openTools() {
    std::vector<TToolWindow*> out;
    if (!TProgram::deskTop || !TProgram::deskTop->first()) return out;
    TView* f = TProgram::deskTop->first();
    TView* v = f;
    do {
        if (auto* t = dynamic_cast<TToolWindow*>(v)) out.push_back(t);
        v = v->next;
    } while (v && v != f);
    return out;
}

TToolWindow* runTool(const std::string& name, const TRect* at, std::string& err) {
    ToolFile tf = readTool(name);
    if (!tf.found) { err = "no tool " + upper(name) + ".BAS"; return nullptr; }
    TRect r;
    if (at) r = *at;
    else {
        TRect d = TProgram::deskTop->getExtent();
        int w = std::min(d.b.x - 4, 44), h = std::min(d.b.y - 4, 14);
        int n = (int)openTools().size();
        r = TRect(4 + 2 * n, 2 + n, 4 + 2 * n + w, 2 + n + h);
    }
    auto* w = new TToolWindow(r, tf);
    life::insertLifeWindow(w);
    resident::settle();
    w->start();
    return w;
}

class TToolListWindow : public TWindow, public ww::Describable {
public:
    TToolListWindow(const TRect& r) : TWindowInit(&TToolListWindow::initFrame), TWindow(r, "TOOLS", wnNoNumber) {
        TRect in = getExtent();
        in.grow(-1, -1);
        view = new TToolView(in);
        insert(view);
        refresh();
    }
    void refresh() {
        names = toolNames();
        view->clear();
        view->put(0, 0, "Tools the residents wrote. Press a number to run one.");
        int y = 2;
        for (size_t i = 0; i < names.size() && i < 9; ++i) {
            ToolFile tf = readTool(names[i]);
            std::string line = std::to_string(i + 1) + "  " + names[i] + ".BAS";
            line += std::string(std::max<int>(1, 17 - (int)line.size()), ' ') + "by " + tf.author + "  " + tf.about;
            view->put(1, y++, line);
        }
        view->drawView();
    }
    std::string describeState() const override {
        return "list of the residents' tools: " + std::to_string(names.size()) + " in TOOLS/";
    }
    bool describeText(std::vector<std::string>& out) const override {
        out = view->rows;
        for (auto& r : out) while (!r.empty() && r.back() == ' ') r.pop_back();
        return true;
    }
    void handleEvent(TEvent& ev) override {
        if (ev.what == evKeyDown) {
            char c = ev.keyDown.charScan.charCode;
            if (c >= '1' && c <= '9' && (size_t)(c - '1') < names.size()) {
                std::string err;
                runTool(names[c - '1'], nullptr, err);
                clearEvent(ev);
                return;
            }
        }
        TWindow::handleEvent(ev);
    }
    TToolView* view;
    std::vector<std::string> names;
};

const char* kClock =
    "REM A clock: the time in the middle and a bar that fills with the minute's seconds.\n"
    "ON START\n"
    "  CLS\n"
    "END\n"
    "ON TICK 250\n"
    "  CLS\n"
    "  LET T$ = PAD$(HOUR) + \":\" + PAD$(MIN) + \":\" + PAD$(SEC)\n"
    "  AT (W - 8) / 2, H / 2 - 1 PRINT T$\n"
    "  LET I = 0\n"
    "  LET N = SEC * (W - 2) / 59\n"
    "bar:\n"
    "  IF I < N THEN CELL 1 + I, H / 2 + 1, \"#\"\n"
    "  IF I >= N THEN CELL 1 + I, H / 2 + 1, \".\"\n"
    "  LET I = I + 1\n"
    "  IF I < W - 2 THEN GOTO bar\n"
    "END\n";

const char* kEtch =
    "REM An etch-a-sketch: arrow keys move the pen, space lifts or lowers it, c clears.\n"
    "ON START\n"
    "  LET X = W / 2\n"
    "  LET Y = H / 2\n"
    "  LET PEN = 1\n"
    "  LET UNDER$ = \" \"\n"
    "  CLS\n"
    "  CELL X, Y, \"@\"\n"
    "END\n"
    "ON KEY\n"
    "  IF PEN = 1 THEN CELL X, Y, \"*\"\n"
    "  IF PEN = 0 THEN CELL X, Y, UNDER$\n"
    "  IF KEY$ = \"UP\" AND Y > 0 THEN LET Y = Y - 1\n"
    "  IF KEY$ = \"DOWN\" AND Y < H - 1 THEN LET Y = Y + 1\n"
    "  IF KEY$ = \"LEFT\" AND X > 0 THEN LET X = X - 1\n"
    "  IF KEY$ = \"RIGHT\" AND X < W - 1 THEN LET X = X + 1\n"
    "  IF KEY$ = \" \" THEN LET PEN = 1 - PEN\n"
    "  IF KEY$ = \"c\" THEN CLS\n"
    "  LET UNDER$ = PEEK$(X, Y)\n"
    "  CELL X, Y, \"@\"\n"
    "END\n";

const char* kPurr =
    "REM A cat who remembers: any key pets her, and she keeps the count from one opening to the next.\n"
    "ON START\n"
    "  DIM EYES$(3)\n"
    "  EYES$(0) = \"o.o\"\n"
    "  EYES$(1) = \"^.^\"\n"
    "  EYES$(2) = \"=.=\"\n"
    "  GOSUB draw\n"
    "  STOP\n"
    "draw:\n"
    "  CLS\n"
    "  LET X = (W - 9) / 2\n"
    "  LET Y = H / 2 - 2\n"
    "  AT X, Y PRINT \" /\\_/\\ \"\n"
    "  LET E$ = EYES$(PETS MOD 3)\n"
    "  IF BLINK = 1 THEN LET E$ = \"-.-\"\n"
    "  AT X, Y + 1 PRINT \"( \" + E$ + \" )\"\n"
    "  AT X, Y + 2 PRINT \" > ^ < \"\n"
    "  AT 1, H - 1 PRINT \"pets: \" + STR$(PETS)\n"
    "  IF RESTORED = 1 THEN AT 1, 0 PRINT \"she remembers you\"\n"
    "  RETURN\n"
    "END\n"
    "ON KEY\n"
    "  LET PETS = PETS + 1\n"
    "  GOSUB draw\n"
    "END\n"
    "ON TICK 1500\n"
    "  LET BLINK = 1 - BLINK\n"
    "  GOSUB draw\n"
    "END\n";

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream is(s);
    std::string l;
    while (std::getline(is, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        out.push_back(l);
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------- public

void seed() {
    struct { const char* name; const char* author; const char* body; } seeds[] = {{"CLOCK", "wob", kClock}, {"ETCH", "wib", kEtch}, {"PURR", "scramble", kPurr}};
    for (auto& s : seeds)
        if (!readTool(s.name).found)
            writeTool(s.name, Who{s.author, Source::Resident}, ", seeded with the app", splitLines(s.body));
}

bool isToolWindow(TWindow* w) { return w && dynamic_cast<TToolWindow*>(w) != nullptr; }
bool isToolListWindow(TWindow* w) { return w && dynamic_cast<TToolListWindow*>(w) != nullptr; }

void openList() {
    if (!TProgram::deskTop) return;
    TRect d = TProgram::deskTop->getExtent();
    int w = std::min(d.b.x - 4, 70), h = std::min(d.b.y - 4, 15);
    life::insertLifeWindow(new TToolListWindow(TRect(6, 3, 6 + w, 3 + h)));
    resident::settle();
}

void tick() {
    for (auto* t : openTools()) t->tick();
}

void saveAll() {
    for (auto* t : openTools()) t->saveState();
}

std::string nameOf(TWindow* w) {
    auto* t = dynamic_cast<TToolWindow*>(w);
    return t ? t->name : "";
}

TWindow* reopen(const std::string& name, int x, int y, int w, int h, std::string& err) {
    TRect r(x, y, x + w, y + h);
    return runTool(name, &r, err);
}

bool owns(const std::string& cmd) {
    return cmd == "tool_save" || cmd == "tool_run" || cmd == "tool_list" || cmd == "tool_read";
}

std::string command(const std::string& name, const KV& kv) {
    Who w = resident::currentWho();
    std::string n = upper(str(kv, "name"));
    if (name == "tool_list") {
        std::ostringstream os;
        os << "{\"dir\":" << q(dir()) << ",\"tools\":[";
        auto names = toolNames();
        for (size_t i = 0; i < names.size(); ++i) {
            ToolFile tf = readTool(names[i]);
            os << (i ? "," : "") << "{\"name\":" << q(tf.name) << ",\"author\":" << q(tf.author) << ",\"source\":"
               << q(tf.source) << ",\"about\":" << q(tf.about) << ",\"lines\":" << tf.lines.size() << "}";
        }
        os << "]}";
        return os.str();
    }
    if (!validName(n)) return "err name= 1 to 8 letters, digits, - or _";
    if (name == "tool_read") {
        ToolFile tf = readTool(n);
        if (!tf.found) return "err no tool " + n + ".BAS";
        std::ostringstream os;
        os << "{\"name\":" << q(tf.name) << ",\"author\":" << q(tf.author) << ",\"standing\":\"data\",\"lines\":[";
        for (size_t i = 0; i < tf.lines.size(); ++i) os << (i ? "," : "") << q(tf.lines[i]);
        os << "]}";
        return os.str();
    }
    if (name == "tool_save") {
        if (w.source != Source::Resident || !(w.actor == "wib" || w.actor == "wob" || w.actor == "scramble"))
            return "err refused: tools are written by a resident (actor=wib|wob|scramble source=resident); this came from " +
                   std::string(resident::sourceName(w.source)) + " " + w.actor;
        ToolFile old = readTool(n);
        if (old.found && old.author != w.actor)
            return "err refused: " + n + ".BAS is " + old.author + "'s; only its author changes it";
        auto lines = splitLines(str(kv, "text"));
        Program p;
        if (!parseProgram(lines, p)) return "err the tool does not parse: " + p.error;
        if (!writeTool(n, w, "", lines)) return "err could not write " + n + ".BAS";
        return "{\"ok\":true,\"name\":" + q(n) + ",\"file\":" + q(dir() + "/" + n + ".BAS") + ",\"author\":" + q(w.actor) + "}";
    }
    // tool_run
    std::string err;
    TRect r, *at = nullptr;
    if (kv.count("w") && kv.count("h")) {
        int x = std::atoi(str(kv, "x", "2").c_str()), y = std::atoi(str(kv, "y", "1").c_str());
        r = TRect(x, y, x + std::atoi(str(kv, "w").c_str()), y + std::atoi(str(kv, "h").c_str()));
        at = &r;
    }
    TToolWindow* tw = runTool(n, at, err);
    if (!tw) return "err " + err;
    return "{\"ok\":true,\"id\":" + q(resident::hooks().idOf(tw)) + ",\"name\":" + q(n) + ",\"author\":" + q(tw->m.author.actor) +
           ",\"error\":" + q(tw->m.error) + "}";
}

}  // namespace tools
