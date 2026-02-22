/*---------------------------------------------------------*/
/*                                                         */
/*   transparent_text_view.h - Text View with             */
/*   Transparent/Custom Background Support                */
/*                                                         */
/*---------------------------------------------------------*/

#ifndef TRANSPARENT_TEXT_VIEW_H
#define TRANSPARENT_TEXT_VIEW_H

#define Uses_TBackground
#define Uses_TRect
#define Uses_TView
#define Uses_TScroller
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TKeys
#define Uses_TProgram
#define Uses_TDeskTop
#include <tvision/tv.h>

#include <string>
#include <vector>
#include <fstream>

/*---------------------------------------------------------*/
/* TTransparentTextView - Text viewer with custom BG     */
/*   Extends TScroller for proper scroll bar support     */
/*---------------------------------------------------------*/

class TTransparentTextView : public TScroller
{
public:
    TTransparentTextView(const TRect& bounds, TScrollBar* hScroll,
                         TScrollBar* vScroll, const std::string& filePath);
    virtual ~TTransparentTextView() {}

    virtual void draw() override;
    virtual void changeBounds(const TRect& bounds) override;

    // Background color control
    void setBackgroundColor(TColorRGB color);
    void setBackgroundToDefault();
    TColorRGB getBackgroundColor() const { return bgColor; }
    bool hasCustomBackground() const { return useCustomBg; }

    const std::string& getFileName() const { return fileName; }

    // Word wrapping toggle
    void setWordWrap(bool enable);
    bool getWordWrap() const { return wordWrap; }

private:
    std::vector<std::string> rawLines;       // Original file lines
    std::vector<std::string> displayLines;   // After word-wrap
    std::string fileName;
    TColorRGB bgColor;
    TColorRGB fgColor;
    bool useCustomBg;
    bool wordWrap;

    void loadFile(const std::string& path);
    void rebuildDisplayLines();
    static std::vector<std::string> wrapText(const std::string& text, int width);
};

/*---------------------------------------------------------*/
/* TTransparentTextWindow - Window hosting the view      */
/*---------------------------------------------------------*/

class TTransparentTextWindow : public TWindow
{
public:
    TTransparentTextWindow(const TRect& bounds, const std::string& title,
                          const std::string& filePath);

    TTransparentTextView* getTextView() { return textView; }
    const std::string& getFilePath() const;

    virtual void changeBounds(const TRect& bounds) override;

private:
    TTransparentTextView* textView;
    TScrollBar* vScrollBar;
    static TFrame* initFrame(TRect r);
};

#endif // TRANSPARENT_TEXT_VIEW_H
