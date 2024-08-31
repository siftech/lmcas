# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

let
  mkConfig = args: {
    inherit args;
    env = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    cwd = "/";
    syscall_mocks = {
      sys_getuid.uid = 0;
      sys_getgid.gid = 0;
      sys_open.mapping = {
        "/etc/passwd" = ./passwd;
        "/root/.wgetrc" = ./wgetrc;
      };
      sys_stat.mapping = {
        "/etc/passwd" = ./passwd;
        "/root/.wgetrc" = ./wgetrc;
      };
    };
  };

in {
  wget = {
    "binary" = "wget";
    configs = [
      (mkConfig [ "wget" "--method=get" "-O-" ])
      (mkConfig [
        "wget"
        "--method=post"
        "-O-"
        ''
          --body-data={"title": "Wget POST","body": "Wget POST example body","userId":1}''
      ])
    ];
    "use_guiness" = true;
  };
}
