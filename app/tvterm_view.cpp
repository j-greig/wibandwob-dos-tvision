#ifndef __EMSCRIPTEN__
#define Uses_TEvent
#define Uses_TRect
#define Uses_TPoint
#define Uses_TWindow
#define Uses_TKeys
#define Uses_MsgBox
#define Uses_TScreenCell
#define Uses_TFrame
#include <tvision/tv.h>

#include "tvterm_view.h"
#include <tvterm/termctrl.h>
#include <tvterm/termview.h>
#include <tvterm/vtermemu.h>
#include <tvterm/termemu.h>
#include <tvterm/termframe.h>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/proc.h>
#endif
#include "terminal_transport.h"
#include "cast_record.h"

// Command IDs for tvterm integration — must not collide with existing app commands.
// These are wired through TVTermConstants so tvterm-core uses our IDs. Every field is
// named: the struct has cmCopySelection between cmReleaseInput and hcInputGrabbed, and a
// positional list that skips it leaves hcInputGrabbed at 0 (hcNoContext), which made every
// terminal window's title read "(Input Grab)" although none held the keyboard.
enum : ushort
{
    cmTvTermCheckUpdates  = 500,
    cmTvTermUpdated       = 501,
    cmTvTermCopySelection = 505,
};

const tvterm::TVTermConstants TWibWobTerminalWindow::termConsts = []
{
    tvterm::TVTermConstants k {};
    k.cmCheckTerminalUpdates = cmTvTermCheckUpdates;
    k.cmTerminalUpdated = cmTvTermUpdated;
    k.cmGrabInput = termtransport::cmTermGrab;        // Window > Grab Keyboard
    k.cmReleaseInput = termtransport::cmTermRelease;  // Alt-End, a click outside, terminal_release
    k.cmCopySelection = cmTvTermCopySelection;
    k.hcInputGrabbed = termtransport::hcTerminalGrab; // the status line while grabbed
    return k;
}();

TWibWobTerminalWindow::TWibWobTerminalWindow(
    const TRect &bounds,
    tvterm::TerminalController &termCtrl
) noexcept :
    TWindowInit(&TWibWobTerminalWindow::initTermFrame),
    Super(bounds, termCtrl, termConsts)
{
    options |= ofTileable;
    growMode = gfGrowAll | gfGrowRel;
    helpCtx = termtransport::hcTerminal;   // the status line names Alt-P while a terminal is focused
}

void TWibWobTerminalWindow::handleEvent(TEvent &ev)
{
    // Alt-P reaches the window as a key when a resident addresses it (resident_key id=),
    // and as cmTermToggle from the status line or the Window menu when a person presses it.
    if ((ev.what == evKeyDown && ev.keyDown.keyCode == kbAltP) ||
        (ev.what == evCommand && ev.message.command == termtransport::cmTermToggle))
    {
        if (!isDisconnected())
        {
            termtransport::act(this, "toggle", resident::currentWho());
            clearEvent(ev);
            return;
        }
    }
    if (ev.what == evKeyDown && isDisconnected() &&
        !(state & (sfDragging | sfModal)))
    {
        close();
        return;
    }
    Super::handleEvent(ev);
}

void TWibWobTerminalWindow::sizeLimits(TPoint &min, TPoint &max)
{
    Super::sizeLimits(min, max);
    if (owner)
    {
        max = owner->size;
        max.x += 2;
        max.y += 1;
    }
}

void TWibWobTerminalWindow::sendText(const std::string &text)
{
    // Find the TerminalView child to access its controller.
    tvterm::TerminalView *termView = nullptr;
    TView *start = first();
    if (start) {
        TView *v = start;
        do {
            if (auto *tv = dynamic_cast<tvterm::TerminalView*>(v)) {
                termView = tv;
                break;
            }
            v = v->next;
        } while (v != start);
    }
    if (!termView) return;

    // Send each byte as a KeyDown event through the terminal controller.
    for (unsigned char ch : text) {
        tvterm::TerminalEvent termEvent;
        termEvent.type = tvterm::TerminalEventType::KeyDown;
        memset(&termEvent.keyDown, 0, sizeof(termEvent.keyDown));

        if (ch == '\n' || ch == '\r') {
            // Enter key: send \r with proper kbEnter scancode (0x1c0d)
            termEvent.keyDown.charScan.charCode = '\r';
            termEvent.keyDown.keyCode = 0x1c0d; // kbEnter
            termEvent.keyDown.text[0] = '\r';
        } else {
            termEvent.keyDown.charScan.charCode = ch;
            termEvent.keyDown.keyCode = ch;
            termEvent.keyDown.text[0] = static_cast<char>(ch);
        }
        termEvent.keyDown.textLength = 1;
        termView->termCtrl.sendEvent(termEvent);
    }
}

