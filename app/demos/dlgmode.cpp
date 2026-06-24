// DLGMODE.CPP - Modal vs. Modeless dialog box demo (Borland, 1992, Pete Williams)
// Ported to modern magicius/tvision
//
// Original: demonstrates coding modal vs modeless dialog boxes
// and retrieving data from a modeless dialog on close.

#define Uses_TKeys
#define Uses_TRect
#define Uses_TEvent
#define Uses_TLabel
#define Uses_TButton
#define Uses_TInputLine
#define Uses_TDialog
#define Uses_TMenuBar
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TStaticText
#define Uses_MsgBox
#define Uses_TDeskTop
#define Uses_TApplication
#define Uses_TProgram
#define Uses_TWindow
#include <tvision/tv.h>

#include <cstdio>

const int cmModalBox    = 100;
const int cmModelessBox = 101;
const int cmFoo         = 102;

class TMode : public TApplication
{
public:
    TMode() : TProgInit( initStatusLine, initMenuBar, initDeskTop) { }
    void handleEvent( TEvent& event );
    static TMenuBar *initMenuBar( TRect r );
};


class TModalBox : public TDialog
{
public:
    TModalBox();
};


class TModelessBox : public TDialog
{
    static struct modelessData {
        char name[20];
        char rank[20];
    } data;

public:
    TModelessBox();
    virtual void handleEvent( TEvent& event );
};


//
// Code for main() function.
//
int main(void)
{
    TMode modalApp;
    modalApp.run();
    return 0;
}


//
// Code for the TMode object (Application object.)
//

TMenuBar *TMode::initMenuBar( TRect r )
{
    r.b.y =  r.a.y + 1;

    return (new TMenuBar( r, new TMenu(
        *new TMenuItem( "~M~odal Dialog  ", cmModalBox, kbNoKey,
                        hcNoContext, 0,
         new TMenuItem( "Mode~L~ess Dialog", cmModelessBox, kbNoKey,
                        hcNoContext, 0))
        ))
    );
}

void TMode::handleEvent( TEvent& event )
{
    TView *t;

    TApplication::handleEvent(event);

    if (event.what == evCommand)
        {
        switch (event.message.command)
            {
            case cmModalBox:
                t = (TView *) validView( new TModalBox() );
                if( t )
                    deskTop->execView( t );
                destroy( t );
                break;

            case cmModelessBox:
                t = (TView *) validView( new TModelessBox() );
                if( t )
                    deskTop->insert( t );
                break;
            }
        }
}


//
// Code for the TModalBox object.
//

TModalBox::TModalBox() :
    TDialog(TRect(0, 0, 46, 11), "About"),
    TWindowInit( initFrame )
{
    insert( new TStaticText(TRect(3,2,43,10),
        "\003Dialog Box Demo\n \n"
        "\003Modal & Modeless Dialog Boxes\n \n"
        "\003Copyright (C) 1992 Borland International" ));
    insert(new TButton(TRect(18, 8, 28, 10), "~O~K", cmOK, bfDefault));
    options |= ofCentered;
}



//
// Code for the TModelessBox object.
//

TModelessBox::modelessData TModelessBox::data = { "", "" };

TModelessBox::TModelessBox() :
    TDialog( TRect(0, 0, 35, 9), "Modeless Dialog" ),
    TWindowInit( initFrame )
{
    TInputLine *control = new TInputLine( TRect(10,2,32,3), 20);
    insert(control);
    insert( new TLabel( TRect(2,2,9,3), "~N~ame:", control) );

    control = new TInputLine( TRect(10,4,32,5), 20);
    insert(control);
    insert( new TLabel( TRect(2,4,9,5), "~R~ank:", control) );

    insert( new TButton( TRect(6,6,16,8), "~O~K", cmOK, bfDefault) );
    insert( new TButton( TRect(19,6,29,8), "~C~ancel", cmCancel, bfNormal) );
    selectNext( False );

    options |= ofCentered;
    setData(&data);
}


void TModelessBox::handleEvent( TEvent& event )
{
    if( event.what != evKeyboard || event.keyDown.keyCode != kbEsc )
        TDialog::handleEvent(event);

    if( event.what == evCommand )
        {
        switch( event.message.command )
            {
            case cmOK:
            case cmYes:
                getData(&data);
                // fall through
            case cmCancel:
            case cmNo:
                close();
                clearEvent(event);
                break;
            }
        }
}
