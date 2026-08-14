/*---------------------------------------------------------*/
/*                                                         */
/*   workspace_io.cpp - workspace save/load, JSON parse      */
/*   helpers, recent-workspace scanning. Moved verbatim      */
/*   from wwdos_app.cpp (monolith split stage 7b).           */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TView
#define Uses_TFileDialog
#define Uses_MsgBox
#define Uses_TPoint
#include <tvision/tv.h>

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

#include "wwdos_app.h"
#include "workspace_io.h"
#include "core/json_utils.h"
#include "ww_view_utils.h"
#include "theme_manager.h"
#include "test_pattern.h"
#include "windows/pattern_windows.h"
#include "gradient.h"
#include "transparent_text_view.h"
#include "frame_file_player_view.h"
#include "windows/frame_animation_window.h"
#include "window_type_registry.h"
#include "tweet_shader_view.h"
#include "ascii_gallery_view.h"
#include "tuiforge_view.h"
#include "figlet_text_view.h"
#include "room_chat_view.h"
#include "wibwob_background.h"
#include "api_windows.h"

const int kMaxRecentWorkspaces = 5;

std::vector<std::string> scanRecentWorkspacePaths(const char* dirPath, int maxCount)
{
    std::vector<RecentWorkspaceInfo> entries;
    DIR* dir = opendir(dirPath);
    if (!dir)
        return {};

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        const char* name = ent->d_name;
        if (!name || name[0] == '.')
            continue;
        size_t len = std::strlen(name);
        if (len < 6 || std::strcmp(name + len - 5, ".json") != 0)
            continue;

        std::string path = std::string(dirPath) + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        entries.push_back({path, name, st.st_mtime});
    }
    closedir(dir);

    std::sort(entries.begin(), entries.end(),
              [](const RecentWorkspaceInfo& a, const RecentWorkspaceInfo& b) {
                  if (a.mtime != b.mtime)
                      return a.mtime > b.mtime;
                  return a.fileName < b.fileName;
              });

    if ((int)entries.size() > maxCount)
        entries.resize(maxCount);

    std::vector<std::string> out;
    out.reserve(entries.size());
    for (const auto& e : entries)
        out.push_back(e.path);
    return out;
}

int countWindowsInWorkspace(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return -1;
    // Quick count: find "windows" array, count occurrences of "\"type\""
    std::string content;
    char buf[4096];
    while (size_t n = fread(buf, 1, sizeof(buf), f))
        content.append(buf, n);
    fclose(f);
    int count = 0;
    size_t pos = content.find("\"windows\"");
    if (pos == std::string::npos) return 0;
    while ((pos = content.find("\"type\"", pos + 1)) != std::string::npos)
        ++count;
    return count;
}

std::string recentWorkspaceLabel(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    int n = countWindowsInWorkspace(path);
    if (n > 0)
        name += " (" + std::to_string(n) + ")";
    return name;
}

void TWwdosApp::skipWs(const std::string &s, size_t &pos)
{
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\n' || s[pos] == '\r' || s[pos] == '\t')) ++pos;
}

bool TWwdosApp::consume(const std::string &s, size_t &pos, char ch)
{
    skipWs(s, pos);
    if (pos < s.size() && s[pos] == ch) { ++pos; return true; }
    return false;
}

bool TWwdosApp::parseString(const std::string &s, size_t &pos, std::string &out)
{
    skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '"') return false;
    ++pos;
    std::string res;
    while (pos < s.size()) {
        char c = s[pos++];
        if (c == '"') { out = res; return true; }
        if (c == '\\') {
            if (pos >= s.size()) return false;
            char e = s[pos++];
            switch (e) {
                case '"': res.push_back('"'); break;
                case '\\': res.push_back('\\'); break;
                case 'n': res.push_back('\n'); break;
                case 'r': res.push_back('\r'); break;
                case 't': res.push_back('\t'); break;
                default: res.push_back(e); break;
            }
        } else res.push_back(c);
    }
    return false;
}

bool TWwdosApp::parseNumber(const std::string &s, size_t &pos, int &out)
{
    skipWs(s, pos);
    bool neg = false;
    if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) { neg = (s[pos] == '-'); ++pos; }
    long val = 0; bool any=false;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') { any=true; val = val*10 + (s[pos]-'0'); ++pos; }
    if (!any) return false;
    out = neg ? -int(val) : int(val);
    return true;
}

bool TWwdosApp::parseBool(const std::string &s, size_t &pos, bool &out)
{
    skipWs(s, pos);
    if (s.compare(pos, 4, "true") == 0) { out = true; pos += 4; return true; }
    if (s.compare(pos, 5, "false") == 0) { out = false; pos += 5; return true; }
    return false;
}

bool TWwdosApp::parseKeyedString(const std::string &s, size_t objStart, const char *key, std::string &out)
{
    size_t pos = objStart;
    while (pos < s.size()) {
        skipWs(s, pos);
        if (s[pos] == '}' || s[pos] == ']') return false;
        std::string k; size_t kpos = pos;
        if (!parseString(s, kpos, k)) { ++pos; continue; }
        pos = kpos; skipWs(s, pos);
        if (!consume(s, pos, ':')) continue;
        if (k == key) {
            return parseString(s, pos, out);
        }
        // Skip value
        skipWs(s, pos);
        if (pos>=s.size()) break;
        if (s[pos] == '"') { std::string tmp; parseString(s, pos, tmp); }
        else if ((s[pos] >= '0' && s[pos] <= '9') || s[pos]=='-' || s[pos]=='+') { int dummy; parseNumber(s, pos, dummy); }
        else if (s[pos] == 't' || s[pos] == 'f') { bool db; parseBool(s, pos, db); }
        else if (s[pos] == '{') { int depth=1; ++pos; while (pos<s.size()&&depth){ if(s[pos]=='"'){ ++pos; while(pos<s.size()&&s[pos]!='"'){ if(s[pos]=='\\') ++pos; ++pos;} ++pos; continue;} if(s[pos]=='{')depth++; else if(s[pos]=='}')depth--; ++pos; } }
        else if (s[pos] == '[') { int depth=1; ++pos; while (pos<s.size()&&depth){ if(s[pos]=='"'){ ++pos; while(pos<s.size()&&s[pos]!='"'){ if(s[pos]=='\\') ++pos; ++pos;} ++pos; continue;} if(s[pos]=='[')depth++; else if(s[pos]==']')depth--; ++pos; } }
        skipWs(s, pos); if (pos<s.size() && s[pos]==',') ++pos;
    }
    return false;
}

