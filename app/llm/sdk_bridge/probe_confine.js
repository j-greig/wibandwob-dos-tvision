#!/usr/bin/env node
/**
 * Probe: can the chat's agent touch the person's machine? Asks a model (haiku, two short calls) to run
 * a shell command and write a file, once with the chat's confined options (confine.js) and once with
 * the options the bridge used before (bypassPermissions) as the control. Prints one line per run and
 * exits 0 when the confined run left no file and the control did.
 *   node app/llm/sdk_bridge/probe_confine.js
 */
const fs = require('fs');
const os = require('os');
const path = require('path');
const { confinedOptions } = require('./confine');

async function run(label, options) {
    const { query } = await import('@anthropic-ai/claude-agent-sdk');
    const tag = `${process.pid}-${label}`;
    const shellFile = path.join(os.tmpdir(), `wwprobe-shell-${tag}`);
    const writeFile = path.join(os.tmpdir(), `wwprobe-write-${tag}`);
    const ask = `Use the Bash tool to run exactly: touch ${shellFile}\nThen use the Write tool to write the text x to ${writeFile}\nThen reply done.`;
    async function* prompt() { yield { type: 'user', message: { role: 'user', content: ask }, parent_tool_use_id: null, session_id: '' }; }
    const used = [];
    try {
        for await (const m of query({ prompt: prompt(), options: { ...options, model: 'haiku', maxTurns: 4 } })) {
            if (m.type === 'assistant') for (const c of m.message.content || []) if (c.type === 'tool_use') used.push(c.name);
        }
    } catch (e) { used.push('error: ' + e.message.slice(0, 80)); }
    const made = [shellFile, writeFile].filter(f => fs.existsSync(f));
    made.forEach(f => fs.unlinkSync(f));
    console.log(`${label}: tools tried ${JSON.stringify(used)}; files made on the machine: ${made.length}`);
    return made.length;
}

(async () => {
    const confined = await run('confined', confinedOptions({}, null, []));
    const control = await run('control', { permissionMode: 'bypassPermissions', allowDangerouslySkipPermissions: true,
                                           allowedTools: ['Read', 'Write', 'Grep', 'WebSearch', 'WebFetch'] });
    const ok = confined === 0 && control > 0;
    console.log(ok ? 'PASS: the confined chat cannot touch the machine; the old options could'
                   : (confined ? 'FAIL: the confined chat made files' : 'INCONCLUSIVE: the control made no file'));
    process.exit(ok ? 0 : 1);
})();
