// EXPDEMO.CPP - Exploding Window/Dialog Demo
// Original: Eric Woodruff (CIS: 72134,1150), 1993
// Ported to modern magicius/tvision by Wib & Wob
//
// Reimplements TExplodeWindow/TExplodeDialog from the missing texplode.h.
// The "explode" effect: window grows from center point outward in steps,
// each step flushing to screen with a configurable delay.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

#define Uses_MsgBox
#define Uses_TApplication
#define Uses_TButton
#define Uses_TCheckBoxes
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TEvent
#define Uses_TGroup
#define Uses_TInputLine
#define Uses_TKeys
#define Uses_TLabel
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TRadioButtons
#define Uses_TRect
#define Uses_TScreen
#define Uses_TSItem
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#define Uses_TView
#define Uses_TWindow

#include <tvision/tv.h>

// --- Exploding animation helper ---
// Grows a view from its center to full size in N steps.
// delayMs controls speed: 0 = fastest, 50 = leisurely.

static void doExplodeAnimation(TView *view, TGroup *owner, int delayMs)
{
    if (!view || !owner) return;

    TRect finalBounds = view->getBounds();
    int cx = (finalBounds.a.x + finalBounds.b.x) / 2;
    int cy = (finalBounds.a.y + finalBounds.b.y) / 2;
    int fullW = finalBounds.b.x - finalBounds.a.x;
    int fullH = finalBounds.b.y - finalBounds.a.y;

    // Number of animation steps (more steps = smoother)
    int steps = (fullW > fullH ? fullW : fullH) / 2;
    if (steps < 3) steps = 3;
    if (steps > 12) steps = 12;

    int sleepUs = delayMs * 300; // scale to microseconds per step
    if (sleepUs < 800) sleepUs = 800;

    for (int i = 1; i <= steps; i++)
    {
        int w = fullW * i / steps;
        int h = fullH * i / steps;
        if (w < 3) w = 3;
        if (h < 2) h = 2;

        TRect r;
        r.a.x = cx - w / 2;
        r.a.y = cy - h / 2;
        r.b.x = r.a.x + w;
        r.b.y = r.a.y + h;

        // Clamp to stay within final bounds area
        if (r.a.x < finalBounds.a.x) { r.b.x += finalBounds.a.x - r.a.x; r.a.x = finalBounds.a.x; }
        if (r.a.y < finalBounds.a.y) { r.b.y += finalBounds.a.y - r.a.y; r.a.y = finalBounds.a.y; }
        if (r.b.x > finalBounds.b.x) r.b.x = finalBounds.b.x;
        if (r.b.y > finalBounds.b.y) r.b.y = finalBounds.b.y;

        view->locate(r);
        owner->redraw();
        TScreen::flushScreen();
        std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
    }

    // Ensure final position is exact
    view->locate(finalBounds);
    owner->redraw();
    TScreen::flushScreen();
}

// --- TExplodeWindow ---

class TExplodeWindow : public TWindow
{
    int explodeDelay;
public:
    TExplodeWindow(const TRect& r, TStringView title, short aNumber) :
        TWindowInit(&TExplodeWindow::initFrame),
        TWindow(r, title, aNumber), explodeDelay(15) {}

    void MakeItExplode(int delay) { explodeDelay = delay; }

    // Hook: animate after being inserted into a group
    virtual void setState(ushort aState, Boolean enable)
    {
        TWindow::setState(aState, enable);
        if ((aState & sfVisible) && enable && owner)
            doExplodeAnimation(this, owner, explodeDelay);
    }
};

class TExplodeDialog : public TDialog
{
    int explodeDelay;
public:
    TExplodeDialog(const TRect& r, TStringView title) :
        TWindowInit(&TExplodeDialog::initFrame),
        TDialog(r, title), explodeDelay(15) {}

    void MakeItExplode(int delay) { explodeDelay = delay; }

    virtual void setState(ushort aState, Boolean enable)
    {
        TDialog::setState(aState, enable);
        if ((aState & sfVisible) && enable && owner)
            doExplodeAnimation(this, owner, explodeDelay);
    }
};

