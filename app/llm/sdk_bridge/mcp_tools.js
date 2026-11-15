/**
 * MCP tools for the Wib&Wob chat: the desktop's own command table, through the desktop's socket.
 *
 *   tui_list_commands  every command, from the app's registry (get_capabilities)
 *   tui_menu_command   one command by name, run as wib or wob (as=), source resident
 *
 * The tools reach the app's socket directly (app_socket.js): no HTTP server needs to run. Each act is
 * logged in the resident's name and its hand is seen moving to the window first. Rule commands (the
 * residents' manners, marks, hand, owners, reflexes) are refused from here as from every socket caller.
 */

const { createSdkMcpServer, tool } = require('@anthropic-ai/claude-agent-sdk');
const { z } = require('zod');
const { send, act, socketPath } = require('./app_socket');

function mcpResult(text) {
    return { content: [{ type: "text", text }] };
}
function mcpError(text) {
    return { content: [{ type: "text", text }], isError: true };
}

const SOCKET_COMMANDS = 'Also on the socket (not in the registry list): get_state, create_window type= x= y= w= h= title=, ' +
    'move_window id= x= y=, resize_window id= width= height=, focus_window id=, close_window id=.';

function createTuiMcpServer() {
    return createSdkMcpServer({
        name: "tui-control",
        version: "4.0.0",
        description: "The WibWob-DOS desktop's commands, run as Wib or Wob",

        tools: [
            tool(
                "tui_list_commands",
                "List every command the desktop has, with what each does. Start with describe (no args) to see which windows are open. " + SOCKET_COMMANDS,
                {},
                async () => {
                    try {
                        return mcpResult((await send('get_capabilities')) + '\n' + SOCKET_COMMANDS);
                    } catch (error) {
                        return mcpError(`The desktop's socket (${socketPath()}) did not answer: ${error.message}`);
                    }
                }
            ),

            tool(
                "tui_menu_command",
                "Run one desktop command by name, as Wib or Wob. Examples: describe; resident_key id=w3 text=hello; " +
                "resident_mouse id=w3 action=click x=5 y=2 local=1; resident_menu choose=Window>Text Editor; " +
                "create_window type=text_editor x=2 y=1 w=40 h=10 title=notes.",
                {
                    command: z.string().describe("Command name from tui_list_commands"),
                    as: z.enum(["wib", "wob"]).optional().describe("Who acts: wib or wob (default wib)"),
                    args: z.record(z.union([z.string(), z.number()]).transform(v => String(v))).optional()
                        .describe("Command arguments as key-value pairs")
                },
                async (params) => {
                    try {
                        const r = await act(params.as || 'wib', params.command, params.args || {});
                        return r.startsWith('err') ? mcpError(r) : mcpResult(r);
                    } catch (error) {
                        return mcpError(`Command '${params.command}' failed: ${error.message}`);
                    }
                }
            ),
        ]
    });
}

module.exports = { createTuiMcpServer };
