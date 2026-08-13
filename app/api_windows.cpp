/*---------------------------------------------------------*/
/*                                                         */
/*   api_windows.cpp - window-management / generic-spawn    */
/*   api_* bridge functions. Moved verbatim from            */
/*   wwdos_app.cpp (monolith split stage 6e).                */
/*                                                         */
/*---------------------------------------------------------*/

#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TView
#define Uses_TObject
#define Uses_TProgram
#include <tvision/tv.h>

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>

#include "api_windows.h"
#include "api_paint.h"
#include "wwdos_app.h"
#include "core/json_utils.h"
#include "ww_view_utils.h"
#include "theme_manager.h"
#include "test_pattern.h"
#include "windows/pattern_windows.h"
#include "transparent_text_view.h"
#include "frame_file_player_view.h"
#include "windows/frame_animation_window.h"
#include "window_type_registry.h"
#include "command_registry.h"
#include "tuiforge_view.h"
#include "text_editor_view.h"
#include "browser_view.h"
#include "tweet_shader_view.h"
#include "disk_library_view.h"
#include "generative_verse_view.h"
#include "generative_mycelium_view.h"
#include "generative_orbit_view.h"
#include "generative_torus_view.h"
#include "generative_cube_view.h"
#include "game_of_life_view.h"
#include "animated_blocks_view.h"
#include "animated_score_view.h"
#include "animated_ascii_view.h"
#include "animated_gradient_view.h"
#include "generative_monster_cam_view.h"
#include "contour_map_view.h"
#include "generative_lab_view.h"
#include "backrooms_tv_view.h"
#include "generative_monster_verse_view.h"
#include "generative_monster_portal_view.h"
#include "micropolis_ascii_view.h"
#include "quadra_view.h"
#include "snake_view.h"
#include "rogue_view.h"
#include "deep_signal_view.h"
#include "app_launcher_view.h"
#include "ascii_gallery_view.h"
#include "wibwob_view.h"
#include "tvterm_view.h"

void api_spawn_test(TWwdosApp& app) { app.newTestWindow(); }

void api_spawn_gradient(TWwdosApp& app, const std::string& kind) {
    if (kind == "horizontal") app.newGradientWindow(TGradientWindow::gtHorizontal);
    else if (kind == "vertical") app.newGradientWindow(TGradientWindow::gtVertical);
    else if (kind == "radial") app.newGradientWindow(TGradientWindow::gtRadial);
    else if (kind == "diagonal") app.newGradientWindow(TGradientWindow::gtDiagonal);
    else app.newGradientWindow(TGradientWindow::gtHorizontal);
}

void api_open_animation_path(TWwdosApp& app, const std::string& path) {
    app.openAnimationFilePath(path);
}

void api_spawn_test(TWwdosApp& app, const TRect* bounds) {
    if (bounds) {
        app.newTestWindow(*bounds);
    } else {
        app.newTestWindow();
    }
}

void api_spawn_gradient(TWwdosApp& app, const std::string& kind, const TRect* bounds) {
    TGradientWindow::GradientType type = TGradientWindow::gtHorizontal;
    if (kind == "horizontal") type = TGradientWindow::gtHorizontal;
    else if (kind == "vertical") type = TGradientWindow::gtVertical;
    else if (kind == "radial") type = TGradientWindow::gtRadial;
    else if (kind == "diagonal") type = TGradientWindow::gtDiagonal;

    if (bounds) {
        app.newGradientWindow(type, *bounds);
    } else {
        app.newGradientWindow(type);
    }
}

void api_open_text_view_path(TWwdosApp& app, const std::string& path, const TRect* bounds) {
    if (path.empty()) return;
    int wn = app.nextWindowNumber();
    size_t lastSlash = path.find_last_of("/\\");
    std::string baseName = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    std::string title = baseName + " (Transparent)";
    TRect r;
    if (bounds && (bounds->b.x - bounds->a.x) > 0 && (bounds->b.y - bounds->a.y) > 0) {
        r = *bounds;
    } else {
        int offset = (wn - 1) % 10;
        r = TRect(2 + offset * 2, 1 + offset, 82 + offset * 2, 25 + offset);
    }
    TTransparentTextWindow* window = new TTransparentTextWindow(r, title, path);
    app.deskTop->insert(window);
    app.registerWindow(window);
}

