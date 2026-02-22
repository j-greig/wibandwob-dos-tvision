/*---------------------------------------------------------*/
/*   ascii_gallery_view.cpp — ASCII Art Gallery Browser    */
/*---------------------------------------------------------*/

#define Uses_TWindow
#define Uses_TView
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TEvent
#define Uses_TRect
#define Uses_TFrame
#define Uses_TProgram
#include <tvision/tv.h>

#include "ascii_gallery_view.h"
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <cctype>
#include <sys/stat.h>

// Internal commands
static const ushort cmGalleryTabChanged = 9910;
static const ushort cmGalleryFileChanged = 9911;
static const ushort cmGalleryOpenFile = 9912;

// ═══════════════════════════════════════════════════
//  TGalleryTabBar
// ═══════════════════════════════════════════════════

constexpr const char* TGalleryTabBar::tabLabels[];

TGalleryTabBar::TGalleryTabBar(const TRect& bounds)
    : TView(bounds), selected(0)
{
    eventMask |= evMouseDown | evKeyDown;
}

bool TGalleryTabBar::matchesTab(int tabIndex, char firstChar)
{
    char c = (char)std::tolower((unsigned char)firstChar);
    switch (tabIndex) {
        case 0: return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'c');
        case 1: return c >= 'd' && c <= 'l';
        case 2: return c == 'm';
        case 3: return c >= 'n' && c <= 's';
        case 4: return c >= 't' && c <= 'z';
        default: return false;
    }
}

void TGalleryTabBar::draw()
{
    TDrawBuffer b;
    TColorAttr normalAttr = {TColorRGB(0xAA, 0xAA, 0xAA), TColorRGB(0x20, 0x20, 0x20)};
    TColorAttr selectedAttr = {TColorRGB(0xFF, 0xFF, 0xFF), TColorRGB(0x00, 0x70, 0x70)};
    TColorAttr numAttr = {TColorRGB(0xFF, 0xFF, 0x00), TColorRGB(0x20, 0x20, 0x20)};
    TColorAttr numSelAttr = {TColorRGB(0xFF, 0xFF, 0x00), TColorRGB(0x00, 0x70, 0x70)};

    b.moveChar(0, ' ', normalAttr, size.x);

    int x = 1;
    for (int i = 0; i < NUM_TABS && x < size.x - 1; i++) {
        bool sel = (i == selected);
        TColorAttr attr = sel ? selectedAttr : normalAttr;
        TColorAttr nAttr = sel ? numSelAttr : numAttr;

        // Draw " N:Label "
        if (x < size.x) b.moveChar(x, ' ', attr, 1); x++;
        // Number key hint
        char num = '1' + i;
        if (x < size.x) b.moveChar(x, num, nAttr, 1); x++;
        if (x < size.x) b.moveChar(x, ':', attr, 1); x++;

        const char* label = tabLabels[i];
        int len = (int)strlen(label);
        for (int j = 0; j < len && x < size.x; j++) {
            b.moveChar(x, label[j], attr, 1);
            x++;
        }
        if (x < size.x) b.moveChar(x, ' ', attr, 1); x++;
        // Gap between tabs
        if (x < size.x) b.moveChar(x, ' ', normalAttr, 1); x++;
    }

    writeLine(0, 0, size.x, 1, b);
}

void TGalleryTabBar::handleEvent(TEvent& event)
{
    TView::handleEvent(event);

    if (event.what == evMouseDown) {
        // Determine which tab was clicked by x position
        int x = 1;
        for (int i = 0; i < NUM_TABS; i++) {
            int tabWidth = 4 + (int)strlen(tabLabels[i]) + 1; // " N:Label "
            int gap = 1;
            if (event.mouse.where.x - origin.x >= x &&
                event.mouse.where.x - origin.x < x + tabWidth) {
                if (i != selected) {
                    selected = i;
                    drawView();
                    message(owner, evCommand, cmGalleryTabChanged, this);
                }
                clearEvent(event);
                return;
            }
            x += tabWidth + gap;
        }
    }

    if (event.what == evKeyDown) {
        int newSel = -1;
        switch (event.keyDown.keyCode) {
            case kbTab:
                newSel = (selected + 1) % NUM_TABS;
                break;
            case kbShiftTab:
                newSel = (selected + NUM_TABS - 1) % NUM_TABS;
                break;
            default:
                // Number keys 1-5
                if (event.keyDown.charScan.charCode >= '1' &&
                    event.keyDown.charScan.charCode <= '0' + NUM_TABS) {
                    newSel = event.keyDown.charScan.charCode - '1';
                }
                break;
        }
        if (newSel >= 0 && newSel != selected) {
            selected = newSel;
            drawView();
            message(owner, evCommand, cmGalleryTabChanged, this);
            clearEvent(event);
        }
    }
}

