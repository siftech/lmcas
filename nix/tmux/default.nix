# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

let
  flake = (import
    (let lock = builtins.fromJSON (builtins.readFile ../../flake.lock);
    in fetchTarball {
      url =
        "https://github.com/edolstra/flake-compat/archive/${lock.nodes.flake-compat.locked.rev}.tar.gz";
      sha256 = lock.nodes.flake-compat.locked.narHash;
    }) { src = ../..; }).defaultNix;
  pkgs = flake.inputs.nixpkgs.legacyPackages.x86_64-linux;
in pkgs.tmux
