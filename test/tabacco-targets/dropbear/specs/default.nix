# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  dropbear = {
    "binary" = "dropbear";
    configs = [{
      "args" = [ "dropbear" "-w" "-j" "-E" "-F" "-R" "-I" "60" "-p" "12348" ];
      "env" = {
        "CWD" = "/";
        "PATH" = "/bin";
      };
      "cwd" = "/";
    }];
    use_guiness = true;
  };
}
