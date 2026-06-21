// MELTCODE.CPP — a syntax highlighter that melts its own source.
//
// Daring homage to Andreas Gysin's Meltdown, built on the same engine ideas
// found in his on-chain source (meltd.ooo): an RGB Char cell grid, token
// colouring, and a per-column melt mask. Here: real C++ scrolls as a syntax-
// highlighted listing, wireframe windows glide over it, and a per-column melt
// drips the whole coloured buffer downward. Immediate-mode, TVision used as a
// truecolor framebuffer (TColorAttr w/ TColorRGB) — answering "does it have a
// frame buffer" in the loudest way.
//
// Keys: space freeze . / , melt+/-   d melt direction   r rainbow
//       l line-numbers   + / - scroll speed   Esc/Alt-X quit
//
// Built by Wib & Wob for Zilla, 2026-06-21.   wib&wob

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cmath>
#include <vector>
#include <chrono>

#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TProgram
#define Uses_TRect
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TDrawBuffer
#define Uses_TView
#include <tvision/tv.h>

// --- self-referential source (the thing that melts) ------------------------
static const char* SRC[] = {
"// meltcode — a highlighter that melts its own source",
"#include <vector>",
"#include <cmath>",
"",
"struct Char { int idx; TColorRGB fg, bg; };",
"",
"class Grid {",
"    int w, h;",
"    std::vector<Char> cells;",
"public:",
"    void fillRect(int x, int y, int tw, int th, Char v) {",
"        for (int j = 0; j < th; ++j)",
"            for (int i = 0; i < tw; ++i)",
"                set(x + i, y + j, v);",
"    }",
"    void melt(double amount) {",
"        for (int x = 0; x < w; ++x) {",
"            int off = (int)(amount * noise(x));",
"            for (int y = h - 1; y >= 0; --y)",
"                cells[y*w+x] = (y - off >= 0)",
"                    ? cells[(y - off)*w + x] : BLANK;",
"        }",
"    }",
"};",
"",
"void draw(Grid& g, double t) {",
"    g.clear(0x000010);",
"    for (auto& win : windows)",
"        rasterize(g, win, 0xFFFF00, 0x441111);",
"    g.melt(t * 9.0);   // the drip",
"}",
"",
"int main() {",
"    while (running) { update(); draw(grid, now); }",
"    return 0;   // win-win",
"}",
};
static const int NSRC = sizeof(SRC)/sizeof(SRC[0]);

static const char* KW[] = {"include","struct","class","public","private","void",
"int","for","auto","while","return","double","std","const","if","else","new"};
static bool isKw(const char* s, int n) {
    for (auto k : KW) if ((int)strlen(k)==n && !strncmp(k,s,n)) return true;
    return false;
}

// token colours (RGB)
enum { C_DEF=0xB0B0B0, C_KW=0x6699FF, C_NUM=0x55E0E0, C_STR=0xE06666,
       C_COM=0x707070, C_OP=0xE0E055, C_PUN=0x909090, C_LN=0x506080 };

struct Cell { char ch; int fg; int bg; };

