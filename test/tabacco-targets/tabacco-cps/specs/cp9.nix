# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  "binary" = "cp9";
  configs = [{
    "args" = [ "cp9" "example.txt" ];
    "env" = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    "cwd" = "/";
    "syscall_mocks" = {
      "sys_open" = {
        "mapping" = { "example.txt" = ../../misc_text_files/dracula-para.txt; };
      };
    };
  }];
  "use_neck_miner" = false;
}
