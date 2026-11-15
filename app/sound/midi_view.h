// The MIDI window: what arrives and what leaves, newest at the bottom, with the device
// and counts. Read-only; commands drive MIDI (sound_host.cpp).
#pragma once

#define Uses_TView
#define Uses_TWindow
#define Uses_TRect
#define Uses_TEvent
#include <tvision/tv.h>

#include <string>
#include <vector>

#include "describe.h"
#include "ega_theme.h"

class TMidiView : public TView, public ww::Describable {
public:
    explicit TMidiView(const TRect& bounds);
    ~TMidiView() override;
    void draw() override;
    void handleEvent(TEvent& ev) override;
    std::string describeState() const override;
    bool describeText(std::vector<std::string>& out) const override;
    static std::string line(double ms, bool out, const std::string& from, const std::string& text);
private:
    TTimerId timer_ = nullptr;
};

class TMidiWindow : public TWindow {
public:
    explicit TMidiWindow(const TRect& bounds);
    TMidiView* view() const { return view_; }
    WW_COLOUR_PALETTE_OVERRIDE
private:
    TMidiView* view_ = nullptr;
};
