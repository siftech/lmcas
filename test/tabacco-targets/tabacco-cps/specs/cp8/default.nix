# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  "binary" = "cp8";
  configs = [{
    "args" = [ "cp8" ];
    "env" = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    "cwd" = "/";
    "syscall_mocks" = {
      sys_getuid.uid = 1000;
      "sys_open" = {
        "mapping" = {
          "/etc/passwd" = ./cp8.passwd;
          "/home/debloatuser/.cp8.conf" = ./cp8.conf;
        };
      };
    };
  }];
  "use_guiness" = true;
}
