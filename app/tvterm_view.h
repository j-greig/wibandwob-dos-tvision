#ifndef TVTERM_VIEW_H
#define TVTERM_VIEW_H

#define Uses_TWindow
#define Uses_TRect
#define Uses_TPoint
#include <tvision/tv.h>

#include <tvterm/termwnd.h>
#include <tvterm/consts.h>

#include <string>

class TWibWobTerminalWindow : public tvterm::BasicTerminalWindow
{
public:
    static const tvterm::TVTermConstants termConsts;

    TWibWobTerminalWindow(const TRect &bounds, tvterm::TerminalController &termCtrl) noexcept;

    void handleEvent(TEvent &ev) override;
    void sizeLimits(TPoint &min, TPoint &max) override;

    // Send text as keyboard input to this terminal.
    // Each byte is injected as a KeyDown event through the terminal controller.
    void sendText(const std::string &text);

    // Return the visible text content of this terminal as a UTF-8 string.
    // Each row is newline-terminated; trailing whitespace per row is stripped.
    std::string getOutputText() const;

    // A title that stays put: the program's own title and "(Disconnected)" are not shown.
    void setFixedTitle(const std::string &t);
    const char *getTitle(short maxSize) override;

    // Exec terminals: tools/term_exec.sh writes its pid here. Closing the window hangs
    // up that process group (tvterm does not always do it), so no program outlives
    // its window.
    void setChildPidFile(const std::string &path) { childPidFile_ = path; }
    // Make the next draw copy every cell (after the window's buffer was dropped).
    void markDirty();
    void shutDown() override;

private:
    using Super = tvterm::BasicTerminalWindow;
    std::string fixedTitle_;
    std::string childPidFile_;
};

TWindow* createTerminalWindow(const TRect &bounds);

// A terminal window whose program is `exec` (run by tools/term_exec.sh with no prompt
// and no echo) instead of the user's shell. hold=true keeps the last screen after the
// program exits, until the window is closed. Returns nullptr when tools/term_exec.sh is
// missing (the app must run from the repo root) or the pty fails.
TWindow* createTerminalWindowExec(const TRect &bounds, const std::string &exec, bool hold);

#endif // TVTERM_VIEW_H
