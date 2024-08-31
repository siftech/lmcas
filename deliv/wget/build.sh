#!/usr/bin/env bash
set -euo pipefail

if [[ -e /root/wget-1.21.3 ]]; then
	echo "/root/wget-1.21.3 already exists, refusing to write over it" >&2
	exit 1
fi

set -x
tar xf /usr/src/wget-1.21.3.tar.lz -C /root
cd /root/wget-1.21.3
./configure --prefix=/root --without-ssl --without-zlib
patch -p1 < /root/wget/neck.patch
make -j$(nproc)
cd /root/wget
tabacco -o debloated spec.json
tabacco -o nondebloated --no-debloat spec.json
