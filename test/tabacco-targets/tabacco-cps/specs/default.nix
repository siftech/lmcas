# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  cp1 = import ./cp1.nix;
  cp2 = import ./cp2.nix;
  cp3 = import ./cp3.nix;
  cp4 = import ./cp4.nix;
  cp5 = import ./cp5.nix;
  cp6 = import ./cp6.nix;
  cp8 = import ./cp8;
  cp9 = import ./cp9.nix;
  cp_sighup_after_neck = import ./cp_sighup_after_neck.nix;
  cp_sighup_before_neck = import ./cp_sighup_before_neck.nix;
  cp_umask = import ./cp_umask.nix;
  cp_fcntl_getfd = import ./cp_fcntl_getfd.nix;
}
