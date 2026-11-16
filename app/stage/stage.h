// Stage: a beat-clock script that moves, opens, closes and retitles the desktop's windows,
// so a page can play a short multi-window sequence (a token, a film) from one text file.
//
// /boot/stage.txt, one command per line, '#' comments:
//   clock 8                         ticks per second (default 8)
//   size 130 34                     the desktop the stage was laid out for; centred on a
//                                   bigger desktop; on a smaller one positions and the sizes the
//                                   script gives are scaled. A page resize lays the stage out again.
//   @2.5 CMD ARGS                   run CMD when the clock reaches 2.5 s (lines are sorted)
// commands (NAME is a window name of your choosing; |TITLE is the text after a bar, less one leading space if it has one):
//   open NAME X Y W H [ghost] [COLOUR] /boot/file.txt|title   W H = 0 0 fits the file's first frame;
//                                                      ghost = no frame, no shadow (a sprite); COLOUR is a
//                                                      colour family (see colour); with no file and W H given
//                                                      the window opens empty; open on an existing NAME moves and retitles it
//   open NAME X Y W H ghost clear ...                  a clear sprite: its blank cells show what is under it and its
//                                                      characters take the background of the cell they cover; it keeps
//                                                      its size (W H = 0 0 is the drawing's own size, no 16 x 6 floor)
//                                                      and tile and cascade leave it alone
//   open NAME X Y W H mirror [flip] [block] [of NAME2] |title   a window showing the live desktop (or window NAME2)
//                                                      scaled to its interior; flip reverses left to right; block
//                                                      keeps the commonest character per cell that is not fill
//                                                      (default: nearest cell); see mirror_view.h
//   move NAME X Y [in S] [ease]       glide the window to X Y over S seconds (default: at once)
//   rect NAME X Y W H [in S] [ease]   glide position and size; W H = 0 0 is the file's own size again
//   close NAME | raise NAME | lower NAME
//   pin NAME [off]                    a pinned window stays: Close All and Alt-F3 leave it, Tile and Cascade skip it, and it
//                                     stays in front of every unpinned window; it can still be dragged by its title bar,
//                                     and the script can still close, move or raise it. off unpins it
//   title NAME |text
//   swap NAME /boot/file.txt [frame K] [fps N] [loop|once]   new contents, same place and title. A PAINT file or a
//                                     picture film (in open too) shows as a colour picture from the window's top-left
//                                     corner, its frame chosen on the stage clock as for mark
//   say NAME |text [for S]            a speech bubble above the window for S seconds (default 3)
//   hint |text                        the status line's right-hand text
//   bg PRESET                         desktop preset: default jet_black dark_grey terminal cga_cyan
//                                     cga_green noise white_paper gallery_wall ega ega_solid
//   shake S                           every stage window jitters for S seconds, then settles; the jitter is a
//                                     hash of the seed, the window's name and the tick, so it repeats exactly
//                                     (a window moved, glided or opened again on its open NAME during a shake shakes
//                                     about its new place and stops there; one opened during it, or closed and
//                                     opened again, stays still)
//   seed N                            the seed of everything random (shake); default 0
//   floors off|on                     off: from here on a stage window keeps the size it was given when it is moved,
//                                     glided, reopened, resized, shaken or tiled (a ghost down to 1 x 1, a framed one
//                                     down to 2 x 2). on (the default): Turbo Vision's floor, 16 x 6, framed rows 3
//   weather rain|flash|off            rain falls across the whole desktop under the windows; flash
//                                     lights it for a moment (lightning); off clears the sky
//   tile | cascade | bars on|off
//   sound NAME                        play the page's audio clip NAME (scene manifest "sounds"); the engine also
//                                     sends ~open ~close ~say ~shake ~tile ~flash for the page's synthesised effects
//   colour NAME blue|cyan|grey|black|magenta|red|green|brown [INK 0-15|contrast]   a window's colour family and text ink;
//                                     contrast (a ghost window): each character black on a light background and white
//                                     on a dark one, cell by cell; it makes the window clear. A number sets a fixed ink again
//   theme ega|monochrome              the whole desktop's palette
//   mark NAME X Y /boot/file.txt [ink N] [in S]   line art in one ink (EGA 0-15, default 0 black) on the desktop itself, under every window; X may be
//                                     "left" or "right" and Y "top" or "bottom" (the real desktop edge; a colour picture
//                                     too); frames animate, a frame every 3 ticks from the tick it was placed; in S glides
//                                     A file whose first line is "PAINT #rrggbb #rrggbb ..." is a colour picture: each cell
//                                     of each row is three characters, ink index, paper index, glyph (indexes 0-9 a-z A-Z
//                                     into that list; ".." leaves the cell as it is). ink N is ignored for such a file.
//   mark NAME X Y /boot/F.txt [z N] [wrap x|y|xy] [frame K] [fps N] [loop|once]   layers (options on mark):
//                                     marks draw in z order (default 0; lower first, then by name); wrap x repeats the
//                                     picture across the desktop's width, wrap y down its height; frame K shows frame K of
//                                     a file of frames; fps N steps N frames a second on the stage clock from this command
//                                     (loops; once stops on the last). Without frame or fps a file of frames animates on
//                                     its own, as before. A picture film (first line "PFILM", docs/stage.md) follows the
//                                     clock at its own rate from frame K (default 0) and holds its last frame (loop
//                                     repeats it); frame K alone holds frame K. A command naming a file, frame or fps
//                                     starts the frame clock again; X Y alone moves the mark. In a PAINT or picture film
//                                     cell, paper "." keeps the paper of whatever is below (a glyph over another layer).
//   drift NAME VX VY                  the mark moves VX cells a second right and VY down from now, a whole cell at a time
//                                     (its place is worked out from the clock, so a seek lands on it); drift NAME 0 0 stops
//                                     it where it is; a mark command with X Y places it again and the drift goes on from there
//   mark NAME off
//   text NAME |line1/line2/...        the window shows these lines; place, size, title and colour stay
//   bar NAME FROM TO over S [width W] [|label]   the window shows "label ████░░░░  45%", filling from FROM to TO
//                                     percent over S seconds; W cells of bar (default: what fits the window)
//   counter NAME FROM TO over S |format   the window shows format with %d replaced by a number running
//                                     from FROM to TO over S seconds ("over 0" sets it at once); '/' splits lines
//   gate                              the clock waits here until the page calls wwdos_stage_go() (its first
//                                     click or key, which also turns sound on); passes at once when sound is
//                                     already on, the page has no sounds, or the URL has ?t= or ?autostart=1.
//                                     Primers keep animating while it waits. Seek and resize replays skip it.
//   gate cue                          a GO cue between sections: waits for a click or a key every time it is reached,
//                                     sound or no sound (a talk deck); passes at once on a tool's page (?paused=1, or
//                                     ?cues=0; natively --paused). Seek and resize replays skip it. A gate of either kind
//                                     passes when a jump is already queued (an input, a follow): the jump lands on its bar.
//   on EVENT [WHAT] ACTION [TARGET]   viewer input from now on: EVENT key, click, esc, any (a key or a click), drag,
//                                     close or submit; WHAT a key (key x, key Space, key Enter) or a window name
//                                     (click door); ACTION jump TARGET (a section name, or |a name with spaces, or
//                                     seconds; queued and landed under the quantise, at once at a gate), next (the next
//                                     section, queued), exit (out of the film: the page's Exit), go (open a gate), off
//                                     (forget the handler). A later on for the same EVENT and WHAT replaces it. A plain
//                                     "key" or "any" leaves out the viewer's keys (F1-F12, PgUp, PgDn, Tab, Esc, Q, M).
//                                     Handlers are state: a seek has those its script set up to there.
//   follow BARS TARGET [TARGET ...]   after BARS bars from this command, jump to TARGET (a section name or seconds);
//                                     with several, one chosen by the seed. Queued like a jump: a viewer's jump before
//                                     it lands replaces it. Armed only when playing passes the command (a seek drops it).
//   section |name                     marks where a part of the piece starts; PgDn and PgUp (View menu) jump between them
//   scrubber on|off                   a small timeline in the status line, left of the sound icon: filled to the
//                                     present, a mark at each section; a click jumps to the section clicked
//   scrubber wide                     a two-row timeline across the desktop's bottom two rows, in front of every
//                                     window: section names above, one cell per bar below (N bars a cell when the
//                                     bars do not fit). A click queues a jump to that bar or section (see Quantise)
//   tempo BPM BEATS                   bar 1 of the film starts at this command's time; BPM beats a minute, BEATS
//                                     a bar. The compiler writes it from bpm/beats at the "film" label (else the
//                                     first section). The engine uses the first one only.
//   loop [T]                          clock back to T seconds (default 0) and run on
//   end                               stop the clock
//   xray on|off|toggle|inspect        live only: the page's x-ray view (see X-ray below); a script line does nothing
//   reset                             live only: the stage's windows back where the script has them now (Window > Reset
//                                     windows, Alt+E); same tick, sound and clock untouched; a script line does nothing
//   restart                           live only: the film from the start, sound from the top (File > Restart, Alt+R)
// Quantise: a queued jump lands on the next bar line (BAR, the default), on the next beat at the same beat of the
// target bar (BEAT), or at once (OFF); Q cycles the mode. A second jump before landing replaces the first. PgDn and
// PgUp queue too. While paused or at a gate a jump lands at once. With no tempo command the engine assumes 120 bpm
// 4/4 from the first section. The page adds bars(), bar(), jump(BAR), quantise(MODE), sectionsList() to wwdosStage.
// Anchors: in open, move and rect, X may be "right" or "centre" and Y "bottom" or "centre": the window sits
// against that edge, or in the middle, of the real desktop (not the laid-out size). open k centre bottom 0 0 ...
// X-ray: xray on|off|toggle|inspect (live only) shows the page's x-ray view: a box and a label over every window,
// sprite, picture, mark and the gate, drawn by the page on top of the cells (web/shell-webgl.html). It is a view,
// never part of the film: it changes no cell the engine draws, no stage time and no log; in a script it does nothing.
// Live: runLive(LINE) runs one command now, without a time field (the page's wwdosStage.run). Windows it opens are
// stage windows: the next seek or replay closes them. loop, end, gate, tempo, section, clock and size are refused.
// Commands aimed at a window that is not open do nothing; the page trace (window.wwdosTrace) marks them warn.
// Log: every live line that runs is kept with the tick it ran on (time() then: the tick to run next while playing, the
// tick shown after a seek) and the script position (how many script commands had run), so a replay puts it back in
// order: before the script's commands of a later position, after those already run. A resize replay keeps the
// logged lines. Recording off (the default), a seek or a loop ends the log, as it always closed live windows.
// Recording on (wwdosStage.record(true)), the log outlives seeks and loops: seek(T) applies the lines logged up to T
// and playing on runs later ones when their tick comes, so the log is an overdub of the script and the picture is a
// function of the script, the stage tick and the log. Nothing is kept anywhere but the page's memory.
// Windows: a drawing fills the whole interior, its last column included (a one-frame drawing's text view once kept a
// scroll-bar column). A framed window's title is cut to width - 10 cells while it is the active (front) window, which
// shows the close and zoom icons, and to width - 4 (the corners and a space each side) while it is not.
//
// Time: the picture is a function of the script, the stage time and the seed. A drawing's frame is counted from
// the tick its window was opened or swapped (FPS frames a second of stage time; between ticks it runs on, and it
// keeps moving while the clock waits at a gate); rain, a mark's frames, a flash and shake are counted in ticks too.
// So the cells after seek(T) are the cells shown when playing reaches T (tools/stage_time_check.py). loop T puts
// the desktop in the state seek(T) gives, then plays on: windows left open at a loop do not carry into the next pass.
// Layers and picture windows follow the same rule: a mark's place (glide, drift) and frame, and a picture window's
// frame, are counted from the tick of the command that set them, so a seek, a resize replay, a scrubber jump and a
// loop show what playing shows. A mark whose file animates on its own (no frame or fps) steps every 3 ticks from the
// tick it was placed. At loop T the stage is replayed to T like a seek, so every clock restarts from its command.
// Clock: ticks follow a monotonic clock (late timer events run the missing ticks, at most half a second's worth;
// after a pause, a seek or a gap over 1 s the clock starts again from the present).
// Seek: seek(S) closes the stage's windows and runs the script quietly up to S, then holds or plays on from
// there. Length is the time of the first loop or end command, else of the last command. The page offers
// ?t=S, window.wwdosStage = {time(), seek(S), length()}, and keys F7 (back 2 s) and F5 (forward 2 s).
#ifndef WWDOS_STAGE_H
#define WWDOS_STAGE_H

