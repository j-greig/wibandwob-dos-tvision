/*---------------------------------------------------------*/
/*                                                         */
/*   workspace_manager_dialog.cpp - Manage Workspaces dialog */
/*   (list + live preview + load/rename/delete). Moved      */
/*   verbatim from wwdos_app.cpp (monolith split stage 7a).  */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TView
#define Uses_TWindow
#define Uses_TDialog
#define Uses_TDrawBuffer
#define Uses_TListBox
#define Uses_TScrollBar
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TStringCollection
#define Uses_MsgBox
#define Uses_TRect
#define Uses_TEvent
#define Uses_TColorAttr
#define Uses_TKeys
#include <tvision/tv.h>

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <cctype>
#include <cstdio>

#include "wwdos_app.h"
#include "workspace_io.h"
#include "ui/ui_helpers.h"

struct WsPreviewWindow {
    std::string type;
    int x, y, w, h;
};

static std::vector<WsPreviewWindow> parseWorkspaceWindows(const std::string& path)
{
    std::vector<WsPreviewWindow> wins;
    std::ifstream in(path);
    if (!in) return wins;
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // Find canvas dimensions
    int canvasW = 80, canvasH = 24;
    {
        size_t cpos = data.find("\"canvas\"");
        if (cpos != std::string::npos) {
            size_t brace = data.find('{', cpos);
            if (brace != std::string::npos) {
                TWwdosApp::parseKeyedNumber(data, brace+1, "width", canvasW);
                TWwdosApp::parseKeyedNumber(data, brace+1, "height", canvasH);
            }
        }
    }

    // Parse windows array
    size_t wpos = data.find("\"windows\"");
    if (wpos == std::string::npos) return wins;

    size_t arrStart = data.find('[', wpos);
    if (arrStart == std::string::npos) return wins;

    size_t pos = arrStart;
    while (true) {
        size_t objStart = data.find('{', pos);
        if (objStart == std::string::npos) break;
        size_t objEnd = data.find('}', objStart);
        if (objEnd == std::string::npos) break;

        // Find nested props brace and skip past it
        std::string obj = data.substr(objStart, objEnd - objStart + 1);
        // Handle nested braces by finding proper end
        int depth = 0;
        size_t realEnd = objStart;
        for (size_t i = objStart; i < data.size(); i++) {
            if (data[i] == '{') depth++;
            else if (data[i] == '}') { depth--; if (depth == 0) { realEnd = i; break; } }
        }
        obj = data.substr(objStart, realEnd - objStart + 1);

        WsPreviewWindow pw;
        pw.x = 0; pw.y = 0; pw.w = 20; pw.h = 10;
        TWwdosApp::parseKeyedString(obj, 0, "type", pw.type);
        TWwdosApp::parseKeyedNumber(obj, 0, "x", pw.x);
        TWwdosApp::parseKeyedNumber(obj, 0, "y", pw.y);
        TWwdosApp::parseKeyedNumber(obj, 0, "w", pw.w);
        TWwdosApp::parseKeyedNumber(obj, 0, "h", pw.h);
        wins.push_back(pw);

        pos = realEnd + 1;
    }
    return wins;
}

// TView that renders a miniature workspace layout preview
class TWorkspacePreview : public TView {
public:
    TWorkspacePreview(const TRect& bounds) : TView(bounds) {
        growMode = gfGrowHiX | gfGrowHiY;
    }

    void setWindows(const std::vector<WsPreviewWindow>& wins, int canvasW = 80, int canvasH = 24) {
        windows_ = wins;
        canvasW_ = canvasW;
        canvasH_ = canvasH;
        drawView();
    }

