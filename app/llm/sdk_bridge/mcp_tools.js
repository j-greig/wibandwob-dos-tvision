/**
 * MCP Tools for TUI Application Control (SDK Bridge)
 *
 * This file defines tools passed to the Claude Agent SDK for Wib&Wob Chat.
 * The SDK takes tool definitions inline (not via HTTP), so this is a separate
 * surface from the FastAPI auto-generated MCP tools.
 *
 * ARCHITECTURE:
 *   - "Special" tools that need custom REST calls (create_window, move_window,
 *     get_state, close_window, send_text/figlet, paint_*, terminal_*) are
 *     defined here with typed schemas.
 *   - ALL other commands (open_gallery, cascade, tile, scramble_say, etc.)
 *     are accessed via tui_list_commands + tui_menu_command — the generic
 *     discovery+execute pair that delegates to the C++ command registry.
 *   - This means new C++ commands are instantly available without touching
 *     this file.
 */

const { createSdkMcpServer, tool } = require('@anthropic-ai/claude-agent-sdk');
const { z } = require('zod');
const axios = require('axios');

// API Server base URL (assuming running on default port)
const API_BASE_URL = 'http://127.0.0.1:8089';

// HTTP client with error handling
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

// Helper: standard success/error response wrapper
function mcpResult(text) {
    return { content: [{ type: "text", text }] };
}
function mcpError(text) {
    return { content: [{ type: "text", text }], isError: true };
}

/**
 * Create MCP server with TUI control tools.
 * Only "special" tools that need custom REST endpoints are defined here.
 * Everything else is accessed via tui_list_commands + tui_menu_command.
 */
