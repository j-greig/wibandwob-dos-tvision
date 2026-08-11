#pragma once

#include <map>
#include <string>
#include <vector>

class TWwdosApp;

struct CommandCapability {
    const char* name;
    const char* description;
    bool requires_path;
};

const std::vector<CommandCapability>& get_command_capabilities();
std::string get_command_capabilities_json();
std::string exec_registry_command(
    TWwdosApp& app,
    const std::string& name,
    const std::map<std::string, std::string>& kv);

// Execute a registry command against the running app instance. Main thread
// only (views, menu handlers). Defined in wwdos_app.cpp; returns the command
// result string, or an error if no app is running.
std::string wwdos_exec_command(
    const std::string& name,
    const std::map<std::string, std::string>& kv);
