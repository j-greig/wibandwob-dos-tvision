/*-----------------------------------------------------------*/
/*   tuiforge_view.h — TUIFORGE.DSK render viewer            */
/*   Opens tuiforge grids: tui.txt (UTF-8 glyphs) plus       */
/*   tui.fg / tui.bg (one hex CGA digit per cell).           */
/*   Art is authoritative: cells render in authentic CGA     */
/*   RGB via ThemeManager::cgaRgb, immune to skin palettes.  */
/*   Corpus: ~/Repos/tuiforge/renders/<scene>[/sub]/default  */
/*   80 cols wide; 25 rows landscape, 50 rows portrait.      */
/*-----------------------------------------------------------*/

#ifndef TUIFORGE_VIEW_H
#define TUIFORGE_VIEW_H

#define Uses_TWindow
#define Uses_TView
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TEvent
#include <tvision/tv.h>

#include <string>
#include <vector>

// One rendered cell: a UTF-8 glyph plus CGA fg/bg indices (0-15).
struct TuiforgeCell {
    std::string glyph;   // one grid-true glyph (1-4 bytes)
    uint8_t fg = 7;
    uint8_t bg = 0;
};

// The grid itself, loaded from a render directory.
struct TuiforgeGrid {
    std::vector<std::vector<TuiforgeCell>> rows;
    int width = 0;                 // max cells across
    std::string dir;               // source directory (for reload/serialise)
    std::string error;             // non-empty = load failed, message inside

    int height() const { return (int)rows.size(); }
    bool ok() const { return error.empty() && !rows.empty(); }
};

// Load tui.txt + tui.fg + tui.bg from `dir`. Missing fg/bg files degrade to
// light-grey-on-black rather than failing: the txt alone is still art.
TuiforgeGrid loadTuiforgeGrid(const std::string& dir);

// Resolve a user-supplied name to a render dir. Accepts absolute paths,
// paths relative to the corpus root, bare scene names ("kevart/cat3d"),
// and appends "/default" when the target has no tui.txt of its own.
std::string resolveTuiforgeDir(const std::string& nameOrPath);
const std::string& tuiforgeRoot();   // ~/Repos/tuiforge/renders

// List every render dir under the corpus root that has default/tui.txt,
// as corpus-relative names ("the-missing-9-painter-b", "kevart/cat3d").
std::vector<std::string> listTuiforgeRenders();

/*---------------------------  viewer  ----------------------*/

class TTuiforgeView : public TView {
public:
    TTuiforgeView(const TRect& bounds, TuiforgeGrid&& grid);
    ~TTuiforgeView();

    virtual void draw() override;
    virtual void handleEvent(TEvent& event) override;
    virtual void changeBounds(const TRect& bounds) override;

    const TuiforgeGrid& grid() const { return grid_; }

    // TUIFORGE.TV — dead channel that tunes itself: auto-cycles the whole
    // corpus on a timer, shuffled once per power-on. Space pauses, N skips,
    // channel banner bottom-left names what's playing.
    void startChannel(int periodMs = 8000);
    bool isChannel() const { return channel_; }

private:
    TuiforgeGrid grid_;
    int scrollX_ = 0, scrollY_ = 0;
    void clampScroll();

    bool channel_ = false;
    std::vector<std::string> playlist_;
    size_t station_ = 0;
    TTimerId timerId_ = 0;
    int periodMs_ = 8000;
    void tuneNext();
};

class TTuiforgeWindow : public TWindow {
public:
    // Auto-sizes to the grid (clamped to owner) and spreads via the app's
    // placement when inserted through the open_tuiforge command.
    // channel=true powers on TUIFORGE.TV (grid ignored; corpus auto-cycles).
    TTuiforgeWindow(const TRect& bounds, const std::string& title,
                    TuiforgeGrid&& grid, bool channel = false);

    // "tv" for channel windows — workspaces respawn the channel, not a still.
    const std::string& renderDir() const { return renderDir_; }

    // Channel power-on happens at first expose — see setState (timers need
    // the owner chain up to TProgram, absent during construction).
    virtual void setState(ushort aState, Boolean enable) override;

private:
    std::string renderDir_;
    bool channelPending_ = false;
};

/*---------------------------  picker  ----------------------*/

// Scrolling list of every render in the corpus; Enter / double-click boots
// the focused render in its own TTuiforgeWindow (via open_tuiforge).
class TTuiforgePickerView : public TView {
public:
    TTuiforgePickerView(const TRect& bounds, TScrollBar* aScrollBar);

    virtual void draw() override;
    virtual void handleEvent(TEvent& event) override;

private:
    std::vector<std::string> names_;
    int focused_ = 0;
    int top_ = 0;
    TScrollBar* vScroll_;

    void openFocused();
    void syncScroll();
};

class TTuiforgePickerWindow : public TWindow {
public:
    TTuiforgePickerWindow(const TRect& bounds);
};

#endif // TUIFORGE_VIEW_H