#define Uses_TRect
#define Uses_TPoint
#include <tvision/tv.h>

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "picture.h"

class TDeskTop;
class TFrameAnimationWindow;

// What a mark (a layer on the desktop) shows at the present tick; the host draws it (StageHost::stageMark).
struct StageMark
{
    std::string path;                 // empty: remove the mark
    int x = 0, y = 0;                 // top-left cell on the desktop; with right / bottom, the offset from that edge (drift)
    bool right = false, bottom = false;   // line art against the desktop's real right / bottom edge
    int ink = 0;                      // line art's EGA ink
    int z = 0;                        // draw order: lower first, then by name
    bool wrapX = false, wrapY = false;
    int frame = -1;                   // frame to show; -1: the file animates on its own (line art, PAINT without fps)
    pic::Fx fx;                       // fade and tint of a colour picture
    bool operator==(const StageMark& o) const
    {
        return path == o.path && x == o.x && y == o.y && right == o.right && bottom == o.bottom && ink == o.ink && z == o.z
            && wrapX == o.wrapX && wrapY == o.wrapY && frame == o.frame && fx.fade == o.fx.fade
            && fx.fadeTo == o.fx.fadeTo && fx.tint == o.fx.tint;
    }
};

// Files named /boot/NAME. The page writes them into the wasm file system; the native build
// (wwdos-stage --boot DIR) reads and writes DIR/NAME instead (minimal_app.cpp).
#ifdef __EMSCRIPTEN__
inline const std::string& bootPath(const std::string& p) { return p; }
#else
std::string bootPath(const std::string& p);
#endif