function createTuiMcpServer() {
    return createSdkMcpServer({
        name: "tui-control",
        version: "2.0.0",
        description: "Tools for controlling TUI application windows and layout",

        tools: [
            // ── Dynamic Command Discovery & Execution ──────────────────
            // PRIMARY INTERFACE: discover what's available, then execute.

            tool(
                "tui_list_commands",
                "Discover ALL available TUI commands with descriptions and parameter hints. Call this FIRST to see what you can do (open_gallery, cascade, tile, scramble_say, open_terminal, etc.), then use tui_menu_command to execute any command by name.",
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
                "Execute any TUI command by name. Use tui_list_commands first to discover available commands. Works for ALL commands: open_gallery, cascade, tile, open_terminal, scramble_say, open_primer, gallery_list, etc.",
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

            // ── State & Screenshot (custom REST endpoints) ─────────────

            tool(
                "tui_get_state",
                "Get current state of TUI application including all windows, canvas size, and pattern mode",
                {},
                async () => {
                    try {
                        const response = await apiClient.get('/state');
                        const state = response.data;
                        const windowList = state.windows.map(w =>
                            `- ${w.id}: ${w.type} "${w.title}" at (${w.rect.x},${w.rect.y}) ${w.rect.w}x${w.rect.h}${w.focused ? ' [FOCUSED]' : ''}`
                        ).join('\n');
                        return mcpResult(`TUI State:\nCanvas: ${state.canvas.width}x${state.canvas.height} (${state.canvas.cols}x${state.canvas.rows})\nPattern Mode: ${state.pattern_mode}\nUptime: ${Math.floor(state.uptime_sec)}s\n\nWindows (${state.windows.length}):\n${windowList || '(no windows)'}`);
                    } catch (error) {
                        return mcpError(`Error getting state: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_screenshot",
                "Take a screenshot of the TUI canvas and return the text capture",
                {},
                async () => {
                    try {
                        const response = await apiClient.post('/screenshot');
                        return mcpResult(JSON.stringify(response.data, null, 2));
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            // ── Window Management (custom REST endpoints) ──────────────

            tool(
                "tui_create_window",
                "Create a new window. Use tui_list_commands to see available window types.",
                {
                    type: z.string().describe("Window type slug (e.g. test_pattern, gradient, verse, terminal, browser, paint, text_editor, orbit, life, blocks)"),
                    title: z.string().optional().describe("Window title"),
                    x: z.number().int().optional().describe("X position"),
                    y: z.number().int().optional().describe("Y position"),
                    width: z.number().int().optional().describe("Window width"),
                    height: z.number().int().optional().describe("Window height"),
                    props: z.record(z.any()).optional().describe("Additional window properties")
                },
                async (args) => {
                    try {
                        const payload = {
                            type: args.type,
                            title: args.title,
                            rect: (args.x !== undefined && args.y !== undefined && args.width && args.height) ? {
                                x: args.x, y: args.y, w: args.width, h: args.height
                            } : undefined,
                            props: args.props || {}
                        };
                        const response = await apiClient.post('/windows', payload);
                        return mcpResult(`Created ${args.type} window: ${response.data.id}\nTitle: ${response.data.title}\nPosition: (${response.data.rect.x}, ${response.data.rect.y})\nSize: ${response.data.rect.w}x${response.data.rect.h}`);
                    } catch (error) {
                        return mcpError(`Error creating window: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_move_window",
                "Move and/or resize a window by ID",
                {
                    windowId: z.string().describe("Window ID"),
                    x: z.number().int().optional().describe("New X position"),
                    y: z.number().int().optional().describe("New Y position"),
                    width: z.number().int().optional().describe("New width"),
                    height: z.number().int().optional().describe("New height")
                },
                async (args) => {
                    try {
                        const payload = {};
                        if (args.x !== undefined) payload.x = args.x;
                        if (args.y !== undefined) payload.y = args.y;
                        if (args.width !== undefined) payload.w = args.width;
                        if (args.height !== undefined) payload.h = args.height;
                        const response = await apiClient.post(`/windows/${args.windowId}/move`, payload);
                        return mcpResult(`Moved window ${args.windowId}`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_close_window",
                "Close a specific window by ID",
                { windowId: z.string().describe("Window ID") },
                async (args) => {
                    try {
                        await apiClient.post(`/windows/${args.windowId}/close`);
                        return mcpResult(`Closed window ${args.windowId}`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            // ── Text Editor (custom REST endpoints) ────────────────────

            tool(
                "tui_send_text",
                "Send text to a text editor window (auto-creates one if needed)",
                {
                    windowId: z.string().optional().describe("Text editor window ID (omit to auto-create)"),
                    content: z.string().describe("Text content to send"),
                    mode: z.enum(["append", "replace", "insert"]).default("append").describe("Insert mode")
                },
                async (args) => {
                    try {
                        const endpoint = args.windowId ?
                            `/windows/${args.windowId}/send_text` : '/text_editor/send_text';
                        await apiClient.post(endpoint, { content: args.content, mode: args.mode });
                        return mcpResult(`Text sent (${args.mode} mode)`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_send_figlet",
                "Send FIGlet ASCII art text to a text editor window",
                {
                    text: z.string().describe("Text to convert to ASCII art"),
                    font: z.string().default("standard").describe("Figlet font name"),
                    windowId: z.string().optional().describe("Text editor window ID (omit to auto-create)")
                },
                async (args) => {
                    try {
                        const endpoint = args.windowId ?
                            `/windows/${args.windowId}/send_figlet` : '/text_editor/send_figlet';
                        await apiClient.post(endpoint, { text: args.text, font: args.font });
                        return mcpResult(`Figlet "${args.text}" (${args.font}) sent`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            // ── Terminal (custom processing needed) ────────────────────

            tool(
                "tui_terminal_write",
                "Write text/commands to the terminal window. Include a real newline to press Enter.",
                { text: z.string().describe("Text to write (include newline to press Enter)") },
                async (args) => {
                    try {
                        // Unescape literal \n and \r that the LLM may produce
                        const text = args.text.replace(/\\n/g, '\n').replace(/\\r/g, '\r');
                        const payload = { command: "terminal_write", args: { text } };
                        await apiClient.post('/menu/command', payload);
                        return mcpResult(`Wrote to terminal: ${args.text.substring(0, 60)}`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_terminal_read",
                "Read current terminal output buffer",
                {},
                async () => {
                    try {
                        const payload = { command: "terminal_read" };
                        const response = await apiClient.post('/menu/command', payload);
                        const output = response.data.result || '';
                        const clean = output.replace(/\0/g, '').trim();
                        return mcpResult(`Terminal output:\n${clean.substring(0, 2000)}`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            // ── Paint Canvas (custom arg formatting) ───────────────────

            tool(
                "tui_paint_cell",
                "Set a single cell on a paint canvas. Colours 0-15 (CGA): 0=black 1=dark blue 2=dark green 3=dark cyan 4=dark red 5=dark magenta 6=brown 7=light grey 8=dark grey 9=blue 10=green 11=cyan 12=red 13=magenta 14=yellow 15=white",
                {
                    window_id: z.string().describe("Paint canvas window ID"),
                    x: z.number().describe("Column (0=left)"),
                    y: z.number().describe("Row (0=top)"),
                    fg: z.number().optional().describe("Foreground colour 0-15 (default 15)"),
                    bg: z.number().optional().describe("Background colour 0-15 (default 0)"),
                },
                async ({ window_id, x, y, fg, bg }) => {
                    try {
                        const payload = { command: "paint_cell", args: { id: window_id, x: String(x), y: String(y), fg: String(fg ?? 15), bg: String(bg ?? 0) } };
                        await apiClient.post('/menu/command', payload);
                        return mcpResult(`Cell set at (${x},${y})`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_paint_text",
                "Write text on a paint canvas at a given position",
                {
                    window_id: z.string().describe("Paint canvas window ID"),
                    x: z.number().describe("Starting column"),
                    y: z.number().describe("Row"),
                    text: z.string().describe("Text to draw"),
                    fg: z.number().optional().describe("Foreground colour 0-15 (default 15)"),
                    bg: z.number().optional().describe("Background colour 0-15 (default 0)"),
                },
                async ({ window_id, x, y, text, fg, bg }) => {
                    try {
                        const payload = { command: "paint_text", args: { id: window_id, x: String(x), y: String(y), text, fg: String(fg ?? 15), bg: String(bg ?? 0) } };
                        await apiClient.post('/menu/command', payload);
                        return mcpResult(`Text drawn at (${x},${y})`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_paint_line",
                "Draw a line on a paint canvas between two points",
                {
                    window_id: z.string().describe("Paint canvas window ID"),
                    x0: z.number().describe("Start column"),
                    y0: z.number().describe("Start row"),
                    x1: z.number().describe("End column"),
                    y1: z.number().describe("End row"),
                    erase: z.boolean().optional().describe("Erase instead of draw"),
                },
                async ({ window_id, x0, y0, x1, y1, erase }) => {
                    try {
                        const payload = { command: "paint_line", args: { id: window_id, x0: String(x0), y0: String(y0), x1: String(x1), y1: String(y1), erase: erase ? "1" : "0" } };
                        await apiClient.post('/menu/command', payload);
                        return mcpResult(`Line drawn (${x0},${y0})->(${x1},${y1})`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_paint_rect",
                "Draw a rectangle outline on a paint canvas",
                {
                    window_id: z.string().describe("Paint canvas window ID"),
                    x0: z.number().describe("Left column"),
                    y0: z.number().describe("Top row"),
                    x1: z.number().describe("Right column"),
                    y1: z.number().describe("Bottom row"),
                    erase: z.boolean().optional().describe("Erase instead of draw"),
                },
                async ({ window_id, x0, y0, x1, y1, erase }) => {
                    try {
                        const payload = { command: "paint_rect", args: { id: window_id, x0: String(x0), y0: String(y0), x1: String(x1), y1: String(y1), erase: erase ? "1" : "0" } };
                        await apiClient.post('/menu/command', payload);
                        return mcpResult(`Rectangle drawn (${x0},${y0})->(${x1},${y1})`);
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_paint_clear",
                "Clear the entire paint canvas",
                { window_id: z.string().describe("Paint canvas window ID") },
                async ({ window_id }) => {
                    try {
                        const payload = { command: "paint_clear", args: { id: window_id } };
                        await apiClient.post('/menu/command', payload);
                        return mcpResult("Canvas cleared");
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_paint_read",
                "Read the paint canvas content as text",
                { window_id: z.string().describe("Paint canvas window ID") },
                async ({ window_id }) => {
                    try {
                        const payload = { command: "paint_export", args: { id: window_id } };
                        const response = await apiClient.post('/menu/command', payload);
                        return mcpResult(JSON.stringify(response.data));
                    } catch (error) {
                        return mcpError(`Error: ${error.message}`);
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
