// MENUZ.CPP - Building menus with operator+ (Borland, 1992)
// Ported to modern magicius/tvision
//
// Original: demonstrates simplifying menu construction using an
// overloaded operator+ for TMenuItem objects.
// Note: modern tvision already defines operator+(TMenuItem&, TMenuItem&)
// in menus.h, so the custom definition is removed.

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
const unsigned cmAbout  = 100;
const unsigned cmGreet  = 101;
const unsigned cmDoIt   = 102;
const unsigned cmOne    = 103;
const unsigned cmTwo    = 104;
const unsigned cmThree  = 105;
const unsigned cmSimple = 106;
const unsigned cmRare   = 107;
const unsigned cmWell   = 108;
const unsigned cmCore   = 109;
const unsigned cmRoll   = 110;
const unsigned cmCafe   = 111;
const unsigned cmTimes  = 112;

// NOTE: The original had a custom operator+(TMenuItem&, TMenuItem&).
// Modern tvision already provides this in menus.h, so it is removed.

//========================================================================
class TApp : public TApplication {
public:
    TApp();
    static TMenuBar *initMenuBar( TRect r );
    void handleEvent( TEvent &event );
    void AboutBox();
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

    TMenuItem *top1 = new TMenuItem( "~\360~", kbAltSpace, new TMenu(
        *new TMenuItem( "~A~bout", cmAbout, kbNoKey, hcNoContext, 0 ) +
        newLine() +
        *new TMenuItem( "~E~xit", kbNoKey, new TMenu(
            *new TMenuItem( "~S~ave", cmQuit, kbNoKey, hcNoContext, 0 ) +
            *new TMenuItem( "A~b~andon", cmQuit, kbNoKey, hcNoContext, 0 ) +
            *new TMenuItem( "Just ~Q~uit", cmQuit, kbNoKey, hcNoContext, 0 )
        ) ) +
        *new TMenuItem( "~G~reeting", cmGreet, kbNoKey, hcNoContext, 0 )
    ), hcNoContext );

    TMenuItem *top2 = new TMenuItem( "~D~oIt!", cmDoIt, kbAltD, hcNoContext, 0 );

    TMenuItem *top3 = new TMenuItem( "~O~ptions", kbAltO, new TMenu(
        *new TMenuItem( "Option ~1~", cmOne, kbNoKey, hcNoContext, 0 ) +
        *new TMenuItem( "Option ~2~", cmTwo, kbNoKey, hcNoContext, 0 ) +
        *new TMenuItem( "Option ~3~", cmThree, kbNoKey, hcNoContext, 0 )
    ), hcNoContext );

    TMenuItem *top4 = new TMenuItem( "~C~omplex", kbAltC, new TMenu(
        *new TMenuItem( "~S~imple", cmSimple, kbNoKey, hcNoContext, 0 ) +
        *new TMenuItem( "~M~edium", kbNoKey, new TMenu(
            *new TMenuItem( "~R~are", cmRare, kbNoKey, hcNoContext, 0 ) +
            *new TMenuItem( "~W~ell", cmWell, kbNoKey, hcNoContext, 0 )
        ) ) +
        *new TMenuItem( "~H~ard", kbNoKey, new TMenu(
            *new TMenuItem( "~C~ore", cmCore, kbNoKey, hcNoContext, 0 ) +
            *new TMenuItem( "~R~ock", kbNoKey, new TMenu(
                *new TMenuItem( "&~R~oll", cmRoll, kbNoKey, hcNoContext, 0 ) +
                *new TMenuItem( "~C~afe", cmCafe, kbNoKey, hcNoContext, 0 )
            ) ) +
            *new TMenuItem( "~T~imes", cmTimes, kbNoKey, hcNoContext, 0 )
        ) )
    ) );

    return( new TMenuBar ( r, new TMenu( *top1 + *top2 + *top3 + *top4 ) ) );
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
                AboutBox();
                clearEvent( event );
                break;
            case cmGreet:
                messageBox( "\003Howdy!", mfInformation | mfOKButton );
                break;
            case cmDoIt:
                messageBox( "\003Just do it!", mfInformation | mfOKButton );
                break;
            case cmOne:
                messageBox( "\003One", mfInformation | mfOKButton );
                break;
            case cmTwo:
                messageBox( "\003Two", mfInformation | mfOKButton );
                break;
            case cmThree:
                messageBox( "\003Three", mfInformation | mfOKButton );
                break;
            case cmSimple:
                messageBox( "\003Simple!", mfInformation | mfOKButton );
                break;
            case cmRare:
                messageBox( "\003Medium Rare", mfInformation | mfOKButton );
                break;
            case cmWell:
                messageBox( "\003Medium Well", mfInformation | mfOKButton );
                break;
            case cmCore:
                messageBox( "\003Hard Core", mfInformation | mfOKButton );
                break;
            case cmRoll:
                messageBox( "\003Hard Rock&Roll", mfInformation | mfOKButton );
                break;
            case cmCafe:
                messageBox( "\003Hard Rock Cafe", mfInformation | mfOKButton );
                break;
            case cmTimes:
                messageBox( "\003Hard Times", mfInformation | mfOKButton );
                break;
            default:
                messageBox( "\003handling error", mfError | mfOKButton );
        }
        clearEvent( event );
    }
}

//------------------------------------------------------------------------
void TApp::AboutBox()
{
    TDialog *pd = new TDialog( TRect( 0, 0, 35, 12 ), "About" );
    if( pd )
    {
        pd->insert( new TStaticText( TRect( 1, 2, 34, 7 ),
                "\003Turbo Vision\n\003\n"
                "\003Simplifying complex menus\n\003\n"
                "\003Borland Technical Support"));
        pd->insert( new TButton( TRect( 3, 9, 32, 11), "~O~k",
                cmOK, bfDefault ) );
        pd->options |= ofCentered;
        deskTop->execView( pd );
    }
    destroy( pd );
}

//========================================================================
int main( void )
{
    TApp tv6App;
    tv6App.run();
    return 0;
}