std::string TWibWobTerminalWindow::getOutputText() const
{
    // Find the TerminalView child to access its controller.
    tvterm::TerminalView *termView = nullptr;
    TView *start = const_cast<TWibWobTerminalWindow*>(this)->first();
    if (start) {
        TView *v = start;
        do {
            if (auto *tv = dynamic_cast<tvterm::TerminalView*>(v)) {
                termView = tv;
                break;
            }
            v = v->next;
        } while (v != start);
    }
    if (!termView) return "";

    std::string result;
    termView->termCtrl.lockState([&](tvterm::TerminalState &state) {
        int rows = state.surface.size.y;
        int cols  = state.surface.size.x;
        for (int y = 0; y < rows; ++y) {
            std::string row;
            row.reserve(cols);
            for (int x = 0; x < cols; ++x) {
                const TScreenCell &cell = state.surface.at(y, x);
                const TScreenCharacter   &ch   = cell.character;
                if (!ch.isWideCharTrail()) {
                    TStringView sv = ch.getText();
                    row.append(sv.data(), sv.size());
                }
            }
            // Strip null bytes that may appear in empty cells
            row.erase(std::remove(row.begin(), row.end(), '\0'), row.end());
            // Strip trailing whitespace per row
            size_t last = row.find_last_not_of(" \t");
            if (last != std::string::npos)
                row.erase(last + 1);
            else
                row.clear();
            result += row;
            result += '\n';
        }
    });
    return result;
}

static void onTermError(const char *reason)
{
    messageBox(mfError | mfOKButton, "Cannot create terminal: %s.", reason);
}

static void onTermErrorQuiet(const char *reason)
{
    fprintf(stderr, "[tvterm] cannot create terminal: %s\n", reason);
}

void TWibWobTerminalWindow::shutDown()
{
    if (recorder_) recorder_->stop();
    if (paused_)
    {
        // A stopped process holds SIGHUP until it runs again: hang up, then continue, so
        // closing the window still ends a paused program (and tvterm's wait does not hang).
        for (pid_t g : stopped_) kill(-g, SIGHUP);
        for (auto it = stopped_.rbegin(); it != stopped_.rend(); ++it) kill(-*it, SIGCONT);
        stopped_.clear();
        paused_ = false;
    }
    if (!childPidFile_.empty())
        unlink((childPidFile_ + ".exit").c_str());
    if (!childPidFile_.empty())
    {
        pid_t pid = 0;
        if (FILE *f = fopen(childPidFile_.c_str(), "r"))
        {
            long v = 0;
            if (fscanf(f, "%ld", &v) == 1) pid = (pid_t) v;
            fclose(f);
        }
        unlink(childPidFile_.c_str());
        childPidFile_.clear();
        if (pid > 1)
        {
            // The program is a session leader (setsid in the pty child): its group is pid.
            kill(-pid, SIGHUP);
            kill(pid, SIGHUP);
            for (int i = 0; i < 30; ++i)
            {
                if (waitpid(pid, nullptr, WNOHANG) == pid || kill(pid, 0) != 0)
                {
                    pid = 0;
                    break;
                }
                usleep(10000);
            }
            if (pid > 1)
            {
                kill(-pid, SIGKILL);
                kill(pid, SIGKILL);
                waitpid(pid, nullptr, WNOHANG);
            }
        }
    }
    Super::shutDown();
}

void TWibWobTerminalWindow::markDirty()
{
    // TerminalView repaints every cell after an exposure change.
    forEach([](TView *v, void *) {
        if (dynamic_cast<tvterm::TerminalView*>(v) && (v->state & sfExposed))
        {
            v->setState(sfExposed, False);
            v->setState(sfExposed, True);
        }
    }, nullptr);
}

void TWibWobTerminalWindow::setFixedTitle(const std::string &t)
{
    fixedTitle_ = t;
    if (frame) frame->drawView();
}

