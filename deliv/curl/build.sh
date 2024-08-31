#!/usr/bin/env bash
set -euo pipefail

if [[ -e /root/curl-7.83.1 ]]; then
	echo "/root/curl-7.83.1 already exists, refusing to write over it" >&2
	exit 1
fi

set -x

# 1. Build the program using our compiler wrapper. It's /bin/cc inside the
# container, so we don't need to pass it specially to configure or make.
tar xf /usr/src/curl-7.83.1.tar.bz2 -C /root
cd /root/curl-7.83.1
./configure --prefix=/root --without-ssl --disable-shared --enable-static
make -j$(nproc)

# 2. Patch the program to add the neck marker.
patch -p1 < /root/curl/neck.patch

# 3. Build the program with the neck marker added.
make

# 4. Write a debloating specification. (We already did this, it's
# /root/curl/spec.json).
cd /root/curl

# 5. Run the tabacco command-line tool, passing it the debloating
# specification. We build both a debloated one and a non-debloated one using
# the same compiler optimizations, for comparison.
tabacco -o debloated spec.json
tabacco -o nondebloated --no-debloat spec.json
