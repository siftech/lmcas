# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  binary = "rm";
  configs = [{
    args = [ "rm" "-r" ];
    env = {
      CWD = "/";
      PATH = "/bin";
    };
    cwd = "/";
    syscall_mocks.sys_ioctl = {
      terminal_dimensions = {
        row = 25;
        col = 80;
      };
      unsafe_allow_tiocgwinsz = true;
    };
  }];
  "use_guiness" = true;
}
