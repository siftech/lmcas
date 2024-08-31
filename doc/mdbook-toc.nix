# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ lib, fetchFromGitHub, rustPlatform, }:

rustPlatform.buildRustPackage rec {
  pname = "mdbook-toc";
  version = "0.11.0";

  src = fetchFromGitHub {
    owner = "badboy";
    repo = pname;
    rev = version;
    hash = "sha256-ORJV2+Uh8GwXU+EWUQ2ls+AcplYbpYhl6hvCuFdKpTk=";
  };

  cargoHash = "sha256-s+xlrHaynHTMmm7rfjYrWNlIJRHO0QTjMlcV+LjqHNs=";

  meta = {
    description =
      "A preprocessor for mdbook to add inline Table of Contents support.";
    homepage = "https://github.com/badboy/mdbook-toc";
    license = [ lib.licenses.mpl20 ];
  };
}
