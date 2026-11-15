// Casts: a terminal window's output as a timed asciinema v2 file (a JSON header line, then
// [seconds, "o", "text"] lines). The recorder is fed from tvterm's reader thread; the player
// (cast_player.h) plays a cast back with no child process. docs/terminal-transport.md.
#ifndef CAST_RECORD_H
#define CAST_RECORD_H

#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace ww {

class CastRecorder {
public:
    ~CastRecorder();
    // Opens path, writes the header and the visible screen as the first event (t = 0).
    bool start(const std::string& path, int width, int height, const std::string& title,
               const std::vector<std::string>& screen, std::string& err);
    // Closes the file; returns the number of output events written (-1 when not recording).
    long stop();
    bool active();
    std::string path();
    double seconds();          // the recording's clock now
    // Output the program wrote to the terminal, as it arrives (any thread).
    void output(const char* data, size_t n);
    // The recording's clock holds while the program is paused, so a paused stretch is not in the cast.
    void hold(bool on);

private:
    std::mutex m_;
    FILE* f_ = nullptr;
    std::string path_, pending_;
    long events_ = 0;
    std::chrono::steady_clock::time_point t0_, heldAt_;
    double heldTotal_ = 0;
    bool held_ = false;
    std::string out_;              // output joined into the next event
    double outT_ = 0, outLast_ = 0;
    double clockLocked() const;
    void writeLocked(double t, const std::string& data);
    void writeRawLocked(double t, const std::string& data);
    void flushLocked();
};

// JSON string escaping and decoding for cast lines.
std::string castEscape(const std::string& s);

struct CastEvent {
    double t;
    std::string data;
};
struct Cast {
    int width = 80, height = 24;
    std::string title;
    std::vector<CastEvent> events;   // "o" events in file order
    double duration() const { return events.empty() ? 0 : events.back().t; }
};
// Reads an asciinema v2 file; false with err when it is not one.
bool loadCast(const std::string& path, Cast& out, std::string& err);

}  // namespace ww

#endif
