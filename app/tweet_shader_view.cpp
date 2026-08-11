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

static const char* kRamp = " .:-=+*#%@";

static const int kPhosphorIdx[4] = { 15, 10, 14, 11 };  // white green amber cyan

} // namespace

TTweetShaderView::TTweetShaderView(const TRect& bounds, unsigned aPeriodMs)
    : TView(bounds), periodMs(aPeriodMs)
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

    for (int y = 0; y < H; ++y) {
        TDrawBuffer b;
        for (int x = 0; x < W; ++x) {
            // FC.yx/r quirk of the original: swap axes going in
            float u = (float)((H - 1 - y) * 2) / N;
            float v = (float)x / N;
            float lum = shade(u, v, t);
            lum = std::pow(lum, 1.6f);   // gamma: keep texture in the glow
            int idx = (int)(lum * 9.999f);
            if (idx < 0) idx = 0; if (idx > 9) idx = 9;
            char ch = kRamp[idx];
            // faint cells get the dim grey, bright cells the phosphor
            b.moveChar(x, ch, idx >= 3 ? ink : dim, 1);
        }
        writeLine(0, y, W, 1, b);
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
    TTweetShaderWindow(const TRect& bounds)
        : TWindowInit(&TTweetShaderWindow::initFrame),
          TWindow(bounds, "MONO.SHDR \xE2\x80\x94 \xE3\x81\xA4\xE3\x81\xB6\xE3\x82\x84\xE3\x81\x8DGLSL port", wnNoNumber)
    {
        flags = wfMove | wfGrow | wfClose | wfZoom;
        growMode = gfGrowAll;
        TRect r = getExtent();
        r.grow(-1, -1);
        insert(new TTweetShaderView(r));
    }
};
} // namespace

TWindow* createTweetShaderWindow(const TRect& bounds)
{
    return new TTweetShaderWindow(bounds);
}