class TMeltCode : public TView {
    int W=0, H=0;
    std::vector<Cell> fb;
    double clock=0, scroll=0, melt=0, speed=4.0, autoT=0;
    int meltDir=0;                 // 0 down 1 up 2 right 3 left
    bool frozen=false, rainbow=false, lineNums=true, autoMode=true;
    std::chrono::steady_clock::time_point last;
    // drifting wireframe windows
    struct Win { double x,y,tx,ty; int w,h; } wins[5];

public:
    TMeltCode(const TRect& r) : TView(r) {
        growMode = gfGrowHiX | gfGrowHiY;
        options |= ofSelectable; eventMask |= evKeyDown;
        last = std::chrono::steady_clock::now();
        ensure();
        for (int i=0;i<5;i++){ wins[i]={ (double)(8+i*22), (double)(3+i*5),0,0, 20+i*4, 7+i }; reseed(i); }
    }
    void ensure(){ if(size.x!=W||size.y!=H){ W=size.x;H=size.y; fb.assign((size_t)W*H,{' ',C_DEF,0x000010}); } }
    inline void put(int x,int y,char c,int fg,int bg){ if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H) fb[(size_t)y*W+x]={c,fg,bg}; }
    void reseed(int i){ Win&w=wins[i]; w.tx = 2 + (i*37)%std::max(1,W-w.w-2); w.ty = 2 + (i*19)%std::max(1,H-w.h-2); }

    static int hue(double t){ // t in [0,1) -> rgb
        double r=std::sin(t*6.283)*0.5+0.5, g=std::sin((t+0.333)*6.283)*0.5+0.5, b=std::sin((t+0.666)*6.283)*0.5+0.5;
        return ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255);
    }

    void renderLine(int sy, const char* L, int baseCol) {
        int n=(int)strlen(L), x=baseCol;
        bool inStr=false;
        for (int i=0;i<n && x<W;) {
            char c=L[i];
            if(!inStr && c=='/' && i+1<n && L[i+1]=='/'){ for(;i<n&&x<W;i++,x++) put(x,sy,L[i],C_COM,0x000010); break; }
            if(c=='"'){ put(x,sy,c,C_STR,0x000010); x++; i++; inStr=!inStr; continue; }
            if(inStr){ put(x,sy,c,C_STR,0x000010); x++; i++; continue; }
            if(isdigit((unsigned char)c)){ int j=i; while(j<n&&(isalnum((unsigned char)L[j])||L[j]=='.'))j++; for(;i<j&&x<W;i++,x++) put(x,sy,L[i],C_NUM,0x000010); continue; }
            if(isalpha((unsigned char)c)||c=='_'){ int j=i; while(j<n&&(isalnum((unsigned char)L[j])||L[j]=='_'))j++; int col=isKw(L+i,j-i)?C_KW:C_DEF; for(;i<j&&x<W;i++,x++) put(x,sy,L[i],col,0x000010); continue; }
            if(c==' '){ put(x,sy,' ',C_DEF,0x000010); x++; i++; continue; }
            int col = strchr("+-*/=<>&|!%^~",c)?C_OP:C_PUN; put(x,sy,c,col,0x000010); x++; i++;
        }
    }

    void compose() {
        ensure();
        for(auto&c:fb) c={' ',C_DEF,0x000010};
        int top=(int)scroll;
        int gutter = lineNums?5:0;
        for(int sy=1; sy<H; ++sy){
            int li=((top+sy-1)%NSRC+NSRC)%NSRC;
            if(lineNums){ char ln[8]; snprintf(ln,sizeof(ln),"%3d ",li+1); for(int k=0;k<4&&k<W;k++) put(k,sy,ln[k],C_LN,0x000010); }
            renderLine(sy, SRC[li], gutter);
        }
        // wireframe windows gliding over the code (borders only -> code shows through)
        for(auto&w:wins){
            int x=(int)w.x,y=(int)w.y,ww=w.w,wh=w.h;
            for(int xx=x+1;xx<x+ww;xx++){ put(xx,y,'-',0xFFCC33,0x000010); put(xx,y+wh,'-',0xFFCC33,0x000010); }
            for(int yy=y+1;yy<y+wh;yy++){ put(x,yy,'|',0xFFCC33,0x000010); put(x+ww,yy,'|',0xFFCC33,0x000010); }
            put(x,y,'+',0xFFCC33,0x000010); put(x+ww,y,'+',0xFFCC33,0x000010);
            put(x,y+wh,'+',0xFFCC33,0x000010); put(x+ww,y+wh,'+',0xFFCC33,0x000010);
            char t[24]; snprintf(t,sizeof(t)," %dx%d ",ww,wh);
            for(int k=0;t[k]&&x+2+k<x+ww;k++) put(x+2+k,y,t[k],0x101010,0xFFCC33);
        }
        if(rainbow){ for(int x=0;x<W;x++){ int h=hue(std::fmod(x*0.012+clock*0.15,1.0)); for(int y=1;y<H;y++){ Cell&c=fb[(size_t)y*W+x]; if(c.ch!=' ') c.fg=h; } } }
        if(melt>0) meltPass();
    }

    void meltPass(){
        int m=(int)melt; if(m<=0) return;
        auto shiftCol=[&](int x,int off,bool up){ if(off<=0)return; if(!up){ for(int y=H-1;y>=1;--y){int s=y-off; fb[(size_t)y*W+x]= s>=1? fb[(size_t)s*W+x]:Cell{' ',C_DEF,0x000010}; } } else { for(int y=1;y<H;++y){int s=y+off; fb[(size_t)y*W+x]= s<H? fb[(size_t)s*W+x]:Cell{' ',C_DEF,0x000010}; } } };
        auto shiftRow=[&](int y,int off,bool right){ if(off<=0)return; if(right){ for(int x=W-1;x>=0;--x){int s=x-off; fb[(size_t)y*W+x]= s>=0? fb[(size_t)y*W+s]:Cell{' ',C_DEF,0x000010}; } } else { for(int x=0;x<W;++x){int s=x+off; fb[(size_t)y*W+x]= s<W? fb[(size_t)y*W+s]:Cell{' ',C_DEF,0x000010}; } } };
        if(meltDir<2){ for(int x=0;x<W;x++){ unsigned h=(unsigned)x*2654435761u; int off=(int)(m*(0.4+0.6*((h>>16&0xff)/255.0))); shiftCol(x,off,meltDir==1);} }
        else { for(int y=1;y<H;y++){ unsigned h=(unsigned)y*2654435761u; int off=(int)(m*(0.4+0.6*((h>>16&0xff)/255.0))); shiftRow(y,off,meltDir==2);} }
    }

    void draw() override {
        TDrawBuffer b;
        for(int y=0;y<H;y++){
            for(int x=0;x<W;x++){ Cell&c=fb[(size_t)y*W+x]; b.moveChar(x,c.ch,TColorAttr(TColorDesired(c.fg),TColorDesired(c.bg)),1); }
            writeLine(0,y,(short)W,1,b);
        }
        char st[96]; snprintf(st,sizeof(st)," meltcode  melt=%.0f dir=%c %s%s  .,melt d dir r rainbow l nums ",
            melt,"DURL"[meltDir], rainbow?"RAINBOW ":"", frozen?"FROZEN":"");
        TDrawBuffer sb; sb.moveChar(0,' ',TColorAttr(TColorDesired(0xE0E0E0),TColorDesired(0x202040)),(short)W);
        sb.moveStr(0,st,TColorAttr(TColorDesired(0xE0E0E0),TColorDesired(0x202040))); writeLine(0,0,(short)W,1,sb);
    }

    void step(double dt){
        clock+=dt; scroll+=dt*speed*0.25;
        if(autoMode){ autoT+=dt; double p=std::fmod(autoT,12.0); melt = p>8 ? (p-8)*7.0 : 0; if(p<0.1) meltDir=(meltDir+1)&3; }
        for(auto&w:wins){ double a=1-std::exp(-2.5*dt); w.x+=(w.tx-w.x)*a; w.y+=(w.ty-w.y)*a; if(std::fabs(w.x-w.tx)<1&&std::fabs(w.y-w.ty)<1){ static int s=0; reseed((s++)%5);} }
        compose(); drawView();
    }

    void handleEvent(TEvent& e) override {
        TView::handleEvent(e);
        if(e.what==evKeyDown){ switch(e.keyDown.charScan.charCode){
            case ' ': frozen=!frozen; clearEvent(e); break;
            case '.': melt+=4; autoMode=false; clearEvent(e); break;
            case ',': melt=std::max(0.0,melt-4); clearEvent(e); break;
            case 'd': meltDir=(meltDir+1)&3; clearEvent(e); break;
            case 'r': rainbow=!rainbow; clearEvent(e); break;
            case 'l': lineNums=!lineNums; clearEvent(e); break;
            case 'a': autoMode=!autoMode; autoT=0; clearEvent(e); break;
            case '+': case '=': speed+=1; clearEvent(e); break;
            case '-': case '_': speed=std::max(0.0,speed-1); clearEvent(e); break;
        }}
    }
    void idle(){ auto now=std::chrono::steady_clock::now(); double dt=std::chrono::duration_cast<std::chrono::microseconds>(now-last).count()/1e6; if(dt<1.0/40.0)return; last=now; if(!frozen) step(dt); }
};

class TApp : public TApplication {
    TMeltCode* v;
public:
    TApp(): TProgInit(&TApp::initStatusLine,&TApplication::initMenuBar,&TApplication::initDeskTop){
        v=new TMeltCode(deskTop->getExtent()); deskTop->insert(v);
        v->makeFirst(); deskTop->setCurrent(v,TView::normalSelect);
    }
    static TStatusLine* initStatusLine(TRect r){ r.a.y=r.b.y-1; return new TStatusLine(r,
        *new TStatusDef(0,0xFFFF)+
        *new TStatusItem("~.,~melt ~d~ir ~r~ainbow ~l~ines ~Space~freeze ~a~uto",0,0)+
        *new TStatusItem("~Alt-X~",kbAltX,cmQuit)); }
    void idle() override { TApplication::idle(); if(v) v->idle(); }
};

int main(){ TApp app; app.run(); return 0; }
