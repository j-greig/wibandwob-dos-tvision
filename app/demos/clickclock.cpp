// CLICKCLOCK - 12/24h toggling clock
// Ported from gadgets_clkclk: CLIKCLOK.CPP + CLIKCLOK.H
// TClickClock is a TView updated via idle(). Double-click toggles 12/24h.

#define Uses_TEvent
#define Uses_TRect
#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TApplication
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TKeys
#define Uses_TProgram
#define Uses_TGroup
#include <tvision/tv.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// ---- TClickClock (inlined from CLIKCLOK.H + CLIKCLOK.CPP) ----

class TClickClock : public TView
{
public:
    TClickClock( const TRect& r, Boolean TwelveH );
    virtual void draw();
    virtual void update();
    virtual void handleEvent( TEvent& event );

private:
    char lastTime[16];
    char curTime[16];
    Boolean TwelveHour;
};

TClickClock::TClickClock( const TRect& r, Boolean TwelveH ) : TView( r )
{
    strcpy( lastTime, "          " );
    strcpy( curTime, "          " );
    TwelveHour = TwelveH;
}

void TClickClock::handleEvent( TEvent& event )
{
    if( event.what == evMouseDown ) {
        if( event.mouse.eventFlags & meDoubleClick ) {
            TwelveHour = Boolean( !TwelveHour );
            update();
        }
    }
    TView::handleEvent( event );
}

void TClickClock::draw()
{
    TDrawBuffer buf;
    char c = getColor(2);

    buf.moveChar(0, ' ', c, size.x);
    buf.moveStr(0, curTime, c);
    writeLine(0, 0, size.x, 1, buf);
}

void TClickClock::update()
{
    time_t t = time(0);
    char *date = ctime(&t);

    date[19] = '\0';

    if( TwelveHour == True ) {
        char ampm[3] = "am";
        char hour[3] = "";
        int inthr;
        hour[0] = date[11];
        hour[1] = date[12];
        inthr = atoi( hour );

        if( inthr == 12 ) {
            strcpy( ampm, "pm" );
        }
        else if( inthr > 12 ) {
            inthr -= 12;
            strcpy( ampm, "pm" );
        }
        else if( inthr == 0 ) {
            inthr += 12;
        }

        snprintf( hour, sizeof(hour), "%d", inthr );
        if( inthr > 9 ) {
            date[11] = hour[0];
            date[12] = hour[1];
        }
        else {
            date[11] = ' ';
            date[12] = hour[0];
        }

        strcpy( curTime, &date[11] );
        strcat( curTime, ampm );
    }
    else {
        strcpy( curTime, &date[11] );
    }

    if( strcmp(lastTime, curTime) ) {
        drawView();
        strcpy(lastTime, curTime);
    }
}

// ---- App wrapper ----

class TClockApp : public TApplication
{
public:
    TClockApp();
    static TMenuBar *initMenuBar( TRect r );
    static TStatusLine *initStatusLine( TRect r );
    void idle();
private:
    TClickClock *clock;
};

TClockApp::TClockApp() :
    TProgInit( &TClockApp::initStatusLine,
               &TClockApp::initMenuBar,
               &TClockApp::initDeskTop )
{
    TRect r = getExtent();
    r.a.x = r.b.x - 12;
    r.b.y = r.a.y + 1;
    clock = new TClickClock( r, True );
    insert( clock );
    clock->update();
}

TMenuBar *TClockApp::initMenuBar( TRect r )
{
    r.b.y = r.a.y + 1;
    return new TMenuBar( r,
        *new TSubMenu( "~F~ile", kbAltF ) +
            *new TMenuItem( "E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X" )
    );
}

TStatusLine *TClockApp::initStatusLine( TRect r )
{
    r.a.y = r.b.y - 1;
    return new TStatusLine( r,
        *new TStatusDef( 0, 0xFFFF ) +
            *new TStatusItem( "~Alt-X~ Exit", kbAltX, cmQuit ) +
            *new TStatusItem( "Double-click clock to toggle 12/24h", kbNoKey, cmNo ) +
            *new TStatusItem( 0, kbF10, cmMenu )
    );
}

void TClockApp::idle()
{
    TApplication::idle();
    clock->update();
}

int main()
{
    TClockApp app;
    app.run();
    return 0;
}
