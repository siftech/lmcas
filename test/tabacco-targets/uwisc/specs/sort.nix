# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  "binary" = "sort";
  configs = [{
    "args" = [ "sort" "-u" ];
    "env" = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    "cwd" = "/";
    "syscall_mocks" = { "sys_rt_sigaction" = { "replace_sighup" = false; }; };
  }];
  "use_guiness" = true;
}
