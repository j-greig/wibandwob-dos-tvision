// The WibWob-DOS machine in a browser tab: a Linux kernel (unkernel.wasm) whose whole
// user space is wwdos.cpio.gz, shown on an xterm.js terminal. No server takes part
// after the files are loaded.
import { checkSupport, createKernel } from "/unkernel/web.mjs";

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

  const vm = await createKernel({
    initrd,
    // wwdos.shell=1 (?shell=1) boots to a shell on the console instead of the desktop.
    cmdline: "console=hvc0 rdinit=/init quiet loglevel=1" + (params.get("shell") === "1" ? " wwdos.shell=1" : ""),
    memoryMiB: Number(params.get("mem") || 256),
  });
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
