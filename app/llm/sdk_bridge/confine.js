/**
 * The chat's agent may use the desktop's tools and nothing on the person's machine: no shell, no
 * files, no web, no settings, hooks or MCP servers from the person's configuration, and a working
 * directory that is an empty temporary folder. Used by claude_sdk_bridge.js and by probe_confine.js.
 */
const fs = require('fs');
const os = require('os');
const path = require('path');

const BUILT_IN = ['Bash', 'BashOutput', 'KillShell', 'KillBash', 'Read', 'Write', 'Edit', 'MultiEdit', 'Glob', 'Grep',
    'LS', 'NotebookEdit', 'NotebookRead', 'WebFetch', 'WebSearch', 'Task', 'Agent', 'TodoWrite', 'ExitPlanMode',
    'SlashCommand', 'Skill', 'AskUserQuestion', 'ListMcpResourcesTool', 'ReadMcpResourceTool'];

let emptyDir = null;
function workDir() {
    if (!emptyDir) emptyDir = fs.mkdtempSync(path.join(os.tmpdir(), 'wwdos-chat-'));
    return emptyDir;
}

const DESK_TOOLS = ['mcp__tui-control__tui_list_commands', 'mcp__tui-control__tui_menu_command'];
const isDeskTool = name => name === 'mcp__tui-control' || name.startsWith('mcp__tui-control__');

function confinedOptions(base, mcpServer, mcpToolNames) {
    const allowed = [...new Set([...mcpToolNames, ...DESK_TOOLS])].filter(isDeskTool);
    return {
        ...base,
        tools: [],                         // no built-in tools at all
        allowedTools: allowed,
        disallowedTools: BUILT_IN,
        permissionMode: 'default',         // never bypassPermissions: canUseTool decides
        canUseTool: async (name, input) => isDeskTool(name)
            ? { behavior: 'allow', updatedInput: input }
            : { behavior: 'deny', message: `only the desktop's tools are available here; ${name} is not one` },
        settingSources: [],                // the person's settings, hooks and CLAUDE.md files stay out
        strictMcpConfig: true,             // only the desktop's MCP server
        mcpServers: mcpServer ? { 'tui-control': mcpServer } : {},
        cwd: workDir(),
    };
}

module.exports = { confinedOptions, BUILT_IN, DESK_TOOLS, workDir };
