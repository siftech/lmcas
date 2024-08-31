# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

let
  pkgs = import (builtins.fetchTarball {
    url =
      "https://github.com/nixos/nixpkgs/archive/52dc75a4fee3fdbcb792cb6fba009876b912bfe0.tar.gz";
    sha256 = "sha256:1mc7qncf38agvyd589akch0war71gx5xwfli9lh046xqsqsbhhl0";
  }) { system = "x86_64-linux"; };
in pkgs.callPackage ./package.nix { }
