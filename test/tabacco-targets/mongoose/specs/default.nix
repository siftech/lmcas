# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  switched-server = {
    "binary" = "switched-server";
    configs = [{
      "args" = [ "switched-server" "--no-websocket" ];
      "env" = {
        "CWD" = "/";
        "PATH" = "/bin";
      };
      "cwd" = "/";
    }];
    "use_guiness" = true;
  };
}
