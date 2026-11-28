// The WibWob-DOS machine in a browser tab: a Linux kernel (unkernel.wasm) whose whole
// user space is wwdos.cpio.gz, shown on an xterm.js terminal. No server takes part
// after the files are loaded.
import { checkSupport, createKernel, wispBackend } from "/unkernel/web.mjs";

const note = document.getElementById("note");
const params = new URLSearchParams(location.search);

const term = new Terminal({
  fontFamily: '"DejaVu Sans Mono", Menlo, Consolas, "Liberation Mono", monospace',
  fontSize: Number(params.get("font") || 15),
  cursorBlink: false,
  scrollback: 0,
  allowProposedApi: true,
  macOptionIsMeta: true,
  theme: { background: "#000000" },
});
const fit = new FitAddon.FitAddon();
term.loadAddon(fit);
term.open(document.getElementById("screen"));
fit.fit();

function askKey() {
  const ask = params.get("key");
  if (ask === "0") return null;
  const stored = localStorage.getItem("wwdos.anthropicKey");
  if (ask !== "1" && (stored || localStorage.getItem("wwdos.noKey"))) return stored;
  return new Promise((done) => {
    note.textContent = "WIBWOB-DOS\n\nAnthropic API key for Wib&Wob chat and Scramble (optional).\nIt stays in this browser and goes to the machine in this tab.\n";
    const input = Object.assign(document.createElement("input"), { type: "password", size: 44, placeholder: "sk-ant-..." });
    const start = Object.assign(document.createElement("button"), { textContent: "Start" });
    const without = Object.assign(document.createElement("button"), { textContent: "Start without a key" });
    const finish = (key) => {
      if (key) { localStorage.setItem("wwdos.anthropicKey", key); localStorage.removeItem("wwdos.noKey"); }
      else { localStorage.removeItem("wwdos.anthropicKey"); localStorage.setItem("wwdos.noKey", "1"); }
      note.textContent = "WIBWOB-DOS";
      note.style.pointerEvents = "";
      done(key || null);
    };
    start.onclick = () => finish(input.value.trim());
    without.onclick = () => finish("");
    input.onkeydown = (event) => { if (event.key === "Enter") finish(input.value.trim()); };
    note.style.pointerEvents = "auto";
    note.append(input, document.createElement("br"), start, " ", without);
    input.focus();
  });
}

async function boot() {
  const support = await checkSupport();
  if (!support.ok) {
    note.textContent = "This browser cannot run the kernel:\n" + support.problems.join("\n");
    return;
  }
  const t0 = performance.now();
  const response = await fetch("/wwdos.cpio.gz");
  if (!response.ok) throw new Error("no initramfs: run unkernel/build.sh");
  const initrd = new Uint8Array(await response.arrayBuffer());

  // The network: a Wisp proxy carries the guest's TCP. serve.mjs runs one at /wisp/ (the default on
  // localhost); ?wisp=wss://host/ names another, ?net=0 boots without a network.
  const local = ["localhost", "127.0.0.1"].includes(location.hostname);
  const wispUrl = params.get("net") === "0" ? null : params.get("wisp") || (local ? `ws://${location.host}/wisp/` : null);

  // The API key for the desktop's model calls: asked for once, kept in this browser's storage, and handed to
  // /init as ANTHROPIC_API_KEY at boot. ?key=1 asks again, ?key=0 boots without one.
  const apiKey = await askKey();

  const vm = await createKernel({
    initrd,
    vsock: true, // the control bridge in /init (window.wwdos)
    net: wispUrl ? wispBackend(wispUrl) : undefined,
    // wwdos.shell=1 (?shell=1) boots to a shell on the console instead of the desktop.
    cmdline: "console=hvc0 rdinit=/init quiet loglevel=1" + (params.get("shell") === "1" ? " wwdos.shell=1" : "") +
      (apiKey ? " wwdos.env=1" : ""),
    memoryMiB: Number(params.get("mem") || 256),
  });
  if (apiKey) {
    // /init listens on vsock port 7138 a moment after the kernel starts.
    for (let tries = 0; tries < 100; ++tries) {
      try {
        const conn = await vm.vsock.connect(7138);
        await conn.writable.getWriter().write("ANTHROPIC_API_KEY=" + apiKey + "\n.\n");
        for await (const chunk of conn.readable) void chunk; // /init closes when it has read the lines
        conn.close();
        break;
      } catch {
        await new Promise((done) => setTimeout(done, 50));
      }
    }
  }
  window.vm = vm;
  window.term = term;
  vm.resize(term.cols, term.rows);

  let first = true;
  vm.onOutput((bytes) => {
    if (first) {
      first = false;
      note.hidden = true;
      window.bootMs = Math.round(performance.now() - t0);
      console.log(`WibWob-DOS on screen after ${window.bootMs} ms`);
    }
    term.write(bytes);
  });
  term.onData((data) => vm.write(data));
  term.onBinary((data) => vm.write(Uint8Array.from(data, (c) => c.charCodeAt(0))));
  term.onResize(({ cols, rows }) => vm.resize(cols, rows));
  addEventListener("resize", () => fit.fit());
  term.focus();

  // window.wwdos("cmd:open_primer path=being-jelly.txt x=4 y=2 w=40 h=20"): one command line to the
  // desktop's command socket, through the bridge in /init (vsock port 7137); resolves to the answer.
  window.wwdos = async (line) => {
    const conn = await vm.vsock.connect(7137);
    const writer = conn.writable.getWriter();
    await writer.write(line.trim() + "\n");
    let text = "";
    const decoder = new TextDecoder();
    for await (const chunk of conn.readable) text += decoder.decode(chunk, { stream: true });
    conn.close();
    return text.trim();
  };
  const screenText = () => Array.from({ length: term.rows }, (_, i) =>
    term.buffer.active.getLine(i)?.translateToString(true) ?? "").join("\n");
  // On localhost, serve.mjs relays commands to this page: POST /ctl there, answered here.
  const relayOpen = () => {
    const relay = new EventSource("/ctl/events");
    relay.onmessage = async (event) => {
      const { id, cmd, keys, screen } = JSON.parse(event.data);
      let body;
      try {
        if (keys) { vm.write(keys); body = "ok"; }
        else if (screen) body = screenText();
        else body = await window.wwdos(cmd);
      } catch (err) {
        body = "err " + (err && err.message ? err.message : err);
      }
      fetch("/ctl/reply?id=" + id, { method: "POST", body });
    };
    relay.onerror = () => { if (relay.readyState === EventSource.CLOSED) setTimeout(relayOpen, 3000); };
  };
  if (local) relayOpen();

  const { kind, reason } = await vm.exited;
  term.reset();
  note.hidden = false;
  note.textContent = (kind === "halt" ? "It is now safe to close this tab." : `The machine stopped: ${reason || kind}`) + "\n";
  const again = document.createElement("button");
  again.textContent = "Power on";
  again.onclick = () => location.reload();
  note.append(again);
}

boot().catch((err) => {
  note.hidden = false;
  note.textContent = "Boot failed: " + (err && err.message ? err.message : err);
  console.error(err);
});