bool TWwdosApp::parseKeyedNumber(const std::string &s, size_t objStart, const char *key, int &out)
{
    size_t pos = objStart;
    while (pos < s.size()) {
        skipWs(s, pos);
        if (s[pos] == '}' || s[pos] == ']') return false;
        std::string k; size_t kpos = pos;
        if (!parseString(s, kpos, k)) { ++pos; continue; }
        pos = kpos; skipWs(s, pos);
        if (!consume(s, pos, ':')) continue;
        if (k == key) {
            return parseNumber(s, pos, out);
        }
        // Skip value
        skipWs(s, pos);
        if (pos>=s.size()) break;
        if (s[pos] == '"') { std::string tmp; parseString(s, pos, tmp); }
        else if ((s[pos] >= '0' && s[pos] <= '9') || s[pos]=='-' || s[pos]=='+') { int dummy; parseNumber(s, pos, dummy); }
        else if (s[pos] == 't' || s[pos] == 'f') { bool db; parseBool(s, pos, db); }
        else if (s[pos] == '{') { int depth=1; ++pos; while (pos<s.size()&&depth){ if(s[pos]=='"'){ ++pos; while(pos<s.size()&&s[pos]!='"'){ if(s[pos]=='\\') ++pos; ++pos;} ++pos; continue;} if(s[pos]=='{')depth++; else if(s[pos]=='}')depth--; ++pos; } }
        else if (s[pos] == '[') { int depth=1; ++pos; while (pos<s.size()&&depth){ if(s[pos]=='"'){ ++pos; while(pos<s.size()&&s[pos]!='"'){ if(s[pos]=='\\') ++pos; ++pos;} ++pos; continue;} if(s[pos]=='[')depth++; else if(s[pos]==']')depth--; ++pos; } }
        skipWs(s, pos); if (pos<s.size() && s[pos]==',') ++pos;
    }
    return false;
}

bool TWwdosApp::parseKeyedBool(const std::string &s, size_t objStart, const char *key, bool &out)
{
    size_t pos = objStart;
    while (pos < s.size()) {
        skipWs(s, pos);
        if (s[pos] == '}' || s[pos] == ']') return false;
        std::string k; size_t kpos = pos;
        if (!parseString(s, kpos, k)) { ++pos; continue; }
        pos = kpos; skipWs(s, pos);
        if (!consume(s, pos, ':')) continue;
        if (k == key) {
            return parseBool(s, pos, out);
        }
        // Skip value
        skipWs(s, pos);
        if (pos>=s.size()) break;
        if (s[pos] == '"') { std::string tmp; parseString(s, pos, tmp); }
        else if ((s[pos] >= '0' && s[pos] <= '9') || s[pos]=='-' || s[pos]=='+') { int dummy; parseNumber(s, pos, dummy); }
        else if (s[pos] == 't' || s[pos] == 'f') { bool db; parseBool(s, pos, db); }
        else if (s[pos] == '{') { int depth=1; ++pos; while (pos<s.size()&&depth){ if(s[pos]=='"'){ ++pos; while(pos<s.size()&&s[pos]!='"'){ if(s[pos]=='\\') ++pos; ++pos;} ++pos; continue;} if(s[pos]=='{')depth++; else if(s[pos]=='}')depth--; ++pos; } }
        else if (s[pos] == '[') { int depth=1; ++pos; while (pos<s.size()&&depth){ if(s[pos]=='"'){ ++pos; while(pos<s.size()&&s[pos]!='"'){ if(s[pos]=='\\') ++pos; ++pos;} ++pos; continue;} if(s[pos]=='[')depth++; else if(s[pos]==']')depth--; ++pos; } }
        skipWs(s, pos); if (pos<s.size() && s[pos]==',') ++pos;
    }
    return false;
}

bool TWwdosApp::parseBounds(const std::string &s, size_t objStart, int &x,int &y,int &w,int &h)
{
    size_t pos = objStart;
    while (pos < s.size()) {
        skipWs(s, pos);
        if (s[pos] == '}' || s[pos] == ']') return false;
        std::string k; size_t kpos = pos;
        if (!parseString(s, kpos, k)) { ++pos; continue; }
        pos = kpos; skipWs(s, pos);
        if (!consume(s, pos, ':')) continue;
        if (k == "bounds") {
            skipWs(s, pos);
            if (!consume(s, pos, '{')) return false;
            int tx=0,ty=0,tw=0,th=0; bool okX=false,okY=false,okW=false,okH=false;
            while (pos < s.size()) {
                skipWs(s, pos);
                if (s[pos] == '}') { ++pos; break; }
                std::string bk; if (!parseString(s, pos, bk)) return false; if (!consume(s,pos,':')) return false;
                if (bk == "x") { okX = parseNumber(s,pos,tx); }
                else if (bk == "y") { okY = parseNumber(s,pos,ty); }
                else if (bk == "w") { okW = parseNumber(s,pos,tw); }
                else if (bk == "h") { okH = parseNumber(s,pos,th); }
                skipWs(s,pos); if (pos<s.size() && s[pos]==',') ++pos;
            }
            if (okX && okY && okW && okH) { x=tx; y=ty; w=tw; h=th; return true; }
            return false;
        }
        // Skip value
        skipWs(s, pos);
        if (pos>=s.size()) break;
        if (s[pos] == '"') { std::string tmp; parseString(s, pos, tmp); }
        else if ((s[pos] >= '0' && s[pos] <= '9') || s[pos]=='-' || s[pos]=='+') { int dummy; parseNumber(s, pos, dummy); }
        else if (s[pos] == 't' || s[pos] == 'f') { bool db; parseBool(s, pos, db); }
        else if (s[pos] == '{') { int depth=1; ++pos; while (pos<s.size()&&depth){ if(s[pos]=='"'){ ++pos; while(pos<s.size()&&s[pos]!='"'){ if(s[pos]=='\\') ++pos; ++pos;} ++pos; continue;} if(s[pos]=='{')depth++; else if(s[pos]=='}')depth--; ++pos; } }
        else if (s[pos] == '[') { int depth=1; ++pos; while (pos<s.size()&&depth){ if(s[pos]=='"'){ ++pos; while(pos<s.size()&&s[pos]!='"'){ if(s[pos]=='\\') ++pos; ++pos;} ++pos; continue;} if(s[pos]=='[')depth++; else if(s[pos]==']')depth--; ++pos; } }
        skipWs(s, pos); if (pos<s.size() && s[pos]==',') ++pos;
    }
    return false;
}