struct StageHost
{
    virtual ~StageHost() {}
    virtual TDeskTop* stageDesk() = 0;
    virtual void stageTile() = 0;
    virtual void stageCascade() = 0;
    virtual void stageBars(bool show) = 0;
    virtual void stageHint(const std::string& text) = 0;
    virtual void stageBackground(const std::string& preset) = 0;
    virtual void stageWeather(const std::string& kind) = 0;
    virtual void stageEveryTick() = 0;
    // The tick now being shown; called before each command runs and at every advance (ticks, seeks, replays), so
    // the host's own moving parts (rain, a flash, a mark's frames) are counted in stage ticks.
    virtual void stageNow(int) {}
    // Redraw the stage's mirrors until they stop changing (a mirror copies the screen, itself included).
    // full: until nothing more can change (after a seek, on a pause); else one pass (each tick while playing).
    virtual void stageSettle(bool) {}
    virtual void stageSound(const std::string& name) = 0;
    virtual void stageTheme(const std::string& name) = 0;
    // m.path empty removes the mark. The host may draw at once or on stageFlush.
    virtual void stageMark(const std::string& name, const StageMark& m) = 0;
    // end of a seek, a replay or a live command: draw what changed (a tick ends with stageEveryTick instead)
    virtual void stageFlush() {}
    // every command the clock runs, with its script time; warn is set when it did nothing (no such window)
    virtual void stageTrace(double, const std::string&, const char*) {}
    virtual bool stageGatePass() { return true; }
    virtual bool stageCuePass() { return false; }    // a "gate cue" passes at once (a tool's page)
    // Seconds ahead of its script time a clip may be handed to the host, which then starts it at the exact time
    // ("NAME#SECONDS"). 0 = the host cannot schedule (the terminal build): clips start when their tick comes.
    virtual double stageSoundLead() { return 0; }
    virtual void stageScrubber(int) {}    // 0 off, 1 the small one in the status line, 2 wide (two desktop rows)
    // The host's audio clock as script time (-1: none). After a quantised jump the stage puts its next tick where the
    // audio says it belongs and asks the host to fire the stage timer in secs (stageRetime), so pictures and the
    // re-placed clips stay together.
    virtual double stageAudioTime() { return -1; }
    virtual void stageRetime(double) {}
    // Where the host drew the mark NAME at the tick on screen, in desktop cells, and the frame it showed (x-ray map).
    virtual bool stageMarkRect(const std::string&, int&, int&, int&, int&, int&, int&) { return false; }
    // "xray on|off|toggle|inspect" from a live line: the page's x-ray view (never part of the film).
    virtual void stageXray(const std::string&) {}
    virtual void stageRestart() {}         // the live line "restart": the host restarts the film (Stage::restart and play)
};

