# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  "binary" = "head";
  configs = [{
    "args" = [ "head" "-n3" ];
    "env" = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    "cwd" = "/";
  }];
  "use_guiness" = true;
}