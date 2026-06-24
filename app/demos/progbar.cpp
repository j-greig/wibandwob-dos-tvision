// PROGBAR - Progress Bar Demo
// Original: Jay Perez, modified by Barnaby W. Falls (CIS: 70662,1523)
// Ported to modern magicius/tvision by Wib & Wob
//
// Demonstrates a TProgressBar widget: a thermometer-style bar that
// fills from left to right showing percentage. The demo runs a
// modeless dialog through 3 phases (0-100, 101-200, 201-300) with
// cancel support and explanatory text that changes per phase.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

#define Uses_TApplication
#define Uses_TBackground
#define Uses_TButton
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TLabel
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TRect
#define Uses_TScreen
#define Uses_TSItem
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TView
#define Uses_TWindow
#define Uses_MsgBox
#define Uses_TGroup
#define Uses_TStreamableClass
#define Uses_TStreamable
#define Uses_opstream
#define Uses_ipstream

#include <tvision/tv.h>

// =========================================================================
// TProgressBar - thermometer-style progress indicator
// =========================================================================

class TProgressBar : public TView
{
public:
    TProgressBar(const TRect& r, unsigned long iters, char abackChar = '\xB2');
    ~TProgressBar();
    virtual void draw();
    virtual TPalette& getPalette() const;
    virtual void update(unsigned long aProgress);

    unsigned long getTotal()    { return total; }
    unsigned long getProgress() { return progress; }

    void setTotal(unsigned long newTotal);
    void setProgress(unsigned long newProgress);

protected:
    char          backChar;
    unsigned long total;
    unsigned long progress;
    char *        bar;
    unsigned int  dispLen;
    unsigned int  curPercent;
    unsigned int  curWidth;
    unsigned int  numOffset;
    double        charValue;

private:
    void calcPercent();
};

#define cpProgressBar "\x04"

TProgressBar::TProgressBar(const TRect& bounds, unsigned long aTotal, char abackChar) :
    TView(bounds)
{
    backChar  = abackChar;
    total     = aTotal;
    numOffset = (size.x / 2) - 3;
    bar       = new char[size.x + 1];
    memset(bar, backChar, size.x);
    bar[size.x] = '\0';
    charValue = (double)100 / (double)size.x;
    progress  = 0;
    curPercent = 0;
    curWidth  = 0;
    dispLen   = 0;
}

TProgressBar::~TProgressBar()
{
    delete[] bar;
}

void TProgressBar::draw()
{
    char string[4];
    snprintf(string, sizeof(string), "%3u", curPercent > 999 ? 999 : curPercent);

    TDrawBuffer nbuf;
    uchar colorNormal, colorHiLite;
    colorNormal = (uchar)getColor(1);
    uchar fore = colorNormal >> 4;
    colorHiLite = fore + ((colorNormal - (fore << 4)) << 4);
    nbuf.moveChar(0, backChar, colorNormal, (ushort)size.x);
    nbuf.moveStr(numOffset, string, colorNormal);
    nbuf.moveStr(numOffset + 3, " %", colorNormal);
    for (int i = 0; i < (int)curWidth; i++)
        nbuf.putAttribute(i, colorHiLite);
    writeLine(0, 0, (short)size.x, 1, nbuf);
}

TPalette& TProgressBar::getPalette() const
{
    static TPalette palette(cpProgressBar, sizeof(cpProgressBar) - 1);
    return palette;
}

void TProgressBar::update(unsigned long aProgress)
{
    progress = aProgress;
    calcPercent();
    drawView();
}

void TProgressBar::calcPercent()
{
    unsigned int percent = (unsigned int)(((double)progress / (double)total) * 100.0);
    if (percent != curPercent)
    {
        curPercent = percent;
        unsigned int width = (unsigned int)((double)curPercent / charValue);
        if (width != curWidth)
            curWidth = width;
    }
}

void TProgressBar::setTotal(unsigned long newTotal)
{
    unsigned long tmp = total;
    total      = newTotal;
    memset(bar, backChar, size.x);
    curWidth   = 0;
    progress   = 0;
    curPercent = 0;
    if (tmp)
        drawView();
}

void TProgressBar::setProgress(unsigned long newProgress)
{
    progress = newProgress;
    calcPercent();
    drawView();
}

// =========================================================================
// Demo application
// =========================================================================

