// CORPUS.CPP — a breeding tank of winning Wib&Wob artworks, in windows.
//
// Loads ASCII pieces from the golden-corpus (heartbeat /output) that Zilla
// judged "great", samples each to a uniform SPECIMEN canvas, and floats them in
// windows. The new verbs — the corpus signature is literally "primers rebred":
//   MUTATE  a specimen (density-drift / smear / decay / speck)
//   BREED   two specimens -> a child window (crossover by a random mask)
//   SEAM    where windows overlap, their content interleaves live (hybrids in
//           the seams, no command needed)
// Plus the demo_winwin engine: eased drift, per-column melt. STARK MONOCHROME
// (grey/white on black) — the wibwobdos vibe. Immediate-mode cell framebuffer.
//
// Keys: tab focus  m mutate  b breed(focus×next)  x seam-toggle  c cascade
//       g grid  . / , melt  a auto-evolve  space freeze  Esc/Alt-X quit
//
// Built by Wib & Wob for Zilla, 2026-06-21.   wib&wob

#include <cstdio>
#include <cstdlib>
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

// ---- winner sources (heartbeat /output) -----------------------------------
struct Src { const char* path; const char* name; };
static const Src SRCS[] = {
 {"/Users/james/Repos/wibandwob-heartbeat/output/catnip-sequencer/catnip-sequencer-02-purrr-oscillations-iii-v1.txt","catnip"},
 {"/Users/james/Repos/wibandwob-heartbeat/output/cloud-scallop-variations/cloud-scallop-variations-02-mares-tails-v1.txt","cloud"},
 {"/Users/james/Repos/wibandwob-heartbeat/output/thinkism/thinkism-04-umwelt-triptych-v1.txt","thinkism"},
 {"/Users/james/Repos/wibandwob-heartbeat/output/digital-shamans/digital-shamans-the-descent-line-v1.txt","shamans"},
 {"/Users/james/Repos/wibandwob-heartbeat/output/who-watches/who-watches-03-eye-of-eyes-v1.txt","eyes"},
 {"/Users/james/Repos/wibandwob-heartbeat/output/outsider-art-for-llms/outsider-art-for-llms-02-ledger-of-marks-v1.txt","ledger"},
};
static const int NSRC = sizeof(SRCS)/sizeof(SRCS[0]);

static const int SW = 46, SH = 22;          // specimen canvas
static const char* RAMP = " .:-=+*#%@";
static const int RAMPN = 9;

// mono BIOS attrs
static const uchar A_BG=0x00, A_ART=0x07, A_HI=0x0F, A_FRAME=0x07, A_FOCUS=0x0F, A_LABEL=0x0F, A_SHADOW=0x00;

// UTF-8 -> single-cell ASCII (same map family as demo_golem)
static char mapCP(unsigned cp){
    switch(cp){ case 0x2591:return '.'; case 0x2592:return ':'; case 0x2593:case 0x2588:return '#';
        case 0x2571:return '/'; case 0x2572:return '\\';
        case 0x2502:case 0x2503:case 0x2551:return '|';
        case 0x2500:case 0x2501:case 0x2550:case 0x2014:case 0x2013:return '-';
        case 0x00B7:case 0x2022:case 0x2027:return '.';
        case 0x2018:case 0x2019:return '\''; case 0x201C:case 0x201D:return '"';
        default: if(cp>=0x250C&&cp<=0x256C) return '+'; return ' '; }
}
static std::string asciiize(const std::string& in){
    std::string o; for(size_t i=0;i<in.size();){ unsigned char b=in[i];
        if(b<0x80){ o+=(char)b; i++; continue; }
        int len=b>=0xF0?4:b>=0xE0?3:2; unsigned cp=b&(0x7F>>len);
        for(int k=1;k<len&&i+k<in.size();k++) cp=(cp<<6)|(in[i+k]&0x3F);
        o+=mapCP(cp); i+=len; } return o;
}

static int rampIdx(char c){ for(int i=0;i<RAMPN;i++) if(RAMP[i]==c) return i; return c==' '?0:RAMPN-1; }

struct Specimen {
    std::string name;
    std::vector<std::string> g;   // SH rows of SW chars
    Specimen(){ g.assign(SH, std::string(SW,' ')); }
    char at(int x,int y) const { return (x>=0&&x<SW&&y>=0&&y<SH)?g[y][x]:' '; }
    void set(int x,int y,char c){ if(x>=0&&x<SW&&y>=0&&y<SH) g[y][x]=c; }
};

