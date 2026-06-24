// ARGA.CPP - Message handling example (Borland, 1992, Greg Myers)
// Ported to modern magicius/tvision by Wib & Wob
//
// Original: demonstrates message passing between TListBox and TView
// so the TView displays the current selection in the TListBox.

const unsigned cmTechInfo = 101;
const unsigned cmNewData  = 102;

#define Uses_TApplication
#define Uses_TButton
#define Uses_TCollection
#define Uses_TDialog
#define Uses_TDeskTop
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TListBox
#define Uses_TMenuBar
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TRect
#define Uses_TScrollBar
#define Uses_TStaticText
#define Uses_TStatusLine
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TView
#define Uses_TWindow
#define Uses_TStringCollection
#define Uses_MsgBox
#define Uses_ipstream
#define Uses_opstream

#include <tvision/tv.h>

#include <cstring>

// NOTE: operator+(TMenuItem&, TMenuItem&) already defined in modern tvision

// --- Collection ---

class MyTCollection : public TStringCollection
{
public:
    MyTCollection() : TStringCollection(10, 5) {}
    void *readItem( ipstream& ) { return this; }
    void writeItem( void *, opstream& ) {}
};

char data[10][10] = {
    "one", "two", "three", "four", "five",
    "six", "seven", "eight", "nine", "ten"
};
MyTCollection *tc;


// --- Forward declarations ---

class TMyApplication : public TApplication
{
public:
    TMyApplication();
    static TMenuBar *initMenuBar(TRect);
    static TStatusLine *initStatusLine(TRect);
    void TechInfo();
    void handleEvent(TEvent&);
};

class TechInfoList : public TListBox
{
public:
    TechInfoList(TRect&, ushort, TScrollBar*);
    void handleEvent(TEvent&);
};

class TechInfoDialog : public TDialog
{
public:
    TechInfoDialog(TRect& r) :
        TWindowInit(&TechInfoDialog::initFrame),
        TDialog(r, "Data ListBox")
    {
        TRect t = getExtent();
        t.a.x++;
        t.a.y++;
        t.b.x--;
        t.b.y -= 2;
        TScrollBar *sb = new TScrollBar(
            TRect(t.b.x - 2, t.a.y + 1, t.b.x - 1, t.b.y - 2));
        insert(sb);
        insert(new TechInfoList(t, 1, sb));
        insert(new TButton(TRect(10, t.b.y - 1, 20, t.b.y + 1),
                           "~O~k", cmClose, bfDefault));
    }
};

class TechInfoView : public TView
{
    const char *curdata;
public:
    TechInfoView(TRect r) : TView(r)
    {
        eventMask |= evBroadcast;
        curdata = "Press Up or Down Arrow";
    }
    void draw();
    void handleEvent(TEvent&);
};


// --- Implementations ---

void TechInfoView::handleEvent(TEvent &event)
{
    TView::handleEvent(event);
    if (event.what == evBroadcast)
        if (event.message.command == cmNewData)
        {
            curdata = (const char *)event.message.infoPtr;
            drawView();
        }
}

void TechInfoList::handleEvent(TEvent &event)
{
    if (event.what == evKeyDown)
        switch (event.keyDown.keyCode)
        {
        case kbUp:
            message(TProgram::deskTop, evBroadcast,
                    cmNewData, tc->at(focused > 0 ? focused - 1 : focused));
            break;
        case kbDown:
            message(TProgram::deskTop, evBroadcast,
                    cmNewData, tc->at(focused + 1));
            break;
        default:
            break;
        }
    TListBox::handleEvent(event);
}

void TechInfoView::draw()
{
    TDrawBuffer tb;
    tb.moveChar(0, ' ', getColor(1), size.x);
    int len = (int)strlen(curdata);
    if (len > size.x) len = size.x;
    tb.moveBuf(0, curdata, getColor(1), (ushort)len);
    writeLine(0, 0, (short)size.x, 1, tb);
}

TechInfoList::TechInfoList(TRect& r, ushort numcols, TScrollBar *sb) :
    TListBox(TRect(r.a.x + 2, r.a.y + 1, r.b.x - 3, r.b.y - 2), numcols, sb)
{
    tc = new MyTCollection();
    for (int c = 0; c < 10; c++)
        tc->insert((void *)data[c]);
    newList(tc);
}

TStatusLine *TMyApplication::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    return new TStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
            *new TStatusItem("~Alt-F3~ Close", kbAltF3, cmClose)
    );
}

TMenuBar *TMyApplication::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;
    return new TMenuBar(r,
        new TMenu(
            *new TMenuItem("I~n~fo", kbAltN,
                new TMenu(
                    *new TMenuItem("~I~nfo", cmTechInfo, kbAltI, hcNoContext, "Alt-I") +
                    *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X")
                )
            )
        )
    );
}

TMyApplication::TMyApplication() :
    TProgInit(&initStatusLine,
              &initMenuBar,
              &initDeskTop)
{
}

void TMyApplication::handleEvent(TEvent& event)
{
    TApplication::handleEvent(event);
    if (event.what == evCommand)
    {
        switch (event.message.command)
        {
        case cmTechInfo:
            TechInfo();
            break;
        default:
            return;
        }
        clearEvent(event);
    }
}

void TMyApplication::TechInfo()
{
    TRect tr;

    tr.a.x = 10;
    tr.a.y = 17;
    tr.b.x = tr.a.x + 30;
    tr.b.y = tr.a.y + 3;
    TechInfoView *tv = new TechInfoView(TRect(1, 1, 10, 3));
    TWindow *twv = new TWindow(
        TRect(tr.a.x - 1, tr.a.y - 1, tr.b.x + 1, tr.b.y + 1),
        "Individual View", 2);
    twv->insert(tv);
    deskTop->insert(twv);

    tr.a.x = 10;
    tr.a.y = 1;
    tr.b.x = tr.a.x + 30;
    tr.b.y = tr.a.y + 10;
    TechInfoDialog *ti = new TechInfoDialog(tr);
    deskTop->execView(ti);
}

int main()
{
    TMyApplication tmyapp;
    tmyapp.run();
    return 0;
}