void api_open_animation_path(TWwdosApp& app, const std::string& path, const TRect* bounds, bool frameless, bool shadowless, const std::string& title) {
    if (bounds) {
        app.openAnimationFilePath(path, *bounds, frameless, shadowless, title);
    } else if (frameless || shadowless) {
        TRect autoBounds = app.calculateWindowBounds(path);
        app.openAnimationFilePath(path, autoBounds, frameless, shadowless, title);
    } else {
        app.openAnimationFilePath(path);
    }
}

void api_cascade(TWwdosApp& app) { app.cascade(); }

void api_tile(TWwdosApp& app) { app.tile(); }

void api_close_all(TWwdosApp& app) { app.closeAll(); }

void api_set_pattern_mode(TWwdosApp& app, const std::string& mode) {
    bool continuous = (mode == "continuous");
    // Set the mode directly without showing a modal dialog (which blocks the event loop).
    USE_CONTINUOUS_PATTERN = continuous;
}

void api_save_workspace(TWwdosApp& app) {
    // Use saveWorkspacePath instead of saveWorkspace() to avoid modal dialog
    // that blocks the IPC thread when triggered via API.
    mkdir("workspaces", 0755);
    app.saveWorkspacePath("workspaces/last_workspace.json");
    // Also write a timestamped snapshot (matching saveWorkspace behaviour)
    char tsName[32];
    std::time_t t = std::time(nullptr);
    std::tm *lt = std::localtime(&t);
    std::strftime(tsName, sizeof(tsName), "%y%m%d_%H%M", lt);
    std::string snapPath = std::string("workspaces/last_workspace_") + tsName + ".json";
    app.saveWorkspacePath(snapPath);
}

bool api_save_workspace_path(TWwdosApp& app, const std::string& path) { return app.saveWorkspacePath(path); }

bool api_open_workspace_path(TWwdosApp& app, const std::string& path) {
    return app.openWorkspacePath(path);
}

void api_screenshot(TWwdosApp& app) { app.takeScreenshot(false); }

std::string api_take_last_registered_window_id(TWwdosApp& app) {
    return app.takeLastRegisteredWindowId();
}

const char* windowTypeName(TWindow* w) {
    const auto& specs = all_window_type_specs();
    for (const auto& spec : specs) {
        if (spec.matches && spec.matches(w))
            return spec.type;
    }
    // Fallback to first registry entry (canonical default type slug).
    return specs.empty() ? "test_pattern" : specs.front().type;
}

