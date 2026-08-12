#define Uses_TDrawBuffer
#define Uses_TColorAttr
#include <tvision/tv.h>

#include "frame_animation_window.h"

#include "frame_file_player_view.h"
#include "notitle_frame.h"
#include "theme_manager.h"

#include <cstring>

// ── TCGAFrame::draw ──────────────────────────────────────────────────────────
// Chunky MSDOS frame: solid '█' border in the window's accent colour (the
// content view's fg colour if set, else white), title as " TITLE " in an
// inverse tab, close [■] and zoom [↑] drawn tab-style at TFrame's standard
// hotspot columns so mouse handling keeps working.
void TCGAFrame::draw()
{
    if (!ThemeManager::cgaChrome()) {
        TFrame::draw();
        return;
    }

    auto* win = (TWindow*)owner;
    const int w = size.x, h = size.y;
    if (w <= 0 || h <= 0) return;

    // Accent colour: follow the content view's explicit fg, else the
    // skin's frame role (active/passive by focus).
    bool active = win && (win->state & sfActive);
    TColorRGB accent = ThemeManager::cgaColor(ThemeManager::fgIndex(
        active ? SkinRole::FrameActive : SkinRole::FramePassive));
    if (win) {
        TView* start = win->first();
        TView* v = start;
        if (v) do {
            if (auto* fp = dynamic_cast<FrameFilePlayerView*>(v)) {
                if (fp->foregroundIndex() >= 0)
                    accent = ThemeManager::cgaColor(fp->foregroundIndex());
                break;
            }
            if (auto* tv = dynamic_cast<TTextFileView*>(v)) {
                if (tv->foregroundIndex() >= 0)
                    accent = ThemeManager::cgaColor(tv->foregroundIndex());
                break;
            }
            v = v->next;
        } while (v != start);
    }

    // Mockup style: uniform bright frames regardless of focus.
    TColorAttr frameAttr(accent, accent);              // solid block colour
    TColorAttr tabAttr = ThemeManager::attr(SkinRole::Bar);  // skin bar colours

    TDrawBuffer b;

    // Top row: solid border + centred title tab + icon hotspots.
    b.moveChar(0, ' ', frameAttr, w);
    const char* t = win ? win->title : nullptr;
    if (t && t[0]) {
        char tab[128];
        int tl = (int)std::strlen(t);
        if (tl > w - 12) tl = w - 12 > 0 ? w - 12 : 0;
        if (tl > 0 && tl + 2 < (int)sizeof(tab)) {
            tab[0] = ' ';
            std::memcpy(tab + 1, t, tl);
            tab[tl + 1] = ' ';
            tab[tl + 2] = '\0';
            int tx = (w - (tl + 2)) / 2;
            if (tx < 5) tx = 5;
            b.moveStr(tx, tab, tabAttr);
        }
    }
    // Hotspots (TFrame::handleEvent expects close at x=2, zoom at width-5).
    b.moveStr(2, "[\xFE]", tabAttr);              // [■] close
    if (w > 8) b.moveStr(w - 5, "[\x18]", tabAttr); // [↑] zoom
    writeLine(0, 0, w, 1, b);

    // Side columns.
    for (int y = 1; y < h - 1; ++y) {
        b.moveChar(0, ' ', frameAttr, w);
        // interior transparent: only write 1-char edges
        TDrawBuffer eb;
        eb.moveChar(0, ' ', frameAttr, 1);
        writeLine(0, y, 1, 1, eb);
        writeLine(w - 1, y, 1, 1, eb);
    }

    // Bottom row: solid border.
    b.moveChar(0, ' ', frameAttr, w);
    writeLine(0, h - 1, w, 1, b);
}

// Keep these IDs aligned with wwdos_app.cpp context-menu commands.
static const ushort cmCtxToggleShadow = 250;
static const ushort cmCtxClearTitle = 251;
static const ushort cmCtxToggleFrame = 252;
static const ushort cmCtxGalleryToggle = 253;

