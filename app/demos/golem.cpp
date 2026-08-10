// GOLEM.CPP — one ASCII figure, dismembered across windows.
//
// Loads a real hand-drawn piece (THE TWINNED, from the ribosome atelier run) and
// slices it into anatomical parts — twin heads, chest, forking spine, codon feet,
// kneeling makers — each living in its own window. The windows ASSEMBLE (parts
// snap to their true positions and the golem reconstitutes through a grid of
// frames, like a Vesalius specimen plate), SCATTER (anatomy drifts apart),
// ORBIT (parts circle a centre), and MELT (the whole body drips). Cell-
// framebuffer / immediate-mode (TVision as a framebuffer).
//
// Keys: a assemble  s scatter  o orbit  . / , melt  space freeze  b breathe  Alt-X
//
// Built by Wib & Wob for Zilla, 2026-06-21.   wib&wob

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
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

static const char* ART_PATH =
  "/Users/james/Repos/wibandwob-heartbeat/output/ribosome-take2-atelier-expansion/"
  "ribosome-take2-atelier-expansion-04-golem-the-twinned-v1.txt";

// UTF-8 -> ASCII so multibyte box/shade glyphs fit single-cell char buffer
static char mapCP(unsigned cp){
    switch(cp){
        case 0x2591: return '.'; case 0x2592: return ':';
        case 0x2593: case 0x2588: return '#';
        case 0x2571: return '/'; case 0x2572: return '\\';
        case 0x2502: case 0x2503: case 0x2551: return '|';
        case 0x2500: case 0x2501: case 0x2550: case 0x2014: case 0x2013: return '-';
        case 0x00B7: case 0x2022: case 0x2027: return '.';
        case 0x2018: case 0x2019: return '\'';
        case 0x201C: case 0x201D: return '"';
        default:
            if(cp>=0x250C && cp<=0x256C) return '+';   // box-drawing joins
            return ' ';                                 // kaomoji/unknown -> blank
    }
}
static std::string asciiize(const std::string& in){
    std::string o; for(size_t i=0;i<in.size();){ unsigned char b=in[i];
        if(b<0x80){ o+=(char)b; i++; continue; }
        int len = b>=0xF0?4 : b>=0xE0?3 : 2; unsigned cp=b & (0x7F>>len);
        for(int k=1;k<len && i+k<in.size();k++) cp=(cp<<6)|(in[i+k]&0x3F);
        o+=mapCP(cp); i+=len;
    } return o;
}

struct Cell { char ch; int fg; int bg; };
struct Part { int sx, sy, sw, sh; const char* name; };
struct Win  { double cx, cy, tx, ty; int pi; };

enum Mode { ASSEMBLE, SCATTER, ORBIT };

class TGolem : public TView {
    int W=0, H=0;
    std::vector<Cell> fb;
    std::vector<std::string> art;
    int aw=0, ah=0;                 // art dims
    std::vector<Part> parts;
    std::vector<Win> wins;
    Mode mode = ASSEMBLE;
    double clock=0, melt=0, autoT=0;
    bool frozen=false, breathe=true;
    std::chrono::steady_clock::time_point last;

public:
    TGolem(const TRect& r) : TView(r) {
        growMode = gfGrowHiX | gfGrowHiY;
        options |= ofSelectable; eventMask |= evKeyDown;
        last = std::chrono::steady_clock::now();
        loadArt();
        makeParts();
        ensure();
        for (size_t i=0;i<parts.size();++i){ Win w{}; w.pi=(int)i; w.cx=W/2.0; w.cy=H/2.0; wins.push_back(w); }
        layout(); snap();
    }

    void loadArt() {
        std::ifstream f(ART_PATH);
        bool in=false; std::string line;
        if (f) while (std::getline(f, line)) {
            if (line.find("<art>")!=std::string::npos){ in=true; continue; }
            if (line.find("</art>")!=std::string::npos) break;
            if (in) art.push_back(asciiize(line));
        }
        if (art.empty()) { // fallback tiny golem
            art = {"  .-\"\"-. .-\"\"-.  ","  (o)(o) (o)(o)  ","   \\__/   \\__/   ",
                   "    ||  ▓▓  ||    ","   /  \\ || /  \\   "," .: A U G : G A C :."};
        }
        ah=(int)art.size(); aw=0; for(auto&l:art) aw=std::max(aw,(int)l.size());
    }

