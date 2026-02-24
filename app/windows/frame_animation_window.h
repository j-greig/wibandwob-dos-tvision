#ifndef APP_WINDOWS_FRAME_ANIMATION_WINDOW_H
#define APP_WINDOWS_FRAME_ANIMATION_WINDOW_H

#define Uses_TWindow
#define Uses_TRect
#define Uses_TFrame
#define Uses_TEvent
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TMenuBox
#define Uses_TKeys
#include <tvision/tv.h>

#include <string>

class TFrameAnimationWindow : public TWindow
{
public:
    TFrameAnimationWindow(const TRect& bounds, const char* aTitle,
                          const std::string& filePath,
                          bool frameless = false, bool shadowless = false);

    virtual void changeBounds(const TRect& bounds) override;
    static TFrame* initFrame(TRect r);
    static TFrame* initFrameless(TRect r);

    const std::string& getFilePath() const { return filePath_; }
    bool isFrameless() const { return frameless_; }

    virtual void handleEvent(TEvent& event) override;

private:
    std::string filePath_;
    bool frameless_ {false};
    std::string savedTitle_;
};

#endif
