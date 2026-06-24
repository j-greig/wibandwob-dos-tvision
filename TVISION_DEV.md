# Turbo Vision Development Guide

Patterns, porting recipes, and widget cookbook for building with the modern [magicius/tvision](https://github.com/magicius/tvision) C++ library. Learned from porting 20 vintage Borland demos (1991-1996) to arm64 macOS.

---

## Build

Demos build standalone (no tvterm/micropolis submodules needed):

```bash
cmake -B build/demos -S app/demos
cmake --build build/demos -j8
```

Each `.cpp` in `app/demos/` auto-builds as `demo_<name>`. Add a new file, rebuild, done.

---

## Porting Borland TV to Modern tvision

Mechanical changes that apply to virtually every vintage TV source:

| Old (Borland) | New (modern tvision) | Why |
|---|---|---|
| `#include <tv.h>` | `#include <tvision/tv.h>` | Subdirectory includes |
| `#pragma hdrstop` | remove | Borland precompiled headers |
| custom `operator+(TMenuItem&, TMenuItem&)` | remove | Already in `menus.h` |
| `char *s = "hello"` | `const char *s = "hello"` | C++14 string literals |
| `void main()` | `int main()` | Standard C++ |
| `const cmFoo = 100` | `const unsigned cmFoo = 100` | No implicit int |
| `TDrawBuffer *tb = new TDrawBuffer` | `TDrawBuffer tb;` | Stack allocate |
| `writeBuf(x,y,w,h,buf)` | `writeLine(x,y,w,h,buf)` | Modern draw API |
| `random(n)` / `randomize()` | `rand() % n` / remove | `<cstdlib>` |
| `delay(ms)` | `std::this_thread::sleep_for(std::chrono::milliseconds(ms))` | `<chrono>` + `<thread>` |
| `itoa(n,buf,10)` | `snprintf(buf,sizeof(buf),"%d",n)` | Standard C |
| `getch()` | remove or use TV events | No `<conio.h>` |
| `_bios_timeofday()` | `std::chrono::steady_clock` | No `<bios.h>` |
| `class far TRect` | `class TRect` | No `far` keyword |
| `register` keyword | remove | C++14 deprecated |
| DOS headers (`dos.h`, `conio.h`, `mem.h`, `strstrea.h`, `iomanip.h`, `graphics.h`, `alloc.h`, `io.h`) | remove, use C++ equivalents | Platform-specific |
| `coreleft()` / `farcoreleft()` | stub or remove | DOS memory API |

### Missing `Uses_*` declarations

Modern tvision is stricter. Always add:
- `Uses_TProgram` (needed for `TProgram::deskTop`, `appPalette`, etc)
- `Uses_TSubMenu` (if using `TSubMenu`)
- `Uses_TGroup` (if using `owner`, `forEach`, etc)
- `Uses_ipstream` / `Uses_opstream` (if overriding `readItem`/`writeItem`)
- `Uses_TDrawBuffer` (if calling `moveChar`, `moveStr`, etc)

---

## Widget Patterns

### Animated view via idle()

The core pattern for any live-updating widget. Used by: spinner, clock, gauge bars, progress bars.

```cpp
// 1. The widget: a TView with update() and draw()
class TMyWidget : public TView {
    int state;
public:
    TMyWidget(TRect r) : TView(r) { state = 0; }

    void draw() {
        TDrawBuffer buf;
        char c = getColor(2);
        buf.moveChar(0, ' ', c, (short)size.x);
        // ... render state into buf ...
        writeLine(0, 0, (short)size.x, 1, buf);
    }

    void update() {
        state++;  // advance animation
        drawView();
    }
};

// 2. The app: override idle() to tick the widget
class TMyApp : public TApplication {
    TMyWidget *widget;
    std::chrono::steady_clock::time_point lastTick;
public:
    TMyApp() : TProgInit(&initStatusLine, &initMenuBar, &initDeskTop) {
        TRect r = getExtent();
        r.b.y = r.a.y + 1;
        r.b.x = r.a.x + 2;
        widget = new TMyWidget(r);
        insert(widget);  // insert into app, overlays menu area
        lastTick = std::chrono::steady_clock::now();
    }

    void idle() {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick);
        if (ms.count() >= 55) {  // ~18Hz, classic BIOS tick rate
            lastTick = now;
            widget->update();
        }
        TApplication::idle();
    }
};
```

**Placement matters**: `insert(widget)` on the app itself puts it above the desktop (overlaying menu bar area). `deskTop->insert(widget)` puts it in the desktop workspace.

### Spinner (rotating ASCII indicator)

Four-frame animation: `|` `/` `-` `\`

```cpp
void TSpinViewer::update() {
    switch(spinner) {
    case '|':  spinner = '/';  break;
    case '/':  spinner = '-';  break;
    case '-':  spinner = '\\'; break;
    case '\\': spinner = '|';  break;
    }
    drawView();
}
```

### Progress bar (thermometer)

A `TView` that fills with block chars based on percentage. Color inversion shows the filled portion:

```cpp
void TProgressBar::draw() {
    TDrawBuffer nbuf;
    uchar colorNormal = (uchar)getColor(1);
    uchar fore = colorNormal >> 4;
    uchar colorHiLite = fore + ((colorNormal - (fore << 4)) << 4);
    nbuf.moveChar(0, backChar, colorNormal, (ushort)size.x);
    // Draw percentage text centered
    nbuf.moveStr(numOffset, percentStr, colorNormal);
    // Invert attributes for filled portion
    for (int i = 0; i < (int)curWidth; i++)
        nbuf.putAttribute(i, colorHiLite);
    writeLine(0, 0, (short)size.x, 1, nbuf);
}
```

### Gauge bar (value-driven fill)

Extends `TParamText`. Fills a char buffer proportionally:

```cpp
void TGageBar::setValue(int aValue) {
    int charsToFill = (int)((long)aValue * size.x / maxValue);
    for (int j = 0; j < size.x; j++)
        gageBuffer[j] = (j < charsToFill) ? '#' : ' ';
    TParamText::setText(gageBuffer);
}
```

### Custom desktop background

Override `TBackground::draw()` and provide a custom `TDeskTop` factory:

```cpp
class TMyBackground : public TBackground {
public:
    TMyBackground(const TRect& b) : TBackground(b, ' ') {}
    void draw() {
        TRect rect = getClipRect();
        for (int i = rect.a.x; i < rect.b.x; i++)
            for (int j = rect.a.y; j < rect.b.y; j++)
                writeChar(i, j, "# =."[rand() % 4], 1, 1);
    }
};

class TMyDeskTop : public TDeskTop {
public:
    TMyDeskTop(TRect b) : TDeskTop(b), TDeskInit(TMyDeskTop::initBackground) {}
    static TBackground* initBackground(TRect r) {
        return new TMyBackground(r);
    }
};

// In app class:
static TDeskTop *initDeskTop(TRect b) { return new TMyDeskTop(b); }
```

### Title bar (chrome above menu)

A `TView` inserted into the app that pushes the desktop down:

```cpp
// In app constructor:
TRect r = deskTop->getBounds();
++r.a.y;                         // shrink desktop down one line
deskTop->setBounds(r);

r = getExtent();
r.b.y = r.a.y + 1;              // one row at the very top
titleLine = new TitleLine(r, "My App Title");
insert(titleLine);               // insert into app, above menu
```

### Exploding window animation

Grow a window from center to full size in steps:

```cpp
void doExplodeAnimation(TView *view, TGroup *owner, int delayMs) {
    TRect finalBounds = view->getBounds();
    int cx = (finalBounds.a.x + finalBounds.b.x) / 2;
    int cy = (finalBounds.a.y + finalBounds.b.y) / 2;
    int steps = max(3, min(12, max(width, height) / 2));

    for (int i = 1; i <= steps; i++) {
        int w = fullW * i / steps;
        int h = fullH * i / steps;
        TRect r(cx - w/2, cy - h/2, cx - w/2 + w, cy - h/2 + h);
        view->locate(r);
        owner->redraw();
        TScreen::flushScreen();
        std::this_thread::sleep_for(std::chrono::microseconds(delayMs * 300));
    }
    view->locate(finalBounds);
}
```

Hook it via `setState(sfVisible)` override.

### Hint status line

Override `TStatusLine::hint()` to show context-sensitive descriptions:

```cpp
class THintStatusLine : public TStatusLine {
    // Map help context IDs to description strings
    const char* hint(ushort aHelpCtx) {
        const char *p = lookupHint(aHelpCtx);
        return p ? p : TStatusLine::hint(aHelpCtx);
    }
};
```

### Modeless dialog with cancel

Insert (don't execView) for modeless. Poll events manually:

```cpp
TProgram::deskTop->insert(pd);  // modeless

for (int i = 0; i <= total; i++) {
    progressBar->update(i);
    idle();                      // pump events
    // check for cancel
    TEvent event;
    pd->getEvent(event);
    pd->handleEvent(event);
    if (event.what == evCommand && event.message.command == cmCancel)
        break;
}
destroy(pd);
```

### Custom palette

Override `getPalette()` to change app-wide colors:

```cpp
TPalette& TMyApp::getPalette() const {
    static TPalette color(cpMyColor, sizeof(cpMyColor) - 1);
    static TPalette bw(cpMyBW, sizeof(cpMyBW) - 1);
    static TPalette mono(cpMyMono, sizeof(cpMyMono) - 1);
    static TPalette *p[] = { &color, &bw, &mono };
    return *(p[appPalette]);
}
```

### Message passing between views

Broadcast messages from one view, catch in another:

```cpp
// Sender (e.g. TListBox):
message(TProgram::deskTop, evBroadcast, cmNewData, dataPtr);

// Receiver (any TView with evBroadcast in eventMask):
void handleEvent(TEvent &event) {
    TView::handleEvent(event);
    if (event.what == evBroadcast && event.message.command == cmNewData) {
        curdata = (const char *)event.message.infoPtr;
        drawView();
    }
}
```

Receiver must set `eventMask |= evBroadcast` in constructor.

---

## TView::draw() Essentials

The `draw()` method is called whenever the view needs repainting. Key API:

```cpp
void MyView::draw() {
    TDrawBuffer buf;
    ushort color = getColor(1);             // get color from palette

    buf.moveChar(0, ' ', color, size.x);    // fill with spaces
    buf.moveStr(0, "Hello", color);         // write string at offset 0
    buf.moveCStr(0, "~H~ello", color);      // write with hotkey highlighting
    buf.putAttribute(3, hiColor);           // change attribute at position

    writeLine(0, 0, size.x, 1, buf);        // write one line
    writeLine(0, 0, size.x, size.y, buf);   // write same line to all rows
    writeChar(x, y, ch, color, count);      // write char directly (no buffer)
}
```

---

## Demo Inventory

20 ported demos in `app/demos/`, built as `build/demos/demo_<name>`:

| Demo | Origin | Visual interest | Shows |
|---|---|---|---|
| gagedemo | gadgets | sine-wave gauge bars + spinner | idle() animation, TParamText, custom palette |
| expdemo | xpwndg | exploding windows | setState() hooks, TScreen::flushScreen() |
| tback | borland | random desktop texture | TBackground override, custom TDeskTop factory |
| progbar | gadgets | thermometer progress bar | color inversion, modeless dialog, cancel |
| clickclock | gadgets | 12/24h toggling clock | idle() tick, mouse double-click, time formatting |
| titlebar | dsktop | chrome title bar | TView above menu, dynamic title updates |
| ezhint | dsktop | context-sensitive hints | TStatusLine::hint() override |
| color1 | borland | desktop pattern colors | TBackground customization |
| desklogo | borland | desktop background | TBackground draw override |
| expdemo | xpwndg | exploding windows/dialogs | tile, cascade, window management |
| dlgdraw | borland | dynamic dialog text | TDrawBuffer in dialogs |
| dlgmode | borland | modal vs modeless | execView vs insert |
| listbox | borland | list box in dialog | TListBox, TStringCollection |
| menunest | borland | nested menus | TSubMenu cascading |
| menuz | borland | menu construction | operator+ for menus |
| popup1 | borland | pop-up menus | TMenuBox |
| popupx | borland | pop-up + gadgets | TMenuBox + heapview |
| radio | borland | radio buttons | TRadioButtons, TCheckBoxes |
| statbox | borland | live-updating display | TCalcDisplay, delay loop |
| tvtxtdm2 | gui | text after exit | file I/O, cleanup |

---

## Key Architecture Notes

- **TApplication** owns: statusLine, menuBar, deskTop. Insert into app itself for chrome that overlays the menu area.
- **TDeskTop** is the workspace. Insert windows/dialogs here for normal use.
- **idle()** is your animation loop. Called when event queue is empty. Throttle with chrono to ~18Hz (55ms) or faster.
- **execView()** = modal (blocks until closed). **insert()** = modeless (returns immediately).
- **TScreen::flushScreen()** forces immediate screen update (useful for animations outside normal event loop).
- **Palette** is inherited hierarchically: TApplication → TWindow → TView. Each level maps indices to parent indices. `getColor(n)` resolves the chain.
- **eventMask** controls which events a view receives. Add `evBroadcast` to receive broadcast messages.
- **growMode** flags (`gfGrowAll`, `gfGrowHiX`, etc) control how views resize when the terminal resizes.

---

## Source

Demos ported from [Sergio Sigala's Turbo Vision resources](http://www.sigala.it/sergio/tvision/resources.html), a collection of C++ sources found on the net from the Borland Turbo Vision era (1991-1996).
