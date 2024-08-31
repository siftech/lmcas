# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ buildEnv, dockerTools, lmcasPkgs, runCommandNoCC, self, writeTextDir
, writeScriptBin

# Basic system packages
, autoconf, automake, bashInteractive, binutils, bison, bzip2, cacert, cmake
, coreutils, diffutils, file, findutils, flex, gawk, gdb, git, gnugrep, gnulib
, gnumake, gnused, gnutar, gzip, htop, iproute2, jq, less, lzip, m4, man-db
, man-pages, meson, nano, ncurses, nmap, ninja, openssh, patch, perl, pkg-config
, procps, python39, runtimeShell, shadow, strace, texinfo, time, tmux
, util-linux, which, xz,

# Sources we want to bundle
targets, zlib }:

let
  system = buildEnv {
    name = "system";
    paths = [
      # Basic system utilities
      autoconf
      automake
      bashInteractive
      binutils
      bison
      bzip2
      cmake
      coreutils
      diffutils
      file
      findutils
      flex
      gawk
      gdb
      git
      gnugrep
      gnulib
      gnumake
      gnused
      gnutar
      gzip
      htop
      iproute2
      jq
      less
      lzip
      m4
      man-db
      man-pages
      meson
      ncurses
      nmap
      ninja
      openssh
      patch
      perl
      pkg-config
      procps
      python39
      shadow
      strace
      texinfo
      time
      tmux
      which
      util-linux
      xz

      # Editors
      nano
      (import ../nix/vim)

      # Config files
      (writeTextDir "etc/pam.d/system-auth" ''
        # Account management.
        account required pam_unix.so

        # Authentication management.
        auth sufficient pam_unix.so   likeauth try_first_pass
        auth required pam_deny.so

        # Password management.
        password sufficient pam_unix.so nullok sha512

        # Session management.
        session required pam_env.so conffile=/etc/pam/environment readenv=0
        session required pam_unix.so

      '')
      (writeTextDir "etc/passwd" ''
        root:x:0:0::/root:/bin/bash
      '')
      (writeTextDir "etc/group" ''
        root:x:0:
        nogroup:x:65534:
      '')
      (writeTextDir "etc/shells" "/bin/bash")
      (writeTextDir "etc/shadow" ''
        root:!x:::::::
      '')
      (writeTextDir "etc/gshadow" ''
        root:x::
      '')
      (runCommandNoCC "misc-system-stuff" { } ''
        mkdir -p $out/bin $out/usr/bin $out/var/cache
        ln -s /bin/env $out/usr/bin/env
        ln -s /bin/musl-gclang $out/bin/cc
        ln -s /bin/nvim $out/bin/vim
      '')

      # Our software
      lmcasPkgs.musl-gclang
      lmcasPkgs.tabacco-driver
      (runCommandNoCC "doc" { } ''
        mkdir -p $out/usr/share/doc
        ln -s ${lmcasPkgs.doc} $out/usr/share/doc/tabacco
        cp -r ${lmcasPkgs.doc} tabacco-doc
        find tabacco-doc -type d -exec chmod 755 {} \;
        find tabacco-doc -type f -exec chmod 644 {} \;
        tar czf $out/usr/share/doc/tabacco.tgz tabacco-doc
      '')
      (writeScriptBin "serve-docs" ''
        #!${runtimeShell}
        cd /usr/share/doc/tabacco
        python -m http.server 80
      '')
      (writeTextDir "etc/lmcas-release" ''
        LMCAS_REV=${
          self.rev or (builtins.abort
            "Must build delivery releases with a clean tree")
        }
      '')

      # Targets to ship
      (runCommandNoCC "target-sources" { } ''
        mkdir -p $out/usr/src
        cp -L ${targets.wget.src} $out/usr/src/${targets.wget.name}.tar.lz
        cp -L ${targets.dropbear.src} $out/usr/src/${targets.dropbear.name}.tar.bz2
        cp -L ${zlib.src} $out/usr/src/${zlib.name}.tar.gz
      '')
    ];
  };

in dockerTools.buildLayeredImage {
  name = "lmcas-deliv";
  tag = "nightly";
  created = "now";

  config = {
    Cmd = [ "/bin/bash" ];
    Volumes."/tmp" = { };
    WorkingDir = "/root";
    Env = [ "CFLAGS=-I/usr/local/include" "LDFLAGS=-L/usr/local/lib" ];
  };
  maxLayers = 2;

  contents = system;
  fakeRootCommands = ''
    install -m 0644 -o 0 -g 0 -DT ${./bashrc}       ./root/.bashrc
    install -m 0755 -o 0 -g 0 -DT ${./test_http.py} ./root/test_http.py

    install -m 0644 -o 0 -g 0 -DT ${./cp1/main.c}    ./root/example/main.c
    install -m 0644 -o 0 -g 0 -DT ${./cp1/spec.json} ./root/example/spec.json

    install -m 0644 -o 0 -g 0 -DT ${./wget/passwd}    ./root/wget/passwd
    install -m 0644 -o 0 -g 0 -DT ${./wget/spec.json} ./root/wget/spec.json
    install -m 0644 -o 0 -g 0 -DT ${./wget/wgetrc}    ./root/wget/wgetrc

    install -m 0644 -o 0 -g 0 -DT ${./dropbear/spec.json} \
      ./root/dropbear/spec.json
    install -m 0600 -o 0 -g 0 -DT ${./dropbear/ssh/authorized_keys} \
      ./root/.ssh/authorized_keys
    install -m 0600 -o 0 -g 0 -DT ${./dropbear/ssh/dropbear_ed25519_host_key} \
      ./root/.ssh/dropbear_ed25519_host_key
    install -m 0600 -o 0 -g 0 -DT ${./dropbear/ssh/id_ed25519} \
      ./root/.ssh/id_ed25519
    install -m 0600 -o 0 -g 0 -DT ${./dropbear/ssh/id_ed25519.pub} \
      ./root/.ssh/id_ed25519.pub
    install -m 0600 -o 0 -g 0 -DT ${./dropbear/ssh/known_hosts} \
      ./root/.ssh/known_hosts

    echo 'source "$HOME/.bashrc"' > ./root/.bash_profile
  '';
}
