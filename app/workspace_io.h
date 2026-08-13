// Declarations for the workspace save/load slice of the api_* bridge
// functions defined in wwdos_app.cpp (monolith split stage 5).
// Do NOT include wwdos_app.h here.
#pragma once

#include <string>

class TWwdosApp;

void api_save_workspace(TWwdosApp&);
bool api_save_workspace_path(TWwdosApp&, const std::string& path);
bool api_open_workspace_path(TWwdosApp&, const std::string& path);
