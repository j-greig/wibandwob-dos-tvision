/*---------------------------------------------------------*/
/*                                                         */
/*   app_chrome.cpp - menu bar / status line chrome:         */
/*   TCustomMenuBar (animated kaomoji), TCustomStatusLine    */
/*   (LLM + API indicators), the Recent Workspaces submenu   */
/*   builder, the FIGlet font submenu builder, and           */
/*   TWwdosApp::initMenuBar/initStatusLine/initDeskTop.       */
/*   Moved verbatim from wwdos_app.cpp                       */
/*   (monolith split stage 8).                                */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TKeys
#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TMenuBox
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TMenu
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TColorAttr
#define Uses_TBackground
#include <tvision/tv.h>

#include <string>
#include <vector>
#include <cstring>
#include <chrono>
#include <cstdlib>

#include "app_chrome.h"
#include "wwdos_app.h"
#include "wwdos_commands.h"
#include "workspace_io.h"
#include "theme_manager.h"
#include "wibwob_background.h"
#include "test_pattern.h"
#include "figlet_text_view.h"
#include "figlet_utils.h"
#include "llm/base/auth_config.h"
#include "api_ipc.h"
#include "room_chat_view.h"

static TMenuItem* buildRecentWorkspacesSubmenuItem()
{
    std::vector<std::string> recents = scanRecentWorkspacePaths("workspaces", kMaxRecentWorkspaces);
    TMenuItem* items = nullptr;
    if (recents.empty()) {
        items = new TMenuItem("(none)", cmNoOp, kbNoKey);  // must be non-zero; 0 = "has submenu" → null deref crash
    } else {
        for (int i = (int)recents.size() - 1; i >= 0; --i) {
            std::string label = recentWorkspaceLabel(recents[i]);
            char* labelStr = new char[label.size() + 1];
            std::strcpy(labelStr, label.c_str());
            items = new TMenuItem(labelStr,
                                  cmRecentWorkspace + i,
                                  kbNoKey, hcNoContext, nullptr, items);
        }
    }
    return new TMenuItem("~R~ecent >", kbNoKey, new TMenu(*items), hcNoContext, nullptr);
}

class TCustomMenuBar : public TMenuBar
{
public:
    enum KaomojiMood {
        NEUTRAL,      // つ◕‿◕‿◕༽つ - Default
        EXCITED,      // つ◉‿◉‿◉༽つ - Tool use, window spawning
        THINKING,     // つ●‿●‿●༽つ - LLM processing
        SLEEPY,       // つ◡‿◡‿◡༽つ - Idle, blinking
        CURIOUS,      // つ○‿○‿○༽つ - User input
        MEMORY,       // つ■‿■‿■༽つ - Symbient memory tool
        GEOMETRIC,    // つ□‿□‿□༽つ - Geometric tool/pattern mode
        SURPRISED     // つ◎‿◎‿◎༽つ - Errors, unexpected events
    };

    TCustomMenuBar(const TRect& bounds, TMenu* aMenu) : TMenuBar(bounds, aMenu) {
        // Start blink timer (3-6 seconds between blinks)
        scheduleNextBlink();
    }
    TCustomMenuBar(const TRect& bounds, TSubMenu& aMenu) : TMenuBar(bounds, aMenu) {
        scheduleNextBlink();
    }

    virtual TColorAttr mapColor(uchar index) noexcept override
    {
        TColorRGB trueBlack(0, 0, 0);
        TColorRGB trueWhite(255, 255, 255);

        switch(index) {
            case 1:  case 3:  case 4:  case 6: {
                TColorAttr skinAttr;
                if (ThemeManager::tryAttr(SkinRole::Bar, skinAttr)) return skinAttr;
                return TColorAttr(trueBlack, trueWhite);
            }
            case 2:  case 5: {
                TColorAttr skinAttr;
                if (ThemeManager::tryAttr(SkinRole::BarSel, skinAttr)) return skinAttr;
                return TMenuBar::mapColor(index);
            }
            default:
                return TMenuBar::mapColor(index);
        }
    }

    virtual void draw() override
    {
        TMenuBar::draw();

        // Update blink state
        auto now = std::chrono::steady_clock::now();
        if (now >= nextBlinkTime && currentMood == NEUTRAL) {
            isBlinking = true;
            blinkStartTime = now;
        }

        // End blink after 150ms
        if (isBlinking && std::chrono::duration_cast<std::chrono::milliseconds>(now - blinkStartTime).count() > 150) {
            isBlinking = false;
            scheduleNextBlink();
        }

        // Get kaomoji based on current mood and blink state
        const char* kaomoji = getKaomojiForState();
        int kaomojiWidth = 12;
        int xPos = size.x - kaomojiWidth;

        if (xPos > 1) {
            TDrawBuffer b;
            TAttrPair cNormal = getColor(0x0301);
            b.moveChar(0, ' ', cNormal, kaomojiWidth);
            b.moveStr(0, kaomoji, cNormal);
            writeBuf(xPos, 0, kaomojiWidth, 1, b);
        }
    }

