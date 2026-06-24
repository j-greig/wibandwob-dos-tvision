// GAGEDEMO - Animated gauge bars + spinner
// Ported from gadgets_gagedemo: 6 source files merged into 1
// Uses sine waves to animate 3 gauge bars with phase offset 120 degrees

#define Uses_TKeys
#define Uses_TApplication
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TDialog
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TEvent
#define Uses_TProgram
#define Uses_TStaticText
#define Uses_TParamText
#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TGroup
#define Uses_MsgBox
#include <tvision/tv.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <chrono>

// ---- Custom palette (from GAGEDEMO.H) ----

#define cpMyColor \
             "\x11\x70\x78\x74\x20\x28\x24\x17\x1F\x1A\x31\x31\x1E\x71\x1F" \
        "\x37\x3F\x3A\x13\x13\x3E\x21\x3F\x70\x7F\x7A\x13\x13\x70\x7F\x7E" \
        "\x70\x7F\x7A\x13\x13\x70\x70\x7F\x7E\x20\x2B\x2F\x78\x2E\x70\x30" \
        "\x3F\x3E\x1F\x2F\x1A\x20\x72\x31\x31\x30\x2F\x3E\x31\x13\x38\x00" \
        "\x17\x1F\x1A\x71\x71\x1E\x17\x1F\x1E\x20\x2B\x2F\x78\x2E\x10\x30" \
        "\x3F\x3E\x70\x2F\x7A\x20\x12\x31\x31\x30\x2F\x3E\x31\x13\x38\x00" \
        "\x37\x3F\x3A\x13\x13\x3E\x30\x3F\x3E\x20\x2B\x2F\x78\x2E\x30\x70" \
        "\x7F\x7E\x1F\x2F\x1A\x20\x32\x31\x71\x70\x2F\x7E\x71\x13\x38\x00" \
        "\x37\x3F\x3A\x13\x13\x30\x3E\x1E"

#define cpMyBlackWhite \
             "\x07\x70\x78\x7f\x07\x07\x0f\x07\x0F\x07\x70\x70\x07\x70\x0F" \
        "\x07\x0F\x07\x70\x70\x07\x70\x0F\x70\x7F\x7F\x70\x07\x70\x07\x0F" \
        "\x70\x7F\x7F\x70\x07\x70\x70\x7F\x7F\x07\x0F\x0F\x78\x0F\x78\x07" \
        "\x0F\x0F\x0F\x70\x0F\x07\x70\x70\x70\x07\x70\x0F\x07\x07\x78\x00" \
        "\x07\x0F\x0F\x07\x70\x07\x07\x0F\x0F\x70\x78\x7F\x08\x7F\x08\x70" \
        "\x7F\x7F\x7F\x0F\x70\x70\x07\x70\x70\x70\x07\x7F\x70\x07\x78\x00" \
        "\x70\x7F\x7F\x70\x07\x70\x70\x7F\x7F\x07\x0F\x0F\x78\x0F\x78\x07" \
        "\x0F\x0F\x0F\x70\x0F\x07\x70\x70\x70\x07\x70\x0F\x07\x07\x78\x00" \
        "\x07\x0F\x07\x70\x70\x07\x0F\x70"

#define cpMyMonochrome \
             "\x08\x70\x70\x70\x07\x07\x07\x07\x0F\x07\x70\x70\x07\x70\x00" \
        "\x07\x0F\x07\x70\x70\x07\x70\x00\x70\x70\x70\x07\x07\x70\x07\x00" \
        "\x70\x70\x70\x07\x07\x70\x70\x70\x0F\x07\x07\x0F\x70\x0F\x70\x07" \
        "\x0F\x0F\x07\x70\x07\x07\x70\x07\x07\x07\x70\x0F\x07\x07\x70\x00" \
        "\x70\x70\x70\x07\x07\x70\x70\x70\x0F\x07\x07\x0F\x70\x0F\x70\x07" \
        "\x0F\x0F\x07\x70\x07\x07\x70\x07\x07\x07\x70\x0F\x07\x07\x70\x00" \
        "\x70\x70\x70\x07\x07\x70\x70\x70\x0F\x07\x07\x0F\x70\x0F\x70\x07" \
        "\x0F\x0F\x07\x70\x07\x07\x70\x07\x07\x07\x70\x0F\x07\x07\x70\x00" \
        "\x07\x0F\x07\x70\x70\x07\x0F\x70"

