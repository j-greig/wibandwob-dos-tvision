// POPUPX.CPP - Modal popup menu with HeapView gadget (Borland, 1992)
// Ported to modern magicius/tvision
//
// Original: demonstrates a modal popup menu using TMenuBox with execView,
// plus a HeapView gadget from GADGETS.CPP. HeapView is stubbed out
// (DOS-specific farcoreleft/farheapcheck removed) to show a static display.
// GADGETS.CPP has been merged into this single file.

#define Uses_TKeys
#define Uses_TEvent
#define Uses_TGroup
#define Uses_MsgBox
#define Uses_TApplication
#define Uses_TButton
#define Uses_TMenuView
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TMenuBox
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TStaticText
#define Uses_TRect
#define Uses_TView
#define Uses_TDrawBuffer
#include <tvision/tv.h>

#include <cstdlib>
#include <cstring>
#include <cctype>

//========================================================================
// Stubbed THeapView (from GADGETS.CPP) -- DOS heap functions removed
//========================================================================
class THeapView : public TView
{
public:
    THeapView( TRect& r );
    virtual void draw();
    void update();
private:
    long oldMem;
    long newMem;
    char heapStr[16];
    long heapSize();
};

THeapView::THeapView(TRect& r) : TView( r )
{
    oldMem = 0;
    newMem = heapSize();
}

void THeapView::draw()
{
    TDrawBuffer buf;
    TColorAttr c = getColor(2);
    buf.moveChar(0, ' ', c, size.x);
    buf.moveStr(0, heapStr, c);
    writeLine(0, 0, size.x, 1, buf);
}

void THeapView::update()
{
    if( (newMem = heapSize()) != oldMem )
    {
        oldMem = newMem;
        drawView();
    }
}

long THeapView::heapSize()
{
    // Stub: DOS farcoreleft/farheapcheck not available.
    // Return a fixed value for display purposes.
    snprintf(heapStr, sizeof(heapStr), "%12ld", 999999L);
    return 999999L;
}

//========================================================================
//  global data
//------------------------------------------------------------------------
const unsigned cmAbout = 100;
const unsigned cmPopup = 101;
const unsigned cmOne   = 102;
const unsigned cmTwo   = 103;

//========================================================================
class TMyMenuBox : public TMenuBox {
public:
    TMyMenuBox( const TRect& bounds, TMenu *aMenu, TMenuView *aParentMenu) :
        TMenuBox( bounds, aMenu, aParentMenu) {
        state |= sfShadow;
        options |= ofPreProcess;
    }
    void handleEvent( TEvent& event );
    ushort execute();
};

void TMyMenuBox::handleEvent( TEvent& event )
{
    // This function is never called when TMyMenuBox is modal
    (void)event;
}

ushort TMyMenuBox::execute()
{
    int result;
    result = TMenuView::execute();
    return result;
}


class TApp : public TApplication {
public:
    TApp();
    ~TApp() { delete heap; }

    static TMenuBar *initMenuBar( TRect r );
    void handleEvent( TEvent &event );
    THeapView *heap;

    void AboutDialog();
    void Popup();
    void idle();
};


void TApp::idle()
{
    TProgram::idle();
    heap->update();
}

//========================================================================
TApp::TApp() : TProgInit( TApplication::initStatusLine,
                    TApp::initMenuBar, TApplication::initDeskTop )
{
    TRect r = getExtent();
    r.a.x = r.b.x - 13;
    r.a.y = r.b.y - 1;
    heap = new THeapView( r );
    insert( heap );
}

//------------------------------------------------------------------------
TMenuBar *TApp::initMenuBar( TRect r )
{
    r.b.y = r.a.y + 1;
    return( new TMenuBar( r, new TMenu(
        *new TMenuItem( "~A~bout", cmAbout, kbAltA, hcNoContext, 0,
        new TMenuItem( "P~o~pup", cmPopup, kbAltO, hcNoContext, 0 )
        ))));
}

//------------------------------------------------------------------------
void TApp::handleEvent( TEvent &event )
{
    if( event.what == evCommand )
    {
        switch( event.message.command )
        {
            case cmAbout:
                AboutDialog();
                clearEvent( event );
                break;
            case cmPopup:
                Popup();
                clearEvent( event );
                break;
            case cmOne:
                messageBox( "Item 1 selected", mfOKButton );
                clearEvent( event );
                break;
            case cmTwo:
                messageBox( "Item 2 selected", mfOKButton );
                clearEvent( event );
                break;
            default:
                break;
        }
    }
    TApplication::handleEvent( event );
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
                "\003Creating a Modal Popup Menu\n\003\n"
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
            *new TMenuItem( "Item ~1~", cmOne, kbAltG, hcNoContext, "Alt-G",
            new TMenuItem ( "Item ~2~", cmTwo, kbAltH, hcNoContext, "Alt-H",
            new TMenuItem ( "E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X"
        ) ) ) );

    TMyMenuBox *mb = new TMyMenuBox( bounds, theMenu, 0 );
    mb->options |= ofCentered;

    int result = 0;
    if( validView( mb ) )
    {
        result = deskTop->execView( mb );
        destroy( mb );
    }
    // Note: do not delete theMenu -- it is owned by mb after construction

    // Create an event for the TApp handleEvent
    TEvent event;
    event.what = evCommand;
    event.message.command = result;
    putEvent( event );
    clearEvent( event );
}

//========================================================================
int main(void)
{
    TApp myApp;
    myApp.run();
    return 0;
}