const char *TWibWobTerminalWindow::getTitle(short maxSize)
{
    std::string t;
    if (!fixedTitle_.empty())
    {
        t = fixedTitle_;
        if (state & sfModal) t += " (Input Grab)";
    }
    else if (const char *s = Super::getTitle(maxSize))
        t = s;
    if (paused_) t += " (Paused)";
    if (recorder_ && recorder_->active()) t += " (Rec)";
    titleBuf_ = t;
    return titleBuf_.empty() ? nullptr : titleBuf_.c_str();
}

// ---------------------------------------------------------------- transport

namespace {

struct ProcInfo
{
    pid_t pid {0}, pgid {0}, tpgid {0};
    long long tdev {-1};
    std::string name;
};

bool procInfo(pid_t pid, ProcInfo &out)
{
    if (pid <= 0) return false;
#ifdef __APPLE__
    struct proc_bsdinfo bi;
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bi, sizeof bi) != (int) sizeof bi)
        return false;
    if (bi.pbi_status == SZOMB) return false;
    out.pid = pid;
    out.pgid = (pid_t) bi.pbi_pgid;
    out.tpgid = (pid_t) bi.e_tpgid;
    out.tdev = (bi.e_tdev == (uint32_t) NODEV) ? -1 : (long long) bi.e_tdev;
    out.name = bi.pbi_name[0] ? bi.pbi_name : bi.pbi_comm;
    return true;
#else
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/stat", (int) pid);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char buf[1024] = {0};
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    char *lp = strchr(buf, '('), *rp = strrchr(buf, ')');
    if (!lp || !rp) return false;
    out.pid = pid;
    out.name.assign(lp + 1, rp - lp - 1);
    char st = 0;
    int ppid = 0, pgrp = 0, sess = 0, tty = 0, tpgid = 0;
    if (sscanf(rp + 2, "%c %d %d %d %d %d", &st, &ppid, &pgrp, &sess, &tty, &tpgid) != 6) return false;
    if (st == 'Z') return false;
    out.pgid = pgrp;
    out.tpgid = tpgid;
    out.tdev = tty ? tty : -1;
    return true;
#endif
}

std::vector<pid_t> allPids()
{
    std::vector<pid_t> v;
#ifdef __APPLE__
    int n = proc_listallpids(nullptr, 0);
    if (n <= 0) return v;
    v.resize(n + 64);
    n = proc_listallpids(v.data(), (int) (v.size() * sizeof(pid_t)));
    v.resize(n > 0 ? n : 0);
#else
    if (DIR *d = opendir("/proc"))
    {
        while (dirent *e = readdir(d))
            if (e->d_name[0] >= '0' && e->d_name[0] <= '9') v.push_back((pid_t) atoi(e->d_name));
        closedir(d);
    }
#endif
    return v;
}

// Every process group on the session's terminal, the leader's own group first.
std::vector<pid_t> sessionGroups(pid_t leader)
{
    std::vector<pid_t> groups;
    ProcInfo li;
    if (!procInfo(leader, li)) return groups;
    pid_t mine = getpgrp();
    auto add = [&](pid_t g) {
        if (g > 1 && g != mine && std::find(groups.begin(), groups.end(), g) == groups.end())
            groups.push_back(g);
    };
    add(li.pgid);
    if (li.tdev >= 0)
        for (pid_t p : allPids())
        {
            ProcInfo pi;
            if (p != leader && procInfo(p, pi) && pi.tdev == li.tdev) add(pi.pgid);
        }
    return groups;
}

} // namespace

std::vector<pid_t> termChildPids()
{
    std::vector<pid_t> v;
#ifdef __APPLE__
    v.resize(1024);
    int n = proc_listchildpids(getpid(), v.data(), (int) (v.size() * sizeof(pid_t)));
    v.resize(n > 0 ? n : 0);
#else
    pid_t me = getpid();
    for (pid_t p : allPids())
    {
        char path[64];
        snprintf(path, sizeof path, "/proc/%d/stat", (int) p);
        if (FILE *f = fopen(path, "r"))
        {
            char buf[512] = {0};
            size_t n = fread(buf, 1, sizeof buf - 1, f);
            fclose(f);
            buf[n] = 0;
            char *rp = strrchr(buf, ')');
            char st;
            int ppid = 0;
            if (rp && sscanf(rp + 2, "%c %d", &st, &ppid) == 2 && ppid == me) v.push_back(p);
        }
    }
#endif
    return v;
}

