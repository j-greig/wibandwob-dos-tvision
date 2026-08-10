// MENUNEST.CPP - Nested menus demo (Borland, 1992)
// Ported to modern magicius/tvision
//
// Original: demonstrates creating nested menus using TSubMenu
// and the two TMenuItem constructors.

#define Uses_MsgBox
#define Uses_TApplication
#define Uses_TButton
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TKeys
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#include <tvision/tv.h>

//========================================================================
const unsigned cmAbout = 100;

//========================================================================
class TV6 : public TApplication {
public:
    TV6();
    static TMenuBar *initMenuBar( TRect r );
    void handleEvent( TEvent &event );
    void AboutBox();
};

//========================================================================
TV6::TV6() : TProgInit( &TApplication::initStatusLine,
                    &TV6::initMenuBar, &TApplication::initDeskTop )
{
}

//------------------------------------------------------------------------
TMenuBar *TV6::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;
    TMenuBar *newMenu = new TMenuBar( r,
        *new TSubMenu( "~\360~", kbAltSpace ) +
        *new TMenuItem( "~A~bout", cmAbout, kbAltA, hcNoContext ) +
        newLine() +
        *new TMenuItem( "Exit", 0, new TMenu(
            *new TMenuItem( "Exit & ~S~ave", cmQuit, kbAltX, hcNoContext, 0,
            new TMenuItem( "Exit & ~A~bandon", cmQuit, kbAltY, hcNoContext, 0,
            new TMenuItem( "Just ~Q~uit", cmQuit, kbAltZ, hcNoContext, 0,
            new TMenuItem( "~N~ext Level", 0, new TMenu(
               *new TMenuItem( "~O~ne", cmQuit, kbAltX, hcNoContext, 0,
               new TMenuItem( "~T~wo", cmQuit, kbAltX ))))
        )))), hcNoContext)
        );
    return newMenu;
}

//------------------------------------------------------------------------
void TV6::handleEvent( TEvent &event )
{
    TApplication::handleEvent( event );
    if( event.what == evCommand )
        switch( event.message.command )
        {
            case cmAbout:
                AboutBox();
                clearEvent( event );
                break;
        }
}

//------------------------------------------------------------------------
void TV6::AboutBox()
{
    TDialog *pd = new TDialog( TRect( 0, 0, 35, 12 ), "About" );
    if( pd )
    {
        pd->insert( new TStaticText( TRect( 1, 2, 34, 7 ),
                "\003Turbo Vision\n\003\n"
                "\003Creating a nested menu\n\003\n"
                "\003Borland Technical Support"));
        pd->insert( new TButton( TRect( 3, 9, 32, 11), "~O~key-d~O~key",
                cmOK, bfDefault ) );
        pd->options |= ofCentered;
        deskTop->execView( pd );
    }
    destroy( pd );
}

//========================================================================
int main( void )
{
    TV6 tv6App;
    tv6App.run();
    return 0;
}
