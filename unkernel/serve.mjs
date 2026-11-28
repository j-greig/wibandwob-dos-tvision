// Serves the WibWob-DOS machine page: http://localhost:8137
// The page, the unkernel SDK and kernel (from node_modules), xterm.js, and the initramfs
// that unkernel/build.sh made. Any static host works the same way.
import { createServer } from "node:http";
import { createReadStream, statSync } from "node:fs";
import { dirname, extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { server as wisp, logging } from "@mercuryworkshop/wisp-js/server";

const here = dirname(fileURLToPath(import.meta.url));
const port = Number(process.env.PORT || 8137);
const roots = [
  ["/unkernel/", join(here, "node_modules/unkernel/dist/")],
  ["/xterm/", join(here, "node_modules/@xterm/xterm/")],
  ["/xterm-fit/", join(here, "node_modules/@xterm/addon-fit/")],
  ["/wwdos.cpio.gz", process.env.WWDOS_INITRD || join(here, "../build-unkernel/wwdos.cpio.gz")],
  ["/", join(here, "web/")],
];
const types = {
  ".html": "text/html; charset=utf-8", ".js": "text/javascript", ".mjs": "text/javascript",
  ".css": "text/css", ".wasm": "application/wasm", ".gz": "application/octet-stream",
};

// The control relay (this server only): POST /ctl sends its body to the page that is open, which
// runs it on the desktop and posts the answer back. The body is one desktop command line
// ("cmd:open_primer path=being-jelly.txt x=4 y=2 w=40 h=20"), or JSON: {"keys":"\r"} types on the console,
// {"screen":true} answers with the terminal's text.
const pages = [];         // the event streams of the open pages; commands go to the latest
const waiting = new Map(); // command id -> the POST /ctl response that waits for its answer
let nextId = 1;
const readBody = (req) => new Promise((done) => {
  let body = "";
  req.on("data", (chunk) => (body += chunk)).on("end", () => done(body));
});
async function control(req, res, url) {
  if (url.pathname === "/ctl/events") {
    res.writeHead(200, { "Content-Type": "text/event-stream", "Cache-Control": "no-cache" });
    res.write(":\n\n");
    pages.push(res);
    req.on("close", () => pages.splice(pages.indexOf(res), 1));
  } else if (url.pathname === "/ctl/reply" && req.method === "POST") {
    const answer = waiting.get(url.searchParams.get("id"));
    waiting.delete(url.searchParams.get("id"));
    const body = await readBody(req);
    if (answer) answer.writeHead(200, { "Content-Type": "text/plain; charset=utf-8" }).end(body + "\n");
    res.writeHead(204).end();
  } else if (url.pathname === "/ctl" && req.method === "POST") {
    const body = (await readBody(req)).trim();
    const page = pages.at(-1);
    if (!page) return res.writeHead(503).end("no page open\n");
    const id = String(nextId++);
    waiting.set(id, res);
    const message = body.startsWith("{") ? { ...JSON.parse(body), id } : { id, cmd: body };
    page.write("data: " + JSON.stringify(message) + "\n\n");
    setTimeout(() => { if (waiting.delete(id)) res.writeHead(504).end("the page did not answer\n"); }, 70000);
  } else {
    res.writeHead(404).end("not found");
  }
}

const server = createServer((req, res) => {
  if (req.url.startsWith("/ctl")) return void control(req, res, new URL(req.url, "http://x"));
  let path = decodeURIComponent(new URL(req.url, "http://x").pathname);
  if (path === "/") path = "/index.html";
  const [prefix, dir] = roots.find(([p]) => path.startsWith(p));
  const file = prefix.endsWith("/") ? join(dir, normalize(path.slice(prefix.length))) : dir;
  let size;
  try {
    if (prefix.endsWith("/") && !file.startsWith(dir)) throw new Error("outside");
    const st = statSync(file);
    if (!st.isFile()) throw new Error("not a file");
    size = st.size;
  } catch {
    res.writeHead(404).end("not found");
    return;
  }
  res.writeHead(200, {
    "Content-Type": types[extname(file)] || "application/octet-stream",
    "Content-Length": size,
    "Cache-Control": "no-cache",
    // Cross-origin isolation: lets the kernel run in a worker with shared memory.
    "Cross-Origin-Opener-Policy": "same-origin",
    "Cross-Origin-Embedder-Policy": "require-corp",
  });
  createReadStream(file).pipe(res);
});

// The guest's network: a Wisp proxy at /wisp/ that opens the TCP connections the guest asks for.
// Only this server's own page may use it; any other origin would get a proxy into this machine's network.
logging.set_level(logging.WARN);
server.on("upgrade", (req, socket, head) => {
  const origin = req.headers.origin || "";
  const own = [`http://localhost:${port}`, `http://127.0.0.1:${port}`].includes(origin);
  if (!req.url.startsWith("/wisp/") || !own) return socket.destroy();
  wisp.routeRequest(req, socket, head);
});

server.listen(port, process.env.HOST || "127.0.0.1", () => console.log(`WibWob-DOS machine: http://localhost:${port}`));
