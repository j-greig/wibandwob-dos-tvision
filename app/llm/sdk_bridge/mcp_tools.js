/**
 * MCP Tools for TUI Application Control (SDK Bridge)
 *
 * ARCHITECTURE:
 *   Two tools. That's it.
 *   - tui_list_commands: discovers all available commands from the C++ registry
 *   - tui_menu_command: executes any command by name with args
 *
 *   New C++ commands are instantly available. Never touch this file.
 */

const { createSdkMcpServer, tool } = require('@anthropic-ai/claude-agent-sdk');
const { z } = require('zod');
const axios = require('axios');

const API_BASE_URL = 'http://127.0.0.1:8089';

const apiClient = axios.create({
    baseURL: API_BASE_URL,
    timeout: 5000,
    headers: { 'Content-Type': 'application/json' }
});

apiClient.interceptors.response.use(
    (response) => response,
    (error) => {
        if (error.code === 'ECONNREFUSED') {
            throw new Error('API server not running. Please start the TUI application first.');
        }
        throw error;
    }
);

function mcpResult(text) {
    return { content: [{ type: "text", text }] };
}
function mcpError(text) {
    return { content: [{ type: "text", text }], isError: true };
}

function createTuiMcpServer() {
    return createSdkMcpServer({
        name: "tui-control",
        version: "3.0.0",
        description: "Two tools to discover and execute any TUI command",

        tools: [
            tool(
                "tui_list_commands",
                "Discover ALL available TUI commands with descriptions and parameter hints. Call this FIRST to see what you can do, then use tui_menu_command to execute any command by name. Returns the full live C++ command registry.",
                {},
                async () => {
                    try {
                        const response = await apiClient.get('/commands');
                        return mcpResult(JSON.stringify(response.data, null, 2));
                    } catch (error) {
                        return mcpError(`Error listing commands: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_menu_command",
                "Execute any TUI command by name. Use tui_list_commands first to discover available commands. Covers everything: window management (cascade, tile, close_all), apps (open_gallery, open_terminal, open_scramble), text (send_text, send_figlet), paint canvas, screenshots, themes, and more.",
                {
                    command: z.string().describe("Command name from tui_list_commands"),
                    args: z.record(z.string()).optional().describe("Command arguments as key-value pairs")
                },
                async (params) => {
                    try {
                        const payload = { command: params.command };
                        if (params.args && Object.keys(params.args).length > 0) {
                            payload.args = params.args;
                        }
                        const response = await apiClient.post('/menu/command', payload);
                        return mcpResult(JSON.stringify(response.data, null, 2));
                    } catch (error) {
                        const detail = error.response?.data?.detail || error.message;
                        return mcpError(`Command '${params.command}' failed: ${detail}`);
                    }
                }
            ),
        ]
    });
}

module.exports = {
    createTuiMcpServer,
    API_BASE_URL
};