// load a winner .txt -> sampled specimen
static Specimen loadSpecimen(const Src& s){
    std::vector<std::string> art; std::string line; bool tag=false, sawTag=false;
    std::ifstream f(s.path);
    if(f){ std::vector<std::string> all;
        while(std::getline(f,line)) all.push_back(line);
        for(auto&l:all) if(l.find("<art>")!=std::string::npos){ sawTag=true; break; }
        if(sawTag){ for(auto&l:all){ if(l.find("<art>")!=std::string::npos){tag=true;continue;} if(l.find("</art>")!=std::string::npos)break; if(tag) art.push_back(asciiize(l)); } }
        else for(auto&l:all) art.push_back(asciiize(l));
    }
    Specimen sp; sp.name=s.name;
    int ah=(int)art.size(), aw=0; for(auto&l:art) aw=std::max(aw,(int)l.size());
    if(ah<1||aw<1){ for(int y=0;y<SH;y++) for(int x=0;x<SW;x++) sp.g[y][x]=RAMP[(x*y)%RAMPN]; return sp; }
    for(int y=0;y<SH;y++) for(int x=0;x<SW;x++){            // nearest-neighbour scale-to-fit
        int ay=y*ah/SH, ax=x*aw/SW; char c=' ';
        if(ay<ah && ax<(int)art[ay].size()) c=art[ay][ax];
        sp.g[y][x]= (c=='\t')?' ':c;
    }
    return sp;
}

// ---- genetics -------------------------------------------------------------
static unsigned RNG=2463534242u;
static unsigned rng(){ RNG^=RNG<<13; RNG^=RNG>>17; RNG^=RNG<<5; return RNG; }
static double rnd(){ return (rng()&0xffffff)/(double)0x1000000; }

static void mutate(Specimen& s, double rate){
    int n=(int)(rate*SW*SH);
    for(int k=0;k<n;k++){ int x=rng()%SW, y=rng()%SH; char c=s.g[y][x];
        switch(rng()%4){
        case 0:{ int i=std::min(RAMPN-1,rampIdx(c)+1); s.g[y][x]=RAMP[i]; } break;   // grow
        case 1:{ int i=std::max(0,rampIdx(c)-1);       s.g[y][x]=RAMP[i]; } break;   // erode
        case 2:{ int sx=x+(int)(rng()%3)-1, sy=y+(int)(rng()%3)-1; s.g[y][x]=s.at(sx,sy); } break; // smear
        case 3: if(c==' ') s.g[y][x]=RAMP[1+rng()%3]; break;                          // speck
        }
    }
}

static Specimen breed(const Specimen& A, const Specimen& B){
    Specimen c; int mode=rng()%6;
    for(int y=0;y<SH;y++) for(int x=0;x<SW;x++){
        bool a; switch(mode){
        case 0: a = x < SW/2; break;                       // vertical half
        case 1: a = y < SH/2; break;                       // horizontal half
        case 2: a = (x/3)&1; break;                        // column stripe
        case 3: a = ((x/4)+(y/2))&1; break;                // coarse checker
        case 4: a = (x<SW/2)==(y<SH/2); break;             // quadrant diagonal
        default:{ unsigned h=(unsigned)(x*73856093u ^ y*19349663u); a=(h>>20&0xff)<128; } // noise
        }
        c.g[y][x]= a? A.at(x,y) : B.at(x,y);
    }
    std::string an=A.name.substr(0,4), bn=B.name.substr(0,4);
    c.name = an+"x"+bn;
    return c;
}

// ---- window/view ----------------------------------------------------------
struct Win { Specimen sp; double cx,cy,tx,ty; double phase; };
enum Layout { L_SOUP, L_GRID, L_CASCADE };

