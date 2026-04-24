/* Minimal TVision listbox scroll test.
 *
 * Purpose: rule out architecture. If arrow keys don't scroll here, the
 * issue is environmental (terminal key forwarding, tvision build, etc.)
 * rather than in our viewer.
 *
 * Build:
 *   cd ~/Repos/wibandwob-dos-tvision
 *   cmake --build build --target tvision_listbox_min
 *
 * Run:
 *   ./build/app/tvision_listbox_min
 *
 * Tab focuses the listbox if it doesn't start focused. Arrow keys
 * should move the selection; PgDn/PgUp page; Home/End jump to ends.
 */

#define Uses_TApplication
#define Uses_TWindow
#define Uses_TDeskTop
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TRect
#define Uses_TListBox
#define Uses_TScrollBar
#define Uses_TStringCollection
#define Uses_TKeys
#include <tvision/tv.h>

#include <cstdio>
#include <cstring>

class MinApp : public TApplication {
public:
    MinApp() : TProgInit(&MinApp::initStatusLine,
                         &MinApp::initMenuBar,
                         &MinApp::initDeskTop)
    {
        TRect wr(10, 3, 50, 20);
        TWindow *w = new TWindow(wr, "Listbox Scroll Test", wnNoNumber);

        TRect content = w->getExtent();
        content.grow(-1, -1);  // inside the frame

        // Right-column scrollbar; the listbox fills the rest.
        TRect sbr(content.b.x - 1, content.a.y, content.b.x, content.b.y);
        TScrollBar *sb = new TScrollBar(sbr);
        w->insert(sb);

        TRect lbr(content.a.x, content.a.y, content.b.x - 1, content.b.y);
        TListBox *lb = new TListBox(lbr, 1, sb);

        TStringCollection *items = new TStringCollection(30, 10);
        for (int i = 0; i < 30; ++i) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Item %02d", i);
            items->insert(newStr(buf));
        }
        lb->newList(items);
        lb->focusItem(0);
        w->insert(lb);

        deskTop->insert(w);
        lb->select();  // force focus like in our viewer
    }

    static TMenuBar *initMenuBar(TRect r) {
        r.b.y = r.a.y + 1;
        return new TMenuBar(r,
            *new TSubMenu("~F~ile", kbAltF) +
                *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X"));
    }
    static TStatusLine *initStatusLine(TRect r) {
        r.a.y = r.b.y - 1;
        return new TStatusLine(r,
            *new TStatusDef(0, 0xFFFF) +
                *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit));
    }
};

int main() {
    MinApp app;
    app.run();
    return 0;
}
