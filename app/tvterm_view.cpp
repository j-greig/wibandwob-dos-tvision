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
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// Command IDs for tvterm integration — must not collide with existing app commands.
// These are wired through TVTermConstants so tvterm-core uses our IDs.
enum : ushort
{
    cmTvTermCheckUpdates = 500,
    cmTvTermUpdated      = 501,
    cmTvTermGrabInput    = 502,
    cmTvTermReleaseInput = 503,
    hcTvTermInputGrabbed = 504,
};

const tvterm::TVTermConstants TWibWobTerminalWindow::termConsts =
{
    cmTvTermCheckUpdates,
    cmTvTermUpdated,
    cmTvTermGrabInput,
    cmTvTermReleaseInput,
    hcTvTermInputGrabbed,
};

TWibWobTerminalWindow::TWibWobTerminalWindow(
    const TRect &bounds,
    tvterm::TerminalController &termCtrl
) noexcept :
    TWindowInit(&initFrame),
    Super(bounds, termCtrl, termConsts)
{
    options |= ofTileable;
    growMode = gfGrowAll | gfGrowRel;
}

void TWibWobTerminalWindow::handleEvent(TEvent &ev)
{
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
                const TCellChar   &ch   = cell._ch;
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
    if (!fixedTitle_.empty())
        return fixedTitle_.c_str();
    return Super::getTitle(maxSize);
}

namespace {

// Wraps the vterm factory and adds the environment that makes tools/term_exec.sh the
// terminal's program: SHELL points at the wrapper, WWDOS_TERM_EXEC carries the command.
class ExecEmulatorFactory final : public tvterm::TerminalEmulatorFactory
{
public:
    ExecEmulatorFactory(const std::string &wrapper, const std::string &exec, bool hold,
                        const std::string &pidFile) :
        wrapper_(wrapper), exec_(exec), hold_(hold ? "1" : "0"), pidFile_(pidFile)
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
    tvterm::VTermEmulatorFactory inner_;
    std::string wrapper_, exec_, hold_, pidFile_, shell_;
    tvterm::EnvironmentVar vars_[7];
};

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
    ExecEmulatorFactory factory(wrapper, exec, hold, pidFile);
    TPoint viewSize = BasicTerminalWindow::viewSize(bounds);
    // No message box: this runs from the API with nobody at the keyboard.
    auto *termCtrl = TerminalController::create(viewSize, factory, onTermErrorQuiet);
    if (!termCtrl)
        return nullptr;
    auto *w = new TWibWobTerminalWindow(bounds, *termCtrl);
    w->setChildPidFile(pidFile);
    return w;
}

TWindow* createTerminalWindow(const TRect &bounds)
{
    using namespace tvterm;
    VTermEmulatorFactory factory;
    TPoint viewSize = BasicTerminalWindow::viewSize(bounds);
    auto *termCtrl = TerminalController::create(viewSize, factory, onTermError);
    if (!termCtrl)
        return nullptr;
    return new TWibWobTerminalWindow(bounds, *termCtrl);
}
