// COLOR1.CPP - Modifying the desktop pattern (Borland, 1992)
// Ported to modern magicius/tvision
//
// Original: demonstrates changing the desktop background pattern and color
// by deriving from TDeskTop and TBackground.

#define Uses_TApplication
#define Uses_TBackground
#define Uses_TButton
#define Uses_TKeys
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TRect
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#include <tvision/tv.h>

//========================================================================
//  global data
//------------------------------------------------------------------------
const unsigned cmAbout = 100;

// cpAppDeskTop used in newly defined desktop for background
#define cpNewDeskTop "\x13"

// character to be used for background pattern
#define PATTERN ((char)225)

//========================================================================
//  class definitions
//------------------------------------------------------------------------
class TApp : public TApplication {
public:
    TApp();
    static TMenuBar *initMenuBar( TRect r );
    static TDeskTop *initDeskTop( TRect r );
    void handleEvent( TEvent &event );
    void AboutDialog();
};

//------------------------------------------------------------------------
class TNewDeskTop : public TDeskTop
{
public:
    TNewDeskTop( TRect& r );
    static TBackground* initBackground( TRect r );
};

//------------------------------------------------------------------------
class TNewBackground : public TBackground
{
public:
    TNewBackground( TRect& r, char pattern );
    TPalette& getPalette() const;
};

//========================================================================
//  implementation of TApp
//------------------------------------------------------------------------
TApp::TApp() : TProgInit( &TApplication::initStatusLine,
                    &TApp::initMenuBar, &TApp::initDeskTop )
{
}

//------------------------------------------------------------------------
TMenuBar *TApp::initMenuBar( TRect r )
{
    r.b.y = r.a.y + 1;
    return( new TMenuBar( r, new TMenu(
        *new TMenuItem( "~A~bout", cmAbout, kbAltA, hcNoContext, 0 )
        ) ) );
}

//------------------------------------------------------------------------
TDeskTop *TApp::initDeskTop( TRect r )
{
    r.a.y++;
    r.b.y--;
    return new TNewDeskTop( r );
}

//------------------------------------------------------------------------
void TApp::handleEvent (TEvent &event)
{
    TApplication::handleEvent( event );
    if( event.what == evCommand )
    {
        switch( event.message.command )
        {
            case cmAbout:
                AboutDialog();
                clearEvent( event );
                break;
        }
    }
}

//------------------------------------------------------------------------
void TApp::AboutDialog()
{
    TDialog *pd = new TDialog( TRect( 0, 0, 35, 12 ), "About" );
    if (pd)
    {
        pd->options |= ofCentered;
        pd->insert( new TStaticText( TRect( 1, 2, 34, 7 ),
                "\003Turbo Vision Example\n\003\n"
                "\003Modifying the desk top\n\003\n"
                "\003Borland Technical Support"));
        pd->insert( new TButton( TRect( 3,9,32,11 ), "~O~k",
                                cmOK, bfDefault ) );
        deskTop->execView( pd );
    }
    destroy( pd );
}

//========================================================================
//  implementation of TNewDeskTop
//------------------------------------------------------------------------
TNewDeskTop::TNewDeskTop( TRect& r ) :
                    TDeskTop( r ), TDeskInit( &initBackground )
{
}

//------------------------------------------------------------------------
TBackground *TNewDeskTop::initBackground( TRect r )
{
    return new TNewBackground( r, PATTERN );
}

//========================================================================
//  implementation of TNewBackground
//------------------------------------------------------------------------
TNewBackground::TNewBackground( TRect& r, char pattern ) :
                        TBackground( r, pattern )
{
}

//------------------------------------------------------------------------
TPalette& TNewBackground::getPalette() const
{
    static TPalette palette( cpNewDeskTop, sizeof( cpNewDeskTop ) - 1 );
    return palette;
}

//========================================================================
int main(void)
{
    TApp myApp;
    myApp.run();
    return 0;
}
