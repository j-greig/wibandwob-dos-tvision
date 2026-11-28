// Serves the WibWob-DOS machine page: http://localhost:8137
// The page, the unkernel SDK and kernel (from node_modules), xterm.js, and the initramfs
// that unkernel/build.sh made. Any static host works the same way.
import { createServer } from "node:http";
import { createReadStream, statSync } from "node:fs";
import { dirname, extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";

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

createServer((req, res) => {
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
}).listen(port, () => console.log(`WibWob-DOS machine: http://localhost:${port}`));
