// Declarations for the desktop/theme/skin/screensaver slice of the api_*
// bridge functions defined in wwdos_app.cpp (monolith split stage 5).
// Do NOT include wwdos_app.h here.
#pragma once

#include <string>

class TWwdosApp;

std::string api_desktop_preset(TWwdosApp&, const std::string& preset);
std::string api_desktop_texture(TWwdosApp&, const std::string& ch);
std::string api_desktop_color(TWwdosApp&, int fg, int bg);
std::string api_desktop_rulers(TWwdosApp&, bool on);
std::string api_desktop_gallery(TWwdosApp&, bool on);
std::string api_desktop_get(TWwdosApp&);

std::string api_set_theme_mode(TWwdosApp&, const std::string& mode);
std::string api_set_theme_variant(TWwdosApp&, const std::string& variant);
std::string api_reset_theme(TWwdosApp&);
std::string api_set_skin(TWwdosApp&, const std::string& name);
std::string api_list_skins(TWwdosApp&);
std::string api_reload_skins(TWwdosApp&);
std::string api_skin_save(TWwdosApp&, const std::string& name);

std::string api_screensaver(TWwdosApp&, const std::string& action, int minutes);
void api_note_input(TWwdosApp&);   // reset the saver idle clock (any API command = activity)