TFrameAnimationWindow::TFrameAnimationWindow(const TRect& bounds, const char* aTitle,
                                             const std::string& filePath,
                                             bool frameless, bool shadowless) :
    TWindow(bounds, aTitle, wnNoNumber),
    TWindowInit(frameless ? &TFrameAnimationWindow::initFrameless
                          : &TFrameAnimationWindow::initFrame),
    filePath_(filePath),
    frameless_(frameless)
{
    options |= ofTileable;

    if (shadowless)
        state &= ~sfShadow;

    TRect interior = getExtent();
    if (!frameless)
        interior.grow(-1, -1);

    if (hasFrameDelimiters(filePath)) {
        FrameFilePlayerView* animView = new FrameFilePlayerView(interior, filePath);
        insert(animView);
    } else {
        TTextFileView* textView = new TTextFileView(interior, filePath);
        insert(textView);
    }
}

void TFrameAnimationWindow::changeBounds(const TRect& bounds)
{
    TWindow::changeBounds(bounds);

    setState(sfExposed, True);

    forEach([](TView* view, void*) {
        if (auto* textView = dynamic_cast<TTextFileView*>(view))
            textView->drawView();
    }, nullptr);

    redraw();
}

TFrame* TFrameAnimationWindow::initFrame(TRect r)
{
    return new TCGAFrame(r);  // chunky in CGA chrome; standard TFrame otherwise
}

TFrame* TFrameAnimationWindow::initFrameless(TRect r)
{
    return new TGhostFrame(r);
}

void TFrameAnimationWindow::handleEvent(TEvent& event)
{
    if (event.what == evMouseDown && event.mouse.buttons == mbRightButton) {
        bool hasShadow = (state & sfShadow) != 0;
        TMenu* popup = new TMenu(
            *new TMenuItem(hasShadow ? "Shadow ~O~ff" : "Shadow ~O~n",
                cmCtxToggleShadow, kbNoKey, hcNoContext, nullptr,
            new TMenuItem((title && title[0]) ? "Clear ~T~itle" : "Restore ~T~itle",
                cmCtxClearTitle, kbNoKey, hcNoContext, nullptr,
            new TMenuItem(frameless_ ? "Show ~F~rame" : "Hide ~F~rame",
                cmCtxToggleFrame, kbNoKey, hcNoContext, nullptr,
            new TMenuItem("~G~allery Mode",
                cmCtxGalleryToggle, kbNoKey, hcNoContext, nullptr
            )))));

        TRect deskExt = owner->getExtent();
        TRect r(event.mouse.where.x, event.mouse.where.y,
                deskExt.b.x, deskExt.b.y);

        TMenuBox* box = new TMenuBox(r, popup, nullptr);
        ushort cmd = owner->execView(box);
        destroy(box);

        switch (cmd) {
            case cmCtxToggleShadow:
                setState(sfShadow, !hasShadow);
                if (owner) owner->drawView();
                break;
            case cmCtxClearTitle: {
                if (title && title[0]) {
                    savedTitle_ = title;
                    delete[] (char*)title;
                    title = nullptr;
                } else if (!savedTitle_.empty()) {
                    delete[] (char*)title;
                    title = newStr(savedTitle_);
                }
                if (frame) frame->drawView();
                break;
            }
            case cmCtxToggleFrame: {
                if (!frame) break;
                TRect fBounds = frame->getBounds();
                remove(frame);
                destroy(frame);
                if (frameless_) {
                    frame = initFrame(fBounds);
                    frameless_ = false;
                } else {
                    frame = initFrameless(fBounds);
                    frameless_ = true;
                }
                insertBefore(frame, nullptr);
                if (owner) owner->drawView();
                else drawView();
                break;
            }
            case cmCtxGalleryToggle: {
                TEvent galEvt = {};
                galEvt.what = evCommand;
                galEvt.message.command = cmCtxGalleryToggle;
                putEvent(galEvt);
                break;
            }
        }
        clearEvent(event);
        return;
    }
    TWindow::handleEvent(event);
}
