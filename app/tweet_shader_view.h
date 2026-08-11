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

class TTweetShaderView : public TView {
public:
    explicit TTweetShaderView(const TRect& bounds, unsigned periodMs = 66);
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
};

class TWindow; TWindow* createTweetShaderWindow(const TRect& bounds);

#endif // TWEET_SHADER_VIEW_H