class Stage
{
public:
    explicit Stage(StageHost& host) : host(host) {}

    bool load(const char* path);          // false when the file is missing or has no commands
    bool loaded() const { return !cmds.empty(); }
    int periodMs() const { return 1000 / clockHz; }
    int hz() const { return clockHz; }
    void tick();                          // one clock tick: run due commands, advance tweens
    int due();                            // ticks owed by the monotonic clock since the last call
    void rebase() { wallLast = -1; }      // after a pause or a seek: the clock starts again from now
    void seek(double secs, bool keepClock = false);   // the state at secs, drawn; quiet (no sounds)
    // Tempo and quantised jumps (stage command "tempo"; scrubber wide, PgDn/PgUp, wwdosStage.jump)
    enum Quant { QOFF = 0, QBEAT = 1, QBAR = 2 };
    bool hasTempo() const { return tempoSet; }
    double barLen() const { return beatsPerBar * 60.0 / bpm; }
    double beatLen() const { return 60.0 / bpm; }
    double barZero() const { return tempoT0; }                // script time of bar 1
    int bars() const;                                          // whole bars from bar 1 to the end
    double barAt(double t) const { return 1 + (t - tempoT0) / barLen(); }   // 1-based, fractional
    double barStart(double bar) const { return tempoT0 + (bar - 1) * barLen(); }
    int quant = QBAR;
    void queueJump(double target, bool now);   // target: a script time; now: land at once
    bool jumpQueued() const { return jq; }
    void cancelJump() { jq = false; }
    double jumpShown() const { return jShow; } // the time asked for (before a BEAT offset)
    double jumpLand() const { return jLand; }
    int seeks = 0;                             // counts seeks, so the host drops ticks owed from before one
    double lastSeekMs = 0;                     // wall time of the last seek's replay (script length makes it grow)
    double sectionTimeFrom(double t, int dir) const;
    const std::vector<std::string>& sectionNames() const { return secNames; }
    double time() const { return double(now) / clockHz; }
    // The tick on screen, in seconds. While playing, time() is already one tick ahead of it (the tick to run next);
    // after a seek the two are equal.
    double shownTime() const { return double(shown) / clockHz; }
    double animTime() const;              // the clock for drawings (ww_anim_clock)
    void settle();                        // redraw every stage window at the clock's frame, then the mirrors
    void spriteSettle();                  // draw every clear sprite again, back to front, over what is under it now
    void paused(bool on);                 // the host paused or resumed: drawings hold at the shown tick
    void resized() { relayout(); }        // lay the stage out again if the desktop changed size
    double length() const;
    double sectionTime(int dir) const;
    const std::vector<double>& sectionTimes() const { return sections; }
    double filmStart() const { return sections.empty() ? 0 : sections.front(); }   // the first section: after a title    // start of the next (dir > 0) or previous section, or -1 when there is none
    bool waiting() const { return gated; } // stopped at a gate
    // One command now (see Live above). checkLive returns "" or why the line cannot run; pending holds names that
    // queued open commands will create. runLive applies it and returns "" or why it did nothing.
    std::string checkLive(const std::string& line, const std::set<std::string>& pending) const;
    std::string runLive(const std::string& line);
    // Reset windows: every stage window back where the script (and the open log) has it at the tick on screen, after a
    // viewer's drag, Cascade, Tile or close. No seek: the tick, the clock, queued jumps, gates and sound are untouched,
    // and nothing is retriggered (the replay is quiet). Not logged.
    void reset();
    // Restart: the film again from 0, sound from the top (seek(0)); queued jumps dropped; the log and the inputs are
    // cleared unless recording. Not logged. The host also lets a paused page play.
    void restart();
    // The open log (see Log above): every live line that ran, with the tick it ran on and the script position.
    struct LogEntry { int tick; size_t pos; std::string line; bool done; };
    bool recording() const { return rec; }
    void record(bool on) { rec = on; }
    const std::vector<LogEntry>& log() const { return liveLog; }
    void clearLog() { liveLog.clear(); }
    // Replace the log (a page restoring a session): entries are "TICK POS LINE" lines; the desktop is rebuilt at the
    // present tick from the script and the log, as a resize replay does. Returns the number of entries read.
    int loadLog(const std::string& text);
    std::string logText() const;          // the log as "TICK POS LINE" lines, oldest position first
    void go() { if (gated) { gated = false; rebase(); } }
    // Viewer input (the page's events out, docs/stage.md § Events): the front stage window under desktop cell p ("" for
    // none; speech bubbles count as their own windows), a window's rect, and the names of stage windows the viewer
    // closed (Alt+F3, the close icon) since the last call.
    std::string nameAt(TPoint p);
    bool rectOf(const std::string& name, TRect& r);
    std::vector<std::string> takeClosed()
    {
        std::vector<std::string> names, v;
        for (auto& kv : wins) names.push_back(kv.first);
        for (auto& n : names) win(n);            // a window closed by hand is found (and forgotten) by win()
        v.swap(handClosed);
        return v;
    }
    // The name of the front (active) stage window, "" when the front window is not a stage window.
    std::string frontName();
    // Viewer input (on, follow, stage.h above). Called by the host for each event out; acts on the first handler that
    // matches (a jump is queued here and lands in tick()). Returns what the host must do: "", "exit" or "go".
    std::string input(const std::string& kind, const std::string& name, const std::string& value);
    // Inputs while recording, with the session tick (ticks run since the page started, through seeks and jumps):
    // feeding the same inputs at the same session ticks replays a session exactly.
    struct Input { long stick; std::string kind, name, value; };
    const std::vector<Input>& inputs() const { return inputLog; }
    long sessionTick() const { return stick; }
    // The stage's windows, front first, as JSON: [{"name","x","y","w","h","title","ghost","file"}] (speech bubbles
    // are named _sayN). Cells are desktop cells: the menu bar row is not counted. The page's wwdosStage.windows().
    std::string windowsJson();
    // Every object on the stage for the x-ray view, as JSON: the desktop's place on the screen, whether the clock waits
    // at a gate, the windows front first (kind, content, rect, frame) and the marks in draw order (stage.cpp).
    std::string objectsJson();

private:
    struct Cmd { double t; std::string line; };
    struct Tween { std::string name; TRect from, to; int start, dur; bool ease; };
    struct Bubble { std::string name; int closeAt; };
    std::vector<double> sections;
    std::vector<std::string> secNames;
    bool tempoSet = false; double tempoT0 = 0, bpm = 120; int beatsPerBar = 4;
    bool jq = false, jSent = false, jCut = false; double jTarget = 0, jLand = 0, jShow = 0;
    std::vector<char> soundSent;          // clips already handed to the host ahead of time
    double curT = 0;                      // script time of the command being run         // times of the script's "section" commands
    struct Mark {
        std::string path; int x = 0, y = 0, fx = 0, fy = 0, tx = 0, ty = 0, start = 0, dur = 0; bool right = false, bottom = false; int ink = 0;
        int z = 0; bool wrapX = false, wrapY = false;
        int frame0 = -1; double fps = -1; int loopMode = 0; int fstart = 0;   // frames (PicClock rules); loopMode 1 loop, 2 once
        double vx = 0, vy = 0; int dstart = 0, dx0 = 0, dy0 = 0;              // drift: offset dx0 + floor(vx * secs)
        StageMark sent; bool hasSent = false;
    };
    struct PicClock { int frame0 = -1; double fps = -1; int loopMode = 0; int start = 0; int shown = -2; };
    std::map<std::string, PicClock> winPics;   // windows showing a colour picture
    int frameAt(const std::string& path, int frame0, double fps, int loopMode, int start) const;
    void markOpts(std::istringstream& h, int& frame0, double& fps, int& loopMode, std::string& path, bool& any);
    StageMark markView(const Mark& m) const;
    void sendMark(const std::string& name, Mark& m, bool force = false);
    void driftOffset(const Mark& m, int& dx, int& dy) const;
    void picWindows();
    struct Widget { bool bar; double from, to; int start, dur, width; std::string text; int shown; };
    std::map<std::string, Widget> widgets;   // bar and counter values running over time
    double wallLast = -1, wallAcc = 0;
    std::map<std::string, Mark> marks;
    bool quiet = false;                   // replaying after a resize: no sounds
    const char* warn = nullptr;           // why the last command did nothing, for the trace
    bool gated = false;
    void sfx(const char* name) { if (!quiet) host.stageSound(name); }

