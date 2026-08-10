#ifndef NOTITLE_FRAME_H
#define NOTITLE_FRAME_H

#define Uses_TFrame
#define Uses_TDrawBuffer
#include <tvision/tv.h>

// For windows without titles, just use regular TFrame.
// The title gap isn't noticeable since we pass "" as the title.
typedef TFrame TNoTitleFrame;

// TCGAFrame — chunky MSDOS-style frame: solid block border in the window's
// accent colour with the title in an inverse white tab, plus working
// close/zoom hotspots. Falls back to standard TFrame drawing when the CGA
// chrome variant is off (ThemeManager::cgaChrome()). draw() implemented in
// frame_animation_window.cpp (needs the viewer classes for accent lookup).
class TCGAFrame : public TFrame
{
public:
    TCGAFrame(const TRect& r) : TFrame(r) {}
    virtual void draw() override;
};

// TGhostFrame — a completely invisible frame for frameless gallery windows.
//
// draw() writes nothing, so the 1-char border area is transparent (shows
// the desktop pattern behind it). The content view should be positioned
// with grow(0, 0) — i.e. at the full window bounds — so that art fills
// edge-to-edge. Window focus/move/resize still work normally because the
// frame view still exists and occupies the expected area; it's just silent.
class TGhostFrame : public TFrame
{
public:
    TGhostFrame(const TRect& r) : TFrame(r) {}

    virtual void draw() override
    {
        // Draw nothing — content bleeds to the edge, no chrome visible.
    }
};

#endif