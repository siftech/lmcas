# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  "binary" = "cp_fcntl_getfd";
  configs = [{
    "args" = [ "cp_fcntl_getfd" "-o" ];
    "env" = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    "cwd" = "/";
    "syscall_mocks" = { sys_open.mapping = { "file1" = ./file1; }; };
  }];
  "use_guiness" = true;
}
