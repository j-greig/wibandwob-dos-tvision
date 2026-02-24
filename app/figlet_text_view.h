#ifndef FIGLET_TEXT_VIEW_H
#define FIGLET_TEXT_VIEW_H

#define Uses_TView
#define Uses_TRect
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TMenuBox
#define Uses_TKeys
#include <tvision/tv.h>

#include "notitle_frame.h"

#include <string>
#include <vector>

// ── Shared FIGlet command IDs ────────────────────────────────────────────────
// Used by both the view (right-click) and the app (Edit menu).
// Font commands: cmFigletCatFontBase + index into figlet::allFontsSorted().
static const ushort cmFigletEditText    = 350;
static const ushort cmFigletMoreFonts   = 351;
static const ushort cmFigletCatFontBase = 400;  // 400..599 = font index

/*---------------------------------------------------------*/
/* TFigletTextView — renders FIGlet ASCII art typography   */
/*                                                         */
/* Shells out to figlet CLI, caches rendered lines,        */
/* re-renders on resize. Right-click for font picker.      */
/*---------------------------------------------------------*/

class TFigletTextView : public TView
{
public:
    TFigletTextView(const TRect& bounds, const std::string& text,
                    const std::string& font = "standard");
    virtual ~TFigletTextView() {}

    virtual void draw() override;
    virtual void handleEvent(TEvent& event) override;
    virtual void changeBounds(const TRect& bounds) override;

    void setText(const std::string& text);
    void setFont(const std::string& font);
    void setFgColor(TColorRGB color);
    void setBgColor(TColorRGB color);

    const std::string& getText() const { return text_; }
    const std::string& getFont() const { return font_; }
    TColorRGB getFgColor() const { return fgColor_; }
    TColorRGB getBgColor() const { return bgColor_; }

    void showFontMenu(TPoint where);
    void showEditTextDialog();
    void showCombinedMenu(TPoint where);
    void showFontListDialog();

private:
    std::string text_;
    std::string font_;
    std::vector<std::string> renderedLines_;
    TColorRGB fgColor_;
    TColorRGB bgColor_;

    void rerender();
};

/*---------------------------------------------------------*/
/* TFigletTextWindow — window wrapper with chrome control  */
/*                                                         */
/* Follows TFrameAnimationWindow pattern:                  */
/*   frameless + shadowless axes, TGhostFrame, right-click */
/*   context menu for shadow/frame/title toggles.          */
/*---------------------------------------------------------*/

class TFigletTextWindow : public TWindow
{
public:
    TFigletTextWindow(const TRect& bounds, const std::string& text,
                      const std::string& font = "standard",
                      bool frameless = false, bool shadowless = false);

    virtual void handleEvent(TEvent& event) override;
    virtual void changeBounds(const TRect& bounds) override;

    TFigletTextView* getFigletView() { return figletView_; }
    bool isFrameless() const { return frameless_; }

    static TFrame* initFrame(TRect r)     { return new TNoTitleFrame(r); }
    static TFrame* initFrameless(TRect r) { return new TGhostFrame(r); }

private:
    TFigletTextView* figletView_;
    bool frameless_;
    std::string savedTitle_;
};

#endif // FIGLET_TEXT_VIEW_H
