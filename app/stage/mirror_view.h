// Mirror: a view that shows the live screen, sampled at a reduced scale, inside a window.
// Stage: open NAME X Y W H mirror [flip] [block] [of NAME2] |title
//   no "of": the whole desktop scaled to the window's interior; "of NAME2": that window's rectangle.
//   flip reverses positions left to right (characters keep their shape).
//   block: each output cell takes the most frequent character of the source cells it covers, skipping
//   blanks and the source's commonest character (the desktop fill); the default takes the nearest source cell.
// The view reads TScreen::screenBuffer, which holds the frame drawn before, so a mirror inside the area
// it shows contains its own previous frame: a nested tunnel that settles after one tick per level.
// It redraws itself on its own timer.
#ifndef WWDOS_MIRROR_VIEW_H
#define WWDOS_MIRROR_VIEW_H

#define Uses_TView
#define Uses_TWindow
#define Uses_TDeskTop
#include <tvision/tv.h>

class TMirrorView : public TView
{
public:
    TMirrorView(const TRect& r, TDeskTop* desk, TView* source, bool flip, bool block);
    void draw() override;
    void blank();           // spaces, before a full settle
    void handleEvent(TEvent& ev) override;
    void shutDown() override;

    TDeskTop* desk;
    TView* source;          // a window on desk, or null for the whole desktop
    bool flip, block;
private:
    TTimerId timer {0};
};

// Puts a mirror view over the window's interior, or updates the one already there.
void stageMirrorSettle(TDeskTop* desk, bool full);
void stageMirror(TWindow* w, TDeskTop* desk, TView* source, bool flip, bool block);

#endif
