# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ callPackage, mdbook, mdbook-linkcheck, mdbook-mermaid, python3, stdenvNoCC }:

let mdbook-toc = callPackage ./mdbook-toc.nix { };

in stdenvNoCC.mkDerivation {
  name = "doc";
  nativeBuildInputs =
    [ mdbook mdbook-linkcheck mdbook-mermaid mdbook-toc python3 ];
  src = ./.;
  buildPhase = "mdbook build";
  installPhase = "mv book/html $out";
}
