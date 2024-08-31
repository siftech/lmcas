# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ runCommandNoCC, writeShellScript, clang-tools, fd, nixfmt, python39Packages
, shellcheck, llvmPkgs }:

let
  clangTools' = clang-tools.override { llvmPackages = llvmPkgs; };
  black = python39Packages.black;
  nativeBuildInputs = [ clangTools' fd nixfmt black shellcheck ];

  blackFlags = ''
    --exclude="/(\.direnv|\.eggs|\.git|\.hg|\.mypy_cache|\.nox|\.tox|\.venv|venv|\.svn|_build|buck-out|build|dist|partial-interpretation|test\/results|nix\/store)/"'';
in {
  lint = runCommandNoCC "lint" { inherit nativeBuildInputs; } ''
    set -euo pipefail

    # We can't use find -exec because it doesn't return non-zero when
    # an exec'd command does.

    dir=${../.}

    # C/C++
    while IFS= read -r -d "" file; do
      clang-format --dry-run -Werror "$file"
    done < <(find $dir \( -name '*.c' -or -name '*.cpp' -or -name '*.h' \) -a ! -path './test/results/**' -a ! -path './result/**' -print0)
    echo clang-format passed

    # Nix
    while IFS= read -r -d "" file; do
      nixfmt -c "$file"
    done < <(find $dir -name '*.nix' -a ! -path './test/results/**' -a ! -path './result/**' -a ! -path '**/Cargo.nix' -print0 )
    echo nixfmt passed

    # Python
    black --check ${blackFlags} $dir
    echo black passed

    # Shellcheck
    shellcheck $dir/tools/host/lmcas-dev-shell
    echo shellcheck passed

    mkdir $out
  '';

  fix = writeShellScript "fix" ''
    set -euo pipefail
    dir=.

    # C/C++
    ${fd}/bin/fd -e c -e cpp -e h -x ${clangTools'}/bin/clang-format -i

    # Nix
    ${fd}/bin/fd -e nix -x ${nixfmt}/bin/nixfmt

    # Python
    ${black}/bin/black ${blackFlags} $dir
  '';
}