// ═══════════════════════════════════════════════════
//  TGalleryFileList
// ═══════════════════════════════════════════════════

static const std::string emptyStr;

TGalleryFileList::TGalleryFileList(const TRect& bounds, TScrollBar* aVScrollBar)
    : TView(bounds), focused(0), scrollOffset(0), vScrollBar(aVScrollBar)
{
    eventMask |= evMouseDown | evKeyDown;
    growMode = gfGrowHiY;
}

const std::string& TGalleryFileList::focusedFile() const
{
    if (focused >= 0 && focused < (int)fullPaths.size())
        return fullPaths[focused];
    return emptyStr;
}

void TGalleryFileList::setFiles(const std::vector<std::string>& names)
{
    // names contains pairs: display name at even indices, full path at odd
    // Actually let's use a simpler approach
    files = names;
    focused = 0;
    scrollOffset = 0;
    adjustScrollBar();
    drawView();
}

void TGalleryFileList::adjustScrollBar()
{
    if (vScrollBar) {
        vScrollBar->setParams(scrollOffset, 0,
            std::max(0, (int)files.size() - size.y),
            size.y - 1, 1);
    }
}

void TGalleryFileList::ensureFocusVisible()
{
    if (focused < scrollOffset)
        scrollOffset = focused;
    else if (focused >= scrollOffset + size.y)
        scrollOffset = focused - size.y + 1;
    adjustScrollBar();
}

void TGalleryFileList::draw()
{
    TColorAttr normalAttr = {TColorRGB(0xCC, 0xCC, 0xCC), TColorRGB(0x10, 0x10, 0x10)};
    TColorAttr focusedAttr = {TColorRGB(0xFF, 0xFF, 0xFF), TColorRGB(0x00, 0x50, 0x80)};
    TColorAttr animAttr = {TColorRGB(0x80, 0xFF, 0x80), TColorRGB(0x10, 0x10, 0x10)};
    TColorAttr animFocAttr = {TColorRGB(0x80, 0xFF, 0x80), TColorRGB(0x00, 0x50, 0x80)};

    for (int y = 0; y < size.y; y++) {
        TDrawBuffer b;
        int idx = scrollOffset + y;
        if (idx < (int)files.size()) {
            bool isFocused = (idx == focused);
            // Check if animated (has "----" delimiter) - indicate with marker
            bool isAnim = false;
            if (idx < (int)fullPaths.size()) {
                const std::string& p = fullPaths[idx];
                // Quick check: animated files tend to be larger
                // We'll mark with ▶ prefix for animated, · for static
                // (actual check done at scan time via flag)
            }
            TColorAttr attr = isFocused ? focusedAttr : normalAttr;

            b.moveChar(0, ' ', attr, size.x);
            // Draw prefix + filename
            const std::string& name = files[idx];
            int maxLen = size.x - 2;
            for (int i = 0; i < (int)name.size() && i < maxLen; i++) {
                b.moveChar(i + 1, name[i], attr, 1);
            }
        } else {
            b.moveChar(0, ' ', normalAttr, size.x);
        }
        writeLine(0, y, size.x, 1, b);
    }
}

void TGalleryFileList::handleEvent(TEvent& event)
{
    TView::handleEvent(event);

    if (event.what == evMouseDown) {
        int clickY = event.mouse.where.y - origin.y;
        int idx = scrollOffset + clickY;
        if (idx >= 0 && idx < (int)files.size()) {
            focused = idx;
            drawView();
            message(owner, evCommand, cmGalleryFileChanged, this);
            // Double-click to open
            if (event.mouse.eventFlags & meDoubleClick) {
                message(owner, evCommand, cmGalleryOpenFile, this);
            }
            clearEvent(event);
        }
        return;
    }

    if (event.what == evKeyDown) {
        int oldFocused = focused;
        switch (event.keyDown.keyCode) {
            case kbUp:
                if (focused > 0) focused--;
                break;
            case kbDown:
                if (focused < (int)files.size() - 1) focused++;
                break;
            case kbPgUp:
                focused = std::max(0, focused - size.y);
                break;
            case kbPgDn:
                focused = std::min((int)files.size() - 1, focused + size.y);
                break;
            case kbHome:
                focused = 0;
                break;
            case kbEnd:
                focused = std::max(0, (int)files.size() - 1);
                break;
            case kbEnter:
                message(owner, evCommand, cmGalleryOpenFile, this);
                clearEvent(event);
                return;
            default:
                return; // Don't consume
        }
        if (focused != oldFocused) {
            ensureFocusVisible();
            drawView();
            message(owner, evCommand, cmGalleryFileChanged, this);
            clearEvent(event);
        }
    }

    // Scroll bar changes
    if (event.what == evBroadcast && event.message.command == cmScrollBarChanged) {
        if (event.message.infoPtr == vScrollBar && vScrollBar) {
            scrollOffset = vScrollBar->value;
            drawView();
            clearEvent(event);
        }
    }
}

