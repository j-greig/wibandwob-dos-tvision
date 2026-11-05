// TR-808 Rhythm Composer window: the ASCII 808 face, the 16-step grid with a
// timer-driven playhead, keyboard control, API actions, step events and local
// preview audio. Port of microapps/wibwob-tr808 (renderer.ts, index.ts).
#pragma once

#define Uses_TView
#define Uses_TWindow
#define Uses_TRect
#define Uses_TEvent
#include <tvision/tv.h>

#include <map>
#include <string>

#include "ega_theme.h"
#include "tr808_engine.h"

// Receives every event the TR-808 publishes. `body` is the JSON object body
// without braces; the app adds the window id and calls publish_event.
using Tr808EventSink = void (*)(TWindow* window, const char* type, const std::string& body);
void tr808SetEventSink(Tr808EventSink sink);

class TTR808View : public TView {
public:
    explicit TTR808View(const TRect& bounds);
    ~TTR808View() override;

    void draw() override;
    void handleEvent(TEvent& ev) override;

    tr808::Engine& engine() { return eng_; }

    // One API action: open/play/stop/toggle/tempo/select/toggle_step/set_step/
    // set_param/load_preset/clear/set_pattern/swing/scale/accent/master/
    // last_step/mute/solo/audio/random/copy/paste/text/state.
    // Returns a JSON object string or "err ...".
    std::string apiAction(const std::string& action, const std::map<std::string, std::string>& kv);
    std::string stateJson() const;

    void play();
    void stop();

private:
    struct Pal;
    Pal palette() const;
    void schedule();
    void onTimer();
    void fire(const tr808::StepEvent& ev);
    void publish(const char* type, const std::string& body);
    std::string transportBody(const char* change) const;
    void setTempo(int bpm);
    std::string textCommand(const std::string& line);

    tr808::Engine eng_;
    int cursor_ = 0;
    size_t presetIdx_ = 0;
    TTimerId timer_ = nullptr;
};

class TTR808Window : public TWindow {
public:
    explicit TTR808Window(const TRect& bounds);
    TTR808View* view() const { return view_; }
    WW_COLOUR_PALETTE_OVERRIDE
private:
    TTR808View* view_ = nullptr;
};

// Opens at `bounds`. kv: preset=<name>, play=1, audio=0|1.
TWindow* createTR808Window(const TRect& bounds, const std::map<std::string, std::string>& kv = {});
