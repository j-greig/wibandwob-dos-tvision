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
    cmake ninja-build cpio
unkernel/build.sh                       # build-unkernel/wwdos.cpio.gz (about 10 MB)
cd unkernel && npm install              # the kernel and SDK, xterm.js; needs Node 24+
npm run boot                            # in this terminal; Ctrl-] quits
npm run serve                           # in Chrome 137+: http://localhost:8137
```

Page parameters: `?shell=1` boots to a shell on the console, `?mem=512` sets guest memory in MiB,
`?font=13` the font size.

## Files

| File | What |
| --- | --- |
| `init.c` | PID 1: mounts `/dev`, `/proc`, `/sys`, `/tmp`, `/dev/pts`; runs `/wwdos/wwdos` on `/dev/hvc0`; powers off when it exits |
| `build.sh` | cross-builds `wwdos` (static, aarch64), stages the root, packs the initramfs |
| `aarch64-linux.cmake` | the CMake toolchain file for that build |
| `web/` | the page: xterm.js as the screen, the SDK's `createKernel({ initrd })` |
| `serve.mjs` | a static server for the page (sets the cross-origin isolation headers) |

## State (2026-11-28)

Measured in this repository's cloud container: the kernel reaches `/init` 0.18 s after it starts under
Node 24, and the desktop is on screen about 0.5 s after the page starts loading in headless Chromium 158
(files served from localhost). Checked there: the boot screen, menus by keyboard, a terminal window
running busybox commands, mouse reports reaching the desktop, Alt-X power-off.

Not done:

- Sound. The mixer's output is an AudioQueue (macOS); the image runs it with `WWDOS_SOUND_OUT=null`.
  unkernel offers an ALSA card played through Web Audio, so an ALSA output in `app/sound/stream.cpp`
  and `sound:` in `web/main.js` would connect it.
- Network. The page passes no `net`, and `/init` runs no DHCP client. Model calls from the guest need
  a Wisp proxy of your own (`wispBackend()`); the public one accepts only known origins.
- Persistence. `/root/.wwdos` is in memory: every boot is a first waking. A data disk (`openDataDisk()`)
  mounted at `/root` would keep the self in the browser.
- The API server, the chat bridge (Node) and `open_terminal exec=` (its wrapper needs perl) are not in
  the image.
- Only this path was tried: Chromium, one CPU, kernel on the page's thread.

The kernel in the npm package is GPL-2.0; the SDK is MIT.
