/*---------------------------------------------------------*/
/*   ascii_gallery_view.h — ASCII Art Gallery Browser      */
/*   Tabbed file browser with live preview pane            */
/*---------------------------------------------------------*/

#ifndef ASCII_GALLERY_VIEW_H
#define ASCII_GALLERY_VIEW_H

#define Uses_TWindow
#define Uses_TView
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TEvent
#define Uses_TRect
#include <tvision/tv.h>

#include <string>
#include <vector>

// ── Letter-group tab bar ────────────────────────────────
class TGalleryTabBar : public TView {
public:
    TGalleryTabBar(const TRect& bounds);
    virtual void draw() override;
    virtual void handleEvent(TEvent& event) override;

    int selected;  // 0..NUM_TABS-1
    static const int NUM_TABS = 5;
    static constexpr const char* tabLabels[NUM_TABS] = {
        "#-C", "D-L", "M", "N-S", "T-Z"
    };

    // Returns true if filename's first char belongs to this tab index
    static bool matchesTab(int tabIndex, char firstChar);
};

// ── File list (left pane) ───────────────────────────────
class TGalleryFileList : public TView {
public:
    TGalleryFileList(const TRect& bounds, TScrollBar* aVScrollBar);
    virtual void draw() override;
    virtual void handleEvent(TEvent& event) override;

    void setFiles(const std::vector<std::string>& files);
    int focusedIndex() const { return focused; }
    const std::string& focusedFile() const;

private:
    std::vector<std::string> files;      // display names (basename)
    std::vector<std::string> fullPaths;  // full paths
    int focused;
    int scrollOffset;
    TScrollBar* vScrollBar;

    void adjustScrollBar();
    void ensureFocusVisible();

    friend class TGalleryWindow;
};

// ── Preview pane (right side) ───────────────────────────
// Renders the selected file content (static text preview)
class TGalleryPreview : public TView {
public:
    TGalleryPreview(const TRect& bounds);
    virtual void draw() override;

    void loadFile(const std::string& path);
    void clear();

private:
    std::vector<std::string> lines;
    std::string currentPath;
};

// ── Gallery window ──────────────────────────────────────
class TGalleryWindow : public TWindow {
public:
    TGalleryWindow(const TRect& bounds, const std::string& primerDir);
    virtual void handleEvent(TEvent& event) override;

private:
    TGalleryTabBar* tabBar;
    TGalleryFileList* fileList;
    TGalleryPreview* preview;
    TScrollBar* listScrollBar;

    std::string primerDir;
    std::vector<std::string> allFiles;  // all filenames (basename)
    std::vector<std::string> allPaths;  // all full paths

    void scanFiles();
    void applyFilter();
    void updatePreview();
};

TWindow* createAsciiGalleryWindow(const TRect& bounds);

#endif // ASCII_GALLERY_VIEW_H
