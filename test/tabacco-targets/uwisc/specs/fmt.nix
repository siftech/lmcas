# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  "binary" = "fmt";
  configs = [{
    "args" = [ "fmt" "-c" ];
    "env" = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    "cwd" = "/";
  }];
  "use_guiness" = true;
}