// ---- TSpinViewer (from SPINNER.H + SPINNER.CPP) ----

class TSpinViewer : public TView
{
protected:
    char spinner;
    virtual void draw();
public:
    TSpinViewer( TRect r );
    virtual void update(void);
};

TSpinViewer::TSpinViewer(TRect r) : TView(r)
{
    spinner = '|';
}

void TSpinViewer::draw()
{
    TDrawBuffer buf;
    char c = getColor(2);
    buf.moveChar(0, ' ', c, (short)size.x);
    buf.moveChar(0, spinner, c, 1);
    writeLine(0, 0, (short)size.x, 1, buf);
}

void TSpinViewer::update(void)
{
    switch(spinner)
    {
    case '|':
        spinner = '/';
        break;
    case '/':
        spinner = '-';
        break;
    case '-':
        spinner = '\\';
        break;
    case '\\':
        spinner = '|';
        break;
    default:
        spinner = 'X';
        break;
    }
    drawView();
}

// ---- TGageBar (from GAGEBAR.H + GAGEBAR.CPP) ----

#define FILLCHAR  '#'
#define HALFCHAR  '='
#define BLANKCHAR ' '

class TGageBar : public TParamText
{
public:
    TGageBar(const TRect& bounds);
    ~TGageBar();
    void setParams(int aValue, int aMax);
    void setValue(int aValue);
    int getValue(void);
protected:
    char *gageBuffer;
    int maxValue;
    int currentValue;
};

TGageBar::TGageBar(const TRect& bounds)
    : TParamText(bounds)
{
    gageBuffer = new char[size.x+1];
    gageBuffer[size.x] = '\0';
    for(int j = 0; j < size.x; j++)
        gageBuffer[j] = BLANKCHAR;
    options |= ofFramed;
}

TGageBar::~TGageBar()
{
    delete [] gageBuffer;
}

void TGageBar::setParams(int aValue, int aMax)
{
    maxValue = aMax;
    setValue(aValue);
}

void TGageBar::setValue(int aValue)
{
    int charsToFill = 0;
    currentValue = aValue;
    if(currentValue > maxValue)
        currentValue = maxValue;
    if(currentValue < 0)
        currentValue = 0;

    for(int j = 0; j < size.x; j++)
        gageBuffer[j] = BLANKCHAR;

    charsToFill = (int) ((long)currentValue * size.x / maxValue);
    for(int k = 0; k < charsToFill; k++)
        gageBuffer[k] = FILLCHAR;

    if( ((long)currentValue * size.x*10 / maxValue - charsToFill*10) >= 5 )
        if(charsToFill < size.x)
            gageBuffer[charsToFill] = HALFCHAR;

    TParamText::setText(gageBuffer);
}

int TGageBar::getValue(void)
{
    return currentValue;
}

// ---- TGageDlg (from GAGEDLG.H + GAGEDLG.CPP) ----

class TGageDlg : public TDialog
{
public:
    TGageDlg(const TRect& r, const char *str);
    void insertResources(void);
    void update(void);
protected:
    TStaticText *captionText;
    TStaticText *gage1Text;
    TStaticText *gage2Text;
    TStaticText *gage3Text;
    TGageBar    *gageBar1;
    TGageBar    *gageBar2;
    TGageBar    *gageBar3;
    TParamText  *gage1Value;
    TParamText  *gage2Value;
    TParamText  *gage3Value;
    int degrees;
};

