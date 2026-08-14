// Declarations for the workspace save/load slice of the api_* bridge
// functions defined in wwdos_app.cpp (monolith split stage 5).
// Do NOT include wwdos_app.h here.
#pragma once

#include <string>
#include <vector>
#include <ctime>

class TWwdosApp;

void api_save_workspace(TWwdosApp&);
bool api_save_workspace_path(TWwdosApp&, const std::string& path);
bool api_open_workspace_path(TWwdosApp&, const std::string& path);

// Recent-workspace scanning helpers. Definitions currently still in
// wwdos_app.cpp (external linkage, monolith split stage 7a promotion);
// move to workspace_io.cpp in stage 7b. Shared by TWwdosApp::
// manageWorkspaces (workspace_manager_dialog.cpp) and
// buildRecentWorkspacesSubmenuItem (wwdos_app.cpp until stage 8).
extern const int kMaxRecentWorkspaces;

struct RecentWorkspaceInfo {
    std::string path;
    std::string fileName;
    time_t mtime;
};

std::vector<std::string> scanRecentWorkspacePaths(const char* dirPath, int maxCount);
int countWindowsInWorkspace(const std::string& path);
std::string recentWorkspaceLabel(const std::string& path);
