// TITLEBAR - Title bar above menu
// Ported from dsktop_ttlbar: BARTEST.CPP + TITLELIN.CPP
// TitleLine is a TView that draws a centered title with custom palette

#define Uses_MsgBox
#define Uses_TEvent
#define Uses_TApplication
#define Uses_TKeys
#define Uses_TRect
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TProgram
#define Uses_TGroup
#include <tvision/tv.h>
#include <string.h>

// ---- TitleLine (inlined from TITLELIN.CPP / titlelin.h) ----

#define cpTitleLine "\x09\x0D"    // bright white/yellow on blue

class TitleLine : public TView
{
public:
    TitleLine( const TRect& r, const char *aTitle );
    void setTitle( const char *s );
    virtual TPalette& getPalette() const;
    virtual void draw();
private:
    char title[80];
};

TitleLine::TitleLine( const TRect& r, const char *aTitle )
  : TView( r )
{
  strncpy( title, aTitle, sizeof(title)-1 );
  title[sizeof(title)-1] = '\0';
}

void TitleLine::setTitle( const char *s )
{
  strncpy( title, s, sizeof(title)-1 );
  title[sizeof(title)-1] = '\0';
  drawView();
}

TPalette& TitleLine::getPalette() const
{
  static TPalette palette( cpTitleLine, sizeof(cpTitleLine)-1 );
  return palette;
}

void TitleLine::draw()
{
  TDrawBuffer b;
  ushort color = getColor( 0x0201 );
  b.moveChar( 0, ' ', color, size.x );
  b.moveCStr( (size.x - (int)strlen(title))/2, title, color );
  writeLine( 0, 0, size.x, 1, b );
}

// ---- Shell application (from BARTEST.CPP) ----

const unsigned cmAbout = 100;
const unsigned cmTest  = 101;

class Shell : public TApplication {
public:
  Shell();
  static TMenuBar *initMenuBar( TRect r );
  static TStatusLine *initStatusLine( TRect r );
  void handleEvent( TEvent& event );
  void idle();
private:
  TitleLine *t;
  void About();
  void Test();
};

Shell::Shell() : TProgInit (
      &Shell::initStatusLine,
      &Shell::initMenuBar,
      &Shell::initDeskTop
      )
{
  TRect r = deskTop->getBounds();
  ++r.a.y;
  deskTop->setBounds( r );

  r = getExtent();
  r.b.y = r.a.y + 1;
  t = new TitleLine( r, "TitleLine Demo" );
  insert(t);
}

TMenuBar *Shell::initMenuBar( TRect r )
{
  ++r.a.y;
  r.b.y = r.a.y + 1;

  TMenuItem& mI =
    *new TMenuItem( "~T~est", cmTest, kbNoKey, hcNoContext );

  return new TMenuBar( r, new TMenu( mI ) );
}

TStatusLine *Shell::initStatusLine( TRect r )
{
  r.a.y = r.b.y - 1;

  TStatusLine *sL = new TStatusLine( r,
    *new TStatusDef(0, 0xFFFF) +
      *new TStatusItem( 0, kbF10, cmMenu ) +
      *new TStatusItem( "~Alt-X~ Quit", kbAltX, cmQuit ) );

  return sL;
}

void Shell::handleEvent( TEvent &event )
{
  TApplication::handleEvent(event);

  if (event.what == evCommand)
  {
    switch (event.message.command)
    {
      case cmAbout:
        About();
        break;
      case cmTest:
        Test();
        break;
      default:
        break;
    }
  }
}

void Shell::idle()
{
  TProgram::idle();

  if( deskTop->current == 0
  && !menuBar->getState( sfSelected ) )
  {
    TEvent event;
    event.what = evCommand;
    event.message.command = cmMenu;
    putEvent( event );
  }
}

void Shell::About()
{
  messageBox( "\003TitleLine Demo",
    mfInformation | mfOKButton );
}

void Shell::Test()
{
  t->setTitle( "TitleLine Demo: ~Test~" );
  messageBox( "\003Function Test() is executing",
    mfInformation | mfOKButton );
  t->setTitle( "TitleLine Demo" );
}

int main()
{
  TEvent init;
  init.what = evCommand;
  init.message.command = cmAbout;

  Shell shell;
  shell.putEvent(init);
  shell.run();
  return 0;
}
