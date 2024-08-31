# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ pkgs }:
(import ./Cargo.nix { inherit pkgs; }).rootCrate.build
