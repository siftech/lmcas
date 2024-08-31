# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ bison, fetchFromGitHub, fetchgit, fetchurl, flex, m4, nix-gitignore
, musl-gclang-stdenv }:

musl-gclang-stdenv.mkDerivation rec {
  name = "uwisc";
  srcs = [
    (fetchurl {
      url = "https://ftp.gnu.org/gnu/coreutils/coreutils-8.32.tar.gz";
      sha256 = "sha256-1asHQ1p0BYq2miAH6Di+T2qQtWNdgSwuJmceOXL8obg=";
    })
    (fetchFromGitHub {
      name = "libpcap";
      owner = "the-tcpdump-group";
      repo = "libpcap";
      rev = "26ec10584082823b64c612ba3edff28b8023da38";
      sha256 = "sha256-NLIOkdNcQXls3xlc3N8E5X/PmRB9fkniWsRESiM/1Yw=";
    })
    (fetchFromGitHub {
      name = "tcpdump";
      owner = "the-tcpdump-group";
      repo = "tcpdump";
      # No revision was present in the original buildDataset.sh, this is a
      # guess based on what makes a small diff.
      rev = "21b12733b62cd4ec2cbe8dd96f13003e61a05944";
      sha256 = "sha256-8ruXFQ1utiZBGFimK9zgclOwEhMI8ArqqZfga1gOhJU=";
    })
    (fetchFromGitHub {
      name = "binutils";
      owner = "bminor";
      repo = "binutils-gdb";
      rev = "427234c78bddbea7c94fa1a35e74b7dfeabeeb43";
      sha256 = "sha256-CkL93lP/322GntTSYe7VGR3HxVJ8qxnsgSgE9hr1G3I=";
    })
  ];
  sourceRoot = ".";

  buildInputs = [ bison flex m4 ];

  postUnpack = ''
    sourceRoot="$(realpath "$sourceRoot")"
    echo "adjusted source root is $sourceRoot"
  '';

  patches = ./patches;
  patchPhase = ''
    patch $sourceRoot/binutils/binutils/objdump.c $patches/objdump.patch
    patch $sourceRoot/binutils/binutils/readelf.c $patches/readelf.patch
    patch $sourceRoot/tcpdump/tcpdump.c $patches/tcpdump.patch

    ln -s ../libpcap $sourceRoot/tcpdump/libpcap
    sed -i "s/HASHNAMESIZE 4096/HASHNAMESIZE 8/" $sourceRoot/tcpdump/addrtoname.c
    sed -i "s/HASHNAMESIZE 4096/HASHNAMESIZE 8/" $sourceRoot/tcpdump/print-atalk.c

    find $sourceRoot/binutils -name configure -exec sed -i "s/ -Werror//" '{}' \;
    find $sourceRoot/binutils -name "Makefile*" -exec sed -i '/^SUBDIRS/s/ doc po//' '{}' \;
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir $sourceRoot/prefix

    mkdir $sourceRoot/coreutils-8.32/build
    cd $sourceRoot/coreutils-8.32/build
    CFLAGS=-g ../configure --disable-nls --prefix=$sourceRoot/prefix

    cd $sourceRoot/libpcap
    ./configure --disable-largefile --disable-shared --without-gcc --without-libnl \
      --disable-dbus --without-dag --without-snf --prefix=$sourceRoot/prefix
    sed -i "s/-fpic//" Makefile

    # tcpdump can't be configured until libpcap is built...

    mkdir $sourceRoot/binutils/build
    cd $sourceRoot/binutils/build
    ../configure --disable-nls --disable-largefile --disable-gdb --disable-sim --disable-readline \
      --disable-libdecnumber --disable-libquadmath --disable-libstdcxx --disable-ld \
      --disable-gprof --disable-gas --disable-intl --disable-etc --prefix=$sourceRoot/prefix
    sed -i 's/ -static-libstdc++ -static-libgcc//' Makefile

    cd $sourceRoot
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild

    makeIn() {
      dir="$1"
      shift
      make -C $dir \
        -j''${NIX_BUILD_CORES} -l''${NIX_BUILD_CORES} \
        SHELL=$SHELL \
        $makeFlags ''${makeFlagsArray+"''${makeFlagsArray[@]}"} \
        $buildFlags ''${buildFlagsArray+"''${buildFlagsArray[@]}"} \
        $@
    }

    makeIn coreutils-8.32/build
    makeIn libpcap

    # tcpdump can't be configured until libpcap is built...
    cd tcpdump
    ./configure --without-sandbox-capsicum --without-crypto --without-cap-ng \
      --without-smi --prefix=$sourceRoot/prefix
    cd ..

    makeIn tcpdump
    makeIn binutils/build all-binutils

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    make -C coreutils-8.32/build install
    make -C libpcap install
    make -C tcpdump install
    make -C binutils/build install-binutils

    cd $sourceRoot/prefix/bin
    install -Dt $out/bin basename
    install -Dt $out/bin basenc
    install -Dt $out/bin chown
    install -Dt $out/bin comm
    install -Dt $out/bin date
    install -Dt $out/bin du
    install -Dt $out/bin echo
    install -Dt $out/bin fmt
    install -Dt $out/bin fold
    install -Dt $out/bin head
    install -Dt $out/bin id
    install -Dt $out/bin kill
    install -Dt $out/bin realpath
    install -Dt $out/bin rm
    install -Dt $out/bin sort
    install -Dt $out/bin uniq
    install -Dt $out/bin wc
    install -Dt $out/bin readelf tcpdump objdump

    runHook postInstall
  '';

  passthru.bin-paths = {
    basename = "bin/basename";
    basenc = "bin/basenc";
    chown = "bin/chown";
    comm = "bin/comm";
    date = "bin/date";
    du = "bin/du";
    echo = "bin/echo";
    fmt = "bin/fmt";
    fold = "bin/fold";
    head = "bin/head";
    id = "bin/id";
    kill = "bin/kill";
    realpath = "bin/realpath";
    rm = "bin/rm";
    sort = "bin/sort";
    uniq = "bin/uniq";
    wc = "bin/wc";
  };

  passthru.specs = import ../../test/tabacco-targets/uwisc/specs;
}
