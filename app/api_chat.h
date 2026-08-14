// Declarations for the Scramble cat / Wib&Wob chat / room-chat slice of the
// api_* bridge functions defined in wwdos_app.cpp (monolith split stage 5).
// Do NOT include wwdos_app.h here.
#pragma once

#include <string>

class TWwdosApp;
class TRect;

void api_toggle_scramble(TWwdosApp&);
void api_expand_scramble(TWwdosApp&);
std::string api_scramble_say(TWwdosApp&, const std::string& text);
std::string api_scramble_pet(TWwdosApp&);
std::string api_chat_receive(TWwdosApp&, const std::string& sender, const std::string& text);
std::string api_wibwob_ask(TWwdosApp&, const std::string& text);
std::string api_get_chat_history(TWwdosApp&);

void api_spawn_room_chat(TWwdosApp&, const TRect* bounds);
std::string api_room_chat_receive(TWwdosApp&, const std::string& sender,
                                   const std::string& text, const std::string& ts);
std::string api_room_presence(TWwdosApp&, const std::string& participants_json);
std::string api_get_room_chat_pending(TWwdosApp&);
std::string api_get_room_chat_display_name(TWwdosApp&);
