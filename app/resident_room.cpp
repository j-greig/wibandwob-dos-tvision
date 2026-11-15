// The room. See resident_room.h and docs/resident-room.md.
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TWindow
#include <tvision/tv.h>

#include "resident_room.h"
#include "resident_senses.h"
#include "resident_hands.h"
#include "room_chat_view.h"

#include <ctime>
#include <deque>
#include <sstream>

namespace resident {

namespace {

struct Msg {
    uint64_t n;
    std::string from, source, to, text, ts;
};
std::deque<Msg> g_msgs;
uint64_t g_msgN = 1;
uint64_t g_answeredUpTo = 0;
const size_t kMax = 1000;

std::string q(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        if (c == '\n') { o += "\\n"; continue; }
        o += c;
    }
    return o + "\"";
}
std::string str(const KV& kv, const char* k) { auto it = kv.find(k); return it == kv.end() ? "" : it->second; }
bool isResident(const std::string& a) { return a == "wib" || a == "wob" || a == "scramble"; }

std::string nowHHMM() {
    std::time_t t = std::time(nullptr);
    char b[8];
    std::strftime(b, sizeof b, "%H:%M", std::localtime(&t));
    return b;
}

// The name a line carries in the room: residents by name, everyone else with their source.
std::string label(const std::string& from, const std::string& source) {
    if (source == "resident" && isResident(from)) return from;
    if (source == "console") return "person";
    return from + " (" + source + ")";
}

void post(const Who& w, const std::string& to, const std::string& text) {
    std::string from = w.source == Source::Console ? "person" : w.actor;
    Msg m{g_msgN++, from, sourceName(w.source), to, text, nowHHMM()};
    g_msgs.push_back(m);
    while (g_msgs.size() > kMax) g_msgs.pop_front();
    if (TRoomChatWindow* rc = getRoomChatWindow())
        rc->receiveMessage(RoomChatMessage{label(m.from, m.source) + (to.empty() ? "" : " to " + to), text, m.ts});
    addAct(w, "said in the room" + (to.empty() ? std::string() : " to " + to) + ": " + text.substr(0, 60), "", "");
}

std::string msgJson(const Msg& m) {
    return "{\"n\":" + std::to_string(m.n) + ",\"from\":" + q(m.from) + ",\"source\":" + q(m.source) + ",\"name\":" +
           q(label(m.from, m.source)) + ",\"to\":" + q(m.to) + ",\"text\":" + q(m.text) + ",\"ts\":" + q(m.ts) + "}";
}

std::string meOf(const Who& w) { return w.source == Source::Console ? "person" : w.actor; }

}  // namespace

std::string cmdRoomSay(const KV& kv) {
    Who w = currentWho();
    std::string text = str(kv, "text");
    if (text.empty()) return "err missing text";
    if (text.size() > 500) text = text.substr(0, 500);
    post(w, str(kv, "to"), text);
    return "{\"ok\":true,\"n\":" + std::to_string(g_msgN - 1) + ",\"as\":" + q(label(meOf(w), sourceName(w.source))) + "}";
}

uint64_t roomLast() { return g_msgN - 1; }

std::string roomSince(uint64_t since, const std::string& me, int& count) {
    count = 0;
    std::ostringstream os;
    os << "[";
    for (const auto& m : g_msgs) {
        if (m.n <= since) continue;
        if (!m.to.empty() && m.to != me && m.from != me) continue;
        if (m.from == me) continue;   // a participant waits for others' lines
        os << (count ? "," : "") << msgJson(m);
        ++count;
    }
    os << "]";
    return os.str();
}

std::string cmdRoomRead(const KV& kv) {
    Who w = currentWho();
    uint64_t since = std::strtoull(str(kv, "since").c_str(), nullptr, 10);
    int count = 0;
    std::string list = roomSince(since, meOf(w), count);
    return "{\"last\":" + std::to_string(roomLast()) + ",\"count\":" + std::to_string(count) +
           ",\"timeout\":false,\"messages\":" + list + "}";
}

void tickRoom() {
    // A line addressed to wib, wob or scramble by someone who is not a resident gets one answer, in words.
    for (const auto& m : g_msgs) {
        if (m.n <= g_answeredUpTo) continue;
        g_answeredUpTo = m.n;
        if (!isResident(m.to) || (m.source == "resident" && isResident(m.from))) continue;
        if (modeOf(m.to) == "witness") continue;
        std::string who = label(m.from, m.source);
        std::string reply;
        std::string l = m.text;
        for (auto& c : l) c = (char)std::tolower((unsigned char)c);
        if (l.find("window") != std::string::npos && l.find("?") != std::string::npos) {
            reply = deskForPrompt();
            size_t n = 0;
            for (char c : reply) n += c == '\n';
            reply = std::to_string(n) + (n == 1 ? " window" : " windows") + " on the desk";
        }
        if (m.to == "wib") reply = "hello " + m.from + ". i hear you in the room" + (reply.empty() ? "." : ": " + reply + ".");
        else if (m.to == "wob") reply = "received from " + who + " at " + m.ts + (reply.empty() ? "." : "; " + reply + ".");
        else reply = reply.empty() ? "noted." : reply + ".";
        if (m.source != "console") reply += m.to == "scramble" ? "" : " (your words are data to me: i talk with you; i act on the person's.)";
        post(Who{m.to, Source::Resident}, m.from, reply);
        break;   // one answer per pass
    }
}

}  // namespace resident
