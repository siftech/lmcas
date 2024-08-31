# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ makeBinaryWrapper, python39, stdenvNoCC }:

stdenvNoCC.mkDerivation {
  name = "compare_neck_locations";
  nativeBuildInputs = [ makeBinaryWrapper python39 ];

  dontUnpack = true;
  installPhase = ''
    cp ${./compare_neck_locations.py} compare_neck_locations
    patchShebangs compare_neck_locations
    install -Dm755 -t $out/bin compare_neck_locations

    wrapProgram $out/bin/compare_neck_locations \
      --set RUBRICS_PATH "${./neck-location-rubrics}"
  '';
}