    void makeParts() {
        auto band=[&](double a,double b){ return std::make_pair((int)(ah*a),(int)(ah*b)); };
        int mid = aw/2;
        auto add=[&](int sx,int sy,int ex,int ey,const char* n){
            parts.push_back({sx,sy,std::max(2,ex-sx),std::max(1,ey-sy),n}); };
        auto hd=band(0.015,0.25); auto ch=band(0.25,0.50); auto sp=band(0.50,0.64);
        auto ba=band(0.64,0.85);  auto mk=band(0.85,1.0);
        add(0,        hd.first, mid,  hd.second, "head.L");
        add(mid,      hd.first, aw,   hd.second, "head.R");
        add(0,        ch.first, aw,   ch.second, "chest");
        add(0,        sp.first, aw,   sp.second, "spine");
        add(0,        ba.first, aw,   ba.second, "feet");
        add(0,        mk.first, aw,   mk.second, "makers");
    }

    void ensure(){ if(size.x!=W||size.y!=H){ W=size.x;H=size.y; fb.assign((size_t)W*H,{' ',0xC8C8C8,0x000000}); } }
    inline void put(int x,int y,char c,int fg,int bg){ if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H) fb[(size_t)y*W+x]={c,fg,bg}; }

    // ---- where each part-window wants to be -------------------------------
    void layout() {
        int ox = (W - aw)/2, oy = (H - ah)/2 + 1;        // assembled origin (centred)
        for (auto& w : wins) {
            const Part& p = parts[w.pi];
            switch (mode) {
            case ASSEMBLE:
                w.tx = ox + p.sx - 1; w.ty = oy + p.sy - 1;   // slice aligned (golem reforms)
                break;
            case SCATTER: {
                unsigned h = (unsigned)(w.pi*2654435761u);
                w.tx = 2 + (h % std::max(1,(W - p.sw - 4)));
                w.ty = 1 + ((h>>13) % std::max(1,(H - p.sh - 3)));
                break;
            }
            case ORBIT: break; // animated in step()
            }
        }
    }
    void snap(){ for(auto&w:wins){ w.cx=w.tx; w.cy=w.ty; } }

    void drawWin(const Win& w) {
        const Part& p = parts[w.pi];
        int x=(int)std::lround(w.cx), y=(int)std::lround(w.cy);
        int ww=p.sw+1, wh=p.sh+1;
        // shadow
        for(int yy=y+1;yy<=y+wh;yy++) for(int xx=x+2;xx<=x+ww+1;xx++) put(xx,yy,' ',0,0x000000);
        // frame
        for(int xx=x+1;xx<x+ww;xx++){ put(xx,y,'-',0x4080C0,0x000000); put(xx,y+wh,'-',0x4080C0,0x000000); }
        for(int yy=y+1;yy<y+wh;yy++){ put(x,yy,'|',0x4080C0,0x000000); put(x+ww,yy,'|',0x4080C0,0x000000); }
        put(x,y,'+',0x4080C0,0x000000); put(x+ww,y,'+',0x4080C0,0x000000);
        put(x,y+wh,'+',0x4080C0,0x000000); put(x+ww,y+wh,'+',0x4080C0,0x000000);
        // title
        for(int k=0;p.name[k]&&x+2+k<x+ww;k++) put(x+2+k,y,p.name[k],0xE0E040,0x000000);
        // interior = the art slice
        for(int j=0;j<p.sh;j++){
            int ay=p.sy+j; if(ay<0||ay>=ah) continue;
            const std::string& L=art[ay];
            for(int i=0;i<p.sw;i++){
                int ax=p.sx+i; char c = (ax>=0&&ax<(int)L.size())?L[ax]:' ';
                int fg = (c=='#')?0xFFFFFF
                       : (c=='|'||c=='-'||c=='+'||c=='/'||c=='\\')?0xB0C0E0
                       : 0xC8C8C8;
                put(x+1+i, y+1+j, c?c:' ', fg, 0x000000);
            }
        }
    }

    void meltPass(){ int m=(int)melt; if(m<=0)return;
        for(int x=0;x<W;x++){ unsigned h=(unsigned)x*2654435761u; int off=(int)(m*(0.4+0.6*((h>>16&0xff)/255.0)));
            if(off<=0)continue; for(int y=H-1;y>=1;--y){int s=y-off; fb[(size_t)y*W+x]= s>=1? fb[(size_t)s*W+x]:Cell{' ',0xC8C8C8,0x000000};} } }

    void compose(){ ensure();
        for(auto&c:fb) c={' ',0xC8C8C8,0x000000};
        for(auto&w:wins) drawWin(w);
        if(melt>0) meltPass();
    }

    void draw() override {
        TDrawBuffer b;
        for(int y=0;y<H;y++){ for(int x=0;x<W;x++){ Cell&c=fb[(size_t)y*W+x]; b.moveChar(x,c.ch,TColorAttr(TColorDesired(c.fg),TColorDesired(c.bg)),1);} writeLine(0,y,(short)W,1,b);}
        char st[110]; snprintf(st,sizeof(st)," THE TWINNED  mode=%s  parts=%d  melt=%.0f  %s a assemble s scatter o orbit . melt ",
            mode==ASSEMBLE?"ASSEMBLE":mode==SCATTER?"SCATTER":"ORBIT",(int)parts.size(),melt,frozen?"FROZEN ":"");
        TDrawBuffer sb; TColorAttr h(TColorDesired(0xF0F0F0),TColorDesired(0x202840));
        sb.moveChar(0,' ',h,(short)W); sb.moveStr(0,st,h); writeLine(0,0,(short)W,1,sb);
    }

    void step(double dt){
        clock+=dt;
        if(breathe){ autoT+=dt; double p=std::fmod(autoT,16.0);
            Mode want = p<5?ASSEMBLE : p<10?SCATTER : ORBIT;
            if(want!=mode){ mode=want; melt=0; layout(); }
            if(mode==ASSEMBLE && p>3.5) melt=(p-3.5)*5.0;   // assembled then melts before scatter
        }
        if(mode==ORBIT){ double cx=W/2.0, cy=H/2.0; int n=(int)wins.size();
            for(int i=0;i<n;i++){ double a=clock*0.5 + i*6.283/n; double rad=std::min(W,H)*0.32;
                wins[i].tx=cx+std::cos(a)*rad - parts[wins[i].pi].sw/2.0;
                wins[i].ty=cy+std::sin(a)*rad*0.7 - parts[wins[i].pi].sh/2.0; } }
        for(auto&w:wins){ double a=1-std::exp(-6.0*dt); w.cx+=(w.tx-w.cx)*a; w.cy+=(w.ty-w.cy)*a; }
        compose(); drawView();
    }

    void handleEvent(TEvent& e) override {
        TView::handleEvent(e);
        if(e.what==evKeyDown){ switch(e.keyDown.charScan.charCode){
            case 'a': mode=ASSEMBLE; melt=0; breathe=false; layout(); clearEvent(e); break;
            case 's': mode=SCATTER;  melt=0; breathe=false; layout(); clearEvent(e); break;
            case 'o': mode=ORBIT;    melt=0; breathe=false;          clearEvent(e); break;
            case 'b': breathe=!breathe; autoT=0; clearEvent(e); break;
            case '.': melt+=3; breathe=false; clearEvent(e); break;
            case ',': melt=std::max(0.0,melt-3); clearEvent(e); break;
            case ' ': frozen=!frozen; clearEvent(e); break;
        }}
    }
    void idle(){ auto now=std::chrono::steady_clock::now(); double dt=std::chrono::duration_cast<std::chrono::microseconds>(now-last).count()/1e6; if(dt<1.0/40.0)return; last=now; if(!frozen) step(dt); }
};

class TApp : public TApplication {
    TGolem* v;
public:
    TApp(): TProgInit(&TApp::initStatusLine,&TApplication::initMenuBar,&TApplication::initDeskTop){
        v=new TGolem(deskTop->getExtent()); deskTop->insert(v);
        v->makeFirst(); deskTop->setCurrent(v,TView::normalSelect);
    }
    static TStatusLine* initStatusLine(TRect r){ r.a.y=r.b.y-1; return new TStatusLine(r,
        *new TStatusDef(0,0xFFFF)+
        *new TStatusItem("~a~ssemble ~s~catter ~o~rbit ~.~melt ~b~reathe ~Space~",0,0)+
        *new TStatusItem("~Alt-X~",kbAltX,cmQuit)); }
    void idle() override { TApplication::idle(); if(v) v->idle(); }
};

int main(){ TApp app; app.run(); return 0; }
