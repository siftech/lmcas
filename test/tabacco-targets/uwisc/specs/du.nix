# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  "binary" = "du";
  configs = [{
    "args" = [ "du" "-bsh" "." ];
    "env" = {
      "CWD" = "/";
      "PATH" = "/bin";
    };
    "cwd" = "/";
  }];
  "use_guiness" = true;
}