bool TWwdosApp::loadWorkspaceFromFile(const std::string& path)
{
    std::ifstream in(path);
    if (!in) {
        std::string msg = std::string("Failed to open ") + path;
        messageBox(msg.c_str(), mfError | mfOKButton);
        return false;
    }
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (data.find("\"version\"") == std::string::npos || data.find("\"windows\"") == std::string::npos) {
        messageBox("Invalid workspace file.", mfError | mfOKButton);
        return false;
    }

    // Extract globals.patternMode + skin (skin FIRST: it repaints chrome and
    // desktop; per-window colours are restored later from each window's props)
    bool continuous = USE_CONTINUOUS_PATTERN;
    size_t globalsPos = data.find("\"globals\"");
    if (globalsPos != std::string::npos) {
        size_t pos = data.find('{', globalsPos);
        if (pos != std::string::npos) {
            std::string pm;
            if (parseKeyedString(data, pos+1, "patternMode", pm))
                continuous = (pm == "continuous");
            std::string skinName;
            if (parseKeyedString(data, pos+1, "skin", skinName)) {
                extern std::string api_set_skin(TWwdosApp&, const std::string&);
                api_set_skin(*this, skinName.empty() ? "off" : skinName);
            }
        }
    }

    // Restore desktop state (texture, colour, gallery mode)
    size_t deskPos = data.find("\"desktop\"");
    if (deskPos != std::string::npos) {
        size_t pos = data.find('{', deskPos);
        if (pos != std::string::npos) {
            extern std::string api_desktop_preset(TWwdosApp&, const std::string&);
            extern std::string api_desktop_texture(TWwdosApp&, const std::string&);
            extern std::string api_desktop_color(TWwdosApp&, int, int);
            extern std::string api_desktop_gallery(TWwdosApp&, bool);
            std::string preset;
            bool presetApplied = false;
            if (parseKeyedString(data, pos+1, "preset", preset)
                && !preset.empty() && preset != "custom") {
                presetApplied = (api_desktop_preset(*this, preset) == "ok");
            }
            if (!presetApplied) {
                // Explicit fields (historic saves wrote preset:"custom" which
                // used to dead-end here and restore nothing)
                std::string ch, chU;
                if (parseKeyedString(data, pos+1, "charUtf8", chU) && !chU.empty())
                    api_desktop_texture(*this, chU);
                else if (parseKeyedString(data, pos+1, "char", ch) && !ch.empty())
                    api_desktop_texture(*this, ch);
                int rgbFg = -1, rgbBg = -1;
                if (parseKeyedNumber(data, pos+1, "rgbFg", rgbFg)
                    && parseKeyedNumber(data, pos+1, "rgbBg", rgbBg)) {
                    if (auto* dbg = dynamic_cast<TWibWobBackground*>(deskTop->background))
                        dbg->setColorRgb((uint32_t)rgbFg, (uint32_t)rgbBg);
                } else {
                    int fg = -1, bgc = -1;
                    if (parseKeyedNumber(data, pos+1, "fg", fg) && parseKeyedNumber(data, pos+1, "bg", bgc))
                        api_desktop_color(*this, fg, bgc);
                }
            }
            bool gallery = false;
            if (parseKeyedBool(data, pos+1, "gallery", gallery) && gallery) {
                api_desktop_gallery(*this, true);
            }
        }
    }

    // Locate windows array and extract each object substring
    size_t winKey = data.find("\"windows\"");
    if (winKey == std::string::npos) {
        messageBox("No windows in workspace.", mfError | mfOKButton);
        return false;
    }
    size_t arrPos = data.find('[', winKey);
    if (arrPos == std::string::npos) return false;
    std::vector<std::string> objects;
    size_t p = arrPos+1; bool inStr=false; int depth=0;
    while (p < data.size()) {
        char c = data[p];
        if (c == '"') { inStr = !inStr; ++p; continue; }
        if (!inStr) {
            if (c == '{') {
                int d=1; size_t q=p+1;
                while (q<data.size() && d) {
                    if (data[q] == '"') { ++q; while (q<data.size() && data[q] != '"') { if (data[q]=='\\') ++q; ++q; } ++q; continue; }
                    if (data[q] == '{') d++; else if (data[q] == '}') d--; ++q;
                }
                objects.emplace_back(data.substr(p, q-p));
                p = q; continue;
            }
            if (c == ']') break;
        }
        ++p;
    }

    // Close current windows
    closeAll();

    // Apply globals
    USE_CONTINUOUS_PATTERN = continuous;

    // Restore windows
    std::vector<TWindow*> created;
    auto captureWindows = [this]() {
        std::vector<TWindow*> out;
        TView *start = deskTop->first();
        if (!start) return out;
        TView *v = start;
        do {
            if (TWindow *w = dynamic_cast<TWindow*>(v))
                out.push_back(w);
            v = v->next;
        } while (v != start);
        return out;
    };
    for (const auto &obj : objects) {
        std::string type; if (!parseKeyedString(obj, 0, "type", type)) continue;
        std::string title; parseKeyedString(obj, 0, "title", title);
        int x=2,y=1,w=50,h=15; parseBounds(obj, 0, x,y,w,h);
        bool zoomed=false; parseKeyedBool(obj, 0, "zoomed", zoomed);

        // Anchor: "right" = x is offset from right edge; "bottom" = y from bottom
        std::string anchor; parseKeyedString(obj, 0, "anchor", anchor);

        // Clamp and anchor
        TRect ext = deskTop->getExtent();
        int maxW = ext.b.x - ext.a.x;
        int maxH = ext.b.y - ext.a.y;
        if (w < 16) w = 16; if (h < 6) h = 6;
        if (w > maxW) w = maxW; if (h > maxH) h = maxH;
        // Right-anchor: x = desktop_width - x_offset - width
        if (anchor.find("right") != std::string::npos)
            x = std::max(0, maxW - x - w);
        if (anchor.find("bottom") != std::string::npos)
            y = std::max(0, maxH - y - h);
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x + w > maxW) x = std::max(0, maxW - w);
        if (y + h > maxH) y = std::max(0, maxH - h);
        TRect bounds(x,y,x+w,y+h);

        TWindow *win = nullptr;
        if (type == "test_pattern") {
            win = new TTestPatternWindow(bounds, "");
        } else if (type == "gradient") {
            std::string gtype; // props.gradientType preferred
            size_t propsPos = obj.find("\"props\"");
            if (propsPos != std::string::npos) {
                size_t brace = obj.find('{', propsPos);
                if (brace != std::string::npos)
                    parseKeyedString(obj, brace+1, "gradientType", gtype);
            }
            TGradientWindow::GradientType gt = TGradientWindow::gtHorizontal;
            if (gtype == "vertical") gt = TGradientWindow::gtVertical;
            else if (gtype == "radial") gt = TGradientWindow::gtRadial;
            else if (gtype == "diagonal") gt = TGradientWindow::gtDiagonal;
            win = new TGradientWindow(bounds, "", gt);
        } else if (type == "room_chat") {
            TWindow* rcWin = createRoomChatWindow(bounds);
            if (rcWin) {
                deskTop->insert(rcWin);
                registerWindow(rcWin);
                windowNumber++;
            }
            continue;
        } else if (type == "scramble") {
            std::string stateStr;
            size_t propsPos = obj.find("\"props\"");
            if (propsPos != std::string::npos) {
                size_t brace = obj.find('{', propsPos);
                if (brace != std::string::npos)
                    parseKeyedString(obj, brace+1, "display", stateStr);
            }
            ScrambleDisplayState sds = sdsSmol;
            if (stateStr == "tall") sds = sdsTall;
            else if (stateStr == "hidden") sds = sdsHidden;
            TWindow* sw = createScrambleWindow(bounds, sds);
            if (sw) {
                scrambleWindow = static_cast<TScrambleWindow*>(sw);
                deskTop->insert(sw);
                registerWindow(sw);
                windowNumber++;
            }
            continue;
        } else if (type == "frame_player") {
            // (colour restore for this branch happens below via propsColours)
            std::string path; unsigned pms = 300;
            bool frameless = false, shadowless = false;
            size_t propsPos = obj.find("\"props\"");
            if (propsPos != std::string::npos) {
                size_t brace = obj.find('{', propsPos);
                if (brace != std::string::npos) {
                    parseKeyedString(obj, brace+1, "path", path);
                    std::string pmsStr;
                    parseKeyedString(obj, brace+1, "periodMs", pmsStr);
                    if (!pmsStr.empty()) pms = (unsigned)std::stoul(pmsStr);
                    parseKeyedBool(obj, brace+1, "frameless", frameless);
                    parseKeyedBool(obj, brace+1, "shadowless", shadowless);
                }
            }
            if (!path.empty()) {
                const std::vector<TWindow*> before = captureWindows();
                openAnimationFilePath(path, bounds, frameless, shadowless, title);
                // restore saved per-window colours (bgIdx/fgIdx props)
                int bgi = -999, fgi = -999;
                if (propsPos != std::string::npos) {
                    size_t brace = obj.find('{', propsPos);
                    if (brace != std::string::npos) {
                        parseKeyedNumber(obj, brace+1, "bgIdx", bgi);
                        parseKeyedNumber(obj, brace+1, "fgIdx", fgi);
                    }
                }
                if (bgi != -999 || fgi != -999) {
                    for (TWindow* cand : captureWindows()) {
                        bool existed = false;
                        for (TWindow* prior : before) if (prior == cand) { existed = true; break; }
                        if (existed) continue;
                        if (auto* fp = ww_get_child_view<FrameFilePlayerView>(cand)) {
                            if (bgi != -999) fp->setBackgroundIndex(bgi);
                            if (fgi != -999) fp->setForegroundIndex(fgi);
                            if (cand->frame) cand->frame->drawView();
                        } else if (auto* tv = ww_get_child_view<TTextFileView>(cand)) {
                            if (bgi != -999) tv->setBackgroundIndex(bgi);
                            if (fgi != -999) tv->setForegroundIndex(fgi);
                            if (cand->frame) cand->frame->drawView();
                        }
                        break;
                    }
                }
            }
            continue; // openAnimationFilePath handles insert + register
        } else if (type == "text_view") {
            std::string path;
            size_t propsPos = obj.find("\"props\"");
            if (propsPos != std::string::npos) {
                size_t brace = obj.find('{', propsPos);
                if (brace != std::string::npos)
                    parseKeyedString(obj, brace+1, "path", path);
            }
            if (!path.empty()) {
                const std::vector<TWindow*> before = captureWindows();
                api_open_text_view_path(*this, path, &bounds);
                int bgi = -999, fgi = -999;
                if (propsPos != std::string::npos) {
                    size_t brace = obj.find('{', propsPos);
                    if (brace != std::string::npos) {
                        parseKeyedNumber(obj, brace+1, "bgIdx", bgi);
                        parseKeyedNumber(obj, brace+1, "fgIdx", fgi);
                    }
                }
                if (bgi != -999 || fgi != -999) {
                    for (TWindow* cand : captureWindows()) {
                        bool existed = false;
                        for (TWindow* prior : before) if (prior == cand) { existed = true; break; }
                        if (existed) continue;
                        if (auto* tv = ww_get_child_view<TTextFileView>(cand)) {
                            if (bgi != -999) tv->setBackgroundIndex(bgi);
                            if (fgi != -999) tv->setForegroundIndex(fgi);
                            if (cand->frame) cand->frame->drawView();
                        }
                        break;
                    }
                }
            }
            continue;   // empty path used to fall through to a null-deref zoom
        } else if (type == "gallery") {
            int tabIndex = 0;
            std::string searchText;
            size_t propsPos = obj.find("\"props\"");
            if (propsPos != std::string::npos) {
                size_t brace = obj.find('{', propsPos);
                if (brace != std::string::npos) {
                    parseKeyedNumber(obj, brace+1, "tab", tabIndex);
                    parseKeyedString(obj, brace+1, "search", searchText);
                }
            }

            const WindowTypeSpec* spec = find_window_type_by_name(type);
            if (!spec || !spec->spawn) continue;

            const std::vector<TWindow*> before = captureWindows();
            std::map<std::string, std::string> kv;
            kv["x"] = std::to_string(x);
            kv["y"] = std::to_string(y);
            kv["w"] = std::to_string(w);
            kv["h"] = std::to_string(h);
            if (!title.empty()) kv["title"] = title;
            // Copy every scalar props key into kv so registry spawns receive
            // their args (shader name, gradient kind, future types) — the old
            // x/y/w/h-only kv silently degraded every parameterised type.
            {
                size_t propsPos = obj.find("\"props\"");
                if (propsPos != std::string::npos) {
                    size_t brace = obj.find('{', propsPos);
                    size_t end = (brace != std::string::npos) ? obj.find('}', brace) : std::string::npos;
                    if (brace != std::string::npos && end != std::string::npos) {
                        std::string pb = obj.substr(brace + 1, end - brace - 1);
                        size_t q = 0;
                        while ((q = pb.find('"', q)) != std::string::npos) {
                            size_t q2 = pb.find('"', q + 1);
                            if (q2 == std::string::npos) break;
                            std::string key = pb.substr(q + 1, q2 - q - 1);
                            std::string sval; int nval = 0;
                            if (parseKeyedString(pb, 0, key.c_str(), sval))
                                kv.emplace(key, sval);
                            else if (parseKeyedNumber(pb, 0, key.c_str(), nval))
                                kv.emplace(key, std::to_string(nval));
                            size_t colon = pb.find(':', q2);
                            q = (colon == std::string::npos) ? q2 + 1 : colon + 1;
                        }
                    }
                }
            }
            const char* err = spec->spawn(*this, kv);
            if (err) continue;

            const std::vector<TWindow*> after = captureWindows();
            for (TWindow* candidate : after) {
                bool existed = false;
                for (TWindow* prior : before) {
                    if (prior == candidate) {
                        existed = true;
                        break;
                    }
                }
                if (!existed) {
                    win = candidate;
                    break;
                }
            }
            if (!win) continue;
            if (auto *gallery = dynamic_cast<TGalleryWindow*>(win)) {
                gallery->setSearchText(searchText);
                gallery->setSelected(tabIndex);
            }
            if (zoomed) win->zoom();
            created.push_back(win);
            continue;
        } else if (type == "figlet_text") {
            std::string ftText, ftFont = "standard", ftFg, ftBg;
            bool ftFrameless = false, ftShadowless = false;
            size_t propsPos = obj.find("\"props\"");
            if (propsPos != std::string::npos) {
                size_t brace = obj.find('{', propsPos);
                if (brace != std::string::npos) {
                    parseKeyedString(obj, brace+1, "text", ftText);
                    parseKeyedString(obj, brace+1, "font", ftFont);
                    parseKeyedString(obj, brace+1, "fg", ftFg);
                    parseKeyedString(obj, brace+1, "bg", ftBg);
                    parseKeyedBool(obj, brace+1, "frameless", ftFrameless);
                    parseKeyedBool(obj, brace+1, "shadowless", ftShadowless);
                }
            }
            if (ftText.empty()) ftText = "Hello";
            TRect r(x, y, x + w, y + h);
            extern void api_spawn_figlet_text(TWwdosApp&, const TRect*,
                const std::string& text, const std::string& font,
                bool frameless, bool shadowless);
            extern std::string api_figlet_set_color(TWwdosApp&, const std::string& id, const std::string& fg, const std::string& bg);
            api_spawn_figlet_text(*this, &r, ftText, ftFont, ftFrameless, ftShadowless);
            // Find the newly spawned window
            TView *vv = deskTop->first();
            if (vv) {
                TView *scan = vv;
                do {
                    if (auto *ftw = dynamic_cast<TFigletTextWindow*>(scan)) {
                        if (auto *fv = ftw->getFigletView()) {
                            if (fv->getText() == ftText) {
                                win = ftw;
                                // Apply colours if saved
                                if (!ftFg.empty() || !ftBg.empty()) {
                                    api_figlet_set_color(*this, "", ftFg, ftBg);
                                }
                                break;
                            }
                        }
                    }
                    scan = scan->next;
                } while (scan != vv);
            }
            if (win && zoomed) win->zoom();
            if (win) created.push_back(win);
            continue;
        } else {
            const WindowTypeSpec* spec = find_window_type_by_name(type);
            if (!spec || !spec->spawn) continue;

            const std::vector<TWindow*> before = captureWindows();
            std::map<std::string, std::string> kv;
            kv["x"] = std::to_string(x);
            kv["y"] = std::to_string(y);
            kv["w"] = std::to_string(w);
            kv["h"] = std::to_string(h);
            if (!title.empty()) kv["title"] = title;
            // Copy every scalar props key into kv so registry spawns receive
            // their args (shader name, gradient kind, future types) — the old
            // x/y/w/h-only kv silently degraded every parameterised type.
            {
                size_t propsPos = obj.find("\"props\"");
                if (propsPos != std::string::npos) {
                    size_t brace = obj.find('{', propsPos);
                    size_t end = (brace != std::string::npos) ? obj.find('}', brace) : std::string::npos;
                    if (brace != std::string::npos && end != std::string::npos) {
                        std::string pb = obj.substr(brace + 1, end - brace - 1);
                        size_t q = 0;
                        while ((q = pb.find('"', q)) != std::string::npos) {
                            size_t q2 = pb.find('"', q + 1);
                            if (q2 == std::string::npos) break;
                            std::string key = pb.substr(q + 1, q2 - q - 1);
                            std::string sval; int nval = 0;
                            if (parseKeyedString(pb, 0, key.c_str(), sval))
                                kv.emplace(key, sval);
                            else if (parseKeyedNumber(pb, 0, key.c_str(), nval))
                                kv.emplace(key, std::to_string(nval));
                            size_t colon = pb.find(':', q2);
                            q = (colon == std::string::npos) ? q2 + 1 : colon + 1;
                        }
                    }
                }
            }
            const char* err = spec->spawn(*this, kv);
            if (err) continue;

            const std::vector<TWindow*> after = captureWindows();
            for (TWindow* candidate : after) {
                bool existed = false;
                for (TWindow* prior : before) {
                    if (prior == candidate) {
                        existed = true;
                        break;
                    }
                }
                if (!existed) {
                    win = candidate;
                    break;
                }
            }
            if (!win) continue;
        }
        if (type == "test_pattern" || type == "gradient")
            deskTop->insert(win);
        if (zoomed) win->zoom();
        created.push_back(win);
    }

    // Focus saved
    int focusedIdx = -1;
    size_t fpos = data.find("\"focusedIndex\"");
    if (fpos != std::string::npos) { size_t pos = data.find(':', fpos); if (pos != std::string::npos) { ++pos; parseNumber(data, pos, focusedIdx); } }
    if (focusedIdx >= 0 && focusedIdx < (int)created.size()) created[focusedIdx]->select();

    currentWorkspacePath_ = path;
    return true;
}

