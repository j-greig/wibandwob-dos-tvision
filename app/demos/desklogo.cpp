// DESKLOGO.CPP - Desktop logo/pattern drawing (Borland, 1992)
// Ported to modern magicius/tvision
//
// Original: demonstrates drawing custom patterns on the desktop
// using overridden draw() in TDeskTop and TBackground.

#define Uses_TApplication
#define Uses_TBackground
#define Uses_TButton
#define Uses_TKeys
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TDrawBuffer
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

#include <string.h>

#define PATTERN ((char)177)

const unsigned cmAbout = 100;

// Simple placeholder lines -- the original used CP437 box-drawing characters
// which don't display properly in modern terminals. Using ASCII patterns instead.
static const char *lines[] = {
    "################################################################################",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "# VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION *** #",
    "#  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO VISION ***  *** TURBO #",
    "################################################################################"
};
static const int numLines = sizeof(lines) / sizeof(lines[0]);

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
    void draw();
};

//==========================================================================
void TNewDeskTop::draw()
{
    TDrawBuffer b;
    TColorAttr color = getColor( 1 );

    for(int i = 0; i < size.y && i < numLines; i++)
    {
        b.moveStr( 0, lines[i], color );
        writeLine(0, i, (ushort)strlen(lines[i]), 1, b);
    }
}

//------------------------------------------------------------------------
class TNewBackground : public TBackground
{
public:
    TNewBackground( TRect& r, char pattern );
    void draw();
};

//===========================================================================
void TNewBackground::draw()
{
    TDrawBuffer b;

    for(int i = 0; i < size.y; i++)
    {
        int lineIdx = i % numLines;
        for( int j = 0; j < size.x && lines[lineIdx][j] != '\0'; j++)
        {
            b.moveChar( j, lines[lineIdx][j], getColor(0x01), 1 );
        }
        writeLine( 0, i, size.x, 1, b );
    }
}

TApp::TApp() : TProgInit( &TApplication::initStatusLine,
                    &TApp::initMenuBar, &TApp::initDeskTop )
{
}

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
    return ( new TNewDeskTop( r ) );
}

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
TNewDeskTop::TNewDeskTop( TRect& r ) :
                    TDeskTop( r ), TDeskInit( &initBackground )
{
}

TBackground *TNewDeskTop::initBackground( TRect r )
{
    return new TNewBackground( r, PATTERN );
}

//========================================================================
TNewBackground::TNewBackground( TRect& r, char pattern ) :
                        TBackground( r, pattern )
{
}

//========================================================================
int main(void)
{
    TApp myApp;
    myApp.run();
    return 0;
}
