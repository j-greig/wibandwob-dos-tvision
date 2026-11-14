// Browser stand-in for the tvterm PTY terminal window.
#define Uses_TWindow
#define Uses_TStaticText
#define Uses_TRect
#include <tvision/tv.h>
#include "tvterm_view.h"

const tvterm::TVTermConstants TWibWobTerminalWindow::termConsts {500};

TWibWobTerminalWindow::TWibWobTerminalWindow(const TRect &bounds) noexcept :
    TWindowInit(&TWindow::initFrame),
    TWindow(bounds, "Terminal", wnNoNumber)
{
    TRect r = getExtent();
    r.grow(-2, -1);
    insert(new TStaticText(r,
        "No terminal in the browser.\n\n"
        "This build runs without a host machine, so there is no shell to "
        "connect to. The native WibWob-DOS opens a real terminal here."));
}

void TWibWobTerminalWindow::setFixedTitle(const std::string &t)
{
    fixedTitle_ = t;
    delete[] (char *) title;
    title = newStr(fixedTitle_.c_str());
}

TWindow* createTerminalWindow(const TRect &bounds)
{
    return new TWibWobTerminalWindow(bounds);
}

TWindow* createTerminalWindowExec(const TRect &bounds, const std::string &, bool)
{
    return new TWibWobTerminalWindow(bounds);
}
