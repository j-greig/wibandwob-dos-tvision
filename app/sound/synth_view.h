// The synth window: a two-octave keyboard played from the computer keyboard in the
// tracker layout, the voice (PC speaker or FM) and patch, and the last notes played
// with who played them. Commands drive the same view (sound_host.cpp).
#pragma once

#define Uses_TView
#define Uses_TWindow
#define Uses_TRect
#define Uses_TEvent
#include <tvision/tv.h>

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "ega_theme.h"
#include "synth.h"

class TSynthView : public TView {
public:
    explicit TSynthView(const TRect& bounds);
    ~TSynthView() override;

    void draw() override;
    void handleEvent(TEvent& ev) override;

    // action=note|notes|voice|patch|octave|gate|audio|render|state (see docs/machine-sound.md).
    // Returns a JSON object or "err ...".
    std::string apiAction(const std::string& action, const std::map<std::string, std::string>& kv);
    std::string stateJson() const;
    std::string sentence() const;

    // Plays notes now (each note's atMs counts from now) as `who`.
    void playNotes(const std::vector<sound::Note>& notes, const std::string& who);

    sound::Voice voice() const { return voice_; }
    const sound::Patch& patch() const;

private:
    struct Played {
        int midi;
        sound::Voice voice;
        std::string patch;
        int vel;
        double startMs, durMs, endMs;
        std::string who;
    };
    sound::Note makeNote(int midi, double atMs, double durMs, int vel) const;
    int keyToSemitone(char ch) const;
    void tick();

    sound::Voice voice_ = sound::Voice::Fm;
    int patch_ = 0;
    int octave_ = 4;
    int gateMs_ = 250;
    std::deque<Played> recent_;
    long played_ = 0, sent_ = 0, silenced_ = 0;
    TTimerId timer_ = nullptr;
};

class TSynthWindow : public TWindow {
public:
    explicit TSynthWindow(const TRect& bounds);
    TSynthView* view() const { return view_; }
    WW_COLOUR_PALETTE_OVERRIDE
private:
    TSynthView* view_ = nullptr;
};

// kv: voice=pcspk|fm, patch=<name>, octave=, gate=, audio=0|1.
TWindow* createSynthWindow(const TRect& bounds, const std::map<std::string, std::string>& kv = {});
