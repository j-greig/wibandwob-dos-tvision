// RADIO.CPP - Radio buttons demo (Borland, 1992)
// Ported to modern magicius/tvision
//
// Original: demonstrates breaking apart TSItems into several sub-groups
// to create large radio button lists.

#define Uses_MsgBox
#define Uses_TApplication
#define Uses_TEventQueue
#define Uses_TEvent
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
#define Uses_TMenu
#define Uses_TDialog
#define Uses_TStaticText
#define Uses_TButton
#define Uses_TSItem
#define Uses_TRadioButtons
#define Uses_TProgram
#include <tvision/tv.h>

#include <cstdlib>

const int cmRadioDialog = 100;

class TMyApp : public TApplication
{
public:
    TMyApp();
    static TMenuBar *initMenuBar( TRect );
    static TStatusLine *initStatusLine( TRect );
    void handleEvent(TEvent& event);
    void RadioDialog(void);
};


TMyApp::TMyApp() :
    TProgInit( &initStatusLine,
               &initMenuBar,
               &initDeskTop
             )
{
}


//--------------------------------------------------------------------------
void TMyApp::RadioDialog(void)
{
    TDialog *pd = new TDialog( TRect( 2,1,78,22), " Radio Buttons ");
    if( validView( pd ) )
    {

     TSItem *J1 = new TSItem("Nietsche",
                  new TSItem("Kant",
                  new TSItem("Daly",
                  new TSItem("Socrates",
                  new TSItem("Descartes",
                  new TSItem("Hui",
                  new TSItem("GLarkin",
                  new TSItem("Plato",
                  new TSItem("Babet",
                  new TSItem("Farrell",
                  new TSItem("Thoreau",
                  new TSItem("Krull",
                  new TSItem("Pippon",
                  new TSItem("Bird",
                  new TSItem("Jordan", 0 )))))))))))))));

     TSItem *J2 = new TSItem("Nietsche2",
                  new TSItem("Kant2",
                  new TSItem("Daly2",
                  new TSItem("Socrates2",
                  new TSItem("Descartes2",
                  new TSItem("Hui2",
                  new TSItem("GLarkin2",
                  new TSItem("Plato2",
                  new TSItem("Babet2",
                  new TSItem("Farrell2",
                  new TSItem("Thoreau2",
                  new TSItem("Krull2",
                  new TSItem("Pippon2",
                  new TSItem("Bird2",
                  new TSItem("Jordan2", J1 )))))))))))))));

     TSItem *J3 = new TSItem("Nietsche3",
                  new TSItem("Kant3",
                  new TSItem("Daly3",
                  new TSItem("Socrates3",
                  new TSItem("Descartes3",
                  new TSItem("Hui3",
                  new TSItem("GLarkin3",
                  new TSItem("Plato3",
                  new TSItem("Babet3",
                  new TSItem("Farrell3",
                  new TSItem("Thoreau3",
                  new TSItem("Krull3",
                  new TSItem("Pippon3",
                  new TSItem("Bird3",
                  new TSItem("Jordan3", J2 )))))))))))))));

     TSItem *J4 = new TSItem("Nietsche4",
                  new TSItem("Kant4",
                  new TSItem("Daly4",
                  new TSItem("Socrates4",
                  new TSItem("Descartes4",
                  new TSItem("Hui4",
                  new TSItem("GLarkin4",
                  new TSItem("Plato4",
                  new TSItem("Babet4",
                  new TSItem("Farrell4",
                  new TSItem("Thoreau4",
                  new TSItem("Krull4",
                  new TSItem("Pippon4",
                  new TSItem("Bird4",
                  new TSItem("Jordan4", J3 )))))))))))))));

      TView *RadioButtons = new TRadioButtons( TRect(2,1,73,16), J4 );

      pd->insert( RadioButtons );

      deskTop->execView( pd );

    }

    destroy(pd);
}


void TMyApp::handleEvent(TEvent& event)
{
    TApplication::handleEvent( event );

    if( event.what == evCommand )
    {
        switch( event.message.command)
        {
            case cmRadioDialog:
                RadioDialog();
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
        *new TStatusItem( "~Alt-R~ Radio Buttons", kbAltR, cmRadioDialog)
        );
}


TMenuBar *TMyApp::initMenuBar( TRect r )
{
    r.b.y = r.a.y + 1;

    TMenuItem *two =
        new TMenuItem("~E~xit", cmQuit, kbAltX);

    TMenuItem *one =
        new TMenuItem("~\xF0~", kbAltSpace,
            new TMenu( *new TMenuItem("~R~adio Buttons", cmRadioDialog, kbAltR)),
              hcNoContext, two);

    return ( new TMenuBar( r, new TMenu( *one ) )  );
}


int main()
{
    TMyApp myApp;
    myApp.run();
    return 0;
}
