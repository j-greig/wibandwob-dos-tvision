/**
 * The desktop's own socket (WWDOS_SOCKET, set by the app when it starts the bridge), with no HTTP
 * server in between. One command line out, one reply back, as tools/api_server and the tests do.
 * No dependencies, so it runs before npm install (tests/resident/test_resident_world.py uses it).
 */
const net = require('net');

function socketPath() {
    if (process.env.WWDOS_SOCKET) return process.env.WWDOS_SOCKET;
    const inst = process.env.WIBWOB_INSTANCE;
    return inst ? `/tmp/wibwob_${inst}.sock` : '/tmp/wwdos.sock';
}

function encode(v) {
    return encodeURIComponent(String(v)).replace(/[!'()*]/g, c => '%' + c.charCodeAt(0).toString(16).toUpperCase());
}

// Send "cmd:<name> k=v ..." and resolve with the reply text.
function send(name, args = {}, timeoutMs = 10000) {
    const line = ['cmd:' + name, ...Object.entries(args).map(([k, v]) => `${k}=${encode(v)}`)].join(' ') + '\n';
    return new Promise((resolve, reject) => {
        const s = net.createConnection(socketPath());
        let out = '';
        const t = setTimeout(() => { s.destroy(); reject(new Error('the desktop did not answer')); }, timeoutMs);
        s.on('connect', () => s.write(line));
        s.on('data', d => { out += d.toString('utf8'); });
        s.on('end', () => { clearTimeout(t); resolve(out.trim()); });
        s.on('error', e => { clearTimeout(t); reject(e); });
    });
}

// A resident's act: the command runs as actor (wib or wob), source resident, so it is logged in
// that name and the actor's hand is seen moving before it lands.
function act(actor, name, args = {}) {
    return send(name, { ...args, actor, source: 'resident' });
}

module.exports = { send, act, socketPath };

if (require.main === module) {
    // node app_socket.js <actor> <command> [k=v ...]
    const [actor, name, ...kv] = process.argv.slice(2);
    const args = Object.fromEntries(kv.map(p => [p.slice(0, p.indexOf('=')), p.slice(p.indexOf('=') + 1)]));
    act(actor, name, args).then(r => { console.log(r); }, e => { console.error(String(e)); process.exit(4); });
}
