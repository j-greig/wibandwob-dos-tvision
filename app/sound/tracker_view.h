// The tracker window: channels across, rows down, a cursor on a cell's note,
// instrument or effect, and the row playing. The person types with the tracker
// keyboard; commands drive the same view (sound_host.cpp). It plays on the sound
// stream at the row's time on the one clock, and the ears hear each cell.
#pragma once

#define Uses_TView
#define Uses_TWindow
#define Uses_TRect
#define Uses_TEvent
#include <tvision/tv.h>

#include <map>
#include <string>
#include <vector>

#include "describe.h"
#include "ega_theme.h"
#include "tracker.h"

class TTrackerView : public TView, public ww::Describable {
public:
    TTrackerView(const TRect& bounds, const sound::Song& song, const std::string& origin);
    ~TTrackerView() override;

    void draw() override;
    void handleEvent(TEvent& ev) override;
    std::string describeState() const override;
    bool describeText(std::vector<std::string>& out) const override;

    // action=state|play|stop|loop|set|write|clear|tempo|rows|text|load|save|cursor. JSON or "err ...".
    std::string apiAction(const std::string& action, const std::map<std::string, std::string>& kv);
    std::string stateJson() const;

    void play(int fromRow = 0);
    void stop();
    const sound::Song& song() const { return song_; }
    double startedMs() const { return startMs_; }
    bool playing() const { return playing_; }

private:
    void tick();
    void playRow(int row, double atMs);
    std::string windowId() const;
    void moveCursor(int dr, int dc);

    sound::Song song_;
    std::string origin_;
    bool playing_ = false, loop_ = true;
    double startMs_ = 0;
    long nextRow_ = 0;      // rows since the start (not wrapped)
    int playRow_ = -1;
    int curRow_ = 0, curChan_ = 0, curField_ = 0;   // field: 0 note, 1 instrument, 2 effect
    int octave_ = 4, instr_ = -1;
    std::string startedBy_;
    TTimerId timer_ = nullptr;
};

class TTrackerWindow : public TWindow {
public:
    TTrackerWindow(const TRect& bounds, const sound::Song& song, const std::string& origin);
    TTrackerView* view() const { return view_; }
    WW_COLOUR_PALETTE_OVERRIDE
private:
    TTrackerView* view_ = nullptr;
};
