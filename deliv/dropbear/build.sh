#!/usr/bin/env bash
set -euo pipefail

if [[ -e /root/dropbear-2020.81 ]]; then
        echo "/root/dropbear-2020.81 already exists, refusing to write over it" >&2
        exit 1
fi

set -x

tar xf /usr/src/dropbear-2020.81.tar.lz -C /root
cd /root/dropbear-2020.81/
./configure --prefix=/root --disable-zlib  --enable-static
make -j$(nproc)
patch -p1 < /root/dropbear/neck.patch
make
cd /root/dropbear
tabacco -o debloated spec.json
tabacco -o nondebloated -N spec.json

mkdir -p /etc/dropbear
cp /root/.ssh/dropbear_ed25519_host_key /etc/dropbear/