TGageDlg::TGageDlg(const TRect& r, const char *str)
    : TDialog(r, str),
      TWindowInit(&TGageDlg::initFrame)
{
    TWindow::flags &= ~wfClose;
    options |= ofCentered;
    degrees = 0;

    captionText = new TStaticText(TRect(15,2,36,3), "Gage Readings");
    gage1Text   = new TStaticText(TRect(3,4,15,5),  "Gage #1");
    gage2Text   = new TStaticText(TRect(3,6,15,7),  "Gage #2");
    gage3Text   = new TStaticText(TRect(3,8,15,9),  "Gage #3");

    gageBar1 = new TGageBar(TRect(16,4,36,5));
    gageBar1->setParams(60, 200);
    gageBar2 = new TGageBar(TRect(16,6,36,7));
    gageBar2->setParams(120, 200);
    gageBar3 = new TGageBar(TRect(16,8,36,9));
    gageBar3->setParams(180, 200);

    gage1Value = new TParamText(TRect(37,4,44,5));
    gage1Value->setText("%3d", 60);
    gage2Value = new TParamText(TRect(37,6,44,7));
    gage2Value->setText("%3d", 120);
    gage3Value = new TParamText(TRect(37,8,44,9));
    gage3Value->setText("%3d", 180);
}

void TGageDlg::insertResources(void)
{
    this->insert(captionText);
    this->insert(gage1Text);
    this->insert(gage2Text);
    this->insert(gage3Text);
    this->insert(gageBar1);
    this->insert(gageBar2);
    this->insert(gageBar3);
    this->insert(gage1Value);
    this->insert(gage2Value);
    this->insert(gage3Value);
}

void TGageDlg::update(void)
{
    int temp_value;

    temp_value = 100 + (int)(100 * sin( (double) degrees/180*3.14159));
    gageBar1->setValue(temp_value);
    gage1Value->setText("%3d", temp_value);

    temp_value = 100 + (int)(100 * sin( (double) (degrees+120)/180*3.14159));
    gageBar2->setValue(temp_value);
    gage2Value->setText("%3d", temp_value);

    temp_value = 100 + (int)(100 * sin( (double) (degrees+240)/180*3.14159));
    gageBar3->setValue(temp_value);
    gage3Value->setText("%3d", temp_value);

    degrees += 2;
    if(degrees > 360)
        degrees = 0;
}

// ---- TDemoApp (from GAGEDEMO.CPP) ----

class TDemoApp : public TApplication
{
private:
    TGageDlg    *gageDialog;
    TSpinViewer *spinView;
    std::chrono::steady_clock::time_point lastTick;
public:
    TDemoApp();
    static TStatusLine *initStatusLine(TRect r);
    static TMenuBar    *initMenuBar   (TRect r);
    void idle(void);
    virtual TPalette& getPalette() const;
    void showDialog(void);
};

TDemoApp::TDemoApp()
    : TProgInit(&TDemoApp::initStatusLine,
                &TDemoApp::initMenuBar,
                &TDemoApp::initDeskTop)
{
    TRect r;
    r = getExtent();
    r.b.y = r.a.y + 1;
    r.b.x = r.a.x + 2;
    spinView = new TSpinViewer(r);
    insert(spinView);
    spinView->update();

    lastTick = std::chrono::steady_clock::now();

    deskTop->redraw();
    menuBar->draw();
    statusLine->draw();
    showDialog();
}

TStatusLine* TDemoApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    return (new TStatusLine(r, *new TStatusDef(0, 0xFFF) +
                               *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit)
                              )
           );
}

TMenuBar* TDemoApp::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;
    return(new TMenuBar(r,
                *new TSubMenu("~F~ile", kbAltF) +
                *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X")
                )
          );
}

TPalette& TDemoApp::getPalette() const
{
    static TPalette newcolor     ( cpMyColor, sizeof( cpMyColor )-1 );
    static TPalette newblackwhite( cpMyBlackWhite, sizeof( cpMyBlackWhite )-1 );
    static TPalette newmonochrome( cpMyMonochrome, sizeof( cpMyMonochrome )-1 );
    static TPalette *palettes[] =
    {
        &newcolor,
        &newblackwhite,
        &newmonochrome
    };
    return *(palettes[appPalette]);
}

void TDemoApp::idle(void)
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick);
    if (elapsed.count() >= 55)
    {
        lastTick = now;
        spinView->update();
        gageDialog->update();
    }
    TApplication::idle();
}

void TDemoApp::showDialog(void)
{
    gageDialog = new TGageDlg(TRect(15,4,65,18), "Look at the pretty gages");
    if(gageDialog)
    {
        gageDialog->insertResources();
        deskTop->insert(gageDialog);
    }
}

int main()
{
    TDemoApp demoapp;
    demoapp.run();
    return 0;
}