// --- Command constants ---

const int cmAboutCmd        = 100;
const int cmScreenSize      = 101;
const int cmDemo1           = 102;
const int cmDemo2           = 103;
const int cmCloseTileable   = 106;

// --- Application ---

class TMyApplication : public TApplication
{
public:
    TMyApplication();
    static TMenuBar    *initMenuBar(TRect r);
    static TStatusLine *initStatusLine(TRect r);
    virtual void handleEvent(TEvent &event);
private:
    void Demo1(), Demo2();
};

// Function that returns True when there is a tileable view on the desktop.
static Boolean isTileable(TView *p, void *)
{
    return (p->options & ofTileable) ? True : False;
}

TMyApplication::TMyApplication() :
    TProgInit(&TMyApplication::initStatusLine, &TMyApplication::initMenuBar,
              &TMyApplication::initDeskTop)
{
    TEvent event;
    event.what = evCommand;
    event.message.command = cmAboutCmd;
    putEvent(event);
}

TMenuBar *TMyApplication::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;

    TSubMenu& MiscMenu = *new TSubMenu("~\xF0~", 0) +
        *new TMenuItem("~A~bout...", cmAboutCmd, kbNoKey) +
         newLine() +
        *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt+X");

    TSubMenu& DemoMenu = *new TSubMenu("~E~xplode Demos", 0) +
        *new TMenuItem("~T~ExplodeWindow demo", cmDemo1, kbF2,
                        hcNoContext, "F2") +
        *new TMenuItem("T~E~xplodeDialog demo", cmDemo2, kbF3,
                        hcNoContext, "F3");

    TSubMenu& WindowMenu = *new TSubMenu("~W~indows", 0) +
        *new TMenuItem("~S~ize/move", cmResize, kbCtrlF5, hcNoContext, "Ctrl+F5") +
        *new TMenuItem("~Z~oom", cmZoom, kbF5, hcNoContext, "F5") +
        *new TMenuItem("~N~ext", cmNext, kbF6, hcNoContext, "F6") +
        *new TMenuItem("~P~revious", cmPrev, kbShiftF6, hcNoContext, "Shift+F6") +
        *new TMenuItem("~C~lose", cmClose, kbAltF3, hcNoContext, "Alt+F3") +
         newLine() +
        *new TMenuItem("C~l~ose all tileable windows", cmCloseTileable, kbNoKey) +
        *new TMenuItem("~T~ile", cmTile, kbNoKey) +
        *new TMenuItem("C~a~scade", cmCascade, kbNoKey);

    return new TMenuBar(r, MiscMenu + DemoMenu + WindowMenu);
}

TStatusLine *TMyApplication::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    return new TStatusLine(r,
      *new TStatusDef(0, 0xFFFF) +
        *new TStatusItem(NULL, kbF10, cmMenu) +
        *new TStatusItem("~Alt+X~ Exit", kbAltX, cmQuit) +
        *new TStatusItem("~Alt+F3~ Close", kbAltF3, cmClose) +
        *new TStatusItem("~F2~ Window", kbF2, cmDemo1) +
        *new TStatusItem("~F3~ Dialog", kbF3, cmDemo2));
}

void TMyApplication::handleEvent(TEvent &event)
{
    TApplication::handleEvent(event);

    if (event.what == evCommand)
    {
        switch (event.message.command)
        {
            case cmAboutCmd:
            {
                TExplodeDialog *aboutBox = new TExplodeDialog(
                    TRect(0, 0, 46, 11), "About");
                aboutBox->options |= ofCentered;
                aboutBox->MakeItExplode(30);

                aboutBox->insert(new TStaticText(TRect(2, 2, 45, 7),
                    "\003TExplodeWindow/TExplodeDialog Demo\n"
                    "\003Ported to modern tvision\n\003\n"
                    "\003Original by Eric Woodruff\n"
                    "\003CIS ID: 72134,1150\n"));

                TButton *b = new TButton(TRect(11, 8, 23, 10),
                    "O~K~", cmOK, bfDefault);
                b->options |= ofCenterX;
                aboutBox->insert(b);

                aboutBox->selectNext(False);
                if (validView(aboutBox))
                {
                    deskTop->execView(aboutBox);
                    destroy(aboutBox);
                }
                break;
            }

            case cmCloseTileable:
                while (deskTop->firstThat(isTileable, 0))
                    message(deskTop, evCommand, cmClose, NULL);
                break;

            case cmTile:
                deskTop->tile(deskTop->getExtent());
                break;

            case cmCascade:
                deskTop->cascade(deskTop->getExtent());
                break;

            case cmDemo1:
                Demo1();
                break;

            case cmDemo2:
                Demo2();
                break;

            default:
                return;
        }
        clearEvent(event);
    }
}