const unsigned cmAboutCmd  = 100;
const unsigned cmStatusCmd = 101;

class TMyApplication : public TApplication
{
public:
    TMyApplication();
    static TMenuBar *initMenuBar(TRect);
    void handleEvent(TEvent &);
private:
    void aboutDlg();
    void statusDlg();
    Boolean isCancel(TDialog *pd);
};

TMyApplication::TMyApplication() :
    TProgInit(&TApplication::initStatusLine, &TMyApplication::initMenuBar,
              &TApplication::initDeskTop)
{
}

TMenuBar *TMyApplication::initMenuBar(TRect bounds)
{
    bounds.b.y = bounds.a.y + 1;
    return new TMenuBar(bounds,
        new TMenu(
            *new TMenuItem("~A~bout", cmAboutCmd, kbAltA, hcNoContext, 0,
                new TMenuItem("~P~rogress Bar", cmStatusCmd, kbAltL, hcNoContext, 0))));
}

void TMyApplication::handleEvent(TEvent &event)
{
    TApplication::handleEvent(event);
    if (event.what == evCommand)
    {
        switch (event.message.command)
        {
        case cmAboutCmd:
            aboutDlg();
            clearEvent(event);
            break;
        case cmStatusCmd:
            statusDlg();
            clearEvent(event);
            break;
        }
    }
}

void TMyApplication::aboutDlg()
{
    TDialog *pd = new TDialog(TRect(0, 0, 35, 12), "About");
    if (pd)
    {
        pd->options |= ofCentered;
        pd->insert(new TStaticText(TRect(1, 2, 34, 7),
            "\003Turbo Vision Example\n\003\n"
            "\003Using a Progress Bar\n\003\n"));
        pd->insert(new TButton(TRect(3, 9, 32, 11), "~O~k", cmOK, bfDefault));
        if (validView(pd) != 0)
        {
            deskTop->execView(pd);
            destroy(pd);
        }
    }
}

Boolean TMyApplication::isCancel(TDialog *pd)
{
    TEvent event;
    pd->getEvent(event);
    pd->handleEvent(event);
    if (event.what == evCommand && event.message.command == cmCancel)
        return (messageBox("Are you sure you want to Cancel",
                mfConfirmation | mfYesButton | mfNoButton) == cmYes ? True : False);
    else
        return False;
}

void TMyApplication::statusDlg()
{
    TDialog *pd = new TDialog(TRect(0, 0, 60, 15), "Example Progress Bar");
    pd->flags &= ~wfClose;
    pd->options |= ofCentered;
    TProgressBar *pbar = new TProgressBar(TRect(2, 2, pd->size.x - 2, 3), 300);
    pd->insert(pbar);
    pd->insert(new TButton(TRect(10, pd->size.y - 3, pd->size.x - 10, pd->size.y - 1),
                            "~C~ancel", cmCancel, bfDefault));
    TProgram::deskTop->insert(pd);  // Modeless!

    int i = 0;
    Boolean keepOnGoing = True;

    TRect r(5, 5, pd->size.x - 5, pd->size.y - 5);
    TStaticText *theMessage;

    // Phase 1: first third
    theMessage = new TStaticText(r,
        "This is a MODELESS dialog box. You can drag this box around the desktop.");
    pd->insert(theMessage);
    for (; i <= 100; i++)
    {
        pbar->update(i);
        idle();
        if (isCancel(pd)) { keepOnGoing = False; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    destroy(theMessage);

    // Phase 2: second third
    if (keepOnGoing)
    {
        theMessage = new TStaticText(r,
            "Notice that only the attribute is changed to show progress");
        pd->insert(theMessage);
        for (; i <= 200; i++)
        {
            pbar->update(i);
            idle();
            if (isCancel(pd)) { keepOnGoing = False; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        destroy(theMessage);
    }

    // Phase 3: final third
    if (keepOnGoing)
    {
        theMessage = new TStaticText(r,
            "Syntax: TProgressBar(TRect &r, double total, char bar);");
        pd->insert(theMessage);
        for (; i <= 300; i++)
        {
            pbar->update(i);
            idle();
            if (isCancel(pd)) { keepOnGoing = False; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        destroy(theMessage);
    }

    destroy(pd);
}

int main()
{
    TMyApplication myApplication;
    myApplication.run();
    return 0;
}
