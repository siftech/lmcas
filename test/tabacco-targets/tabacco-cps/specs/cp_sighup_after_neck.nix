# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  "binary" = "cp_sighup_after_neck";
  use_neck_miner = false;
  configs = [{
    "args" = [ "cp_sighup_after_neck" ];
    "env" = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    "cwd" = "/";
  }];
  "replace_sighup" = true;
  "use_guiness" = true;
}
