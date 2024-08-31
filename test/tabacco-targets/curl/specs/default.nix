# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  curl = {
    "binary" = "curl";
    configs = [{
      "args" = [
        "curl"
        "-X"
        "POST"
        "-d"
        ''{"title": "Curl POST","body": "Curl POST example body","userId":1}''
        "http://127.0.0.1:12346/foo/bar"
      ];
      "env" = {
        "CWD" = "/";
        "CURL_HOME" = "/fake-curl-home";
        "PATH" = "";
      };
      "cwd" = "/";
      "syscall_mocks" = {
        "sys_open" = { "mapping" = { "/fake-curl-home/.curlrc" = ./curlrc; }; };
      };
    }];
    "use_guiness" = true;
  };
}
