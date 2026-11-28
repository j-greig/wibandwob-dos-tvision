#!/usr/bin/env bash
# Builds the WibWob-DOS machine for unkernel (a Linux kernel compiled to WebAssembly):
#   build-unkernel/wwdos.cpio.gz   the whole user space: /init, the desktop, a busybox shell
# Boot it with: npx unkernel --initrd build-unkernel/wwdos.cpio.gz --no-net
#
# Needs (Debian/Ubuntu, with the arm64 architecture added to apt):
#   g++-aarch64-linux-gnu libncurses-dev:arm64 libssl-dev:arm64 libgpm-dev:arm64 cmake ninja-build cpio
# and a static arm64 busybox: BUSYBOX=<file>, else `apt-get download busybox-static:arm64`.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$here")
out=${WWDOS_UNKERNEL_OUT:-$repo/build-unkernel}
libs=/usr/lib/aarch64-linux-gnu
mkdir -p "$out"

# 1. The desktop, static, for aarch64 Linux.
cmake -G Ninja -B "$out/cmake" -S "$repo" \
    -DCMAKE_TOOLCHAIN_FILE="$here/aarch64-linux.cmake" -DCMAKE_BUILD_TYPE=Release \
    -DNCURSESW=$libs/libncursesw.a -DTINFOW=$libs/libtinfo.a -DGPM=$libs/libgpm.a \
    -DOPENSSL_CRYPTO_LIBRARY=$libs/libcrypto.a -DOPENSSL_INCLUDE_DIR=/usr/include >/dev/null
cmake --build "$out/cmake" --target wwdos

# 2. The root file system.
root=$out/root
rm -rf "$root"
mkdir -p "$root"/{bin,dev,proc,sys,tmp,run,root,wwdos/tools,wwdos/app/llm,usr/share/terminfo}

aarch64-linux-gnu-gcc -static -Os -Wall -o "$root/init" "$here/init.c"
aarch64-linux-gnu-strip "$root/init"
aarch64-linux-gnu-strip -o "$root/wwdos/wwdos" "$out/cmake/app/wwdos"

# What the desktop reads from its working directory.
for d in modules scores tracks casts paintings residents rooms workspaces; do
    [ -d "$repo/$d" ] && cp -a "$repo/$d" "$root/wwdos/$d"
done
cp "$repo/tools/term_exec.sh" "$root/wwdos/tools/"
cp -a "$repo/app/llm/config" "$root/wwdos/app/llm/config"

# The terminal descriptions ncurses looks up.
ti=$(dirname "$(dirname "$(find /usr/share/terminfo /lib/terminfo -name xterm-256color 2>/dev/null | head -1)")")
for t in x/xterm-256color x/xterm l/linux v/vt100; do
    mkdir -p "$root/usr/share/terminfo/${t%/*}"
    cp -L "$ti/$t" "$root/usr/share/terminfo/$t"
done

# The UTF-8 locale: without it the C library counts bytes, and wide glyphs (the kaomoji) tear.
mkdir -p "$root/usr/lib/locale"
cp -a /usr/lib/locale/C.utf8 "$root/usr/lib/locale/"

# A shell and the usual tools for the desktop's terminal windows.
busybox=${BUSYBOX:-}
if [ -z "$busybox" ]; then
    mkdir -p "$out/busybox"
    if [ -z "$(find "$out/busybox" -name busybox -type f)" ]; then
        (cd "$out/busybox" && apt-get download busybox-static:arm64 >/dev/null 2>&1 && dpkg-deb -x busybox-static_*.deb .)
    fi
    busybox=$(find "$out/busybox" -name busybox -type f | head -1)
fi
cp "$busybox" "$root/bin/busybox"
for a in sh ash ls cat cp mv rm mkdir rmdir ln echo printf ps top kill sleep date uname free df du \
         mount umount grep sed awk head tail wc sort uniq find vi less more clear reset stty tty env \
         id hostname dmesg poweroff reboot sync touch chmod tar gzip gunzip hexdump seq tr cut tee test true false; do
    ln -sf busybox "$root/bin/$a"
done

# 3. One archive: the initramfs the kernel unpacks and runs /init from.
(cd "$root" && find . | LC_ALL=C sort | cpio --quiet -o -H newc --owner=0:0 | gzip -9n) > "$out/wwdos.cpio.gz"
ls -l "$out/wwdos.cpio.gz"
