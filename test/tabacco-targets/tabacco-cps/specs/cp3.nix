# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  "binary" = "cp3";
  configs = [{
    "args" = [ "cp3" "-v" ];
    "env" = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    "cwd" = "/";
    syscall_mocks.sys_getuid.uid = 0;
  }];
  use_neck_miner = false;
}
