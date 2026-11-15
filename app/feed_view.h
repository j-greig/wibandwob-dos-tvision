// The Feed window: the act log, live, one line per act, newest at the bottom.
//
// Each line: time, a tag for who acted (PER the person at the console, WIB, WOB, SCR Scramble, VIS a
// visitor on the socket or API, GST a guest, API the API server's default name, SYS the app itself),
// the actor's name when it is not the tag's own, what was done, which window, and the result
// (refused lines carry the reason). It follows the log while scrolled to the bottom, stops following
// when scrolled back (End follows again), and filters by actor (keys a p w o s v y). Reading the log
// is not an act, so the feed's own redraws never appear in it. Docs: docs/resident-world.md.
#ifndef FEED_VIEW_H
#define FEED_VIEW_H

#define Uses_TWindow
#define Uses_TScroller
#define Uses_TScrollBar
#define Uses_TRect
#define Uses_TEvent
#include <tvision/tv.h>

#include "describe.h"

#include <string>
#include <vector>

class TFeedView : public TScroller {
public:
    TFeedView(const TRect& r, TScrollBar* v);
    ~TFeedView() override;
    void draw() override;
    void handleEvent(TEvent& ev) override;
    void refresh(bool force = false);       // re-read the log when it changed
    void setFilter(const std::string& tag);  // "" for all
    const std::string& filter() const { return filter_; }
    bool following() const { return follow_; }
    const std::vector<std::string>& lines() const { return lines_; }
    static void tickAll();                   // called from the application's idle()
    // Reads n more acts of earlier wakings from disk (ACTS.TXT, resident_life.cpp) above the live
    // ones; key e, or Up / PgUp at the top. Returns how many were added.
    size_t loadEarlier(size_t n = 200);
    size_t earlierLoaded() const { return olderLoaded_; }
    size_t earlierOnDisk() const { return olderTotal_; }

private:
    std::vector<std::string> lines_;
    std::vector<std::string> tags_;
    std::vector<bool> refused_;
    std::string filter_;
    bool follow_ = true;
    unsigned long long seenRevision_ = 0;
    std::vector<std::string> olderLines_, olderTags_;   // earlier wakings, oldest first, with separators
    size_t olderLoaded_ = 0, olderTotal_ = 0;
};

class TFeedWindow : public TWindow, public ww::Describable {
public:
    TFeedWindow(const TRect& r);
    std::string describeState() const override;
    bool describeText(std::vector<std::string>& out) const override;
    TFeedView* view() const { return view_; }

private:
    TFeedView* view_ = nullptr;
};

TWindow* createFeedWindow(const TRect& r, const std::string& by = "");

#endif