    virtual void draw() override {
        TDrawBuffer b;
        TColorAttr bgAttr = TColorAttr(TColorRGB(0x55, 0x55, 0x55), TColorRGB(0x11, 0x11, 0x11));
        TColorAttr winAttr = TColorAttr(TColorRGB(0xFF, 0xFF, 0xFF), TColorRGB(0x33, 0x33, 0x55));
        TColorAttr borderAttr = TColorAttr(TColorRGB(0x88, 0x88, 0xCC), TColorRGB(0x33, 0x33, 0x55));
        TColorAttr labelAttr = TColorAttr(TColorRGB(0xFF, 0xFF, 0x88), TColorRGB(0x33, 0x33, 0x55));

        // Build a character grid
        int pw = size.x, ph = size.y;
        std::vector<std::string> grid(ph, std::string(pw, ' '));
        std::vector<std::vector<TColorAttr>> colors(ph, std::vector<TColorAttr>(pw, bgAttr));

        float sx = (canvasW_ > 0) ? (float)pw / canvasW_ : 1.0f;
        float sy = (canvasH_ > 0) ? (float)ph / canvasH_ : 1.0f;

        for (const auto& w : windows_) {
            int x0 = (int)(w.x * sx);
            int y0 = (int)(w.y * sy);
            int x1 = (int)((w.x + w.w) * sx);
            int y1 = (int)((w.y + w.h) * sy);
            // Clamp
            if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
            if (x1 > pw) x1 = pw; if (y1 > ph) y1 = ph;
            if (x1 <= x0 || y1 <= y0) continue;

            // Fill interior
            for (int row = y0; row < y1; row++)
                for (int col = x0; col < x1; col++) {
                    grid[row][col] = ' ';
                    colors[row][col] = winAttr;
                }

            // Draw border
            for (int col = x0; col < x1; col++) {
                grid[y0][col] = '\xC4'; colors[y0][col] = borderAttr;  // ─
                if (y1-1 < ph) { grid[y1-1][col] = '\xC4'; colors[y1-1][col] = borderAttr; }
            }
            for (int row = y0; row < y1; row++) {
                grid[row][x0] = '\xB3'; colors[row][x0] = borderAttr;  // │
                if (x1-1 < pw) { grid[row][x1-1] = '\xB3'; colors[row][x1-1] = borderAttr; }
            }
            // Corners
            if (y0 < ph && x0 < pw) { grid[y0][x0] = '\xDA'; colors[y0][x0] = borderAttr; }
            if (y0 < ph && x1-1 < pw) { grid[y0][x1-1] = '\xBF'; colors[y0][x1-1] = borderAttr; }
            if (y1-1 < ph && x0 < pw) { grid[y1-1][x0] = '\xC0'; colors[y1-1][x0] = borderAttr; }
            if (y1-1 < ph && x1-1 < pw) { grid[y1-1][x1-1] = '\xD9'; colors[y1-1][x1-1] = borderAttr; }

            // Type label inside (truncated)
            if (y1 - y0 > 2 && x1 - x0 > 2) {
                std::string label = w.type;
                int maxLen = x1 - x0 - 2;
                if ((int)label.size() > maxLen) label = label.substr(0, maxLen);
                int lx = x0 + 1;
                int ly = y0 + 1;
                for (int i = 0; i < (int)label.size() && lx + i < pw; i++) {
                    grid[ly][lx + i] = label[i];
                    colors[ly][lx + i] = labelAttr;
                }
            }
        }

        // Render grid to draw buffers
        for (int row = 0; row < ph; row++) {
            b.moveChar(0, ' ', bgAttr, pw);
            for (int col = 0; col < pw; col++)
                b.moveChar(col, grid[row][col], colors[row][col], 1);
            writeLine(0, row, pw, 1, b);
        }
    }

private:
    std::vector<WsPreviewWindow> windows_;
    int canvasW_ = 80, canvasH_ = 24;
};

// ── Manage Workspaces dialog ──────────────────────────────────────────────────

static const ushort cmWsLoad   = 281;
static const ushort cmWsRename = 282;
static const ushort cmWsDelete = 283;

// Custom dialog that tracks list focus and updates preview live
class TManageWorkspacesDialog : public TDialog {
public:
    TListBox* listBox;
    TWorkspacePreview* preview;
    std::vector<std::string> paths;
    std::vector<std::string> labels;
    int lastFocused = -1;

    TManageWorkspacesDialog(const TRect& bounds, const char* title)
        : TDialog(bounds, title), TWindowInit(&TDialog::initFrame),
          listBox(nullptr), preview(nullptr) {}

    void updatePreview() {
        int sel = listBox ? listBox->focused : -1;
        if (sel == lastFocused || sel < 0 || sel >= (int)paths.size()) return;
        lastFocused = sel;
        if (preview) {
            auto wins = parseWorkspaceWindows(paths[sel]);
            preview->setWindows(wins);
        }
    }

    void rebuildList(int focusIdx = 0) {
        if (!listBox) return;
        TStringCollection* col = makeStringCollection(labels);
        listBox->newList(col);
        if (focusIdx >= (int)labels.size()) focusIdx = (int)labels.size() - 1;
        if (focusIdx < 0) focusIdx = 0;
        listBox->focusItem(focusIdx);
        lastFocused = -1;  // force preview refresh
        updatePreview();
    }

    virtual void handleEvent(TEvent& event) override {
        // Before processing, snapshot focus to detect change after
        int beforeFocus = listBox ? listBox->focused : -1;

        TDialog::handleEvent(event);

        if (event.what == evCommand) {
            switch (event.message.command) {
                case cmWsLoad:
                case cmWsRename:
                case cmWsDelete:
                    if ((state & sfModal) != 0) {
                        endModal(event.message.command);
                        clearEvent(event);
                    }
                    break;
            }
        }

        // After any event, check if list focus changed
        if (listBox && listBox->focused != beforeFocus) {
            updatePreview();
        }
    }
};