    void setMood(KaomojiMood mood, int durationMs = 2000) {
        currentMood = mood;
        if (durationMs > 0) {
            moodEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);
        }
    }

    void update() {
        // Revert to neutral after mood duration
        auto now = std::chrono::steady_clock::now();
        if (currentMood != NEUTRAL && now >= moodEndTime) {
            currentMood = NEUTRAL;
        }
        drawView();
    }

private:
    KaomojiMood currentMood = NEUTRAL;
    bool isBlinking = false;
    std::chrono::steady_clock::time_point blinkStartTime;
    std::chrono::steady_clock::time_point nextBlinkTime;
    std::chrono::steady_clock::time_point moodEndTime;

    void scheduleNextBlink() {
        // Random blink interval: 3-6 seconds
        int interval = 3000 + (rand() % 3000);
        nextBlinkTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval);
    }

    const char* getKaomojiForState() {
        // Blink overrides mood
        if (isBlinking) return "つ-‿-‿-༽つ";

        switch(currentMood) {
            case EXCITED:    return "つ◉‿◉‿◉༽つ";
            case THINKING:   return "つ●‿●‿●༽つ";
            case SLEEPY:     return "つ◡‿◡‿◡༽つ";
            case CURIOUS:    return "つ○‿○‿○༽つ";
            case MEMORY:     return "つ■‿■‿■༽つ";
            case GEOMETRIC:  return "つ□‿□‿□༽つ";
            case SURPRISED:  return "つ◎‿◎‿◎༽つ";
            case NEUTRAL:
            default:         return "つ◕‿◕‿◕༽つ";
        }
    }
};

/*---------------------------------------------------------*/
/* TCustomStatusLine - Status line with white hotkeys     */
/*---------------------------------------------------------*/
class TCustomStatusLine : public TStatusLine
{
public:
    TCustomStatusLine(const TRect& bounds, TStatusDef& aDefs) : TStatusLine(bounds, aDefs) {}
    
    virtual TColorAttr mapColor(uchar index) noexcept override
    {
        TColorRGB trueBlack(0, 0, 0);
        TColorRGB trueWhite(255, 255, 255);
        
        // Status line uses different indices than menu bar
        switch(index) {
            case 1:  case 2:  case 3:  case 4: {
                TColorAttr skinAttr;
                if (ThemeManager::tryAttr(SkinRole::Bar, skinAttr)) return skinAttr;
                return TColorAttr(trueBlack, trueWhite);
            }
            default:
                return TStatusLine::mapColor(index);
        }
    }

    virtual void draw() override
    {
        TStatusLine::draw();
        drawLlmIndicator();
        drawApiIndicator();
    }

private:
    void drawLlmIndicator()
    {
        const AuthConfig& auth = AuthConfig::instance();
        TColorRGB bg(255, 255, 255);
        {
            TColorAttr barA;
            if (ThemeManager::tryAttr(SkinRole::Bar, barA))
                bg = ThemeManager::cgaColor(ThemeManager::bgIndex(SkinRole::Bar));
        }
        TColorRGB fg;
        const char* label = auth.modeName();  // "LLM AUTH" / "LLM KEY" / "LLM OFF"

        switch (auth.mode()) {
            case AuthMode::ClaudeCode:
                fg = ThemeManager::cgaColor(ThemeManager::fgIndex(SkinRole::Ok));
                break;
            case AuthMode::ApiKey:
                fg = ThemeManager::cgaColor(ThemeManager::fgIndex(SkinRole::Accent));
                break;
            case AuthMode::NoAuth:
                fg = ThemeManager::cgaColor(ThemeManager::fgIndex(SkinRole::Warn));
                break;
        }

        // Place to the left of the API indicator (API indicator ~8 chars from right)
        int labelLen = static_cast<int>(strlen(label));
        int xPos = size.x - labelLen - 10;  // 10 = API indicator width + gap
        if (xPos < 1) return;

        TDrawBuffer b;
        TColorAttr attr(fg, bg);
        b.moveChar(0, ' ', attr, labelLen + 1);
        b.moveStr(0, label, attr);
        writeBuf(xPos, 0, labelLen + 1, 1, b);
    }

