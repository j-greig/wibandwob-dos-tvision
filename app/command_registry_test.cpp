#include "command_registry.h"

#include <iostream>
#include <string>

// Test stubs for symbols referenced by command_registry.cpp dispatch function.
//
// Regeneration recipe: after editing command_registry.cpp's dispatch/extern
// block, rebuild this target and collect undefined symbols, e.g.:
//   cmake --build build --target command_registry_test 2>&1 | grep "undefined symbol"
// then add a stub here matching the extern signature at the top of
// command_registry.cpp (return "ok"/{}/void as appropriate).
class TRect;
class TWwdosApp {};
void api_cascade(TWwdosApp&) {}
void api_tile(TWwdosApp&) {}
void api_close_all(TWwdosApp&) {}
void api_save_workspace(TWwdosApp&) {}
bool api_open_workspace_path(TWwdosApp&, const std::string&) { return true; }
bool api_save_workspace_path(TWwdosApp&, const std::string&) { return true; }
void api_screenshot(TWwdosApp&) {}
void api_set_pattern_mode(TWwdosApp&, const std::string&) {}
std::string api_set_theme_mode(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_set_theme_variant(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_set_skin(TWwdosApp&, const std::string&) { return "ok"; }
void api_spawn_disks(TWwdosApp&, const TRect*) {}
void api_spawn_shader(TWwdosApp&, const TRect*, const std::string&) {}
std::string api_screensaver(TWwdosApp&, const std::string&, int) { return "ok"; }
std::string api_list_skins(TWwdosApp&) { return "{}"; }
std::string api_reload_skins(TWwdosApp&) { return "ok"; }
std::string api_skin_save(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_reset_theme(TWwdosApp&) { return "ok"; }
void api_toggle_scramble(TWwdosApp&) {}
void api_expand_scramble(TWwdosApp&) {}
std::string api_scramble_say(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_scramble_pet(TWwdosApp&) { return "ok"; }
class BackroomsChannel;
void api_spawn_room_chat(TWwdosApp&, const TRect*) {}
std::string api_room_chat_receive(TWwdosApp&, const std::string&, const std::string&, const std::string&) { return "ok"; }
std::string api_room_presence(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_set_window_bg(TWwdosApp&, const std::string&, int) { return "ok"; }
std::string api_set_window_fg(TWwdosApp&, const std::string&, int) { return "ok"; }
std::string api_desktop_rulers(TWwdosApp&, bool) { return "ok"; }
std::string api_get_chat_history(TWwdosApp&) { return "ok"; }
std::string api_window_shadow(TWwdosApp&, const std::string&, bool) { return "ok"; }
std::string api_window_title(TWwdosApp&, const std::string&, const std::string&) { return "ok"; }
std::string api_desktop_preset(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_desktop_texture(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_desktop_color(TWwdosApp&, int, int) { return "ok"; }
std::string api_desktop_gallery(TWwdosApp&, bool) { return "ok"; }
std::string api_desktop_get(TWwdosApp&) { return "ok"; }
std::string api_figlet_set_text(TWwdosApp&, const std::string&, const std::string&) { return "ok"; }
std::string api_figlet_set_font(TWwdosApp&, const std::string&, const std::string&) { return "ok"; }
std::string api_figlet_set_color(TWwdosApp&, const std::string&, const std::string&, const std::string&) { return "ok"; }
std::string api_figlet_list_fonts() { return "ok"; }
void api_open_animation_path(TWwdosApp&, const std::string&, const TRect*, bool, bool, const std::string&) {}
void api_open_animation_path(TWwdosApp&, const std::string&) {}
void api_spawn_gallery(TWwdosApp&, const TRect*) {}
void api_spawn_verse(TWwdosApp&, const TRect*) {}
void api_spawn_mycelium(TWwdosApp&, const TRect*) {}
void api_spawn_orbit(TWwdosApp&, const TRect*) {}
void api_spawn_torus(TWwdosApp&, const TRect*) {}
void api_spawn_cube(TWwdosApp&, const TRect*) {}
void api_spawn_life(TWwdosApp&, const TRect*) {}
void api_spawn_blocks(TWwdosApp&, const TRect*) {}
void api_spawn_score(TWwdosApp&, const TRect*) {}
void api_spawn_ascii(TWwdosApp&, const TRect*) {}
void api_spawn_animated_gradient(TWwdosApp&, const TRect*) {}
void api_spawn_gradient(TWwdosApp&, const std::string&, const TRect*) {}
void api_spawn_monster_cam(TWwdosApp&, const TRect*) {}
void api_spawn_monster_verse(TWwdosApp&, const TRect*) {}
void api_spawn_monster_portal(TWwdosApp&, const TRect*) {}
void api_spawn_backrooms_tv(TWwdosApp&, const TRect*) {}
void api_spawn_backrooms_tv(TWwdosApp&, const TRect*, const BackroomsChannel*) {}
void api_spawn_browser(TWwdosApp&, const TRect*) {}
void api_spawn_figlet_text(TWwdosApp&, const TRect*, const std::string&, const std::string&, bool, bool) {}
void api_spawn_figlet_text_at(TWwdosApp&, const std::string&, const std::string&, int, int, bool, bool) {}
void api_spawn_text_editor(TWwdosApp&, const TRect*, const std::string&) {}
void api_spawn_wibwob(TWwdosApp&, const TRect*) {}
std::string api_gallery_list(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_paint_save(TWwdosApp&, const std::string&, const std::string&) { return "ok"; }
std::string api_paint_load(TWwdosApp&, const std::string&, const std::string&) { return "ok"; }
void api_spawn_paint_with_file(TWwdosApp&, const std::string&) {}
std::string api_paint_stamp_figlet(TWwdosApp&, const std::string&, const std::string&, const std::string&, int, int, uint8_t, uint8_t) { return "ok"; }
std::string api_list_figlet_fonts() { return "ok"; }
std::string api_preview_figlet(const std::string&, const std::string&, int) { return "ok"; }
std::string api_move_window(TWwdosApp&, const std::string&, int, int) { return "ok"; }
std::string api_resize_window(TWwdosApp&, const std::string&, int, int) { return "ok"; }
std::string api_focus_window(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_raise_window(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_lower_window(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_close_window(TWwdosApp&, const std::string&) { return "ok"; }
namespace figlet { int fontHeight(const std::string&) { return 6; } }
std::string findPrimerDir() { return "primers"; }
void api_spawn_paint(TWwdosApp&, const TRect*) {}
void api_spawn_micropolis_ascii(TWwdosApp&, const TRect*) {}
void api_spawn_quadra(TWwdosApp&, const TRect*) {}
void api_spawn_snake(TWwdosApp&, const TRect*) {}
void api_spawn_rogue(TWwdosApp&, const TRect*) {}
void api_spawn_deep_signal(TWwdosApp&, const TRect*) {}
void api_spawn_app_launcher(TWwdosApp&, const TRect*) {}
void api_spawn_terminal(TWwdosApp&, const TRect*) {}
std::string api_terminal_write(TWwdosApp&, const std::string&, const std::string&) { return "ok"; }
std::string api_terminal_read(TWwdosApp&, const std::string&) { return ""; }
std::string api_chat_receive(TWwdosApp&, const std::string&, const std::string&) { return "ok"; }
std::string api_wibwob_ask(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_paint_cell(TWwdosApp&, const std::string&, int, int, uint8_t, uint8_t) { return "ok"; }
std::string api_paint_text(TWwdosApp&, const std::string&, int, int, const std::string&, uint8_t, uint8_t) { return "ok"; }
std::string api_paint_line(TWwdosApp&, const std::string&, int, int, int, int, bool) { return "ok"; }
std::string api_paint_rect(TWwdosApp&, const std::string&, int, int, int, int, bool) { return "ok"; }
std::string api_paint_clear(TWwdosApp&, const std::string&) { return "ok"; }
std::string api_paint_export(TWwdosApp&, const std::string&) { return "ok"; }

int main() {
    const std::string payload = get_command_capabilities_json();

    const char* required[] = {
        "\"name\":\"cascade\"",
        "\"name\":\"tile\"",
        "\"name\":\"close_all\"",
        "\"name\":\"save_workspace\"",
        "\"name\":\"open_workspace\"",
        "\"name\":\"screenshot\"",
        "\"name\":\"pattern_mode\"",
        "\"name\":\"set_theme_mode\"",
        "\"name\":\"set_theme_variant\"",
        "\"name\":\"reset_theme\"",
        "\"name\":\"open_scramble\"",
        "\"name\":\"scramble_expand\"",
        "\"name\":\"scramble_say\"",
        "\"name\":\"scramble_pet\"",
        "\"name\":\"new_paint_canvas\"",
        "\"name\":\"open_micropolis_ascii\"",
        "\"name\":\"open_quadra\"",
        "\"name\":\"open_snake\"",
        "\"name\":\"open_rogue\"",
        "\"name\":\"open_deep_signal\"",
        "\"name\":\"open_apps\"",
        "\"name\":\"open_terminal\"",
        "\"name\":\"terminal_write\"",
        "\"name\":\"chat_receive\"",
        "\"name\":\"paint_cell\"",
        "\"name\":\"paint_text\"",
        "\"name\":\"paint_line\"",
        "\"name\":\"paint_rect\"",
        "\"name\":\"paint_clear\"",
        "\"name\":\"paint_export\"",
    };

    for (const char* token : required) {
        if (payload.find(token) == std::string::npos) {
            std::cerr << "missing capability token: " << token << "\n";
            return 1;
        }
    }
    return 0;
}