void TWwdosApp::manageWorkspaces()
{
    std::vector<std::string> paths = scanRecentWorkspacePaths("workspaces", 50);
    if (paths.empty()) {
        messageBox("No saved workspaces found.", mfInformation | mfOKButton);
        return;
    }

    std::vector<std::string> labels;
    for (const auto& p : paths) {
        std::string label = recentWorkspaceLabel(p);
        int n = countWindowsInWorkspace(p);
        if (n > 0) label += " (" + std::to_string(n) + ")";
        labels.push_back(label);
    }

    int dlgW = 72, dlgH = 22;
    auto* dlg = new TManageWorkspacesDialog(TRect(0, 0, dlgW, dlgH), "Manage Workspaces");
    dlg->options |= ofCentered;
    dlg->paths = paths;
    dlg->labels = labels;

    // List box (left)
    int listW = 32;
    TScrollBar* sb = new TScrollBar(TRect(listW - 1, 2, listW, dlgH - 4));
    dlg->insert(sb);
    TListBox* lb = new TListBox(TRect(2, 2, listW - 1, dlgH - 4), 1, sb);
    dlg->insert(lb);
    dlg->listBox = lb;

    // Preview (right)
    auto* preview = new TWorkspacePreview(TRect(listW + 1, 2, dlgW - 2, dlgH - 4));
    dlg->insert(preview);
    dlg->preview = preview;

    // Populate and show initial preview
    dlg->rebuildList(0);

    // Buttons
    int btnY = dlgH - 3;
    dlg->insert(new TButton(TRect(2, btnY, 14, btnY + 2), "~L~oad", cmWsLoad, bfDefault));
    dlg->insert(new TButton(TRect(15, btnY, 29, btnY + 2), "~R~ename", cmWsRename, bfNormal));
    dlg->insert(new TButton(TRect(30, btnY, 44, btnY + 2), "~D~elete", cmWsDelete, bfNormal));
    dlg->insert(new TButton(TRect(45, btnY, 57, btnY + 2), "~C~lose", cmCancel, bfNormal));

    // Dialog loop
    bool done = false;
    while (!done) {
        ushort cmd = execView(dlg);

        switch (cmd) {
            case cmWsLoad: {
                int sel = lb->focused;
                if (sel >= 0 && sel < (int)dlg->paths.size()) {
                    std::string loadPath = dlg->paths[sel];
                    destroy(dlg);
                    loadWorkspaceFromFile(loadPath);
                    return;
                }
                break;
            }
            case cmWsRename: {
                int sel = lb->focused;
                if (sel < 0 || sel >= (int)dlg->paths.size()) break;
                std::string oldName = recentWorkspaceLabel(dlg->paths[sel]);
                if (oldName.size() > 5 && oldName.substr(oldName.size() - 5) == ".json")
                    oldName = oldName.substr(0, oldName.size() - 5);
                size_t paren = oldName.find(" (");
                if (paren != std::string::npos) oldName = oldName.substr(0, paren);

                char name[256] = {};
                std::strncpy(name, oldName.c_str(), sizeof(name) - 1);
                if (inputBox("Rename Workspace", "New ~n~ame:", name, sizeof(name) - 1) != cmOK)
                    break;

                std::string newName(name);
                if (newName.empty() || newName == oldName) break;
                for (char& c : newName)
                    if (!std::isalnum(c) && c != '-' && c != '_' && c != '.') c = '_';

                std::string newPath = "workspaces/" + newName + ".json";
                if (std::rename(dlg->paths[sel].c_str(), newPath.c_str()) == 0) {
                    dlg->paths[sel] = newPath;
                    std::string newLabel = newName + ".json";
                    int n = countWindowsInWorkspace(newPath);
                    if (n > 0) newLabel += " (" + std::to_string(n) + ")";
                    dlg->labels[sel] = newLabel;
                    dlg->rebuildList(sel);
                    recentWorkspaces_ = scanRecentWorkspacePaths("workspaces", kMaxRecentWorkspaces);
                }
                break;
            }
            case cmWsDelete: {
                int sel = lb->focused;
                if (sel < 0 || sel >= (int)dlg->paths.size()) break;
                std::string fname = recentWorkspaceLabel(dlg->paths[sel]);
                std::string msg = "Delete " + fname + "?";
                if (messageBox(msg.c_str(), mfConfirmation | mfYesButton | mfNoButton) != cmYes)
                    break;
                std::remove(dlg->paths[sel].c_str());
                dlg->paths.erase(dlg->paths.begin() + sel);
                dlg->labels.erase(dlg->labels.begin() + sel);
                if (dlg->paths.empty()) {
                    destroy(dlg);
                    recentWorkspaces_ = scanRecentWorkspacePaths("workspaces", kMaxRecentWorkspaces);
                    return;
                }
                dlg->rebuildList(sel);
                recentWorkspaces_ = scanRecentWorkspacePaths("workspaces", kMaxRecentWorkspaces);
                break;
            }
            case cmCancel:
            default:
                done = true;
                break;
        }
    }
    destroy(dlg);
}


