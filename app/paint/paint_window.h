/*---------------------------------------------------------*/
/*                                                         */
/*   paint_window.h - Paint canvas TWindow wrapper         */
/*                                                         */
/*   Embeds TPaintCanvasView + tools + palette + status    */
/*   as a framed, moveable, tileable window for the main   */
/*   WibWob-DOS app.                                       */
/*                                                         */
/*---------------------------------------------------------*/

#ifndef TVISION_PAINT_WINDOW_H
#define TVISION_PAINT_WINDOW_H

#define Uses_TWindow
#define Uses_TRect
#define Uses_TFrame
#define Uses_TEvent
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TKeys
#include <tvision/tv.h>

#include "paint_canvas.h"

class TPaintCanvasView;
class TMenuBar;

class TPaintWindow : public TWindow {
public:
    TPaintWindow(const TRect &bounds);

    virtual void handleEvent(TEvent &event) override;
    virtual void changeBounds(const TRect &bounds) override;

    TPaintCanvasView* getCanvas() const { return canvas; }

private:
    static TFrame *initFrame(TRect r) { return new TFrame(r); }
    void buildLayout();
    PaintContext ctx;
    TPaintCanvasView *canvas = nullptr;
    TMenuBar *menuBar = nullptr;
};

TWindow* createPaintWindow(const TRect &bounds);

#endif
