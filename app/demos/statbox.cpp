// STATBOX - Status box with live-updating display
// Ported from borland_statbo: CALC.CPP + CALC.H
// TCalcDisplay counts from 0 to 10 with delays

#define Uses_TBackground
#define Uses_TListBox
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TScrollBar
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TStringCollection
#define Uses_MsgBox
#define Uses_TEventQueue
#define Uses_TApplication
#define Uses_TRect
#define Uses_TDeskTop
#define Uses_TView
#define Uses_TWindow
#define Uses_TDialog
#define Uses_TButton
#define Uses_TSItem
#define Uses_TLabel
#define Uses_TInputLine
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TDrawBuffer
#define Uses_TProgram
#define Uses_TGroup
#include <tvision/tv.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <chrono>
#include <thread>

#define DISPLAYLEN  45

enum TCalcState { csFirst = 1, csValid, csError };

const unsigned cmCalcButton = 200;
const unsigned cmAboutCmd   = 100;
const unsigned cmStatusCmd  = 101;

// ---- TCalcDisplay ----

class TCalcDisplay : public TView
{
public:
    TCalcDisplay(const TRect& r);
    ~TCalcDisplay();
    virtual void handleEvent(TEvent& event);
    virtual void draw();
private:
    TCalcState status;
    char *dnfile;
    void calcKey(unsigned char key);
    void ShowFiles();
};

TCalcDisplay::TCalcDisplay(const TRect& r) : TView ( r )
{
    options |= ofSelectable;
    eventMask = (evKeyboard | evBroadcast);
    dnfile = new char[DISPLAYLEN];
    strcpy(dnfile, "0");
}

TCalcDisplay::~TCalcDisplay()
{
    delete [] dnfile;
}

void TCalcDisplay::handleEvent(TEvent& event)
{
    TView::handleEvent(event);

    switch(event.what)
    {
    case evKeyboard:
        calcKey(event.keyDown.charScan.charCode);
        clearEvent(event);
        break;
    case evBroadcast:
        if(event.message.command == cmOK)
        {
            calcKey( ((TButton *) event.message.infoPtr)->title[0]);
            clearEvent(event);
        }
        break;
    }
}

void TCalcDisplay::draw()
{
    char color = getColor(1);
    TDrawBuffer nbuf;
    char Buf[DISPLAYLEN+1];

    nbuf.moveChar(0, ' ', color, size.x);
    snprintf(Buf, sizeof(Buf), " %s", dnfile);
    nbuf.moveStr(0, Buf, color);
    writeLine(0, 0, size.x, 1, nbuf);
}

void TCalcDisplay::calcKey(unsigned char key)
{
    (void)key;
    ShowFiles();
    strcpy(dnfile, "All done!");
    drawView();
    message(owner, evCommand, cmCancel, this);
}

void TCalcDisplay::ShowFiles()
{
    for(int x = 0; x <= 10; x++)
    {
        snprintf(dnfile, DISPLAYLEN, "Count %d", x);
        drawView();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    strcpy(dnfile, "All done!");
    drawView();
}

// ---- TCalculator ----

class TCalculator : public TDialog
{
public:
    TCalculator();
};

TCalculator::TCalculator() :
    TDialog( TRect(5, 3, 69, 18), "Add Files" ),
    TWindowInit( &TCalculator::initFrame )
{
    TView *tv;
    TRect r;

    options |= ofFirstClick;
    options |= ofCentered;

    r = TRect( 5, 9, 16, 11 );
    tv = new TButton( r, "~G~o 4 It", cmOK, bfNormal | bfBroadcast );
    tv->options &= ~ofSelectable;
    insert( tv );
    insert(
        new TButton( TRect( 26, 9, 37, 11 ), "~A~bort", cmCancel, bfNormal ) );

    insert(
        new TStaticText(TRect(2,3,15,6), "Directory:"));

    insert(new TCalcDisplay(TRect(14,3,61,4)));
}

// ---- TMyApplication ----

class TMyApplication : public TApplication
{
public:
    TMyApplication();
    static TMenuBar *initMenuBar(TRect);
    void handleEvent(TEvent &);
private:
    void aboutDlg();
    void statusDlg();
};

TMyApplication::TMyApplication() :
    TProgInit(&TApplication::initStatusLine, &TMyApplication::initMenuBar,
              &TApplication::initDeskTop)
{
}

TMenuBar *TMyApplication::initMenuBar(TRect bounds)
{
    bounds.b.y = bounds.a.y + 1;
    return(new TMenuBar(bounds,
        new TMenu(
            *new TMenuItem("~A~bout", cmAboutCmd, kbAltA, hcNoContext, 0,
             new TMenuItem("~S~tatus Box", cmStatusCmd, kbAltL, hcNoContext, 0)))));
}

void TMyApplication::handleEvent(TEvent &event)
{
    TApplication::handleEvent(event);

    if (event.what == evCommand)
    {
        switch (event.message.command)
        {
            case cmAboutCmd:
            {
                aboutDlg();
                clearEvent(event);
                break;
            }
            case cmStatusCmd:
            {
                statusDlg();
                clearEvent(event);
                break;
            }
        }
    }
}

void TMyApplication::aboutDlg()
{
    TDialog *pd = new TDialog(TRect(0,0,35,12), "About");
    if (pd)
    {
        pd->options |= ofCentered;
        pd->insert(new TStaticText(TRect(1,2,34,7),
                   "\003Turbo Vision Example\n\003\n"
                   "\003Creating a StatusBox\n\003\n"));
        pd->insert(new TButton(TRect(3,9,32,11), "~O~k", cmOK, bfDefault));

        if (validView(pd) != 0)
        {
            deskTop->execView(pd);
            destroy(pd);
        }
    }
}

void TMyApplication::statusDlg()
{
    TCalculator *calc = (TCalculator *) validView(new TCalculator);
    if(calc != 0) {
        message(this, evBroadcast, cmOK, this);
        deskTop->execView(calc);
    }
}

int main()
{
    TMyApplication myApplication;
    myApplication.run();
    return 0;
}