std::string api_get_state(TWwdosApp& app) {
    // Collect currently visible windows in desktop Z-order.
    // Do NOT clear winToId/idToWin here — that would reassign new IDs on
    // every call, causing compute_delta to see "new" windows every poll.
    std::vector<TWindow*> activeWins;
    TView *start = app.deskTop->first();
    if (start) {
        TView *v = start;
        do {
            TWindow *w = dynamic_cast<TWindow*>(v);
            if (w) activeWins.push_back(w);
            v = v->next;
        } while (v != start);
    }

    // Purge registry entries for windows that have been closed (stale pointers).
    // Only purge; never clear — existing live windows keep their stable IDs.
    app.syncWindowRegistry(activeWins);

    std::stringstream json;
    json << "{\"windows\":[";

    bool first = true;
    int z = 0;
    for (TWindow* w : activeWins) {
        std::string id = app.registerWindow(w, false);
        bool focused = (w == app.deskTop->current);
        if (!first) json << ",";
        json << "{\"id\":\"" << id << "\""
             << ",\"type\":\"" << windowTypeName(w) << "\""
             << ",\"x\":" << w->origin.x
             << ",\"y\":" << w->origin.y
             << ",\"w\":" << w->size.x
             << ",\"h\":" << w->size.y
             << ",\"z\":" << z
             << ",\"focused\":" << (focused ? "true" : "false")
             << ",\"title\":\"";
        if (w->title) {
            std::string title(w->title);
            for (char c : title) {
                if (c == '"') json << "\\\"";
                else if (c == '\\') json << "\\\\";
                else json << c;
            }
        }
        json << "\"";
        // Emit path for file-backed window types (needed for remote create_window)
        if (auto* ttw = dynamic_cast<TTransparentTextWindow*>(w)) {
            const std::string& p = ttw->getFilePath();
            if (!p.empty()) json << ",\"path\":\"" << json_escape(p) << "\"";
        } else if (auto* faw = dynamic_cast<TFrameAnimationWindow*>(w)) {
            const std::string& p = faw->getFilePath();
            if (!p.empty()) json << ",\"path\":\"" << json_escape(p) << "\"";
        }
        json << "}";
        first = false;
        ++z;
    }

    json << "]";

    // Append chat_log for multiplayer relay bridge
    json << ",\"chat_log\":[";
    bool firstChat = true;
    for (const auto& entry : app.chatLog()) {
        if (!firstChat) json << ",";
        json << "{\"seq\":" << entry.seq
             << ",\"sender\":\"" << json_escape(entry.sender) << "\""
             << ",\"text\":\"" << json_escape(entry.text) << "\"}";
        firstChat = false;
    }
    json << "]";
    // Chrome truth: theme_variant (reported upstream by the API server) is a
    // separate enum that never learns about CGA chrome — these two fields are
    // the real skin state.
    json << ",\"cga_chrome\":" << (ThemeManager::cgaChrome() ? "true" : "false");
    json << ",\"skin\":\"" << json_escape(ThemeManager::activeSkin()) << "\"";
    json << "}";
    return json.str();
}

std::string api_set_window_fg(TWwdosApp& app, const std::string& id, int idx) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "err window not found";
    if (auto* fp = ww_get_child_view<FrameFilePlayerView>(w)) {
        fp->setForegroundIndex(idx);
        if (w->frame) w->frame->drawView();  // frame accent follows content colour
        return "ok";
    }
    if (auto* tv = ww_get_child_view<TTextFileView>(w)) {
        tv->setForegroundIndex(idx);
        if (w->frame) w->frame->drawView();
        return "ok";
    }
    return "err window has no colourable view";
}

std::string api_set_window_bg(TWwdosApp& app, const std::string& id, int idx) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "err window not found";
    if (auto* fp = ww_get_child_view<FrameFilePlayerView>(w)) {
        fp->setBackgroundIndex(idx);
        if (w->frame) w->frame->drawView();  // frame accent follows content colour
        return "ok";
    }
    if (auto* tv = ww_get_child_view<TTextFileView>(w)) {
        tv->setBackgroundIndex(idx);
        if (w->frame) w->frame->drawView();
        return "ok";
    }
    return "err window has no colourable view";
}

std::string api_move_window(TWwdosApp& app, const std::string& id, int x, int y) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "{\"error\":\"Window not found\"}";

    TRect newBounds = w->getBounds();
    newBounds.move(x - newBounds.a.x, y - newBounds.a.y);
    w->locate(newBounds);

    app.publishEvent("state_changed", std::string("{\"id\":\"") + id + "\"}");
    return "{\"success\":true}";
}

std::string api_resize_window(TWwdosApp& app, const std::string& id, int width, int height) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "{\"error\":\"Window not found\"}";

    TRect newBounds = w->getBounds();
    newBounds.b.x = newBounds.a.x + width;
    newBounds.b.y = newBounds.a.y + height;
    w->locate(newBounds);

    app.publishEvent("state_changed", std::string("{\"id\":\"") + id + "\"}");
    return "{\"success\":true}";
}

std::string api_focus_window(TWwdosApp& app, const std::string& id) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "{\"error\":\"Window not found\"}";

    w->select();  // raises to front AND focuses (uses makeFirst → putInFrontOf)
    return "{\"success\":true}";
}