// ═══════════════════════════════════════════════════
//  TGalleryPreview
// ═══════════════════════════════════════════════════

TGalleryPreview::TGalleryPreview(const TRect& bounds)
    : TView(bounds)
{
    growMode = gfGrowHiX | gfGrowHiY;
}

void TGalleryPreview::clear()
{
    lines.clear();
    currentPath.clear();
    drawView();
}

void TGalleryPreview::loadFile(const std::string& path)
{
    if (path == currentPath) return;
    currentPath = path;
    lines.clear();

    std::ifstream f(path);
    if (!f) {
        lines.push_back("(cannot open file)");
        drawView();
        return;
    }

    std::string line;
    // Read up to first frame only (stop at "----" delimiter)
    int lineCount = 0;
    while (std::getline(f, line) && lineCount < 500) {
        // Strip CR
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        // Stop at frame delimiter — show only first frame
        if (line == "----")
            break;
        lines.push_back(std::move(line));
        lineCount++;
    }
    drawView();
}

void TGalleryPreview::draw()
{
    TColorAttr textAttr = {TColorRGB(0xDD, 0xDD, 0xDD), TColorRGB(0x00, 0x00, 0x00)};
    TColorAttr emptyAttr = {TColorRGB(0x40, 0x40, 0x40), TColorRGB(0x00, 0x00, 0x00)};

    for (int y = 0; y < size.y; y++) {
        TDrawBuffer b;
        if (y < (int)lines.size()) {
            b.moveChar(0, ' ', textAttr, size.x);
            const std::string& line = lines[y];
            for (int x = 0; x < (int)line.size() && x < size.x; x++) {
                b.moveChar(x, line[x], textAttr, 1);
            }
        } else if (lines.empty() && y == size.y / 2) {
            // Empty state message
            const char* msg = "Select a file to preview";
            b.moveChar(0, ' ', emptyAttr, size.x);
            int startX = std::max(0, (size.x - (int)strlen(msg)) / 2);
            for (int i = 0; msg[i] && startX + i < size.x; i++) {
                b.moveChar(startX + i, msg[i], emptyAttr, 1);
            }
        } else {
            b.moveChar(0, ' ', textAttr, size.x);
        }
        writeLine(0, y, size.x, 1, b);
    }
}

// ═══════════════════════════════════════════════════
//  TGalleryWindow
// ═══════════════════════════════════════════════════

TGalleryWindow::TGalleryWindow(const TRect& bounds, const std::string& aPrimerDir)
    : TWindowInit(&TWindow::initFrame),
      TWindow(bounds, "ASCII Gallery", wnNoNumber),
      primerDir(aPrimerDir)
{
    options |= ofTileable;

    TRect interior = getExtent();
    interior.grow(-1, -1); // Inside frame

    // Tab bar at top (1 row)
    TRect tabR(interior.a.x, interior.a.y, interior.b.x, interior.a.y + 1);
    tabBar = new TGalleryTabBar(tabR);
    insert(tabBar);

    // Split: file list on left (~30 cols), preview on right
    int listWidth = std::min(32, interior.b.x / 3);

    // Scroll bar for file list
    TRect sbR(interior.a.x + listWidth - 1, interior.a.y + 1, interior.a.x + listWidth, interior.b.y);
    listScrollBar = new TScrollBar(sbR);
    insert(listScrollBar);

    // File list
    TRect listR(interior.a.x, interior.a.y + 1, interior.a.x + listWidth - 1, interior.b.y);
    fileList = new TGalleryFileList(listR, listScrollBar);
    insert(fileList);

    // Separator line would be nice but we'll just use the scrollbar as divider

    // Preview pane
    TRect prevR(interior.a.x + listWidth, interior.a.y + 1, interior.b.x, interior.b.y);
    preview = new TGalleryPreview(prevR);
    insert(preview);

    // Scan files and apply initial filter
    scanFiles();
    applyFilter();

    // Focus the file list
    fileList->select();
}