void TWwdosApp::openWorkspace()
{
    // Open a dialog rooted at workspaces/ listing JSON files
    char fileName[260];
    std::strncpy(fileName, "workspaces/*.json", sizeof(fileName));
    fileName[sizeof(fileName)-1] = '\0';
    TFileDialog *dlg = new TFileDialog("workspaces/*.json", "Open Workspace", "~N~ame", fdOpenButton, 101);
    ushort res = deskTop->execView(dlg);
    std::string path;
    if (res != cmCancel) {
        dlg->getData(fileName);
        path = fileName;
        // Normalize: if only a filename, prepend workspaces/
        if (!path.empty() && path.find('/') == std::string::npos)
            path = std::string("workspaces/") + path;
    } else {
        path = "workspaces/last_workspace.json";
    }
    destroy(dlg);

    // Fallback if file missing: try default
    std::ifstream test(path.c_str());
    if (!test.good()) {
        test.close();
        path = "workspaces/last_workspace.json";
    } else test.close();

    if (!loadWorkspaceFromFile(path))
        return;
    messageBox("Workspace loaded.", mfInformation | mfOKButton);
}

bool TWwdosApp::openWorkspacePath(const std::string& path)
{
    bool ok = loadWorkspaceFromFile(path);
    fprintf(stderr, "[workspace] open path=%s ok=%s\n", path.c_str(), ok ? "true" : "false");
    return ok;
}

