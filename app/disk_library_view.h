/*-----------------------------------------------------------*/
/*   disk_library_view.h — SYMBIENT SHAREWARE LIBRARY        */
/*   Floppy-disk launcher: each 3.5" disk = one app/command  */
/*   Double-click or Enter = boot the disk.                  */
/*   Design ref: design/figma-refs/wibwob-disks.png +        */
/*   disk-detail-153-7006.png (Figma `floppy-disk-3.5`)      */
/*-----------------------------------------------------------*/

#ifndef DISK_LIBRARY_VIEW_H
#define DISK_LIBRARY_VIEW_H

#define Uses_TWindow
#define Uses_TView
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TKeys
#include <tvision/tv.h>

#include <map>
#include <string>
#include <vector>

// One floppy in the library. Colours are CGA indices 0-15 (rendered as
// authentic RGB via ThemeManager::cgaColor, immune to terminal palettes).
struct DiskDef {
    std::string title;      // label title line (centred on the label)
    std::vector<std::string> art;  // up to 4 label art lines (UTF-8, centred)
    int bodyIdx;            // disk shell colour
    int labelBg, labelFg;   // label sticker colours
    std::string command;    // command-registry name to execute on boot
    std::map<std::string, std::string> args;  // registry args
};

class TDiskLibraryView : public TView {
public:
    TDiskLibraryView(const TRect& bounds, TScrollBar* aScrollBar);

    virtual void draw() override;
    virtual void handleEvent(TEvent& event) override;

    std::vector<DiskDef> disks;
    void bootFocused();

private:
    int focused = 0;
    int scrollOffset = 0;   // in disk rows
    TScrollBar* vScrollBar;

    // Fixed header: WIBWOB figlet logo + film-sprocket strip (Figma parity)
    std::vector<std::string> logo_;
    int headerRows_ = 0;

    // Cell geometry: disk art is DISK_W x DISK_H, cell adds padding.
    static const int DISK_W = 22;
    static const int DISK_H = 11;
    static const int CELL_W = DISK_W + 2;
    static const int CELL_H = DISK_H + 1;

    int cols() const;
    int rowsTotal() const;
    int visibleRows() const;
    void ensureFocusVisible();
    void adjustScrollBar();
    void drawDisk(const DiskDef& d, int x0, int y0, bool selected);
};

class TDiskLibraryWindow : public TWindow {
public:
    TDiskLibraryWindow(const TRect& bounds);
private:
    TDiskLibraryView* grid;
    void populateDisks();
};

TWindow* createDiskLibraryWindow(const TRect& bounds);
bool isDiskLibraryWindow(TWindow* w);

#endif // DISK_LIBRARY_VIEW_H