    void drawApiIndicator()
    {
        // TWwdosApp is incomplete here — use the opaque helper
        auto status = getAppIpcStatus(TApplication::application);

        // Build indicator string and pick colour
        TColorRGB bg(255, 255, 255);
        {
            TColorAttr barA;
            if (ThemeManager::tryAttr(SkinRole::Bar, barA))
                bg = ThemeManager::cgaColor(ThemeManager::bgIndex(SkinRole::Bar));
        }
        TColorRGB fg;
        const char* label;

        if (status.api_active) {
            fg = ThemeManager::cgaColor(ThemeManager::fgIndex(SkinRole::Ok));
            label = "API ON";
        } else if (status.listening) {
            fg = ThemeManager::cgaColor(ThemeManager::fgIndex(SkinRole::Dim));
            label = "API IDLE";
        } else {
            fg = ThemeManager::cgaColor(ThemeManager::fgIndex(SkinRole::Warn));
            label = "API OFF";
        }

        int labelLen = strlen(label);
        int xPos = size.x - labelLen - 1;
        if (xPos < 1) return;

        TDrawBuffer b;
        TColorAttr attr(fg, bg);
        b.moveChar(0, ' ', attr, labelLen + 1);
        b.moveStr(0, label, attr);
        writeBuf(xPos, 0, labelLen + 1, 1, b);
    }

    // Forward-declared helper — implemented after TWwdosApp is defined
    static ApiIpcServer::ConnectionStatus getAppIpcStatus(void* app);
};

static TMenuItem& buildFigletFontSubMenu() {
    const auto& cat = figlet::catalogue();

    // Build "More Fonts..." as the tail item
    TMenuItem* tail = new TMenuItem("~M~ore Fonts...", cmFigletMoreFonts,
                                     kbNoKey, hcNoContext, nullptr, nullptr);
    TMenuItem* sep = &newLine();
    sep->next = tail;

    // Build category submenus in reverse order
    TMenuItem* catChain = sep;
    for (int c = (int)cat.categories.size() - 1; c >= 0; c--) {
        TMenuItem* catItems = figlet::buildCategoryMenuItems(
            cat.categories[c], cmFigletCatFontBase, "");
        if (!catItems) continue;
        TMenu* catSub = new TMenu(*catItems);
        std::string label = cat.categories[c].name;
        char* str = new char[label.size() + 1];
        std::strcpy(str, label.c_str());
        TMenuItem* item = new TMenuItem(str, kbNoKey, catSub, hcNoContext, catChain);
        catChain = item;
    }

    // Wrap as "FIGlet Font ▶ ..." submenu
    TMenu* fontMenu = new TMenu(*catChain);
    return *new TMenuItem("FIGlet ~F~ont", kbNoKey, fontMenu, hcNoContext, nullptr);
}

