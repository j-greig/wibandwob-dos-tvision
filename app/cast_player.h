// The cast player window: plays an asciinema v2 cast with no child process. Its screen is
// ordinary window cells. Keys, frame controls and commands: docs/terminal-transport.md.
#ifndef CAST_PLAYER_H
#define CAST_PLAYER_H

#define Uses_TWindow
#define Uses_TView
#define Uses_TRect
#include <tvision/tv.h>

#include "describe.h"

#include <map>
#include <string>

class TCastView;

class TCastPlayerWindow : public TWindow, public ww::Describable {
public:
    TCastPlayerWindow(const TRect& r, const std::string& file, TCastView* view);
    std::string describeState() const override;
    bool describeText(std::vector<std::string>& out) const override;
    TCastView* view() const { return view_; }
    // action: play | pause | toggle; seek= seconds, speed= factor, loop= 0|1. Returns the state JSON or "err ...".
    std::string control(const std::map<std::string, std::string>& kv);
    std::string stateJson() const;
private:
    TCastView* view_;
    std::string file_;
};

// Opens a player for file at (x, y) or centred; nullptr with err when the file is not a cast.
TCastPlayerWindow* createCastPlayer(const std::string& file, int x, int y, std::string& err);

#endif
