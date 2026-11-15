// The event score window: the sections of a score with the one being played marked,
// the instruction now, the bar and beat, and what the ears heard in each section.
// The app's clock runs it (a timer); commands drive it (sound_host.cpp).
#pragma once

#define Uses_TView
#define Uses_TWindow
#define Uses_TRect
#define Uses_TEvent
#include <tvision/tv.h>

#include <map>
#include <string>
#include <vector>

#include "ega_theme.h"
#include "score.h"

class TScoreView : public TView {
public:
    TScoreView(const TRect& bounds, const sound::Score& score, const std::string& origin);
    ~TScoreView() override;

    void draw() override;
    void handleEvent(TEvent& ev) override;

    // action=start|stop|state. Returns a JSON object or "err ...".
    std::string apiAction(const std::string& action, const std::map<std::string, std::string>& kv);
    std::string stateJson() const;
    std::string sentence() const;

    void start();
    void stop(const char* why);
    bool running() const { return running_; }

private:
    struct Heard {
        std::vector<std::string> notes;
        std::map<std::string, int> by;
        int hits = 0;
    };
    Heard heardIn(int i) const;
    void tick();
    void enter(int i);
    std::string windowId() const;

    sound::Score score_;
    std::string origin_;
    bool running_ = false, done_ = false;
    double startMs_ = 0, stopMs_ = 0;
    int section_ = -1;
    std::string startedBy_;
    std::vector<std::string> actResults_;
    TTimerId timer_ = nullptr;
};

class TScoreWindow : public TWindow {
public:
    TScoreWindow(const TRect& bounds, const sound::Score& score, const std::string& origin);
    TScoreView* view() const { return view_; }
    WW_COLOUR_PALETTE_OVERRIDE
private:
    TScoreView* view_ = nullptr;
};
