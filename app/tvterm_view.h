#ifndef TVTERM_VIEW_H
#define TVTERM_VIEW_H

#include "describe.h"

// Commands and help contexts of the terminal transport (docs/terminal-transport.md).
namespace termtransport {
const unsigned short cmTermToggle = 9990;   // Window > Pause/Play Terminal, Alt-P
const unsigned short cmTermGrab = 9991;     // Window > Grab Keyboard
const unsigned short cmTermRelease = 9992;  // Alt-End while a terminal holds the keyboard
const unsigned short hcTerminal = 9993;     // status line while a terminal window is focused
const unsigned short hcTerminalGrab = 9994; // status line while a terminal holds the keyboard
}

#ifdef __EMSCRIPTEN__
// Browser build: there is no PTY, so the terminal window is a placeholder
// with the same interface. Implementation: wasm/tvterm_stub.cpp.
#define Uses_TWindow
#define Uses_TRect
#define Uses_TPoint
#include <tvision/tv.h>
#include <string>

namespace tvterm { struct TVTermConstants { ushort cmCheckTerminalUpdates; }; }

class TWibWobTerminalWindow : public TWindow, public ww::Describable
{
public:
    std::string describeState() const override { return "terminal (not available in this build)"; }
    static const tvterm::TVTermConstants termConsts;
    TWibWobTerminalWindow(const TRect &bounds) noexcept;
    void sendText(const std::string &) {}
    std::string getOutputText() const { return std::string(); }
    void setFixedTitle(const std::string &t);
    void setChildPidFile(const std::string &) {}
    void markDirty() {}
    // Transport (docs/terminal-transport.md): nothing runs in the browser build.
    enum class RunState { Running, Paused, Exited };
    RunState runState() { return RunState::Exited; }
    bool pause(std::string &err) { err = "no program in this build"; return false; }
    bool resume(std::string &err) { err = "no program in this build"; return false; }
    bool isPaused() const { return false; }
    double secondsPaused() const { return 0; }
    std::string programName() { return std::string(); }
    std::string keyboard() const { return "none"; }
    bool releaseKeyboard() { return false; }
    bool startRecording(const std::string &, std::string &err) { err = "no program in this build"; return false; }
    long stopRecording(std::string &err) { err = "not recording"; return -1; }
    std::string recordingPath() const { return std::string(); }
    double recordingSeconds() const { return 0; }
private:
    std::string fixedTitle_;
};

TWindow* createTerminalWindow(const TRect &bounds);
TWindow* createTerminalWindowExec(const TRect &bounds, const std::string &exec, bool hold);

#else // !__EMSCRIPTEN__

#define Uses_TWindow
#define Uses_TRect
#define Uses_TPoint
#include <tvision/tv.h>

#include <tvterm/termwnd.h>
#include <tvterm/consts.h>

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <sys/types.h>

namespace ww { class CastRecorder; }

class TWibWobTerminalWindow : public tvterm::BasicTerminalWindow, public ww::Describable
{
public:
    std::string describeState() const override;
    bool describeText(std::vector<std::string>& out) const override;
    static const tvterm::TVTermConstants termConsts;

    TWibWobTerminalWindow(const TRect &bounds, tvterm::TerminalController &termCtrl) noexcept;
    // A BasicTerminalFrame with the transport mark (pause/play) beside the zoom icon.
    static TFrame *initTermFrame(TRect r);

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

    // ---- transport (docs/terminal-transport.md)
    // Pause stops every process group on the terminal's session (the shell or the exec
    // program, the job it runs and anything that job started, such as an audio player)
    // with SIGSTOP; resume continues them with SIGCONT, the foreground job before the shell.
    enum class RunState { Running, Paused, Exited };
    RunState runState();
    bool pause(std::string &err);
    bool resume(std::string &err);
    bool isPaused() const { return paused_; }
    double secondsPaused() const;      // this pause so far; 0 while running
    std::string programName();         // the foreground program ("" when unknown)
    // The pty's first process (the shell, or tools/term_exec.sh for exec terminals).
    void setLeaderPid(pid_t p) { leader_ = p; }
    pid_t leaderPid() const { return leader_; }
    // "grab" while the window holds the keyboard (Window > Grab Keyboard), "focus" while
    // it is the focused window, else "none".
    std::string keyboard() const;
    // Ends a grab; false when the window did not hold the keyboard.
    bool releaseKeyboard();

    // ---- casts (asciinema v2): the program's output as it arrives, from tvterm's reader
    void setRecorder(std::shared_ptr<ww::CastRecorder> r) { recorder_ = std::move(r); }
    bool startRecording(const std::string &path, std::string &err);
    long stopRecording(std::string &err);   // output events written, -1 with err
    std::string recordingPath() const;
    double recordingSeconds() const;

private:
    using Super = tvterm::BasicTerminalWindow;
    std::string fixedTitle_;
    std::string childPidFile_;
    std::string titleBuf_;
    pid_t leader_ {0};
    bool paused_ {false};
    std::vector<pid_t> stopped_;
    std::shared_ptr<ww::CastRecorder> recorder_;
    std::chrono::steady_clock::time_point pausedAt_;
    void transportChanged();
};


TWindow* createTerminalWindow(const TRect &bounds);

// A terminal window whose program is `exec` (run by tools/term_exec.sh with no prompt
// and no echo) instead of the user's shell. hold=true keeps the last screen after the
// program exits, until the window is closed. Returns nullptr when tools/term_exec.sh is
// missing (the app must run from the repo root) or the pty fails.
TWindow* createTerminalWindowExec(const TRect &bounds, const std::string &exec, bool hold);

#endif // __EMSCRIPTEN__

#endif // TVTERM_VIEW_H
