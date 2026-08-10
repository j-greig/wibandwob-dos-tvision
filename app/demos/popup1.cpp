// POPUP1.CPP - Creating a popup menu (Borland, 1992)
// Ported to modern magicius/tvision
//
// Original: demonstrates creating a simple popup menu using TMenuBox,
// both modal (execView) and modeless (insert) approaches.

#define Uses_MsgBox
#define Uses_TApplication
#define Uses_TButton
#define Uses_TKeys
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TMenuBox
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TStaticText
#include <tvision/tv.h>

#include <cstdlib>

//========================================================================
const unsigned cmAbout = 100;
const unsigned cmPopup = 101;
const unsigned cmOne   = 102;
const unsigned cmTwo   = 103;

//========================================================================
class TApp : public TApplication {
public:
    TApp();
    static TMenuBar *initMenuBar( TRect r );
    void handleEvent( TEvent &event );
    void AboutDialog();
    void Popup();
};

//========================================================================
TApp::TApp() : TProgInit( &TApplication::initStatusLine,
                    &TApp::initMenuBar, &TApplication::initDeskTop )
{
}

//------------------------------------------------------------------------
TMenuBar *TApp::initMenuBar( TRect r )
{
    r.b.y = r.a.y + 1;
    return( new TMenuBar( r, new TMenu(
        *new TMenuItem( "~A~bout", cmAbout, kbAltA, hcNoContext, 0,
        new TMenuItem( "~P~opup", cmPopup, kbAltP, hcNoContext, 0 )
        ))));
}

//------------------------------------------------------------------------
void TApp::handleEvent( TEvent &event )
{
    TApplication::handleEvent( event );
    if( event.what == evCommand )
    {
        switch( event.message.command )
        {
            case cmAbout:
                AboutDialog();
                break;
            case cmPopup:
                Popup();
                break;
            case cmOne:
                messageBox( "Item 1 selected", mfOKButton );
                break;
            case cmTwo:
                messageBox( "Item 2 selected", mfOKButton );
                break;
        }
        clearEvent( event );
    }
}

//------------------------------------------------------------------------
void TApp::AboutDialog()
{
    TDialog *pd = new TDialog( TRect( 0, 0, 35, 12 ), "About" );
    if( validView( pd ) )
    {
        pd->options |= ofCentered;
        pd->insert ( new TStaticText( TRect( 1, 2, 34, 7 ),
                "\003Turbo Vision Example\n\003\n"
                "\003Creating a Popup Menu\n\003\n"
                "\003Borland Technical Support" ) );
        pd->insert( new TButton( TRect( 3, 9, 32, 11 ), "~O~k",
                                cmOK, bfDefault ) );
        deskTop->execView( pd );
    }
    destroy( pd );
}

//------------------------------------------------------------------------
void TApp::Popup()
{
    TRect bounds( 0, 0, 0, 0 );

    TMenu *theMenu = new TMenu (
            *new TMenuItem( "Item ~1~", cmOne, kbAltI, hcNoContext, "Alt-1",
            new TMenuItem ( "Item ~2~", cmTwo, kbAltT, hcNoContext, "Alt-2",
            new TMenuItem ( "E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X"
        ) ) ) );

    TMenuBox *mb = new TMenuBox( bounds, theMenu, 0 );

    mb->options |= ofCentered;

    if( validView( mb ) )
        deskTop->insert( mb );
}

//========================================================================
int main(void)
{
    TApp myApp;
    myApp.run();
    return 0;
}
