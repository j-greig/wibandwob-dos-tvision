// Declarations for the window-management / generic-spawn slice of the
// api_* bridge functions defined in wwdos_app.cpp (monolith split stage 5).
// command_registry.cpp / api_ipc.cpp / window_type_registry.cpp include this
// instead of hand-rolled extern blocks — do NOT include wwdos_app.h here.
#pragma once

#include <string>

class TWwdosApp;
class TRect;
class TWindow;
struct BackroomsChannel;
class TGenerativeLabView;

// Shared with wwdos_app.cpp (buildWorkspaceJson) — promoted to external
// linkage rather than duplicated (monolith split stage 6e).
const char* windowTypeName(TWindow* w);

// ── spawn / open ─────────────────────────────────────────────────────────
void api_spawn_test(TWwdosApp&);
void api_spawn_test(TWwdosApp&, const TRect* bounds);
void api_spawn_gradient(TWwdosApp&, const std::string& kind);
void api_spawn_gradient(TWwdosApp&, const std::string& kind, const TRect* bounds);
void api_open_animation_path(TWwdosApp&, const std::string& path);
void api_open_animation_path(TWwdosApp&, const std::string& path, const TRect* bounds,
                              bool frameless, bool shadowless, const std::string& title);
void api_open_text_view_path(TWwdosApp&, const std::string& path, const TRect* bounds);
void api_spawn_text_editor(TWwdosApp&, const TRect* bounds, const std::string& title);
void api_spawn_browser(TWwdosApp&, const TRect* bounds);
void api_spawn_wibwob(TWwdosApp&, const TRect* bounds);
void api_spawn_terminal(TWwdosApp&, const TRect* bounds);
void api_spawn_disks(TWwdosApp&, const TRect* bounds);
// TUIFORGE.DSK: pathOrName "" = picker window; else resolve a render
// (corpus-relative name or absolute path) and open it sized to its grid.
void api_spawn_tuiforge(TWwdosApp&, const TRect* bounds, const std::string& pathOrName);
void api_spawn_shader(TWwdosApp&, const TRect* bounds, const std::string& shader);
void api_spawn_verse(TWwdosApp&, const TRect* bounds);
void api_spawn_mycelium(TWwdosApp&, const TRect* bounds);
void api_spawn_orbit(TWwdosApp&, const TRect* bounds);
void api_spawn_torus(TWwdosApp&, const TRect* bounds);
void api_spawn_cube(TWwdosApp&, const TRect* bounds);
void api_spawn_life(TWwdosApp&, const TRect* bounds);
void api_spawn_blocks(TWwdosApp&, const TRect* bounds);
void api_spawn_score(TWwdosApp&, const TRect* bounds);
void api_spawn_ascii(TWwdosApp&, const TRect* bounds);
void api_spawn_animated_gradient(TWwdosApp&, const TRect* bounds);
void api_spawn_monster_cam(TWwdosApp&, const TRect* bounds);
void api_spawn_monster_verse(TWwdosApp&, const TRect* bounds);
void api_spawn_monster_portal(TWwdosApp&, const TRect* bounds);
void api_spawn_contour_map(TWwdosApp&, const TRect* bounds);
void api_spawn_generative_lab(TWwdosApp&, const TRect* bounds);
TGenerativeLabView* api_find_gen_lab_view(TWwdosApp&, const std::string&);
void api_spawn_backrooms_tv(TWwdosApp&, const TRect* bounds);
void api_spawn_backrooms_tv(TWwdosApp&, const TRect* bounds, const BackroomsChannel* ch);
void api_spawn_micropolis_ascii(TWwdosApp&, const TRect* bounds);
void api_spawn_quadra(TWwdosApp&, const TRect* bounds);
void api_spawn_snake(TWwdosApp&, const TRect* bounds);
void api_spawn_rogue(TWwdosApp&, const TRect* bounds);
void api_spawn_deep_signal(TWwdosApp&, const TRect* bounds);
void api_spawn_app_launcher(TWwdosApp&, const TRect* bounds);
void api_spawn_gallery(TWwdosApp&, const TRect* bounds);
std::string api_gallery_list(TWwdosApp&, const std::string& tab);

// ── window management ───────────────────────────────────────────────────
void api_cascade(TWwdosApp&);
void api_tile(TWwdosApp&);
void api_close_all(TWwdosApp&);
void api_screenshot(TWwdosApp&);
void api_set_pattern_mode(TWwdosApp&, const std::string& mode);
std::string api_take_last_registered_window_id(TWwdosApp&);
std::string api_get_state(TWwdosApp&);
std::string api_get_canvas_size(TWwdosApp&);
std::string api_move_window(TWwdosApp&, const std::string& id, int x, int y);
std::string api_set_window_bg(TWwdosApp&, const std::string& id, int idx);
std::string api_set_window_fg(TWwdosApp&, const std::string& id, int idx);
std::string api_resize_window(TWwdosApp&, const std::string& id, int width, int height);
std::string api_focus_window(TWwdosApp&, const std::string& id);
std::string api_raise_window(TWwdosApp&, const std::string& id);
std::string api_lower_window(TWwdosApp&, const std::string& id);
std::string api_close_window(TWwdosApp&, const std::string& id);
std::string api_window_shadow(TWwdosApp&, const std::string& id, bool on);
std::string api_window_title(TWwdosApp&, const std::string& id, const std::string& title);

// ── terminal / send bridges ─────────────────────────────────────────────
std::string api_terminal_write(TWwdosApp&, const std::string& text, const std::string& window_id);
std::string api_terminal_read(TWwdosApp&, const std::string& window_id);
std::string api_send_text(TWwdosApp&, const std::string& id, const std::string& content,
                           const std::string& mode, const std::string& position);
std::string api_send_figlet(TWwdosApp&, const std::string& id, const std::string& text,
                             const std::string& font, int width, const std::string& mode);
std::string api_browser_fetch(TWwdosApp&, const std::string& url);