std::string api_raise_window(TWwdosApp& app, const std::string& id) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "{\"error\":\"Window not found\"}";
    w->select();
    return "{\"success\":true}";
}

std::string api_lower_window(TWwdosApp& app, const std::string& id) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "{\"error\":\"Window not found\"}";
    // putInFrontOf(background) sends to back of z-stack but above wallpaper
    if (app.deskTop->background) {
        w->putInFrontOf(app.deskTop->background);
    }
    return "{\"success\":true}";
}

std::string api_close_window(TWwdosApp& app, const std::string& id) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "{\"error\":\"Window not found\"}";

    auto isWindowAlive = [&](TWindow* target) -> bool {
        if (!target) return false;
        TView* start = app.deskTop->first();
        if (!start) return false;
        TView* v = start;
        do {
            if (v == target) return true;
            v = v->next;
        } while (v != start);
        return false;
    };

    // TWindow::close() can no-op when valid(cmClose) fails.
    // For API semantics, ensure the target is actually removed.
    w->close();
    if (isWindowAlive(w)) {
        if (w->owner)
            w->owner->remove(w);
        TObject::destroy(w);
    }
    if (isWindowAlive(w))
        return "{\"error\":\"Close failed\"}";

    // Remove from registry after successful removal.
    app.forgetWindow(w, id);

    {
        std::string payload = std::string("{\"id\":\"") + id + "\"}";
        app.publishEvent("window_closed", payload);
        app.publishEvent("state_changed", payload);
    }

    return "{\"success\":true}";
}

std::string api_window_shadow(TWwdosApp& app, const std::string& id, bool on) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "err window not found";
    w->setState(sfShadow, on);
    w->owner->drawView();  // redraw desktop to clear/show shadow
    return "ok";
}

std::string api_window_title(TWwdosApp& app, const std::string& id, const std::string& title) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "err window not found";
    delete[] (char*)w->title;
    w->title = title.empty() ? nullptr : newStr(title);
    w->frame->drawView();
    return "ok";
}

std::string api_get_canvas_size(TWwdosApp& app) {
    TRect desktop = TProgram::deskTop->getBounds();
    std::stringstream json;
    json << "{\"width\":" << desktop.b.x
         << ",\"height\":" << desktop.b.y
         << ",\"cols\":" << desktop.b.x
         << ",\"rows\":" << desktop.b.y << "}";
    return json.str();
}

void api_spawn_text_editor(TWwdosApp& app, const TRect* bounds, const std::string& title) {
    TRect r;
    if (bounds) {
        r = *bounds;
    } else {
        r = TProgram::deskTop->getBounds();
        r.grow(-5, -3);
    }
    TWindow* window = createTextEditorWindow(r, title.empty() ? "Text Editor" : title.c_str());
    TProgram::deskTop->insert(window);
}

