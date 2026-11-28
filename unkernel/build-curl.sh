#!/usr/bin/env bash
# Builds a static aarch64 curl for the image: build-curl.sh <out-dir> writes <out-dir>/curl.
# OpenSSL for TLS, zlib, the C library's resolver (/etc/resolv.conf; the image has no nsswitch.conf).
# Needs: gcc make wget xz-utils libssl-dev zlib1g-dev.
set -euo pipefail
out=$1
tarball=curl-8.23.0.tar.xz
sha256=8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5
[ -x "$out/curl" ] && exit 0
mkdir -p "$out" && cd "$out"
[ -f "$tarball" ] || wget -q "https://curl.se/download/$tarball"
echo "$sha256  $tarball" | sha256sum -c - >/dev/null
rm -rf src && mkdir src && tar -xJf "$tarball" -C src --strip-components=1
cd src
./configure --quiet --disable-shared --enable-static --with-openssl --with-zlib \
    --with-ca-bundle=/etc/ssl/certs/ca-certificates.crt \
    --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet --disable-tftp --disable-pop3 \
    --disable-imap --disable-smtp --disable-gopher --disable-mqtt --disable-manual --disable-docs \
    --disable-ntlm --without-libpsl --without-brotli --without-zstd --without-nghttp2 --without-libidn2 --without-libssh2 CFLAGS="-Os" LDFLAGS="-static" >/dev/null
make -s -j"$(nproc)" LDFLAGS="-static -all-static" >/dev/null
strip -o "$out/curl" src/curl
ls -l "$out/curl"
