/*-----------------------------------------------------------*/
/*   tweet_shader_view.cpp — MONO.SHDR                       */
/*   Faithful C++ port of the 280-char GLSL:                 */
/*                                                           */
/*   float i,s,R,e;vec3 q,p,d=vec3(FC.yx/r,1)                */
/*     *rotate3D(.5,vec3(2,2,cos(t*.5)));                    */
/*   for(q.yz--;i++<47.;){e+=i/3e3;i>39.?d/=-d:d;s=4.;       */
/*     p=q+=d*e*R*.17;                                       */
/*     p=vec3(log2(R=length(p))-t*.5,R-p.z/R,atan(p.y,p.x)); */
/*     for(e=--p.y;s<8e2;s+=s)                               */
/*       e+=.1-abs(dot(cos(p*s),sin(p.zxy*s)))/s;            */
/*     o=tanh(e+o);}                                         */
/*                                                           */
/*   (c) shader maths @YoheiNishitsuji, つぶやきGLSL          */
/*-----------------------------------------------------------*/

#define Uses_TView
#define Uses_TWindow
#define Uses_TRect
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TColorAttr
#define Uses_TKeys
#include <tvision/tv.h>

#include "tweet_shader_view.h"
#include "theme_manager.h"

#include <cmath>
#include <vector>