void TMyApplication::Demo1()
{
    // Randomly select an initial size and position.
    TRect r(0, 0, 20 + rand() % 53, 6 + rand() % 16);
    r.move(rand() % 30, rand() % 10);

    TExplodeWindow *xw = new TExplodeWindow(r, "Demo Exploding Window", 0);
    xw->MakeItExplode(rand() % 50);

    // Make it tileable and let it grow and shrink with the screen mode.
    xw->options |= ofTileable;
    xw->growMode = gfGrowAll | gfGrowRel;

    r = xw->getExtent();
    r.grow(-1, -1);

    TStaticText *s = new TStaticText(r, "This is a test.");
    s->growMode = gfGrowHiX | gfGrowHiY;
    xw->insert(s);

    if (validView(xw))
        deskTop->insert(xw);
}

void TMyApplication::Demo2()
{
    TExplodeDialog *dlg = new TExplodeDialog(TRect(4, 0, 76, 22),
        "General Information");

    if (!dlg)
        return;

    dlg->options = ofCentered;

    TButton *b = new TButton(TRect(30, 19, 42, 21), "O~K~", cmOK, bfDefault);
    b->options = ofCenterX;
    dlg->insert(b);

    dlg->insert(new TStaticText(TRect(15, 2, 56, 4),
        "\003TExplodeWindow and TExplodeDialog Classes\n"
        "\003by Eric Woodruff"));

    dlg->insert(new TStaticText(TRect(3, 5, 69, 11),
        "\003This demo shows dialog boxes and windows\n"
        "\003that can be tiled, cascaded, and closed.\n\003\n"
        "\003Originally these had an exploding animation\n"
        "\003effect using custom DOS screen timing."));

    // Radio buttons
    TRadioButtons *rb = new TRadioButtons(TRect(3, 12, 26, 18),
        new TSItem("Radio #1",
        new TSItem("Radio #2",
        new TSItem("Radio #3",
        new TSItem("Radio #4",
        new TSItem("Radio #5",
        new TSItem("Radio #6", 0)))))));
    dlg->insert(rb);
    dlg->insert(new TLabel(TRect(2, 11, 20, 12), "~R~adio Buttons", rb));

    // Checkboxes
    TCheckBoxes *c = new TCheckBoxes(TRect(28, 12, 51, 18),
        new TSItem("Check #1",
        new TSItem("Check #2",
        new TSItem("Check #3",
        new TSItem("Check #4",
        new TSItem("Check #5",
        new TSItem("Check #6", 0)))))));
    dlg->insert(c);
    dlg->insert(new TLabel(TRect(27, 11, 45, 12), "~C~heckboxes", c));

    // Input lines
    TInputLine *i = new TInputLine(TRect(53, 13, 72, 14), 50);
    dlg->insert(i);
    dlg->insert(new TLabel(TRect(52, 12, 63, 13), "Input #~1~", i));

    i = new TInputLine(TRect(53, 16, 72, 17), 50);
    dlg->insert(i);
    dlg->insert(new TLabel(TRect(52, 15, 63, 16), "Input #~2~", i));

    dlg->selectNext(False);
    if (validView(dlg))
    {
        deskTop->execView(dlg);
        destroy(dlg);
    }
}

int main()
{
    TMyApplication MyApp;
    MyApp.run();
    return 0;
}
