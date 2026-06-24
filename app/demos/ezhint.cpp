// EZHINT - Easy Hint Status Line Demo
// Original: Patrick Reilly (CIS: 70274,161)
// Ported to modern magicius/tvision by Wib & Wob
//
// Demonstrates a THintStatusLine that shows context-sensitive hints
// in the status bar as you navigate menus. Each menu item has a
// help context ID mapped to a descriptive string via the Strings class.

#include <cstring>
#include <cstdlib>

#define Uses_TApplication
#define Uses_TStatusLine
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TSubMenu
#define Uses_TKeys
#define Uses_TProgram
#define Uses_TRect
#define Uses_TDeskTop

#include <tvision/tv.h>

// =========================================================================
// Strings - lightweight string lookup by ID (array-based only)
// Original supported file-based string resources too, but the demo
// only uses the StrRef array path, so we keep it simple.
// =========================================================================

const ushort srNull = 0xFFFF;

struct StrRef
{
    ushort id;
    const char *str;
};

class Strings
{
public:
    Strings() : items(0), count(0) {}
    Strings(const Strings& s) : items(0), count(0)
    {
        if (s.items && s.count > 0)
        {
            items = new StrRef*[s.count];
            memcpy(items, s.items, s.count * sizeof(StrRef*));
            count = s.count;
        }
    }
    ~Strings() { clear(); }

    const char* operator[](ushort id) { return get(id); }

    const char* get(ushort id)
    {
        int n;
        if (items && search(id, n))
            return items[n]->str;
        return 0;
    }

    void clear()
    {
        delete[] items;
        items = 0;
        count = 0;
    }

    Boolean load(StrRef *arg)
    {
        clear();
        if (!arg) return False;

        int limit;
        for (limit = 0; arg[limit].id != srNull; limit++)
            ;

        items = new StrRef*[limit];
        if (!items) return False;

        for (int u = 0; u < limit; u++)
        {
            int n;
            search(arg[u].id, n);
            if (n < count)
                memmove(items + n + 1, items + n, (count - n) * sizeof(StrRef*));
            items[n] = &arg[u];
            count++;
        }
        return True;
    }

protected:
    StrRef **items;
    int count;

    Boolean search(ushort id, int& pos)
    {
        Boolean found = False;
        if (!count)
            pos = 0;
        else
        {
            int l = 0;
            int h = count - 1;
            while (l <= h)
            {
                int i = (l + h) >> 1;
                if (items[i]->id < id)
                    l = i + 1;
                else if (items[i]->id > id)
                    h = i - 1;
                else
                {
                    found = True;
                    l = i;
                    h = i - 1;
                }
            }
            pos = l;
        }
        return found;
    }
};

// =========================================================================
// Demo
// =========================================================================

const unsigned cmAbout   = 1000;
const unsigned cmFileNew = 1001;

const ushort hcSystem   = 1000;
const ushort hcAbout    = 1001;
const ushort hcFile     = 1002;
const ushort hcFileExit = 1003;
const ushort hcFileNew  = 1004;

static StrRef strRef[] = {
    { hcSystem,   "system commands" },
    { hcAbout,    "show version and copyright information" },
    { hcFile,     "file-management commands (Open, Save, etc)" },
    { hcFileExit, "exit the program" },
    { hcFileNew,  "create a new file in a new Edit window" },
    { srNull,     0 }
};

class THintStatusLine : public TStatusLine
{
public:
    THintStatusLine(const TRect& bounds, TStatusDef& aDefs) :
        TStatusLine(bounds, aDefs)
    {
        s.load(strRef);
    }

    virtual const char* hint(ushort aHelpCtx);

protected:
    Strings s;
};

const char* THintStatusLine::hint(ushort aHelpCtx)
{
    const char *p = s[aHelpCtx];
    if (p == 0)
        return TStatusLine::hint(aHelpCtx);
    else
        return p;
}

class TApp : public TApplication
{
public:
    TApp() : TProgInit(initStatusLine, initMenuBar, initDeskTop) {}
    static TMenuBar *initMenuBar(TRect);
    static TStatusLine *initStatusLine(TRect);
};

TMenuBar *TApp::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;

    TSubMenu& sub1 = *new TSubMenu("~\xF0~", kbAltSpace, hcSystem) +
        *new TMenuItem("~A~bout...", cmAbout, 0, hcAbout, 0, 0);

    TSubMenu& sub2 = *new TSubMenu("~F~ile", kbNoKey, hcFile) +
        *new TMenuItem("~N~ew", cmFileNew, kbNoKey, hcFileNew, 0) +
        newLine() +
        *new TMenuItem("E~x~it", cmQuit, kbAltX, hcFileExit, "Alt-X");

    TMenuBar *menuBar = new TMenuBar(r, new TMenu((TMenuItem&)(sub1 + sub2)));
    return menuBar;
}

TStatusLine *TApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;

    return new THintStatusLine(r,
        *new TStatusDef(hcSystem, hcFileNew) +
            *new TStatusItem("~F1~ Help", kbF1, cmHelp) +
            *new TStatusItem(0, kbAltX, cmQuit) +
            *new TStatusItem(0, kbAltF3, cmClose) +
            *new TStatusItem(0, kbF5, cmZoom) +
            *new TStatusItem(0, kbCtrlF5, cmResize) +
            *new TStatusItem(0, kbF10, cmMenu) +

        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~F1~ Help", kbF1, cmHelp) +
            *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
            *new TStatusItem(0, kbAltF3, cmClose) +
            *new TStatusItem(0, kbF5, cmZoom) +
            *new TStatusItem(0, kbCtrlF5, cmResize) +
            *new TStatusItem(0, kbF10, cmMenu)
    );
}

int main()
{
    TApp app;
    app.run();
    app.shutDown();
    return 0;
}