namespace {

struct V3 { float x, y, z; };
static inline V3 v3(float x, float y, float z) { return {x, y, z}; }
static inline V3 add(V3 a, V3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
static inline V3 mul(V3 a, float k) { return {a.x*k, a.y*k, a.z*k}; }
static inline float dot3(V3 a, V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float len3(V3 a) { return std::sqrt(dot3(a, a)); }

// GLSL rotate3D(angle, axis): Rodrigues rotation matrix applied to a vector
static V3 rotate3D(V3 v, float angle, V3 axis) {
    float n = len3(axis);
    if (n < 1e-9f) return v;
    V3 a = mul(axis, 1.0f / n);
    float c = std::cos(angle), s = std::sin(angle);
    V3 cross = { a.y*v.z - a.z*v.y, a.z*v.x - a.x*v.z, a.x*v.y - a.y*v.x };
    float d = dot3(a, v) * (1.0f - c);
    return add(add(mul(v, c), mul(cross, s)), mul(a, d));
}

// The tweet, per pixel: returns accumulated luminance (post-tanh, ~0..1)
static float shade(float u, float v, float t) {
    float i = 0, s, R = 0, e = 0, o = 0;
    V3 q = v3(0, -1, -1);
    // GLSL `vec * mat3` is a ROW-vector multiply = transpose = rotation by
    // -angle. Getting this wrong renders a corner-huddled blob instead of
    // the full-frame rock field (found via numpy prototype vs video frame).
    V3 d = rotate3D(v3(u, v, 1), -0.5f, v3(2, 2, std::cos(t * 0.5f)));
    while (i++ < 47.f) {
        e += i / 3000.f;
        if (i > 39.f) d = v3(-1, -1, -1);        // d/=-d, the glitch step
        q = add(q, mul(d, e * R * 0.17f));
        // clamp the runaway march — floats overflow to inf and poison tanh
        if (!(std::fabs(q.x) < 1e9f)) q.x = q.x < 0 ? -1e9f : 1e9f;
        if (!(std::fabs(q.y) < 1e9f)) q.y = q.y < 0 ? -1e9f : 1e9f;
        if (!(std::fabs(q.z) < 1e9f)) q.z = q.z < 0 ? -1e9f : 1e9f;
        V3 p = q;
        R = len3(p);
        if (R < 1e-9f) R = 1e-9f;
        if (R > 1e9f)  R = 1e9f;
        p = v3(std::log2(R) - t * 0.5f, R - p.z / R, std::atan2(p.y, p.x));
        p.y -= 1.0f;                              // e = --p.y
        e = p.y;
        for (s = 4.f; s < 800.f; s += s) {
            V3 ps  = mul(p, s);
            V3 pzs = v3(p.z * s, p.x * s, p.y * s);   // p.zxy * s
            float dc = std::cos(ps.x) * std::sin(pzs.x)
                     + std::cos(ps.y) * std::sin(pzs.y)
                     + std::cos(ps.z) * std::sin(pzs.z);
            e += 0.1f - std::fabs(dc) / s;
        }
        if (!std::isfinite(e)) e = 0;
        o = std::tanh(e + o);
    }
    return o < 0 ? 0 : (o > 1 ? 1 : o);
}

// ── shader: ISO.TOWER — painter-algorithm isometric voxel field ──
// No raymarching: project each column of a breathing heightfield with the
// classic 2:1 iso transform, draw back-to-front (painter), three facet
// tones: top ▓ bright, sun face mid, shade face dark. Crisp by construction.
static void frameIsoTower(int W, int H, float t, float* out)
{
    for (int i = 0; i < W * H; ++i) out[i] = 0.f;
    const int tw = 6, th = 3;          // tile half-extent in cells
    const int hstep = 1;               // rows per height unit
    int G = (W / (2 * tw)) + (H / th) + 6;

    auto plot = [&](int x, int y, float l) {
        if (x >= 0 && x < W && y >= 0 && y < H) out[y * W + x] = l;
    };

    int midX = W / 2;
    int topY = -G;   // pull the grid up so it fills the window

    for (int sdiag = 0; sdiag <= 2 * G; ++sdiag) {          // back to front
        for (int i = 0; i <= sdiag; ++i) {
            int j = sdiag - i;
            if (i > G || j > G) continue;
            // a CITY, not a landmass: ~40% of cells are empty street, so
            // each tower reads as its own object. Heights breathe with t;
            // cap brightness scales with height for depth.
            float cell = std::sin(i * 12.9898f + j * 78.233f) * 43758.5453f;
            cell -= std::floor(cell);                     // hash 0..1
            if (cell < 0.42f) continue;                   // street gap
            float hf = 2.f + 5.f * cell
                     + 2.5f * std::sin(t * 0.8f + cell * 6.28f + (i + j) * 0.3f);
            int h = (int)hf; if (h < 1) h = 1;
            int ox = midX + (i - j) * tw;
            int oy = topY + (i + j) * th - h * hstep + H / 2;

            // column faces first (they sit under the cap): straight prisms
            int faceTop = oy + th;
            int faceH = h * hstep + th;
            for (int f = 0; f < faceH; ++f) {
                for (int c = 1; c <= tw; ++c) {
                    // left = sun, right = shade; edges taper with the diamond
                    plot(ox - c, faceTop + f, 0.50f);
                    plot(ox + c - 1, faceTop + f, 0.22f);
                }
            }
            // top cap: solid diamond; higher towers glow brighter
            float capLum = 0.55f + 0.05f * (float)h;
            if (capLum > 0.98f) capLum = 0.98f;
            for (int r = -th; r <= th; ++r) {
                int span = tw * (th - (r < 0 ? -r : r)) / th;
                for (int c = -span; c < span; ++c)
                    plot(ox + c, oy + r, capLum);
            }

        }
    }
}

// ── shader: SQ.TUNNEL — flying down a glowing square tunnel ──
static float shadeTunnel(float u, float v, float t) {
    float x = (u - 0.5f) * 2.f, y = (v - 0.5f) * 2.f;
    float ax = std::fabs(x), ay = std::fabs(y);
    float m = ax > ay ? ax : ay;              // square radius
    if (m < 1e-4f) m = 1e-4f;
    float depth = 1.f / m + t * 3.f;          // fly forward
    float ang = std::atan2(y, x);
    float wall = std::sin(depth * 2.f) * std::cos(ang * 8.f + t);
    float rings = 0.5f + 0.5f * std::sin(depth * 3.1415f);
    float lum = rings * 0.6f + 0.4f * std::fabs(wall);
    lum *= m;                                  // darken toward the far centre
    return lum < 0 ? 0 : (lum > 1 ? 1 : lum);
}

// ── registry ───────────────────────────────────────────────
struct ShaderDef {
    const char* name;
    float (*fn)(float, float, float);                 // per-pixel, or null
    void (*frame)(int, int, float, float*);           // full-frame, or null
};
static const ShaderDef kShaders[] = {
    { "isotower",    nullptr,     frameIsoTower },
    { "yohei-rocks", shade,       nullptr },
    { "tunnel",      shadeTunnel, nullptr },
};
static const int kShaderCount = 3;

static const char* kRamp = " .:-=+*#%@";

static const int kPhosphorIdx[4] = { 15, 10, 14, 11 };  // white green amber cyan

} // namespace

int shaderCount() { return kShaderCount; }
const char* shaderName(int idx) {
    return (idx >= 0 && idx < kShaderCount) ? kShaders[idx].name : "";
}
int findShaderIndex(const std::string& name) {
    for (int i = 0; i < kShaderCount; ++i)
        if (name == kShaders[i].name) return i;
    return -1;
}

TTweetShaderView::TTweetShaderView(const TRect& bounds, int aShaderIdx, unsigned aPeriodMs)
    : TView(bounds), periodMs(aPeriodMs),
      shaderIdx(aShaderIdx >= 0 && aShaderIdx < kShaderCount ? aShaderIdx : 0)
{
    options |= ofSelectable;
    // gfGrowHiX|HiY: bottom-right corner follows a window resize while the
    // top-left stays put — gfGrowAll TRANSLATES the whole view instead,
    // leaving unpainted window behind it.
    growMode = gfGrowHiX | gfGrowHiY;
    eventMask |= evBroadcast | evKeyboard;
}

TTweetShaderView::~TTweetShaderView() { stopTimer(); }
void TTweetShaderView::startTimer() { if (!timerId) timerId = setTimer(periodMs, (int)periodMs); }
void TTweetShaderView::stopTimer()  { if (timerId) { killTimer(timerId); timerId = 0; } }

void TTweetShaderView::draw()
{
    int W = size.x, H = size.y;
    if (W <= 0 || H <= 0) return;

    float t = frame * 0.066f;
    TColorAttr ink = TColorAttr(ThemeManager::cgaColor(kPhosphorIdx[phosphor]),
                                ThemeManager::cgaColor(0));
    TColorAttr dim = TColorAttr(ThemeManager::cgaColor(8),
                                ThemeManager::cgaColor(0));

    // Square-ish sampling: a cell is ~2x taller than wide, and the original
    // runs on a square canvas — normalise both axes by the same N.
    float N = (float)(W > 2*H ? W : 2*H);

    static std::vector<float> fbuf;
    const ShaderDef& sh = kShaders[shaderIdx];
    if (sh.frame) {
        fbuf.assign((size_t)W * H, 0.f);
        sh.frame(W, H, t, fbuf.data());
    }

    for (int y = 0; y < H; ++y) {
        TDrawBuffer b;
        for (int x = 0; x < W; ++x) {
            float lum;
            if (sh.frame) {
                lum = fbuf[(size_t)y * W + x];
            } else {
                // FC.yx/r quirk of the original: swap axes going in
                float u = (float)((H - 1 - y) * 2) / N;
                float v = (float)x / N;
                lum = sh.fn(u, v, t);
                if (sh.fn == shade) lum = std::pow(lum, 1.6f);  // rocks gamma
            }
            int idx = (int)(lum * 9.999f);
            if (idx < 0) idx = 0; if (idx > 9) idx = 9;
            char ch = kRamp[idx];
            // faint cells get the dim grey, bright cells the phosphor
            b.moveChar(x, ch, idx >= 3 ? ink : dim, 1);
        }
        writeLine(0, y, W, 1, b);
    }
    // shader name tag, bottom-left, dim
    {
        TDrawBuffer tag;
        std::string label = std::string(" ") + kShaders[shaderIdx].name + " [N] ";
        TColorAttr tagA = TColorAttr(ThemeManager::cgaColor(8), ThemeManager::cgaColor(0));
        tag.moveStr(0, TStringView(label.data(), label.size()), tagA);
        writeLine(0, H - 1, (int)label.size() < W ? (int)label.size() : W, 1, tag);
    }
}

void TTweetShaderView::handleEvent(TEvent& ev)
{
    TView::handleEvent(ev);
    if (ev.what == evBroadcast && ev.message.command == cmTimerExpired) {
        if (timerId != 0 && ev.message.infoPtr == timerId) {
            ++frame;
            drawView();
            clearEvent(ev);
        }
    } else if (ev.what == evKeyDown) {
        char ch = ev.keyDown.charScan.charCode;
        bool handled = false;
        switch (ch) {
            case ' ': if (timerId) stopTimer(); else startTimer(); handled = true; break;
            case 'p': case 'P': phosphor = (phosphor + 1) % 4; handled = true; break;
            case 'n': case 'N': case '\t':
                shaderIdx = (shaderIdx + 1) % kShaderCount; frame = 0; handled = true; break;
            default: break;
        }
        if (handled) { drawView(); clearEvent(ev); }
    }
}

void TTweetShaderView::setState(ushort aState, Boolean enable)
{
    TView::setState(aState, enable);
    if (aState & sfExposed) {
        if (enable) startTimer(); else stopTimer();
    }
}

// ── window ─────────────────────────────────────────────────

namespace {
class TTweetShaderWindow : public TWindow {
public:
    TTweetShaderWindow(const TRect& bounds, int shaderIdx)
        : TWindowInit(&TTweetShaderWindow::initFrame),
          TWindow(bounds, "SHADER.SYS", wnNoNumber)
    {
        flags = wfMove | wfGrow | wfClose | wfZoom;
        growMode = gfGrowAll;
        TRect r = getExtent();
        r.grow(-1, -1);
        insert(new TTweetShaderView(r, shaderIdx));
    }
};
} // namespace

TWindow* createTweetShaderWindow(const TRect& bounds, const std::string& name)
{
    int idx = name.empty() ? 0 : findShaderIndex(name);
    if (idx < 0) idx = 0;
    return new TTweetShaderWindow(bounds, idx);
}
