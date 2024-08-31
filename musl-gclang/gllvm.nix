# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ binutils-unwrapped, buildGoModule, fetchFromGitHub, file, llvmPkgs
, makeWrapper, runCommandNoCC, symlinkJoin

# TODO: This is glibc's getconf; we should almost certainly be instead using
# e.g. Alpine's getconf implementation, built against musl.
# https://git.alpinelinux.org/aports/tree/main/musl?h=master
, getconf }:

let
  # This is the copy of GLLVM with our patches, but without any wrappers.
  gllvm-unwrapped = buildGoModule {
    pname = "gllvm-unwrapped";
    version = "1.3.0";

    src = fetchFromGitHub {
      owner = "SRI-CSL";
      repo = "gllvm";
      rev = "938e8515ce5d09f7903e902bdb6f1c54fc0495c9";
      sha256 = "sha256-rxbZTL6iqOQzi1WE+UP0tDl7xEYvDL1lRnuyYi7VMZk=";
    };

    vendorSha256 = "sha256-n+UuXOybCdy/IWNoDuF7dPv/1mjmeFoje7qPXRnmPaM=";

    patches = [
      ./0001-add-linker-flag-embedding.patch
      ./0002-dont-include-object-files.patch
      ./0003-add-resource-dir-flag.patch
      ./0001-Disable-putting-the-optnone-attribute-on-functions-c.patch
    ];

    checkInputs = [ llvmPkgs.clangUseLLVM llvmPkgs.llvm ];
  };

  # Needed since gllvm assumes Clang and LLVM's files are in the same
  # prefix.
  llvm-clang = symlinkJoin {
    name = "llvm-clang";
    paths = [ llvmPkgs.clang-unwrapped llvmPkgs.llvm llvmPkgs.llvm.dev ];
  };

in runCommandNoCC "gllvm-${gllvm-unwrapped.version}" {
  pname = "gllvm";
  inherit (gllvm-unwrapped) version;

  nativeBuildInputs = [ makeWrapper ];
} ''
  mkdir -p $out/bin
  makeWrapper ${gllvm-unwrapped}/bin/gclang $out/bin/gclang \
    --set LLVM_COMPILER_PATH ${llvm-clang}/bin
  makeWrapper ${gllvm-unwrapped}/bin/get-bc $out/bin/get-bc \
    --prefix PATH : ${llvmPkgs.llvm}/bin \
    --prefix PATH : ${file}/bin \
    --prefix PATH : ${getconf}/bin
  makeWrapper ${gllvm-unwrapped}/bin/gsanity-check $out/bin/gsanity-check \
    --set LLVM_COMPILER_PATH ${llvm-clang}/bin \
    --set WLLVM_OUTPUT_LEVEL DEBUG
''