TWibWobTerminalWindow::RunState TWibWobTerminalWindow::runState()
{
    if (isDisconnected()) return RunState::Exited;
    if (!childPidFile_.empty() && access((childPidFile_ + ".exit").c_str(), F_OK) == 0)
        return RunState::Exited;   // hold=1: the program ended, the window keeps its last screen
    if (leader_ > 0 && kill(leader_, 0) != 0 && errno == ESRCH) return RunState::Exited;
    return paused_ ? RunState::Paused : RunState::Running;
}

bool TWibWobTerminalWindow::pause(std::string &err)
{
    if (paused_) return true;
    if (runState() == RunState::Exited) { err = "the program has exited"; return false; }
    if (leader_ <= 0) { err = "no process is known for this terminal"; return false; }
    std::vector<pid_t> groups = sessionGroups(leader_);
    if (groups.empty()) { err = "no process to pause"; return false; }
    // The leader (the shell) first, so it cannot see its job stop and take the terminal back.
    for (pid_t g : groups) kill(-g, SIGSTOP);
    stopped_ = groups;
    paused_ = true;
    if (recorder_) recorder_->hold(true);
    pausedAt_ = std::chrono::steady_clock::now();
    transportChanged();
    return true;
}

bool TWibWobTerminalWindow::resume(std::string &err)
{
    if (!paused_) return true;
    // The job before the shell, the reverse of the stop order.
    for (auto it = stopped_.rbegin(); it != stopped_.rend(); ++it) kill(-*it, SIGCONT);
    stopped_.clear();
    paused_ = false;
    if (recorder_) recorder_->hold(false);
    transportChanged();
    (void) err;
    return true;
}

double TWibWobTerminalWindow::secondsPaused() const
{
    if (!paused_) return 0;
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - pausedAt_).count();
}

std::string TWibWobTerminalWindow::programName()
{
    ProcInfo li;
    if (!procInfo(leader_, li)) return "";
    pid_t fg = li.tpgid > 0 ? li.tpgid : li.pgid;
    ProcInfo fi;
    if (procInfo(fg, fi)) return fi.name;
    for (pid_t p : allPids())
    {
        ProcInfo pi;
        if (procInfo(p, pi) && pi.pgid == fg) return pi.name;
    }
    return li.name;
}

std::string TWibWobTerminalWindow::keyboard() const
{
    if (state & sfModal) return "grab";
    if (state & sfFocused) return "focus";
    return "none";
}

bool TWibWobTerminalWindow::releaseKeyboard()
{
    if (!(state & sfModal)) return false;
    endModal(cmCancel);
    return true;
}

bool TWibWobTerminalWindow::startRecording(const std::string &path, std::string &err)
{
    if (!recorder_) { err = "this terminal cannot record"; return false; }
    std::vector<std::string> screen;
    std::string text = getOutputText(), line;
    for (char ch : text)
    {
        if (ch == '\n') { screen.push_back(line); line.clear(); }
        else line += ch;
    }
    while (!screen.empty() && screen.back().empty()) screen.pop_back();
    const char *t = getTitle(0);
    if (!recorder_->start(path, std::max(1, size.x - 2), std::max(1, size.y - 2), t ? t : "", screen, err))
        return false;
    if (paused_) recorder_->hold(true);
    transportChanged();
    return true;
}

long TWibWobTerminalWindow::stopRecording(std::string &err)
{
    long n = recorder_ ? recorder_->stop() : -1;
    if (n < 0) err = "not recording";
    transportChanged();
    return n;
}

std::string TWibWobTerminalWindow::recordingPath() const
{
    return recorder_ ? recorder_->path() : std::string();
}

double TWibWobTerminalWindow::recordingSeconds() const
{
    return recorder_ ? recorder_->seconds() : 0;
}

void TWibWobTerminalWindow::transportChanged()
{
    if (frame) frame->drawView();
}

// ---------------------------------------------------------------- the frame's mark

namespace {

class TTermTransportFrame : public tvterm::BasicTerminalFrame
{
public:
    using BasicTerminalFrame::BasicTerminalFrame;

    int markX() const { return size.x >= 22 ? size.x - 9 : -1; }

    TWibWobTerminalWindow *win() const { return dynamic_cast<TWibWobTerminalWindow *>(owner); }

