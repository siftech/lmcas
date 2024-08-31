# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ binutils, fix-bitcode-paths, musl-gclang, overrideCC, stdenv
, writeShellScriptBin, }:

let stdenvWithMuslGClang = overrideCC stdenv musl-gclang;

in stdenvWithMuslGClang // {
  # We also want to customize some phases, so we patch mkDerivation.
  mkDerivation = attrs:
    stdenvWithMuslGClang.mkDerivation (attrs // {
      nativeBuildInputs = (attrs.nativeBuildInputs or [ ]) ++ [
        # TODO: Why aren't the original binutils getting picked up? Is this overrideCC's doing?
        binutils

        (writeShellScriptBin "clang" ''
          printf >&2 "\x1b[1;31mclang was invoked; use musl-gclang instead!\x1b[0m\n"
          exit 127
        '')
        (writeShellScriptBin "gcc" ''
          printf >&2 "\x1b[1;31mgcc was invoked; use musl-gclang instead!\x1b[0m\n"
          exit 127
        '')
      ];

      preConfigurePhases = (attrs.preConfigurePhases or [ ])
        ++ [ "exportMuslGClangVarsPhase" ];
      preBuildPhases = (attrs.preBuildPhases or [ ]) ++ [ "gsanityCheckPhase" ];
      preFixupPhases = (attrs.preFixupPhases or [ ])
        ++ [ "fixBitcodePathsPhase" ];

      exportMuslGClangVarsPhase = ''
        export CC=${musl-gclang}/bin/musl-gclang
        export HOSTCC=${musl-gclang}/bin/musl-gclang
      '';
      gsanityCheckPhase = ''
        ${musl-gclang}/bin/gsanity-check
      '';
      fixBitcodePathsPhase = ''
        for path in $outputs; do
          echo "Fixing GLLVM bitcode paths in ''${!path}..."
          ${fix-bitcode-paths}/bin/fix-bitcode-paths ''${!path}
        done
      '';
    });
}
