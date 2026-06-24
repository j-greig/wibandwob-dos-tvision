// LISTBOX.CPP - Creating a TListBox (Borland, 1992)
// Ported to modern magicius/tvision
//
// Original: demonstrates creating a simple list box inserted into
// a dialog box, displaying the Turbo Vision class list.

#define Uses_TApplication
#define Uses_TBackground
#define Uses_TButton
#define Uses_TKeys
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TListBox
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TRect
#define Uses_TScrollBar
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TStringCollection
#include <tvision/tv.h>

#include <cstdlib>

//========================================================================
//  global data
//------------------------------------------------------------------------
const unsigned cmAbout = 100;
const unsigned cmList  = 101;

static const char *theList[] = {
    "fpbase",             "fpstream",           "ifpstream",
    "Int11trap",          "iopstream",          "ipstream",
    "MsgBoxText",         "ofpstream",          "opstream",
    "otstream",           "pstream",            "TApplication",
    "TBackground",        "TBufListEntry",      "TButton",
    "TChDirDialog",       "TCheckBoxes",        "TCluster",
    "TCollection",        "TColorDialog",       "TColorDisplay",
    "TColorGroup",        "TColorGroupList",    "TColorItem",
    "TColorItemList",     "TColorSelector",     "TCommandSet",
    "TCrossRef",          "TDeskInit",          "TDeskTop",
    "TDialog",            "TDirCollection",     "TDirEntry",
    "TDirListBox",        "TDisplay",           "TDrawBuffer",
    "TEditor",            "TEditWindow",        "TEventQueue",
    "TFileCollection",    "TFileDialog",        "TFileEditor",
    "TFileInfoPane",      "TFileInputLine",     "TFileList",
    "TFrame",             "TGroup",             "THelpFile",
    "THelpIndex",         "THelpTopic",         "THelpViewer",
    "THelpWindow",        "THistInit",          "THistory",
    "THistoryViewer",     "THistoryWindow",     "THWMouse",
    "TIndicator",         "TInputLine",         "TLabel",
    "TListBox",           "TListViewer",        "TMemo",
    "TMenu",              "TMenuBar",           "TMenuBox",
    "TMenuItem",          "TMenuView",          "TMonoSelector",
    "TMouse",             "TNSCollection",      "TNSSortedCollection",
    "TObject",            "TPalette",           "TParagraph",
    "TParamText",         "TPoint",             "TPReadObjects",
    "TProgInit",          "TProgram",           "TPWObj",
    "TPWrittenObjects",   "TRadioButtons",      "TRect",
    "TResourceCollection","TResourceFile",      "TScreen",
    "TScrollBar",         "TScroller",          "TSItem",
    "TSortedCollection",  "TSortedListBox",     "TStaticText",
    "TStatusDef",         "TStatusItem",        "TStatusLine",
    "TStreamable",        "TStreamableClass",   "TStreamableTypes",
    "TStrIndexRec",       "TStringCollection",  "TStringList",
    "TStrListMaker",      "TSubMenu",           "TSystemError",
    "TTerminal",          "TTextDevice",        "TView",
    "TVMemMgr",           "TWindow",            "TWindowInit"
};

int numStrings = sizeof( theList ) / sizeof( theList[0] );

//========================================================================
//  class definitions
//------------------------------------------------------------------------
class TApp : public TApplication {
public:
    TApp();
    static TMenuBar *initMenuBar( TRect r );
    void handleEvent( TEvent &event );
    void AboutDialog();
    void ListDialog();
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
        new TMenuItem( "~L~ist", cmList, kbAltL, hcNoContext, 0 )
        ) ) ) );
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
                clearEvent( event );
                break;
            case cmList:
                ListDialog();
                clearEvent( event );
                break;
        }
    }
}

//------------------------------------------------------------------------
void TApp::AboutDialog()
{
    TDialog *pd = new TDialog( TRect( 0, 0, 35, 12 ), "About" );
    if( pd )
    {
        pd->options |= ofCentered;
        pd->insert( new TStaticText( TRect( 1, 2, 34, 7 ),
                "\003Turbo Vision Example\n\003\n"
                "\003Creating a TListBox\n\003\n"
                "\003Borland Technical Support" ) );
        pd->insert( new TButton( TRect( 3, 9, 32, 11 ), "~O~k",
                                cmOK, bfDefault ) );
        deskTop->execView( pd );
    }
    destroy( pd );
}

//------------------------------------------------------------------------
void TApp::ListDialog()
{
    TStringCollection *theCollection = new TStringCollection( 100, 10 );

    for( int i=0; i<numStrings; i++ )
        theCollection->insert( newStr( theList[i] ) );

    TDialog *pd = new TDialog( TRect( 0, 0, 51, 20 ), "TV Classes" );
    pd->options |= ofCentered;

    TScrollBar *listScroller = new TScrollBar( TRect( 47, 2, 48, 17 ) );
    TListBox *listBox = new TListBox( TRect( 3, 2, 47, 17 ), 2, listScroller );
    listBox->newList( theCollection );

    pd->insert( listBox );
    pd->insert( listScroller );

    if( validView( pd ) )
        deskTop->execView( pd );

    destroy( pd );
}

//========================================================================
int main(void)
{
    TApp myApp;
    myApp.run();
    return 0;
}