std::string TWwdosApp::buildWorkspaceJson()
{
    // Screen size
    TRect ext = deskTop->getExtent();
    int sw = ext.b.x - ext.a.x;
    int sh = ext.b.y - ext.a.y;

    // Timestamp (basic)
    char ts[64];
    std::time_t t = std::time(nullptr);
    std::tm *lt = std::localtime(&t);
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", lt);

    std::string json;
    json += "{\n";
    json += "  \"version\": 1,\n";
    json += "  \"app\": \"test_pattern\",\n";
    json += std::string("  \"timestamp\": \"") + ts + "\",\n";
    json += "  \"screen\": { \"width\": " + std::to_string(sw) + ", \"height\": " + std::to_string(sh) + " },\n";
    json += std::string("  \"globals\": { \"patternMode\": \"") + (USE_CONTINUOUS_PATTERN ? "continuous" : "tiled")
          + "\", \"skin\": \"" + json_escape(ThemeManager::activeSkin()) + "\" },\n";

    // Desktop state
    {
        auto* bg = dynamic_cast<TWibWobBackground*>(deskTop->background);
        if (bg) {
            json += "  \"desktop\": { ";
            json += "\"char\": \"";
            char ch = bg->getPattern();
            if (ch == '"') json += "\\\"";
            else if (ch == '\\') json += "\\\\";
            else json += ch;
            json += "\", ";
            json += "\"fg\": " + std::to_string((int)bg->getFg()) + ", ";
            json += "\"bg\": " + std::to_string((int)bg->getBg()) + ", ";
            if (!bg->getPatternUtf8().empty())
                json += "\"charUtf8\": \"" + json_escape(bg->getPatternUtf8()) + "\", ";
            if (bg->isRgb()) {
                json += "\"rgbFg\": " + std::to_string(bg->getRgbFg()) + ", ";
                json += "\"rgbBg\": " + std::to_string(bg->getRgbBg()) + ", ";
            }
            json += std::string("\"gallery\": ") + (galleryMode_ ? "true" : "false");
            std::string presetName = bg->getPresetName();
            if (presetName != "custom")
                json += ", \"preset\": \"" + presetName + "\"";
            json += " },\n";
        }
    }

    json += "  \"windows\": [\n";

    // Collect windows in current z-order (child list is circular)
    int idx = 0;
    int focusedIndex = -1;
    TView *vStart = deskTop->first();
    if (vStart) {
    TView *v = vStart;
    do {
        TView *nextV = v->next; // Always advance even on skips
        TWindow *w = dynamic_cast<TWindow*>(v);
        if (!w) { v = nextV; continue; } // Skip non-window views (e.g., wallpaper)
        if (!w->getState(sfVisible)) { v = nextV; continue; }

        // Determine type and props
        std::string type = windowTypeName(w);
        std::string props = "{}";

        if (type == "test_pattern") {
            props = "{}"; // Pattern mode is global in MVP
        } else if (type == "frame_player") {
            // (colour restore for this branch happens below via propsColours)
            // TFrameAnimationWindow stores the path directly — use its getter
            if (auto *faw = dynamic_cast<TFrameAnimationWindow*>(w)) {
                props = "{\"path\": \"" + json_escape(faw->getFilePath()) + "\"";
                if (auto* fp = ww_get_child_view<FrameFilePlayerView>(w)) {
                    props += ", \"bgIdx\": " + std::to_string(fp->backgroundIndex());
                    props += ", \"fgIdx\": " + std::to_string(fp->foregroundIndex());
                } else if (auto* tv = ww_get_child_view<TTextFileView>(w)) {
                    // some frame_players host a TTextFileView (single-frame
                    // text) — same dual-cast set_window_bg uses
                    props += ", \"bgIdx\": " + std::to_string(tv->backgroundIndex());
                    props += ", \"fgIdx\": " + std::to_string(tv->foregroundIndex());
                }
                props += std::string(", \"frameless\": ") + (faw->isFrameless() ? "true" : "false");
                props += std::string(", \"shadowless\": ") + ((w->state & sfShadow) ? "false" : "true");
                props += "}";
            }
        } else if (type == "tuiforge") {
            // Viewer windows reload their render by dir; the picker
            // serialises with no path and respawns as a picker.
            if (auto* tw = dynamic_cast<TTuiforgeWindow*>(w))
                props = "{\"path\": \"" + json_escape(tw->renderDir()) + "\"}";
        } else if (type == "shader") {
            props = "{\"shader\": \"" + json_escape(shaderWindowShaderName(w)) + "\"}";
        } else if (type == "gradient") {
            // Keep concrete gradient subtype in props for backward compatibility.
            TView *cStart = w->first();
            if (cStart) {
            TView *c = cStart;
            do {
                if (dynamic_cast<THorizontalGradientView*>(c)) {
                    props = "{\"gradientType\": \"horizontal\"}"; break;
                } else if (dynamic_cast<TVerticalGradientView*>(c)) {
                    props = "{\"gradientType\": \"vertical\"}"; break;
                } else if (dynamic_cast<TRadialGradientView*>(c)) {
                    props = "{\"gradientType\": \"radial\"}"; break;
                } else if (dynamic_cast<TDiagonalGradientView*>(c)) {
                    props = "{\"gradientType\": \"diagonal\"}"; break;
                }
                c = c->next;
            } while (c != cStart);
            }
        } else if (type == "text_view") {
            if (auto *ttw = dynamic_cast<TTransparentTextWindow*>(w)) {
                props = "{\"path\": \"" + json_escape(ttw->getFilePath()) + "\"";
                if (auto* tv = ww_get_child_view<TTextFileView>(w)) {
                    props += ", \"bgIdx\": " + std::to_string(tv->backgroundIndex());
                    props += ", \"fgIdx\": " + std::to_string(tv->foregroundIndex());
                }
                props += "}";
            }
        } else if (type == "gallery") {
            if (auto *gallery = dynamic_cast<TGalleryWindow*>(w)) {
                props = std::string("{\"tab\": ") + std::to_string(gallery->getSelected());
                const std::string& searchText = gallery->getSearchText();
                props += std::string(", \"search\": \"") + json_escape(searchText) + "\"}";
            }
        } else if (type == "figlet_text") {
            if (auto *ftw = dynamic_cast<TFigletTextWindow*>(w)) {
                if (auto *fv = ftw->getFigletView()) {
                    props = "{\"text\": \"" + json_escape(fv->getText()) + "\"";
                    props += ", \"font\": \"" + json_escape(fv->getFont()) + "\"";
                    uint32_t fg = fv->getFgColor();
                    uint32_t bg = fv->getBgColor();
                    char hex[16];
                    snprintf(hex, sizeof(hex), "#%06X", fg);
                    props += std::string(", \"fg\": \"") + hex + "\"";
                    snprintf(hex, sizeof(hex), "#%06X", bg);
                    props += std::string(", \"bg\": \"") + hex + "\"";
                    props += std::string(", \"frameless\": ") + (ftw->isFrameless() ? "true" : "false");
                    props += std::string(", \"shadowless\": ") + ((w->state & sfShadow) ? "false" : "true");
                    props += "}";
                }
            }
        }

        // Bounds (outer window rect)
        TRect b = w->getBounds();
        int x = b.a.x, y = b.a.y, ww = b.b.x - b.a.x, hh = b.b.y - b.a.y;

        // Zoomed: compare to max size from sizeLimits
        TPoint minSz, maxSz;
        w->sizeLimits(minSz, maxSz);
        bool zoomed = (w->size.x == maxSz.x && w->size.y == maxSz.y && w->origin.x == 0 && w->origin.y == 0);

        // Track focused window index (selected)
        if (w->getState(sfSelected))
            focusedIndex = idx; // zero-based

        if (idx++ > 0) json += ",\n";
        json += "    {\n";
        json += "      \"id\": \"w" + std::to_string(idx) + "\",\n";
        json += "      \"type\": \"" + type + "\",\n";
        const char *title = w->getTitle(0);
        std::string titleValue;
        if (title && *title) {
            titleValue = title;
        } else if (type == "frame_player") {
            // (colour restore for this branch happens below via propsColours)
            if (auto *faw = dynamic_cast<TFrameAnimationWindow*>(w)) {
                const std::string& fp = faw->getFilePath();
                if (!fp.empty()) {
                    size_t slash = fp.find_last_of("/\\");
                    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
                    std::string base = fp.substr(start);
                    size_t dot = base.find_last_of('.');
                    if (dot != std::string::npos && dot != 0)
                        base = base.substr(0, dot);
                    titleValue = base;
                }
            }
        }
        std::string safeTitle = json_escape(titleValue);
        json += "      \"title\": \"" + safeTitle + "\",\n";
        json += "      \"bounds\": { \"x\": " + std::to_string(x) + ", \"y\": " + std::to_string(y) + ", \"w\": " + std::to_string(ww) + ", \"h\": " + std::to_string(hh) + " },\n";
        json += std::string("      \"zoomed\": ") + (zoomed ? "true" : "false") + ",\n";
        json += "      \"props\": " + props + "\n";
        json += "    }";
        v = nextV;
    } while (v != vStart);
    }

    json += "\n  ]";
    if (focusedIndex >= 0)
        json += ",\n  \"focusedIndex\": " + std::to_string(focusedIndex);
    json += "\n}";
    return json;
}

