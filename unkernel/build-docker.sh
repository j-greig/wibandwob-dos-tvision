#!/usr/bin/env bash
# Runs unkernel/build.sh in an Ubuntu 24.04 container, for hosts without the Debian toolchain (macOS).
# On an arm64 host the container is arm64, so the "cross" compiler names are the native ones.
# Ubuntu 24.04, not Debian trixie: trixie's static libcrypto needs zstd and the link fails.
# Output: build-unkernel/wwdos.cpio.gz in this checkout (or WWDOS_UNKERNEL_OUT).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$here")
out=${WWDOS_UNKERNEL_OUT:-$repo/build-unkernel}
mkdir -p "$out"
docker run --rm --platform linux/arm64 -v "$repo":/src -v "$out":/out -e WWDOS_UNKERNEL_OUT=/out ubuntu:24.04 bash -c '
    set -e
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends g++ gcc libc6-dev libncurses-dev libssl-dev libgpm-dev \
        cmake ninja-build cpio ca-certificates make wget xz-utils zlib1g-dev >/dev/null
    /src/unkernel/build.sh'
