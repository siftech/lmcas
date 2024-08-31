#! /bin/bash

LLVM_LD_CONF="/etc/ld.so.conf.d/llvm.conf"

# Set up the locations of where to find llvm libraries.
echo "Running ldconfig to locate LLVM libraries...."
if [ ! -f "$LLVM_LD_CONF" ]; then
    echo "/lmcas/local/lib" > "$LLVM_LD_CONF"
    ldconfig
fi

# Fixup stack size
ulimit -s 16777216
size=`ulimit -s`
if [ $size -lt 16777216 ]; then
	echo "Danger! Stack size isn't sufficient: $size"
fi

echo "Starting bash..."
cd /lmcas/neck
exec /bin/bash