void TWwdosApp::saveWorkspace()
{
    // Ensure directory exists
    mkdir("workspaces", 0755);

    std::string json = buildWorkspaceJson();
    const char *path = "workspaces/last_workspace.json";
    const char *tmpPath = "workspaces/last_workspace.json.tmp";
    std::ofstream out(tmpPath, std::ios::out | std::ios::trunc);
    if (!out) {
        std::string msg = std::string("Failed to open ") + tmpPath + " for writing";
        messageBox(msg.c_str(), mfError | mfOKButton);
        return;
    }
    out << json;
    out.close();
    if (!out.good()) {
        std::string msg = std::string("Error writing ") + tmpPath;
        messageBox(msg.c_str(), mfError | mfOKButton);
        return;
    }
    // Atomic replace
    std::remove(path); // ignore errors
    std::rename(tmpPath, path);
    // Also write a timestamped snapshot: YYMMDD_HHMM
    char tsName[32];
    std::time_t t = std::time(nullptr);
    std::tm *lt = std::localtime(&t);
    std::strftime(tsName, sizeof(tsName), "%y%m%d_%H%M", lt);
    std::string snapPath = std::string("workspaces/last_workspace_") + tsName + ".json";
    std::ofstream snap(snapPath.c_str(), std::ios::out | std::ios::trunc);
    if (snap) {
        snap << json;
        snap.close();
    }
    std::string ok = std::string("Workspace saved to ") + path + "\nSnapshot: " + snapPath;
    recentWorkspaces_ = scanRecentWorkspacePaths("workspaces", kMaxRecentWorkspaces);
    currentWorkspacePath_ = path;
    messageBox(ok.c_str(), mfInformation | mfOKButton);
}