void TGalleryWindow::scanFiles()
{
    allFiles.clear();
    allPaths.clear();

    DIR* dir = opendir(primerDir.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string name = entry->d_name;
        // Only .txt files
        if (name.size() < 5) continue;
        std::string ext = name.substr(name.size() - 4);
        if (ext != ".txt") continue;

        allFiles.push_back(name);
        allPaths.push_back(primerDir + "/" + name);
    }
    closedir(dir);

    // Sort alphabetically
    // Build index pairs for synchronized sort
    std::vector<size_t> indices(allFiles.size());
    for (size_t i = 0; i < indices.size(); i++) indices[i] = i;
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return allFiles[a] < allFiles[b];
    });

    std::vector<std::string> sortedFiles, sortedPaths;
    sortedFiles.reserve(allFiles.size());
    sortedPaths.reserve(allPaths.size());
    for (size_t i : indices) {
        sortedFiles.push_back(allFiles[i]);
        sortedPaths.push_back(allPaths[i]);
    }
    allFiles = std::move(sortedFiles);
    allPaths = std::move(sortedPaths);
}

void TGalleryWindow::applyFilter()
{
    int tabIdx = tabBar->selected;
    std::vector<std::string> filtered;
    std::vector<std::string> filteredPaths;

    for (size_t i = 0; i < allFiles.size(); i++) {
        if (!allFiles[i].empty() &&
            TGalleryTabBar::matchesTab(tabIdx, allFiles[i][0])) {
            filtered.push_back(allFiles[i]);
            filteredPaths.push_back(allPaths[i]);
        }
    }

    fileList->files = filtered;
    fileList->fullPaths = filteredPaths;
    fileList->focused = 0;
    fileList->scrollOffset = 0;
    fileList->adjustScrollBar();
    fileList->drawView();

    updatePreview();
}

void TGalleryWindow::updatePreview()
{
    const std::string& path = fileList->focusedFile();
    if (path.empty())
        preview->clear();
    else
        preview->loadFile(path);
}

void TGalleryWindow::handleEvent(TEvent& event)
{
    TWindow::handleEvent(event);

    if (event.what == evCommand) {
        switch (event.message.command) {
            case cmGalleryTabChanged:
                applyFilter();
                clearEvent(event);
                break;
            case cmGalleryFileChanged:
                updatePreview();
                clearEvent(event);
                break;
            case cmGalleryOpenFile: {
                // Open the focused file in a proper viewer window
                // Broadcast to app to handle (cmOpenAnimation with path)
                const std::string& path = fileList->focusedFile();
                if (!path.empty()) {
                    // Store path for app to pick up
                    // We'll use a simple approach: post a broadcast with the path pointer
                    // The app's handleEvent will call openAnimationFilePath()
                    message(TProgram::application, evCommand, 109 /* cmOpenAnimation */,
                            (void*)path.c_str());
                }
                clearEvent(event);
                break;
            }
        }
    }

    // Number keys 1-5 for tab switching when file list has focus
    if (event.what == evKeyDown) {
        char ch = event.keyDown.charScan.charCode;
        if (ch >= '1' && ch <= '0' + TGalleryTabBar::NUM_TABS) {
            int newTab = ch - '1';
            if (newTab != tabBar->selected) {
                tabBar->selected = newTab;
                tabBar->drawView();
                applyFilter();
                clearEvent(event);
            }
        } else if (event.keyDown.keyCode == kbTab) {
            tabBar->selected = (tabBar->selected + 1) % TGalleryTabBar::NUM_TABS;
            tabBar->drawView();
            applyFilter();
            clearEvent(event);
        } else if (event.keyDown.keyCode == kbShiftTab) {
            tabBar->selected = (tabBar->selected + TGalleryTabBar::NUM_TABS - 1) % TGalleryTabBar::NUM_TABS;
            tabBar->drawView();
            applyFilter();
            clearEvent(event);
        }
    }
}

// ═══════════════════════════════════════════════════
//  Factory
// ═══════════════════════════════════════════════════

// Find primer directory (same logic as test_pattern_app.cpp)
static std::string galleryFindPrimerDir()
{
    const char* moduleDirs[] = { "modules-private", "modules" };
    for (const char* base : moduleDirs) {
        DIR* dir = opendir(base);
        if (!dir) continue;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            std::string candidate = std::string(base) + "/" + entry->d_name + "/primers";
            struct stat st;
            if (stat(candidate.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                closedir(dir);
                return candidate;
            }
        }
        closedir(dir);
    }
    struct stat st;
    if (stat("app/primers", &st) == 0 && S_ISDIR(st.st_mode))
        return "app/primers";
    return "primers";
}

TWindow* createAsciiGalleryWindow(const TRect& bounds)
{
    std::string primerDir = galleryFindPrimerDir();
    return new TGalleryWindow(bounds, primerDir);
}
