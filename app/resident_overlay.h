// A one-cell view in the application group, in front of the desktop: the resident's
// reading mark and its hand. Include after <tvision/tv.h> with Uses_TView and
// Uses_TDrawBuffer defined. It never takes focus; a click on it takes it down and
// goes to the view underneath.
#ifndef RESIDENT_OVERLAY_H
#define RESIDENT_OVERLAY_H

#include <string>

namespace resident {

class TOverlayCell : public TView {
public:
    std::string text;
    TColorAttr attr;
    explicit TOverlayCell(const TRect& r) : TView(r) {
        options = 0;
        // Clicks only. Turbo Vision gives a wheel event to every view in a group (it is not a
        // positional event), so a wheel event passed back to the owner came straight back here
        // and recursed until the stack ran out.
        eventMask = evMouseDown | evMouseUp;
        growMode = 0;
    }
    void draw() override {
        TDrawBuffer b;
        b.moveStr(0, text.c_str(), attr);
        writeLine(0, 0, size.x, 1, b);
    }
    void handleEvent(TEvent& e) override {
        // A click on the cell goes to the view underneath: hide, then offer it to the owner
        // once more (the hidden cell no longer holds the mouse). Never re-entered.
        static bool passing = false;
        if ((e.what & (evMouseDown | evMouseUp)) && !passing && (state & sfVisible)) {
            hide();
            passing = true;
            if (owner) owner->handleEvent(e);
            passing = false;
            return;
        }
        TView::handleEvent(e);
    }
};

}  // namespace resident

#endif