std::string api_send_text(TWwdosApp& app, const std::string& id,
                         const std::string& content, const std::string& mode,
                         const std::string& position) {
    fprintf(stderr, "[api_send_text] START: id=%s, content_len=%zu, mode=%s\n",
            id.c_str(), content.size(), mode.c_str());

    // Special case: if id is "auto" or no text editor exists, create one
    bool autoSpawn = (id == "auto" || id == "text_editor");
    fprintf(stderr, "[api_send_text] autoSpawn=%d\n", autoSpawn);

    // Find existing text editor windows
    fprintf(stderr, "[api_send_text] Searching for existing text editor...\n");
    TView* view = app.deskTop->first();
    TTextEditorWindow* editorWindow = nullptr;

    // Use nextView() to avoid infinite loop on circular linked list
    for (TView* v = view; v; v = v->nextView()) {
        TTextEditorWindow* candidate = dynamic_cast<TTextEditorWindow*>(v);
        if (candidate) {
            editorWindow = candidate;
            fprintf(stderr, "[api_send_text] Found existing text editor\n");
            break; // Found a text editor
        }
    }

    // If no text editor found and auto-spawn is enabled, create one
    if (!editorWindow && autoSpawn) {
        fprintf(stderr, "[api_send_text] Creating new text editor window...\n");
        TRect r = app.deskTop->getBounds();
        r.grow(-5, -3);
        fprintf(stderr, "[api_send_text] Window bounds: (%d,%d)-(%d,%d)\n",
                r.a.x, r.a.y, r.b.x, r.b.y);

        fprintf(stderr, "[api_send_text] Calling createTextEditorWindow...\n");
        TWindow* newWindow = createTextEditorWindow(r);
        fprintf(stderr, "[api_send_text] Window created, inserting into desktop...\n");

        app.deskTop->insert(newWindow);
        fprintf(stderr, "[api_send_text] Window inserted\n");

        editorWindow = dynamic_cast<TTextEditorWindow*>(newWindow);
        fprintf(stderr, "[api_send_text] Cast to TTextEditorWindow: %p\n", (void*)editorWindow);
    }

    // If we have a text editor, send the text
    if (editorWindow) {
        fprintf(stderr, "[api_send_text] Focusing window...\n");
        // Focus the window
        editorWindow->select();
        fprintf(stderr, "[api_send_text] Window focused\n");

        // Send the text
        fprintf(stderr, "[api_send_text] Getting editor view...\n");
        TTextEditorView* editorView = editorWindow->getEditorView();
        fprintf(stderr, "[api_send_text] Editor view: %p\n", (void*)editorView);

        if (editorView) {
            fprintf(stderr, "[api_send_text] Calling sendText with %zu chars...\n", content.size());
            editorView->sendText(content, mode, position);
            fprintf(stderr, "[api_send_text] sendText completed\n");
            return "ok";
        }
    }

    fprintf(stderr, "[api_send_text] FAILED: no text editor available\n");
    return "err no text editor available";
}

std::string api_send_figlet(TWwdosApp& app, const std::string& id, const std::string& text,
                           const std::string& font, int width, const std::string& mode) {
    // Special case: if id is "auto" or no text editor exists, create one
    bool autoSpawn = (id == "auto" || id == "text_editor");

    // Find existing text editor windows
    TView* view = app.deskTop->first();
    TTextEditorWindow* editorWindow = nullptr;

    // Use nextView() to avoid infinite loop on circular linked list
    for (TView* v = view; v; v = v->nextView()) {
        TTextEditorWindow* candidate = dynamic_cast<TTextEditorWindow*>(v);
        if (candidate) {
            editorWindow = candidate;
            break; // Found a text editor
        }
    }

    // If no text editor found and auto-spawn is enabled, create one
    if (!editorWindow && autoSpawn) {
        TRect r = app.deskTop->getBounds();
        r.grow(-5, -3);
        TWindow* newWindow = createTextEditorWindow(r);
        app.deskTop->insert(newWindow);
        editorWindow = dynamic_cast<TTextEditorWindow*>(newWindow);
    }

    // If we have a text editor, send the figlet text
    if (editorWindow) {
        // Focus the window
        editorWindow->select();

        // Send the figlet text
        TTextEditorView* editorView = editorWindow->getEditorView();
        if (editorView) {
            editorView->sendFigletText(text, font, width, mode);
            return "ok";
        }
    }

    return "err no text editor available";
}

void api_spawn_browser(TWwdosApp& app, const TRect* bounds) {
    if (bounds) {
        app.newBrowserWindow(*bounds);
    } else {
        app.newBrowserWindow();
    }
}

static TRect api_centered_bounds(TWwdosApp& app, int width, int height) {
    TRect d = app.deskTop->getExtent();
    int dw = d.b.x - d.a.x;
    int dh = d.b.y - d.a.y;
    width  = std::max(10, std::min(width,  dw));
    height = std::max(6,  std::min(height, dh));
    int left = d.a.x + (dw - width)  / 2;
    int top  = d.a.y + (dh - height) / 2;
    return TRect(left, top, left + width, top + height);
}

