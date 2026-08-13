// Declarations for the figlet-text slice of the api_* bridge functions
// defined in wwdos_app.cpp (monolith split stage 5).
// Do NOT include wwdos_app.h here.
#pragma once

#include <string>

class TWwdosApp;
class TRect;

void api_spawn_figlet_text(TWwdosApp&, const TRect* bounds,
    const std::string& text, const std::string& font,
    bool frameless, bool shadowless);
void api_spawn_figlet_text_at(TWwdosApp&,
    const std::string& text, const std::string& font, int x, int y,
    bool frameless, bool shadowless);
std::string api_figlet_set_text(TWwdosApp&, const std::string& id, const std::string& text);
std::string api_figlet_set_font(TWwdosApp&, const std::string& id, const std::string& font);
std::string api_figlet_set_color(TWwdosApp&, const std::string& id, const std::string& fg, const std::string& bg);
std::string api_list_figlet_fonts();
std::string api_figlet_list_fonts();
std::string api_preview_figlet(const std::string& text, const std::string& font, int width);