void TWwdosApp::saveWorkspaceAs()
{
    // Pre-fill with current workspace name (filename without path/extension)
    std::string defaultName;
    if (!currentWorkspacePath_.empty()) {
        size_t slash = currentWorkspacePath_.find_last_of("/\\");
        defaultName = (slash == std::string::npos) ? currentWorkspacePath_
                                                    : currentWorkspacePath_.substr(slash + 1);
        // Strip .json extension
        if (defaultName.size() > 5 && defaultName.substr(defaultName.size() - 5) == ".json")
            defaultName = defaultName.substr(0, defaultName.size() - 5);
    }

    char name[256] = {};
    if (!defaultName.empty())
        std::strncpy(name, defaultName.c_str(), sizeof(name) - 1);

    if (inputBox("Save Workspace As", "~N~ame:", name, sizeof(name) - 1) != cmOK)
        return;

    std::string nameStr(name);
    if (nameStr.empty()) return;

    // Sanitise: replace non-alphanumeric (except - _ .) with _
    for (char& c : nameStr) {
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '.')
            c = '_';
    }

    mkdir("workspaces", 0755);
    std::string savePath = "workspaces/" + nameStr + ".json";

    // Check for overwrite
    struct stat st;
    if (stat(savePath.c_str(), &st) == 0) {
        std::string msg = nameStr + ".json already exists. Overwrite?";
        if (messageBox(msg.c_str(), mfConfirmation | mfYesButton | mfNoButton) != cmYes)
            return;
    }

    if (saveWorkspacePath(savePath)) {
        currentWorkspacePath_ = savePath;
        recentWorkspaces_ = scanRecentWorkspacePaths("workspaces", kMaxRecentWorkspaces);
        std::string ok = "Workspace saved as: " + nameStr;
        messageBox(ok.c_str(), mfInformation | mfOKButton);
    }
}

bool TWwdosApp::saveWorkspacePath(const std::string& path)
{
    if (path.empty())
        return false;

    std::string json = buildWorkspaceJson();
    std::string tmpPath = path + ".tmp";

    std::ofstream out(tmpPath.c_str(), std::ios::out | std::ios::trunc);
    if (!out)
        return false;
    out << json;
    out.close();
    if (!out.good())
        return false;

    std::remove(path.c_str()); // ignore errors
    bool ok = (std::rename(tmpPath.c_str(), path.c_str()) == 0);
    if (ok)
        recentWorkspaces_ = scanRecentWorkspacePaths("workspaces", kMaxRecentWorkspaces);
    fprintf(stderr, "[workspace] save path=%s ok=%s\n", path.c_str(), ok ? "true" : "false");
    return ok;
}