TMenuBar* TWwdosApp::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;
    TMenuItem* recentSubmenu = buildRecentWorkspacesSubmenuItem();
    
    return new TCustomMenuBar(r,
        *new TSubMenu("~F~ile", kbAltF) +
            *new TMenuItem("New ~T~est Pattern", cmNewWindow, kbCtrlN) +
            *new TMenuItem("New ~H~-Gradient", cmNewGradientH, kbNoKey) +
            *new TMenuItem("New ~V~-Gradient", cmNewGradientV, kbNoKey) +
            *new TMenuItem("New ~R~adial Gradient", cmNewGradientR, kbNoKey) +
            *new TMenuItem("New ~D~iagonal Gradient", cmNewGradientD, kbNoKey) +
            // REMOVED E009: New Mechs Grid (dead end, handler commented out)
            *new TMenuItem("New ~A~nimation", cmNewDonut, kbCtrlD) +
            newLine() +
            *new TMenuItem("~O~pen Text/Animation...", cmOpenAnimation, kbCtrlO) +
            *new TMenuItem("Open I~m~age...", cmOpenImageFile, kbNoKey) +
            *new TMenuItem("Open Mo~n~odraw...", cmOpenMonodraw, kbNoKey) +
            newLine() +
            *new TMenuItem("~S~ave Workspace", cmSaveWorkspace, kbCtrlS) +
            *new TMenuItem("Save Workspace ~A~s...", cmSaveWorkspaceAs, kbNoKey) +
            *new TMenuItem("Open ~W~orkspace...", cmOpenWorkspace, kbNoKey) +
            *new TMenuItem("~M~anage Workspaces...", cmManageWorkspaces, kbNoKey) +
            *recentSubmenu +
            newLine() +
            *new TMenuItem("E~x~it", cmQuit, cmQuit, hcNoContext, "Alt-X") +
        *new TSubMenu("~E~dit", kbAltE) +
            *new TMenuItem("~C~opy Page", cmCopy, kbCtrlIns) +
            newLine() +
            *new TMenuItem("Sc~r~eenshot", cmScreenshot, kbCtrlP) +
            newLine() +
            (TMenuItem&) (
                *new TSubMenu("Pattern ~M~ode", kbNoKey) +
                    *new TMenuItem(USE_CONTINUOUS_PATTERN ? "\x04 ~C~ontinuous (Diagonal)" : "  ~C~ontinuous (Diagonal)", 
                                  cmPatternContinuous, kbNoKey) +
                    *new TMenuItem(!USE_CONTINUOUS_PATTERN ? "\x04 ~T~iled (Cropped)" : "  ~T~iled (Cropped)", 
                                  cmPatternTiled, kbNoKey)
            ) +
            newLine() +
            *new TMenuItem("FIGlet Edit ~T~ext...", cmFigletEditText, kbNoKey) +
            (TMenuItem&) buildFigletFontSubMenu() +
        *new TSubMenu("~V~iew", kbAltV) +
            *new TMenuItem("ASCII ~G~rid Demo", cmAsciiGridDemo, kbNoKey) +
            *new TMenuItem("~A~nimated Blocks", cmAnimatedBlocks, kbNoKey) +
            *new TMenuItem("Animated Gradie~n~t", cmAnimatedGradient, kbNoKey) +
            *new TMenuItem("Animated S~c~ore", cmAnimatedScore, kbNoKey) +
            *new TMenuItem("Score ~B~G Color...", cmScoreBgColor, kbNoKey) +
            *new TMenuItem("~V~erse Field (Generative)", cmVerseField, kbNoKey) +
            *new TMenuItem("~O~rbit Field (Generative)", cmOrbitField, kbNoKey) +
            *new TMenuItem("M~y~celium Field (Generative)", cmMyceliumField, kbNoKey) +
            *new TMenuItem("~T~orus Field (Generative)", cmTorusField, kbNoKey) +
            *new TMenuItem("C~u~be Spinner (Generative)", cmCubeField, kbNoKey) +
            *new TMenuItem("Monster ~P~ortal (Generative)", cmMonsterPortal, kbNoKey) +
            *new TMenuItem("Monster Ve~r~se (Generative)", cmMonsterVerse, kbNoKey) +
            *new TMenuItem("Monster Cam (Emo~j~i)", cmMonsterCam, kbNoKey) +
            *new TMenuItem("~B~ackrooms TV", cmBackroomsTv, kbNoKey) +
            *new TMenuItem("Mono S~h~ader (Generative)", cmTweetShader, kbNoKey) +
            newLine() +
            *new TMenuItem("~A~pplications", cmAppLauncher, kbNoKey) +
            *new TMenuItem("ASCII ~G~allery", cmAsciiGallery, kbNoKey) +
            *new TMenuItem("Dis~k~ Library", cmDiskLibrary, kbNoKey) +
            (TMenuItem&)(
                *new TSubMenu("S~k~ins", kbNoKey) +
                    *new TMenuItem("~D~flat (D-Flat blue)", cmSkinBase + 0, kbNoKey) +
                    *new TMenuItem("~T~urbo (Turbo Pascal)", cmSkinBase + 1, kbNoKey) +
                    *new TMenuItem("T~e~rra (GeoGraphics)", cmSkinBase + 2, kbNoKey) +
                    *new TMenuItem("~P~ipeline (moody)", cmSkinBase + 3, kbNoKey) +
                    *new TMenuItem("P~h~osphor (green mono)", cmSkinBase + 4, kbNoKey) +
                    *new TMenuItem("He~r~cules (amber)", cmSkinBase + 5, kbNoKey) +
                    *new TMenuItem("P~a~per (daylight)", cmSkinBase + 6, kbNoKey) +
                    *new TMenuItem("~M~idnight (dim stars)", cmSkinBase + 7, kbNoKey) +
                    newLine() +
                    *new TMenuItem("~O~ff (house grey)", cmSkinBase + 8, kbNoKey) +
                    *new TMenuItem("Re~l~oad Skin Files", cmSkinBase + 9, kbNoKey)
            ) +
            newLine() +
            (TMenuItem&)(
                *new TSubMenu("~G~ames", kbNoKey) +
                    *new TMenuItem("~M~icropolis City Builder", cmMicropolisAscii, kbNoKey) +
                    *new TMenuItem("~Q~uadra (Falling Blocks)", cmQuadra, kbNoKey) +
                    *new TMenuItem("~S~nake", cmSnake, kbNoKey) +
                    *new TMenuItem("Wib~W~ob Rogue", cmRogue, kbNoKey) +
                    *new TMenuItem("~D~eep Signal", cmDeepSignal, kbNoKey)
            ) +
            newLine() +
            *new TMenuItem("Pa~i~nt Canvas", cmNewPaintCanvas, kbNoKey) +
            *new TMenuItem("~F~IGlet Text", cmNewFigletText, kbNoKey) +
            newLine() +
            *new TMenuItem("Scra~m~ble Cat", cmScrambleCat, kbF8) +
        *new TSubMenu("~W~indow", kbAltW) +
            *new TMenuItem("~T~ext Editor", cmTextEditor, kbNoKey) +
            *new TMenuItem("~B~rowser", cmBrowser, kbCtrlB) +
            *new TMenuItem("Te~r~minal", cmOpenTerminal, kbNoKey) +
            *new TMenuItem("~O~pen Text File (Transparent)...", cmOpenTransparentText, kbNoKey) +
            newLine() +
            *new TMenuItem("~C~ascade", cmCascade, kbNoKey) +
            *new TMenuItem("Ti~l~e", cmTile, kbNoKey) +
            *new TMenuItem("Send to Bac~k~", cmSendToBack, kbNoKey) +
            newLine() +
            *new TMenuItem("~N~ext", cmNext, kbF6) +
            *new TMenuItem("~P~revious", cmPrev, kbShiftF6) +
            newLine() +
            *new TMenuItem("Clos~e~", cmClose, kbAltF3) +
            *new TMenuItem("Close ~A~ll", cmCloseAll, kbNoKey) +
            // REMOVED E009: Background Color... (retired for now)
        *new TSubMenu("~T~ools", kbAltT) +
            *new TMenuItem("~W~ib&Wob Chat", cmWibWobChat, kbF12) +
            *new TMenuItem("~R~oom Chat", cmRoomChat, kbNoKey) +
            // REMOVED E009: Test A/B/C (dev-only, type fallback to test_pattern)
            // REMOVED E009: Glitch Effects submenu (entire submenu disabled)
            // REMOVED E009: ANSI Editor, Animation Studio (placeholders)
            newLine() +
            *new TMenuItem("~Q~uantum Printer", cmQuantumPrinter, kbF11) +
            newLine() +
            *new TMenuItem("API ~K~ey...", cmApiKey, kbNoKey) +
        *new TSubMenu("~H~elp", kbAltH) +
            *new TMenuItem("~A~bout WIBWOBWORLD", cmAbout, kbNoKey) +
            *new TMenuItem("~K~eyboard Shortcuts", cmKeyboardShortcuts, kbNoKey) +
            *new TMenuItem("A~P~I Key Help", cmApiKeyHelp, kbNoKey) +
            *new TMenuItem("~L~LM Status", cmLlmStatus, kbNoKey)
    );
}