    StageHost& host;
    int clockHz = 8;
    TPoint laidOut {0, 0};
    TPoint offset {0, 0};
    TPoint deskSize {0, 0};               // the desktop size the layout was computed for
    double scaleX = 1, scaleY = 1;
    std::vector<Cmd> cmds;
    size_t next = 0;
    int now = 0;                          // ticks since loop start
    int shown = 0;                        // the tick on screen
    unsigned seed = 0;
    bool inRun = false;                   // a command is running (or a replay): drawings made now start at tick now
    bool held = false;                    // the host is paused
    double tickWall = -1, gateWall = -1;  // wall time of the last tick, and of the gate the clock waits at
    int tickOf(double t) const;           // the tick a command at t runs on (the nearest)
    bool running = true;
    std::map<std::string, TFrameAnimationWindow*> wins;
    std::vector<Tween> tweens;
    std::vector<Bubble> bubbles;
    std::map<std::string, TRect> shakeHome;
    int shakeUntil = -1;
    int bubbleSerial = 0;

    void run(const std::string& line);
    std::vector<std::string> handClosed;
    struct Handler { std::string event, what, action, target; };
    std::vector<Handler> handlers;        // on EVENT ...: state set by the script (replay clears it)
    std::vector<Input> inputLog;
    long stick = 0;                       // session tick: every tick run, through seeks and jumps
    unsigned follows = 0;                 // follow commands fired, for the seeded choice
    double targetTime(const std::string& word) const;   // a section name or seconds; -1 when neither  // stage windows the viewer closed, for the host's close events
    std::vector<LogEntry> liveLog;        // ordered by (tick, pos), then by arrival
    bool rec = false;                     // recording: the log outlives seeks and loops (an overdub of the script)
    void logAdd(const std::string& line);
    void runLogDue();                     // in tick(): logged lines whose tick and position have come
    TFrameAnimationWindow* win(const std::string& name);
    TRect place(int x, int y, int w, int h) const;
    TRect fit(const std::string& path, bool ghost, bool clear = false) const;
    void openWin(const std::string& name, TRect r, bool ghost, const std::string& path, const std::string& title,
                 bool clear = false, bool contrast = false);
    void closeWin(const std::string& name);
    void setTitle(TFrameAnimationWindow* w, const std::string& title);
    void swapWin(const std::string& name, const std::string& path);
    void bubble(const std::string& name, const std::string& text, double secs);
    void restoreShake();
    void put(const std::string& name, TFrameAnimationWindow* w, TRect r);   // locate; during a shake also its home
    void rehome();                        // after tile/cascade during a shake
    void layout(int W, int H);            // offset and scale for a desktop of W x H
    void relayout();                      // desktop resized: run the script again under the new layout
    void replay(int target, size_t upto); // close the stage's windows, run cmds[0..upto) quietly, clock at target
    void anchor(TRect& r, const std::string& xs, const std::string& ys);
    void setText(const std::string& name, const std::string& text);
    void drawWidget(const std::string& name, Widget& w);
    void advance();                       // glides, shake and bubbles at the present tick
    void pinFront();                      // pinned windows in front of the unpinned ones (stage.h, pin)
public:
    void keepPinned() { pinFront(); }     // the host after a viewer brought another window forward
private:
    TRect placeSized(int x, int y, int w, int h) const;
};

#endif