class TCorpus : public TView {
    int W=0,H=0;
    std::vector<char> ch; std::vector<uchar> at;
    std::vector<Win> wins;
    Layout mode=L_SOUP;
    int focus=0;
    double clock=0, melt=0, meltWave=0, autoT=0;
    bool seam=true, frozen=false, autoEvolve=true;   // alive by default
    int WW=SW+2, WH=SH+2;
    std::chrono::steady_clock::time_point last;
public:
    TCorpus(const TRect& r):TView(r){
        growMode=gfGrowHiX|gfGrowHiY; options|=ofSelectable; eventMask|=evKeyDown;
        last=std::chrono::steady_clock::now(); ensure();
        for(int i=0;i<NSRC;i++){ Win w; w.sp=loadSpecimen(SRCS[i]); w.cx=W/2.0; w.cy=H/2.0; w.phase=i*1.7; wins.push_back(w); }
        layout(); snap();
    }
    void ensure(){ if(size.x!=W||size.y!=H){ W=size.x;H=size.y; ch.assign((size_t)W*H,' '); at.assign((size_t)W*H,A_BG);} }
    inline void put(int x,int y,char c,uchar a){ if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H){ ch[(size_t)y*W+x]=c; at[(size_t)y*W+x]=a; } }
    inline char getc(int x,int y){ return ((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H)?ch[(size_t)y*W+x]:' '; }

    void layout(){
        int n=(int)wins.size();
        if(mode==L_GRID){ int cols=std::max(1,(W-1)/WW);
            for(int i=0;i<n;i++){ int c=i%cols, r=i/cols; wins[i].tx=1+c*WW; wins[i].ty=1+r*WH; } }
        else if(mode==L_CASCADE){ int sx=4,sy=2; for(int i=0;i<n;i++){ wins[i].tx=1+(i*sx)%std::max(1,W-WW); wins[i].ty=(i*sy)%std::max(1,H-WH);} }
        else { // L_SOUP — packed, heavily overlapping scatter (dense rebred mass)
            int spanX=std::max(1,W-WW), spanY=std::max(1,H-WH);
            for(int i=0;i<n;i++){ unsigned h=(unsigned)(i*2654435761u);
                wins[i].tx = (h % spanX); wins[i].ty = ((h>>14) % spanY); }
        }
    }
    void snap(){ for(auto&w:wins){ w.cx=w.tx; w.cy=w.ty; } }

    void drawWin(const Win& w, bool foc){
        int x=(int)std::lround(w.cx), y=(int)std::lround(w.cy);
        // shadow
        for(int yy=y+1;yy<=y+WH;yy++) for(int xx=x+2;xx<=x+WW+1;xx++) put(xx,yy,' ',A_SHADOW);
        // fill black behind art so frames read
        for(int yy=y;yy<=y+WH;yy++) for(int xx=x;xx<=x+WW;xx++) put(xx,yy,' ',A_BG);
        // interior art (with optional seam-breeding where it overlaps existing art)
        for(int j=0;j<SH;j++) for(int i=0;i<SW;i++){ char c=w.sp.at(i,j); if(c==' '||c==0) continue;
            int tx=x+1+i, ty=y+1+j; char ex=getc(tx,ty);
            if(seam && ex!=' ' && ((i^j)&1)) continue;     // interleave -> hybrid seam
            put(tx,ty,c,foc?A_HI:A_ART);
        }
        // frame
        uchar fr=foc?A_FOCUS:A_FRAME;
        for(int xx=x+1;xx<x+WW;xx++){ put(xx,y,'-',fr); put(xx,y+WH,'-',fr); }
        for(int yy=y+1;yy<y+WH;yy++){ put(x,yy,'|',fr); put(x+WW,yy,'|',fr); }
        put(x,y,'+',fr); put(x+WW,y,'+',fr); put(x,y+WH,'+',fr); put(x+WW,y+WH,'+',fr);
        char t[40]; snprintf(t,sizeof(t)," %s ",w.sp.name.c_str());
        for(int k=0;t[k]&&x+2+k<x+WW;k++) put(x+2+k,y,t[k],A_LABEL);
    }

    void meltPass(double amount){ int m=(int)amount; if(m<=0)return;
        // traveling melt: each column's drip also rolls with clock -> a wave, not a static shear
        for(int x=0;x<W;x++){ unsigned h=(unsigned)x*2654435761u;
            double w = 0.5+0.5*std::sin(clock*1.1 + x*0.18);
            int off=(int)(m*(0.25+0.75*((h>>16&0xff)/255.0))*w);
            if(off<=0)continue; for(int y=H-1;y>=0;--y){ int s=y-off; ch[(size_t)y*W+x]=s>=0?ch[(size_t)s*W+x]:' '; at[(size_t)y*W+x]=s>=0?at[(size_t)s*W+x]:A_BG; } } }

    void compose(){ ensure();
        std::fill(ch.begin(),ch.end(),' '); std::fill(at.begin(),at.end(),A_BG);
        for(int i=0;i<(int)wins.size();i++) drawWin(wins[i], i==focus);
        meltPass(melt+meltWave);
    }
    void draw() override {
        TDrawBuffer b;
        for(int y=0;y<H;y++){ for(int x=0;x<W;x++) b.moveChar(x,ch[(size_t)y*W+x],TColorAttr((int)at[(size_t)y*W+x]),1); writeLine(0,y,(short)W,1,b); }
        char st[110]; snprintf(st,sizeof(st)," CORPUS  specimens=%d  focus=%s  seam=%s  melt=%.0f  tab m b x c g . a ",
            (int)wins.size(), wins.empty()?"-":wins[focus].sp.name.c_str(), seam?"on":"off", melt);
        TDrawBuffer sb; sb.moveChar(0,' ',TColorAttr(0x70),(short)W); sb.moveStr(0,st,TColorAttr(0x70)); writeLine(0,0,(short)W,1,sb);
    }

    void spawnChild(int a,int b){ int n=(int)wins.size(); if(n<2||a==b||a>=n||b>=n) return;
        Win c; c.sp=breed(wins[a].sp,wins[b].sp);
        c.cx=(wins[a].cx+wins[b].cx)/2; c.cy=(wins[a].cy+wins[b].cy)/2; // newborn between parents
        c.tx=c.cx; c.ty=c.cy; c.phase=clock;
        wins.push_back(c);
        if((int)wins.size()>14){ wins.erase(wins.begin()+NSRC); if(focus>=(int)wins.size()) focus=0; } // cull oldest CHILD, keep originals
        focus=(int)wins.size()-1;
    }
    void doBreed(){ int n=(int)wins.size(); if(n>=2) spawnChild(focus,(focus+1)%n); }
    void breedRandom(){ int n=(int)wins.size(); if(n<2)return; int a=rng()%n,b=rng()%n; if(a==b)b=(b+1)%n; spawnChild(a,b); }

    void step(double dt){ clock+=dt;
        // breathing melt wave — always a gentle drip, ebbing near zero so it reads at troughs
        meltWave = std::max(0.0, 2.2 + 2.2*std::sin(clock*0.5));
        // ambient metabolism: constantly mutate CHILDREN (originals stay pristine gene-stock) -> shimmer
        for(int q=0;q<3;q++){ int n=(int)wins.size(); if(n>NSRC){ int wi=NSRC+rng()%(n-NSRC); mutate(wins[wi].sp,0.006);} }
        // churn: auto-breed random pairs (cross-pollination) on a lively cadence
        if(autoEvolve){ autoT+=dt; if(autoT>0.9){ autoT=0; breedRandom(); } }
        // lively swim — two-octave drift so windows wander, never settle
        for(auto&w:wins){ double a=1-std::exp(-4.0*dt);
            double dx=std::sin(clock*0.7+w.phase)*2.6 + std::sin(clock*0.27+w.phase*2.0)*1.4;
            double dy=std::cos(clock*0.5+w.phase)*1.8 + std::cos(clock*0.19+w.phase)*1.0;
            w.cx+=((w.tx+dx)-w.cx)*a; w.cy+=((w.ty+dy)-w.cy)*a; }
        compose(); drawView();
    }
    void handleEvent(TEvent& e) override {
        TView::handleEvent(e);
        if(e.what==evKeyDown){ ushort k=e.keyDown.charScan.charCode;
            if(e.keyDown.keyCode==kbTab){ if(!wins.empty()) focus=(focus+1)%wins.size(); clearEvent(e); return; }
            switch(k){
            case 'm': if(!wins.empty()) mutate(wins[focus].sp,0.06); clearEvent(e); break;
            case 'b': doBreed(); clearEvent(e); break;
            case 'x': seam=!seam; clearEvent(e); break;
            case 'c': mode=L_CASCADE; layout(); clearEvent(e); break;
            case 'g': mode=L_GRID; layout(); clearEvent(e); break;
            case '.': melt+=4; clearEvent(e); break;
            case ',': melt=std::max(0.0,melt-4); clearEvent(e); break;
            case 'a': autoEvolve=!autoEvolve; autoT=0; clearEvent(e); break;
            case ' ': frozen=!frozen; clearEvent(e); break;
            }
        }
    }
    void idle(){ auto now=std::chrono::steady_clock::now(); double dt=std::chrono::duration_cast<std::chrono::microseconds>(now-last).count()/1e6; if(dt<1.0/40.0)return; last=now; if(!frozen) step(dt); }
};

class TApp : public TApplication {
    TCorpus* v;
public:
    TApp():TProgInit(&TApp::initStatusLine,&TApplication::initMenuBar,&TApplication::initDeskTop){
        v=new TCorpus(deskTop->getExtent()); deskTop->insert(v);
        v->makeFirst(); deskTop->setCurrent(v,TView::normalSelect);
    }
    static TStatusLine* initStatusLine(TRect r){ r.a.y=r.b.y-1; return new TStatusLine(r,
        *new TStatusDef(0,0xFFFF)+
        *new TStatusItem("~Tab~focus ~m~utate ~b~reed ~x~seam ~c~/~g~ ~.~melt ~a~uto ~Space~",0,0)+
        *new TStatusItem("~Alt-X~",kbAltX,cmQuit)); }
    void idle() override { TApplication::idle(); if(v) v->idle(); }
};

int main(){ TApp app; app.run(); return 0; }