    void draw() override
    {
        BasicTerminalFrame::draw();
        auto *w = win();
        int x = markX();
        if (!w || x < 0 || w->runState() == TWibWobTerminalWindow::RunState::Exited) return;
        TDrawBuffer b;
        TColorAttr col = mapColor((state & sfActive) ? 3 : 1);
        b.moveStr(0, w->isPaused() ? "[►]" : "[‖]", col);
        writeLine(x, 0, 3, 1, b);
    }

    void handleEvent(TEvent &ev) override
    {
        if (ev.what == evMouseDown)
        {
            TPoint p = makeLocal(ev.mouse.where);
            int x = markX();
            auto *w = win();
            if (w && x >= 0 && p.y == 0 && p.x >= x && p.x < x + 3 &&
                w->runState() != TWibWobTerminalWindow::RunState::Exited)
            {
                termtransport::act(w, "toggle", resident::currentWho());
                clearEvent(ev);
                return;
            }
        }
        BasicTerminalFrame::handleEvent(ev);
    }
};

} // namespace

TFrame *TWibWobTerminalWindow::initTermFrame(TRect r)
{
    return new TTermTransportFrame(r);
}

namespace {

// Passes everything to tvterm's vterm emulator and gives the program's output, as it is
// read from the pty, to the window's cast recorder (when it is recording).
class RecordingEmulator final : public tvterm::TerminalEmulator
{
public:
    RecordingEmulator(tvterm::TerminalEmulator &inner, std::shared_ptr<ww::CastRecorder> rec) :
        inner_(inner), rec_(std::move(rec)) {}
    ~RecordingEmulator() { delete &inner_; }
    void handleEvent(const tvterm::TerminalEvent &ev) noexcept override
    {
        if (ev.type == tvterm::TerminalEventType::ClientDataRead && rec_)
            rec_->output(ev.clientDataRead.data, ev.clientDataRead.size);
        inner_.handleEvent(ev);
    }
    void updateState(tvterm::TerminalState &state) noexcept override { inner_.updateState(state); }
private:
    tvterm::TerminalEmulator &inner_;
    std::shared_ptr<ww::CastRecorder> rec_;
};

class RecordingEmulatorFactory final : public tvterm::TerminalEmulatorFactory
{
public:
    explicit RecordingEmulatorFactory(std::shared_ptr<ww::CastRecorder> rec) : rec_(std::move(rec)) {}
    tvterm::TerminalEmulator &create(TPoint size, tvterm::Writer &w) noexcept override
    {
        return *new RecordingEmulator(inner_.create(size, w), rec_);
    }
    TSpan<const tvterm::EnvironmentVar> getCustomEnvironment() noexcept override
    {
        return inner_.getCustomEnvironment();
    }
private:
    tvterm::VTermEmulatorFactory inner_;
    std::shared_ptr<ww::CastRecorder> rec_;
};

// Wraps the vterm factory and adds the environment that makes tools/term_exec.sh the
// terminal's program: SHELL points at the wrapper, WWDOS_TERM_EXEC carries the command.
class ExecEmulatorFactory final : public tvterm::TerminalEmulatorFactory
{
public:
    ExecEmulatorFactory(const std::string &wrapper, const std::string &exec, bool hold,
                        const std::string &pidFile, std::shared_ptr<ww::CastRecorder> rec) :
        inner_(rec), wrapper_(wrapper), exec_(exec), hold_(hold ? "1" : "0"), pidFile_(pidFile)
    {
        const char *sh = std::getenv("SHELL");
        shell_ = (sh && *sh && wrapper_ != sh) ? sh : "/bin/zsh";
        vars_[0] = {"TERM", "xterm-256color"};
        vars_[1] = {"COLORTERM", "truecolor"};
        vars_[2] = {"SHELL", wrapper_.c_str()};
        vars_[3] = {"WWDOS_TERM_EXEC", exec_.c_str()};
        vars_[4] = {"WWDOS_TERM_HOLD", hold_.c_str()};
        vars_[5] = {"WWDOS_TERM_SHELL", shell_.c_str()};
        vars_[6] = {"WWDOS_TERM_PIDFILE", pidFile_.c_str()};
    }

    tvterm::TerminalEmulator &create(TPoint size, tvterm::Writer &w) noexcept override
    {
        return inner_.create(size, w);
    }

    TSpan<const tvterm::EnvironmentVar> getCustomEnvironment() noexcept override
    {
        return TSpan<const tvterm::EnvironmentVar>(vars_, 7);
    }

private:
    RecordingEmulatorFactory inner_;
    std::string wrapper_, exec_, hold_, pidFile_, shell_;
    tvterm::EnvironmentVar vars_[7];
};

} // namespace

