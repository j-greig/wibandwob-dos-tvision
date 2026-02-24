/*---------------------------------------------------------*/
/*   figlet_stamp_dialog.h — FIGlet stamp picker dialog    */
/*   Text input + font list + live preview                 */
/*---------------------------------------------------------*/
#ifndef FIGLET_STAMP_DIALOG_H
#define FIGLET_STAMP_DIALOG_H

#define Uses_TDialog
#define Uses_TInputLine
#define Uses_TListBox
#define Uses_TScrollBar
#define Uses_TView
#define Uses_TButton
#define Uses_TLabel
#define Uses_TStringCollection
#include <tvision/tv.h>

#include <string>
#include <vector>

// ── Preview pane ──────────────────────────────────────────

class TFigletPreview : public TView {
public:
    TFigletPreview(const TRect& bounds);
    void draw() override;
    void setLines(const std::vector<std::string>& lines);
private:
    std::vector<std::string> lines_;
};

// ── Stamp dialog ──────────────────────────────────────────

struct FigletStampResult {
    std::string text;
    std::string font;
    bool ok = false;
};

class TFigletStampDialog : public TDialog {
public:
    TFigletStampDialog(const std::string& initialText,
                       const std::string& initialFont);
    void handleEvent(TEvent& event) override;
    FigletStampResult getResult() const;

private:
    void updatePreview();
    int findFontIndex(const std::string& name);

    TInputLine*       textInput_;
    TListBox*         fontList_;
    TScrollBar*       fontScroll_;
    TFigletPreview*   preview_;
    TStringCollection* fontNames_;

    std::string currentFont_;
    int lastFontIdx_ = -1;
    char lastText_[256] = {};
};

#endif // FIGLET_STAMP_DIALOG_H