std::string wwdos_exec_command(const std::string& name,
                               const std::map<std::string, std::string>& kv) {
    auto* app = dynamic_cast<TWwdosApp*>(TProgram::application);
    if (!app) return "err no running app";
    return exec_registry_command(*app, name, kv);
}

void api_spawn_shader(TWwdosApp& app, const TRect* bounds, const std::string& shader) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 92, 32);
    TWindow* w = createTweetShaderWindow(r, shader);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_disks(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 79, 28);
    TWindow* w = createDiskLibraryWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_tuiforge(TWwdosApp& app, const TRect* bounds,
                        const std::string& pathOrName) {
    TWindow* w = nullptr;
    if (pathOrName.empty()) {
        // No render named: open the corpus picker.
        TRect r = bounds ? *bounds : app.findSpreadRect(64, 30);
        w = new TTuiforgePickerWindow(r);
    } else {
        std::string dir = resolveTuiforgeDir(pathOrName);
        TuiforgeGrid g = loadTuiforgeGrid(dir.empty() ? pathOrName : dir);
        // Title = the scene name, not the "default" leaf.
        std::string title = pathOrName;
        size_t slash = title.find_last_of('/');
        if (slash != std::string::npos && title.substr(slash + 1) == "default")
            title = title.substr(0, slash);
        slash = title.find_last_of('/');
        // keep one path segment of context (kevart/cat3d stays whole)
        if (title.size() > 40 && slash != std::string::npos)
            title = title.substr(slash + 1);
        // Window sized to the grid (+frame), spread not stacked; clamp to
        // the desktop — the view scrolls when a 50-row portrait overflows.
        TRect d = app.deskTop->getExtent();
        int ww = std::min(g.width + 2, d.b.x - d.a.x);
        int wh = std::min(g.height() + 2, d.b.y - d.a.y);
        TRect r = bounds ? *bounds
                         : app.findSpreadRect(std::max(10, ww), std::max(6, wh));
        w = new TTuiforgeWindow(r, title, std::move(g));
    }
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_verse(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 96, 30);
    TWindow* w = createGenerativeVerseWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_mycelium(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 96, 30);
    TWindow* w = createGenerativeMyceliumWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_orbit(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 96, 30);
    TWindow* w = createGenerativeOrbitWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_torus(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 90, 28);
    TWindow* w = createGenerativeTorusWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_cube(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 90, 28);
    TWindow* w = createGenerativeCubeWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_life(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 90, 28);
    TWindow* w = createGameOfLifeWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_blocks(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 84, 24);
    TWindow* w = createAnimatedBlocksWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_score(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 108, 34);
    TWindow* w = createAnimatedScoreWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_ascii(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 96, 30);
    TWindow* w = createAnimatedAsciiWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_animated_gradient(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 84, 24);
    TWindow* w = createAnimatedGradientWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_monster_cam(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 96, 30);
    TWindow* w = createGenerativeMonsterCamWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_contour_map(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 96, 30);
    TWindow* w = createContourMapWindow(r, 0, 5, 5, false, false);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_generative_lab(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 96, 30);
    TWindow* w = createGenerativeLabWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_backrooms_tv(TWwdosApp& app, const TRect* bounds, const BackroomsChannel* ch) {
    BackroomsChannel channel;
    if (ch) {
        channel = *ch;
    } else {
        // No channel provided — show config dialog (menu path)
        if (!showBackroomsTvDialog(channel)) return;
    }
    TRect r = bounds ? *bounds : api_centered_bounds(app, 100, 35);
    TWindow* w = createBackroomsTvWindow(r, channel);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_backrooms_tv(TWwdosApp& app, const TRect* bounds) {
    api_spawn_backrooms_tv(app, bounds, nullptr);
}

void api_spawn_monster_verse(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 96, 30);
    TWindow* w = createGenerativeMonsterVerseWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_monster_portal(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 96, 30);
    TWindow* w = createGenerativeMonsterPortalWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_micropolis_ascii(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 110, 34);
    TWindow* w = createMicropolisAsciiWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_quadra(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 42, 26);
    TWindow* w = createQuadraWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_snake(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 60, 30);
    TWindow* w = createSnakeWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_rogue(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 80, 38);
    TWindow* w = createRogueWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_deep_signal(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 70, 34);
    TWindow* w = createDeepSignalWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_app_launcher(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 63, 20);
    TWindow* w = createAppLauncherWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

void api_spawn_gallery(TWwdosApp& app, const TRect* bounds) {
    TRect desk = app.deskTop->getExtent();
    TRect r = bounds ? *bounds : api_centered_bounds(app, desk.b.x * 9 / 10, desk.b.y * 9 / 10);
    TWindow* w = createAsciiGalleryWindow(r);
    app.deskTop->insert(w);
    app.registerWindow(w);
}

std::string api_gallery_list(TWwdosApp& app, const std::string& tab) {
    (void)app;
    // Find primer dir
    std::string primerDir = findPrimerDir();
    DIR* dir = opendir(primerDir.c_str());
    if (!dir) return "{\"error\":\"no primer directory\"}";

    // Resolve tab filter before scanning
    int tabIdx = -1;
    if (!tab.empty()) {
        if (tab == "#-C" || tab == "1") tabIdx = 0;
        else if (tab == "D-L" || tab == "2") tabIdx = 1;
        else if (tab == "M" || tab == "3") tabIdx = 2;
        else if (tab == "N-S" || tab == "4") tabIdx = 3;
        else if (tab == "T-Z" || tab == "5") tabIdx = 4;
        else {
            closedir(dir);
            return "{\"error\":\"unknown tab filter: " + json_escape(tab) + "\"}";
        }
    }

    std::vector<std::string> files;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string name = entry->d_name;
        if (name.size() < 5 || name.substr(name.size() - 4) != ".txt") continue;

        // Apply tab filter
        if (tabIdx >= 0 && !name.empty()) {
            char c = (char)std::tolower((unsigned char)name[0]);
            bool match = false;
            switch (tabIdx) {
                case 0: match = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'c'); break;
                case 1: match = c >= 'd' && c <= 'l'; break;
                case 2: match = c == 'm'; break;
                case 3: match = c >= 'n' && c <= 's'; break;
                case 4: match = c >= 't' && c <= 'z'; break;
            }
            if (!match) continue;
        }
        files.push_back(name);
    }
    closedir(dir);

    std::sort(files.begin(), files.end());

    std::ostringstream os;
    os << "{\"count\":" << files.size() << ",\"files\":[";
    for (size_t i = 0; i < files.size(); i++) {
        if (i > 0) os << ",";
        os << "\"" << json_escape(files[i]) << "\"";
    }
    os << "]}";
    return os.str();
}

void api_spawn_wibwob(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : app.findSpreadRect(80, 27);
    std::string title = "Wib&Wob Chat " + std::to_string(app.nextWindowNumber());
    TWindow* w = createWibWobWindow(r, title);
    if (w) {
        app.deskTop->insert(w);
        app.registerWindow(w);
        w->select();
    }
}

void api_spawn_terminal(TWwdosApp& app, const TRect* bounds) {
    TRect r = bounds ? *bounds : api_centered_bounds(app, 80, 24);
    TWindow* w = createTerminalWindow(r);
    if (w) {
        app.deskTop->insert(w);
        app.registerWindow(w);
    }
}

std::string api_browser_fetch(TWwdosApp& app, const std::string& url) {
    // Find the most recently inserted browser window and trigger a fetch
    TView* start = app.deskTop->first();
    TBrowserWindow* browserWin = nullptr;
    if (start) {
        TView* v = start;
        do {
            TBrowserWindow* candidate = dynamic_cast<TBrowserWindow*>(v);
            if (candidate) {
                browserWin = candidate;
            }
            v = v->next;
        } while (v != start);
    }

    if (!browserWin) {
        return "err no browser window";
    }

    browserWin->fetchUrl(url);
    return "ok";
}
