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
        eventMask = evMouseDown | evMouseUp | evMouseWheel;
        growMode = 0;
    }
    void draw() override {
        TDrawBuffer b;
        b.moveStr(0, text.c_str(), attr);
        writeLine(0, 0, size.x, 1, b);
    }
    void handleEvent(TEvent& e) override {
        if (e.what & (evMouseDown | evMouseUp | evMouseWheel)) {
            hide();
            if (owner) owner->handleEvent(e);
            return;
        }
        TView::handleEvent(e);
    }
};

}  // namespace resident

#endif