// The pty's process: tvterm forks it inside TerminalController::create and does not say
// its pid, so it is the child of the app that was not there before.
static pid_t newChild(const std::vector<pid_t> &before)
{
    pid_t found = 0;
    for (pid_t p : termChildPids())
        if (std::find(before.begin(), before.end(), p) == before.end() && p > found) found = p;
    return found;
}

namespace {

std::string findTermExecWrapper()
{
    const char *repo = std::getenv("WWDOS_REPO");
    std::string candidates[] = {
        repo && *repo ? std::string(repo) + "/tools/term_exec.sh" : std::string(),
        "tools/term_exec.sh",
    };
    for (const auto &c : candidates)
    {
        if (c.empty()) continue;
        char buf[PATH_MAX];
        if (realpath(c.c_str(), buf) && access(buf, X_OK) == 0)
            return buf;
    }
    return "";
}

} // namespace

TWindow* createTerminalWindowExec(const TRect &bounds, const std::string &exec, bool hold)
{
    using namespace tvterm;
    std::string wrapper = findTermExecWrapper();
    if (wrapper.empty())
        return nullptr;
    static unsigned counter = 0;
    char pidFile[96];
    snprintf(pidFile, sizeof(pidFile), "/tmp/wwdos-term-%d-%u.pid", (int) getpid(), ++counter);
    unlink(pidFile);
    auto rec = std::make_shared<ww::CastRecorder>();
    ExecEmulatorFactory factory(wrapper, exec, hold, pidFile, rec);
    TPoint viewSize = BasicTerminalWindow::viewSize(bounds);
    // No message box: this runs from the API with nobody at the keyboard.
    std::vector<pid_t> before = termChildPids();
    auto *termCtrl = TerminalController::create(viewSize, factory, onTermErrorQuiet);
    if (!termCtrl)
        return nullptr;
    auto *w = new TWibWobTerminalWindow(bounds, *termCtrl);
    w->setChildPidFile(pidFile);
    w->setLeaderPid(newChild(before));
    w->setRecorder(rec);
    return w;
}

TWindow* createTerminalWindow(const TRect &bounds)
{
    using namespace tvterm;
    auto rec = std::make_shared<ww::CastRecorder>();
    RecordingEmulatorFactory factory(rec);
    TPoint viewSize = BasicTerminalWindow::viewSize(bounds);
    std::vector<pid_t> before = termChildPids();
    auto *termCtrl = TerminalController::create(viewSize, factory, onTermError);
    if (!termCtrl)
        return nullptr;
    auto *w = new TWibWobTerminalWindow(bounds, *termCtrl);
    w->setLeaderPid(newChild(before));
    w->setRecorder(rec);
    return w;
}

#endif // __EMSCRIPTEN__

#ifndef __EMSCRIPTEN__   // the browser's stand-in window describes itself (tvterm_view.h)
std::string TWibWobTerminalWindow::describeState() const {
    const char* t = const_cast<TWibWobTerminalWindow*>(this)->getTitle(0);
    auto lines = ww::splitText(getOutputText());
    std::string last;
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) if (!it->empty()) { last = ww::clipText(*it, 50); break; }
    std::string s = std::string("terminal") + (t && *t ? std::string(" \"") + t + "\"" : "") + " showing " +
           std::to_string(lines.size()) + " lines of output" + (last.empty() ? "" : "; last line: \"" + last + "\"");
#ifndef __EMSCRIPTEN__
    auto *self = const_cast<TWibWobTerminalWindow*>(this);
    std::string prog = self->programName();
    switch (self->runState()) {
    case RunState::Paused:
        s += "; " + (prog.empty() ? std::string("its program") : prog) + " is paused (" +
             std::to_string((int) secondsPaused()) + " s), Alt-P or terminal_resume plays it";
        break;
    case RunState::Exited: s += "; its program has exited"; break;
    case RunState::Running: if (!prog.empty()) s += "; running " + prog; break;
    }
    if (keyboard() == "grab") s += "; it holds the keyboard (Alt-End or terminal_release gives it back)";
#endif
    return s;
}
bool TWibWobTerminalWindow::describeText(std::vector<std::string>& out) const {
    out = ww::splitText(getOutputText());
    return true;
}
#endif
