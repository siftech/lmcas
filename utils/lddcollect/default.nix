# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ buildPythonPackage, click, fetchPypi, pyelftools, }:

buildPythonPackage rec {
  pname = "lddcollect";
  version = "0.2.0";

  src = fetchPypi {
    inherit pname version;
    sha256 = "sha256-RA5vPYUMLWAb+DF084C8QhvYD9vQC9V7rlObuq9TL4w=";
  };
  patches = [ ./0001-Ship-a-py.typed-file.patch ];

  propagatedBuildInputs = [ click pyelftools ];

  meta = {
    homepage = "https://github.com/Kirill888/lddcollect";
    description =
      "List all shared library files needed to run ELF executable or load elf library";
  };
}
