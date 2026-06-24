// TBACK - Custom desktop background art
// Ported from Borland HELLOMEM.CPP
// Overrides TBackground::draw() to fill desktop with random chars

#define Uses_TKeys
#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TStaticText
#define Uses_TButton
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TMemo
#define Uses_TBackground
#define Uses_TScrollBar
#define Uses_TListBox
#define Uses_TStringCollection
#define Uses_TProgram
#define Uses_TGroup
#include <tvision/tv.h>
#include <stdlib.h>

const unsigned aboutCmd = 100;

class TMyBackground : public TBackground
{
public:
  TMyBackground(const TRect& b) : TBackground(b,' ') { }
  void draw();
};

void TMyBackground::draw()
{
  TRect rect = getClipRect();
  static const char chars[] = { '#', '=', '.', ' ' };

  for(int i = rect.a.x; i < rect.b.x; i++)
  {
    for(int j = rect.a.y; j < rect.b.y; j++)
    {
      int r = rand() % 4;
      writeChar(i, j, chars[r], 1, 1);
    }
  }
}

class TMyDeskTop : public TDeskTop
{
public:
  TMyDeskTop(TRect b) : TDeskTop(b), TDeskInit(TMyDeskTop::initBackground) {}
  static TBackground* initBackground(TRect r)
  {
    return new TMyBackground(r);
  }
};

class THelloApp : public TApplication
{
public:
    THelloApp();
    virtual void handleEvent( TEvent& event );
    static TMenuBar *initMenuBar( TRect );
    static TStatusLine *initStatusLine( TRect );
    static TDeskTop *initDeskTop( TRect b )
    {
      return new TMyDeskTop(b);
    }

private:
    void aboutBox();
};

THelloApp::THelloApp() :
    TProgInit( &THelloApp::initStatusLine,
               &THelloApp::initMenuBar,
               &THelloApp::initDeskTop
             )
{
}

void THelloApp::aboutBox()
{
    TStringCollection *sp = new TStringCollection(50,0);
    sp->insert((void*)"One");
    sp->insert((void*)"Two");
    sp->insert((void*)"Three");
    sp->insert((void*)"Four");
    sp->insert((void*)"Five");
    sp->insert((void*)"Six");
    sp->insert((void*)"Seven");
    sp->insert((void*)"Eight");
    sp->insert((void*)"Nine");
    sp->insert((void*)"Ten");
    sp->insert((void*)"Eleven");
    sp->insert((void*)"Twelve");
    sp->insert((void*)"Thirteen");
    sp->insert((void*)"Fourteen");
    sp->insert((void*)"Fifteen");
    sp->insert((void*)"Sixteen");
    sp->insert((void*)"Seventeen");
    sp->insert((void*)"Eighteen");
    sp->insert((void*)"Nineteen");
    TDialog *d = new TDialog(TRect( 15, 1, 65, 23 ), "About" );
    d->insert( new TStaticText( TRect( 5,1,45,2), "\x003Sample Application" ));
    d->insert( new TStaticText( TRect( 5,3,45,4), "\x003Using TMemo and "
                                                  "overriding" ));
    d->insert( new TStaticText( TRect( 5,5,45,6), "\x003TBackground draw "
                                                  "function" ));
    TScrollBar *vSMemo = new TScrollBar( TRect( 46, 8, 47, 13) );
    d->insert(vSMemo);
    d->insert( new TMemo( TRect(5, 8, 45, 13), 0, vSMemo, 0, 255));
    TScrollBar *vS = new TScrollBar( TRect( 46, 14, 47, 20) );
    d->insert(vS);
    TListBox *lb = new TListBox( TRect(5, 14, 45, 20), 2, vS);
    lb->newList(sp);
    d->insert(lb);
    deskTop->execView( d );
}

void THelloApp::handleEvent( TEvent& event )
{
    TApplication::handleEvent( event );
    if( event.what == evCommand )
        {
        switch( event.message.command )
            {
            case aboutCmd:
                aboutBox();
                clearEvent( event );
                break;
            default:
                break;
            }
        }
}

TMenuBar *THelloApp::initMenuBar( TRect r )
{
    r.b.y = r.a.y+1;

    return new TMenuBar( r,
      *new TSubMenu( "~H~ello", kbAltH ) +
        *new TMenuItem( "~A~bout...", aboutCmd, kbAltG ) +
         newLine() +
        *new TMenuItem( "E~x~it", cmQuit, cmQuit, hcNoContext, "Alt-X" )
        );
}

TStatusLine *THelloApp::initStatusLine( TRect r )
{
    r.a.y = r.b.y-1;
    return new TStatusLine( r,
        *new TStatusDef( 0, 0xFFFF ) +
            *new TStatusItem( "~Alt-X~ Exit", kbAltX, cmQuit ) +
            *new TStatusItem( 0, kbF10, cmMenu )
            );
}

int main()
{
    THelloApp helloWorld;
    helloWorld.run();
    return 0;
}
