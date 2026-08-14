/*---------------------------------------------------------*/
/*                                                         */
/*   pattern_windows.h - Test pattern & gradient windows   */
/*   Extracted verbatim from wwdos_app.cpp (monolith split)*/
/*                                                         */
/*---------------------------------------------------------*/

#ifndef PATTERN_WINDOWS_H
#define PATTERN_WINDOWS_H

#define Uses_TView
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TRect
#define Uses_TDrawBuffer
#include <tvision/tv.h>

#include "test_pattern.h"
#include "gradient.h"
#include "notitle_frame.h"

/*---------------------------------------------------------*/
/* TTestPatternView - The interior view showing pattern   */
/*---------------------------------------------------------*/
class TTestPatternView : public TView
{

public:
    TTestPatternView(const TRect& bounds) : TView(bounds)
    {
        options |= ofFramed;
        growMode = gfGrowHiX | gfGrowHiY;
    }

    virtual void draw()
    {
        TDrawBuffer b;
        int patternHeight = TTestPattern::getPatternHeight();

        for (int y = 0; y < size.y; y++)
        {
            int patternRow = y % patternHeight;
            int offset = 0;

            // Calculate offset for continuous patterns
            if (USE_CONTINUOUS_PATTERN) {
                offset = (y / patternHeight) * size.x;
            }

            TTestPattern::drawPatternRow(b, patternRow, size.x, offset);
            writeLine(0, y, size.x, 1, b);
        }
    }
};

/*---------------------------------------------------------*/
/* TTestPatternWindow - Window containing test pattern    */
/*---------------------------------------------------------*/
class TTestPatternWindow : public TWindow
{
private:
    TTestPatternView* patternView;

public:
    TTestPatternWindow(const TRect& bounds, const char* aTitle) :
        TWindow(bounds, "", wnNoNumber),
        TWindowInit(&TTestPatternWindow::initFrame)
    {
        options |= ofTileable;  // Enable cascade/tile functionality

        // Get the interior bounds (excluding frame)
        TRect interior = getExtent();
        interior.grow(-1, -1);

        // Insert the test pattern view
        patternView = new TTestPatternView(interior);
        insert(patternView);
    }

    TTestPatternView* getPatternView() { return patternView; }

    static TFrame* initFrame(TRect r)
    {
        return new TNoTitleFrame(r);
    }

};

/*---------------------------------------------------------*/
/* TGradientWindow - Window containing gradient           */
/*---------------------------------------------------------*/
class TGradientWindow : public TWindow
{
public:
    enum GradientType {
        gtHorizontal,
        gtVertical,
        gtRadial,
        gtDiagonal
    };

    TGradientWindow(const TRect& bounds, const char* aTitle, GradientType type) :
        TWindow(bounds, "", wnNoNumber),
        TWindowInit(&TGradientWindow::initFrame)
    {
        options |= ofTileable;  // Enable cascade/tile functionality

        // Get the interior bounds (excluding frame)
        TRect interior = getExtent();
        interior.grow(-1, -1);

        // Insert the appropriate gradient view
        TGradientView* gradientView = nullptr;
        switch (type)
        {
            case gtHorizontal:
                gradientView = new THorizontalGradientView(interior);
                break;
            case gtVertical:
                gradientView = new TVerticalGradientView(interior);
                break;
            case gtRadial:
                gradientView = new TRadialGradientView(interior);
                break;
            case gtDiagonal:
                gradientView = new TDiagonalGradientView(interior);
                break;
        }

        if (gradientView)
            insert(gradientView);
    }

    static TFrame* initFrame(TRect r)
    {
        return new TNoTitleFrame(r);
    }
};

#endif // PATTERN_WINDOWS_H