TStatusLine* TWwdosApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    return new TCustomStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
            *new TStatusItem("~Ctrl-N~ New Window", kbCtrlN, cmNewWindow) +
            *new TStatusItem("~F5~ Repaint", kbF5, cmRepaint) +
            *new TStatusItem("~F6~ Next", kbF6, cmNext) +
            *new TStatusItem("~Alt-F3~ Close", kbAltF3, cmClose) +
            *new TStatusItem("~F8~ Scramble", kbF8, cmScrambleCat)
    );
}

// Implemented here because TWwdosApp must be fully defined first.
ApiIpcServer::ConnectionStatus TCustomStatusLine::getAppIpcStatus(void* appPtr) {
    auto* app = dynamic_cast<TWwdosApp*>(static_cast<TApplication*>(appPtr));
    if (app) {
        return app->getIpcStatus();
    }
    return {};
}

TDeskTop* TWwdosApp::initDeskTop(TRect r)
{
    r.a.y = 1;
    r.b.y--;
    TDeskTop* desktop = new TDeskTop(r);

    // Replace default TBackground with TWibWobBackground (colour-controllable)
    if (desktop->background) {
        TRect bgBounds = desktop->background->getBounds();
        desktop->remove(desktop->background);
        destroy(desktop->background);
        auto* bg = new TWibWobBackground(bgBounds, '\xB1', 7, 1);
        bg->growMode = gfGrowHiX | gfGrowHiY;
        desktop->background = bg;
        desktop->insertBefore(bg, desktop->first());
    }

    return desktop;
}


