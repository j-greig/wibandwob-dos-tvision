// DLGDRAW.CPP - Changing text/color and sending messages in a dialog (Borland, 1992)
// Ported to modern magicius/tvision
//
// Original: demonstrates overloading draw(), broadcast messages,
// and idle loop animation in a dialog box.

#define Uses_TEventQueue
#define Uses_TEvent
#define Uses_TProgram
#define Uses_TApplication
#define Uses_TKeys
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TView
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TDialog
#define Uses_TButton
#define Uses_TSItem
#define Uses_TMenu
#define Uses_TDrawBuffer
#include <tvision/tv.h>

#include <cstdlib>
#include <cstring>

#define TIMEOUT       8000
#define MAX_STRINGS      7
#define MAX_COLORS      16

const int cmDrawDialog  = 100;
const int cmDrawLine    = 101;


class TMyApp : public TApplication
{
private:
    unsigned count;

public:
    TMyApp();
    static TMenuBar *initMenuBar( TRect );
    static TStatusLine *initStatusLine( TRect );
    void handleEvent(TEvent& event);
protected:
    void DrawDialog(void);
    void idle(void);
};


class myDialog : public TDialog
{
    const char *dialogStr;
    const char *stringList[MAX_STRINGS];
    unsigned stringNum;

public:
    myDialog(const TRect& bounds, const char *aTitle = "Draw Dialog"):
        TDialog( bounds, aTitle),
        TWindowInit(&myDialog::initFrame)
    {
        stringNum = 0;
        stringList[0] = "Example of...";
        stringList[1] = "catching events and...";
        stringList[2] = "sending messages and...";
        stringList[3] = "changing text...";
        stringList[4] = "in a dialog box.";
        stringList[5] = "Borland International";
        stringList[6] = "         1992        ";
        dialogStr = stringList[0];
    }

    void draw();
    virtual void handleEvent( TEvent& event);
};

//==========================================================================
void myDialog::draw()
{
    TDialog::draw();

    TColorAttr textAttr = getColor(1);
    ushort color = stringNum % MAX_COLORS;

    TDrawBuffer b;
    dialogStr = stringList[ stringNum++ % MAX_STRINGS ];
    b.moveStr( 0, dialogStr, (TColorAttr)( (color << 4) | (textAttr & 0x0F) ) );
    writeLine(14, 7, (ushort)strlen(dialogStr), 1, b);
}

//==========================================================================
void myDialog::handleEvent( TEvent& event)
{
    if ( event.what == evBroadcast )
    {
        switch( event.message.command )
        {
            case cmDrawLine:
                drawView();
                break;
            default:
                break;
        }
    }

    TDialog::handleEvent( event );
}


void TMyApp::DrawDialog()
{
    myDialog *pd = new myDialog( TRect( 15, 4, 65, 20) );

    if( validView(pd) )
    {
        deskTop->execView( pd );
    }
    destroy( pd );
}


TMyApp::TMyApp() :
    TProgInit( &initStatusLine,
               &initMenuBar,
               &initDeskTop
             )
{
    count = 1;
}


//===========================================================================
void TMyApp::idle()
{
    TProgram::idle();
    if( ! (count++ % TIMEOUT) )
        message(deskTop, evBroadcast, cmDrawLine, 0);
}


void TMyApp::handleEvent(TEvent& event)
{
    TApplication::handleEvent( event );

    if( event.what == evCommand )
    {
        switch( event.message.command)
        {
            case cmDrawDialog:
                DrawDialog();
                break;
            default:
                break;
        }
        clearEvent( event );
    }
}


TStatusLine *TMyApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;

    return new TStatusLine( r,
        *new TStatusDef( 0, 0xFFFF) +
        *new TStatusItem( "~Alt-X~ Exit", kbAltX, cmQuit) +
        *new TStatusItem( "~Alt-D~ Draw Dialog", kbAltD, cmDrawDialog)
        );
}


TMenuBar *TMyApp::initMenuBar( TRect r )
{
    r.b.y = r.a.y + 1;

    return new TMenuBar( r,
        *new TSubMenu( "~\xF0~", kbAltSpace ) +
        *new TMenuItem( "~D~raw Dialog", cmDrawDialog, kbAltD, hcNoContext, "Alt-D"));
}


int main()
{
    TMyApp myApp;
    myApp.run();
    return 0;
}
