# unkernel/

WibWob-DOS as an operating system: a Linux kernel whose whole user space is the desktop.

[unkernel](https://github.com/pithings/unkernel) is Linux compiled to WebAssembly; it runs unmodified
aarch64 Linux programs in Node or in a browser tab. The kernel is used as published (the `unkernel` npm
package). What is replaced is everything after it: the initramfs made here holds `/init` (`init.c`), a
static `wwdos`, its data directories, and a busybox shell for the desktop's terminal windows. The kernel
runs `/init`, which runs the desktop on the console. Alt-X powers the machine off.

This is separate from the browser build in `kernel/` (`kernel/build-web.sh`), which compiles the desktop
itself to wasm and has no processes, so no terminal windows. Here the desktop is the native Linux
program, with real ptys.

## Build and run

On Debian or Ubuntu (x86-64), with the arm64 architecture added to apt:

```sh
sudo apt-get install g++-aarch64-linux-gnu libncurses-dev:arm64 libssl-dev:arm64 libgpm-dev:arm64 \
    zlib1g-dev:arm64 \
    cmake ninja-build cpio make wget xz-utils
unkernel/build.sh                       # build-unkernel/wwdos.cpio.gz (about 12.5 MB)
cd unkernel && npm install              # the kernel and SDK, xterm.js; needs Node 24+
npm run boot                            # in this terminal; Ctrl-] quits
npm run serve                           # in Chrome 137+: http://localhost:8137
```

On macOS, with Docker running: `unkernel/build-docker.sh` runs the same build in an Ubuntu 24.04 container
(about 2 minutes), then `npm install` and `npm run serve` as above.

Page parameters: `?shell=1` boots to a shell on the console, `?mem=512` sets guest memory in MiB,
`?font=13` the font size.

## Files

| File | What |
| --- | --- |
| `init.c` | PID 1: mounts `/dev`, `/proc`, `/sys`, `/tmp`, `/dev/pts`; starts the network and the control bridge; runs `/wwdos/wwdos` on `/dev/hvc0`; powers off when it exits |
| `build.sh` | cross-builds `wwdos` (static, aarch64), stages the root, packs the initramfs |
| `build-docker.sh` | runs `build.sh` in an Ubuntu 24.04 container (macOS hosts) |
| `aarch64-linux.cmake` | the CMake toolchain file for that build |
| `web/` | the page: xterm.js as the screen, the SDK's `createKernel({ initrd })` |
| `build-curl.sh` | builds the static `curl` for the image |
| `udhcpc.script` | sets the address, route and DNS from the DHCP lease |
| `layouts/` | saved window layouts as command lines |
| `serve.mjs` | a static server for the page (sets the cross-origin isolation headers), the Wisp proxy, the `/ctl` relay |

## State (2026-11-28)

Measured in this repository's cloud container: the kernel reaches `/init` 0.18 s after it starts under
Node 24, and the desktop is on screen about 0.5 s after the page starts loading in headless Chromium 158
(files served from localhost). Checked there: the boot screen, menus by keyboard, a terminal window
running busybox commands, mouse reports reaching the desktop, Alt-X power-off.

Not done:

- Sound. The mixer's output is an AudioQueue (macOS); the image runs it with `WWDOS_SOUND_OUT=null`.
  unkernel offers an ALSA card played through Web Audio, so an ALSA output in `app/sound/stream.cpp`
  and `sound:` in `web/main.js` would connect it.
- Persistence. `/root/.wwdos` is in memory: every boot is a first waking. A data disk (`openDataDisk()`)
  mounted at `/root` would keep the self in the browser.
- Chat tool calls. With an API key the chat window answers, but a reply that calls a desktop tool stops at
  the printed `<tool_call>`.
- The API server and `open_terminal exec=` (its wrapper needs perl) are not in the image.
- Only this path was tried: Chromium, one CPU, kernel on the page's thread.

The kernel in the npm package is GPL-2.0; the SDK is MIT.

## Network

`serve.mjs` runs a Wisp proxy at `/wisp/` and the page passes it to the kernel as `net`. The proxy accepts
WebSocket upgrades only from this server's own origin. `/init` runs `udhcpc` on `eth0` (`udhcpc.script` sets the
address, route and `/etc/resolv.conf`); `/tmp/net.log` has the result. Page parameters: `?net=0` boots without a
network, `?wisp=wss://host/` names another Wisp server.

Use `curl` for HTTPS (static, OpenSSL, CA bundle at `/etc/ssl/certs/ca-certificates.crt`; `build-curl.sh`).
busybox `wget` works for HTTP only: its TLS fails in the guest with `bad MAC`.

## API key

The page asks once for an Anthropic API key, keeps it in the browser's local storage and hands it to `/init`
over vsock port 7138 at boot; `/init` sets `ANTHROPIC_API_KEY` for the desktop (status line `LLM KEY`).
`?key=1` asks again, `?key=0` boots without a key.

## Control from the host

`/init` listens on vsock port 7137 and forwards one command line per connection to `/tmp/wwdos.sock`.

- In the page: `await wwdos("cmd:get_state")`.
- From a shell, through the latest open page: `curl -s -X POST --data-binary 'cmd:get_state' http://127.0.0.1:8137/ctl`.
  JSON bodies: `{"keys":"\r"}` types on the console, `{"screen":true}` returns the terminal text.
- `layouts/NAME.ctl` holds one command per line; post each `cmd:` line to `/ctl` to rebuild a layout.

The server binds 127.0.0.1; set `HOST` to change it. The guest's files live in RAM: copy a saved workspace out
before reloading the page.
