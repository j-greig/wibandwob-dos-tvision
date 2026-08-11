/*-----------------------------------------------------------*/
/*   tweet_shader_view.h — MONO.SHDR                         */
/*   ASCII port of a つぶやきGLSL raymarcher by               */
/*   @YoheiNishitsuji (x.com/YoheiNishitsuji/status/         */
/*   2087060900528509108): log-spherical fractal tunnel,     */
/*   turbulence field, tanh-accumulated glow — evaluated     */
/*   per character cell, luminance mapped to a glyph ramp.   */
/*   Monochrome by design: phosphor fg on black.             */
/*-----------------------------------------------------------*/

#ifndef TWEET_SHADER_VIEW_H
#define TWEET_SHADER_VIEW_H

#define Uses_TView
#define Uses_TRect
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TColorAttr
#include <tvision/tv.h>

#include <string>

// Shader-agnostic ASCII shader host. Shaders are C++ functions
// float(u, v, t) -> luminance 0..1, registered in kShaders
// (tweet_shader_view.cpp). Keys: Tab/N next shader, P phosphor, space pause.
class TTweetShaderView : public TView {
public:
    explicit TTweetShaderView(const TRect& bounds, int shaderIdx = 0,
                              unsigned periodMs = 66);
    virtual ~TTweetShaderView();

    virtual void draw() override;
    virtual void handleEvent(TEvent& ev) override;
    virtual void setState(ushort aState, Boolean enable) override;

private:
    void startTimer();
    void stopTimer();

    unsigned periodMs;
    TTimerId timerId {0};
    int frame {0};
    int phosphor {0};   // 0 white, 1 green, 2 amber, 3 cyan
    int shaderIdx {0};
};

class TWindow;
TWindow* createTweetShaderWindow(const TRect& bounds,
                                 const std::string& shaderName = "");
// Number of registered shaders + name lookup (for capability listings)
int shaderCount();
const char* shaderName(int idx);
int findShaderIndex(const std::string& name);   // -1 if unknown

#endif // TWEET_SHADER_VIEW_H
