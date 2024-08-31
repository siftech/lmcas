# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{
  inputs.flake-compat = {
    url = "github:edolstra/flake-compat";
    flake = false;
  };
  outputs = { self, flake-compat, flake-utils, nixpkgs, }:
    flake-utils.lib.eachSystem [ "x86_64-linux" ] (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        llvmPkgs = pkgs.llvmPackages_12;

        lmcasPkgs = rec {
          # TaBaCCo driver
          tabacco-driver = pkgs.python39Packages.callPackage ./tabacco-driver {
            inherit annotation-pass gllvm instrumentation-pass
              instrumentation-libc instrumentation-parent lddcollect libsegfault
              llvmPkgs lmcas-dejson lmcas-sh make-external-list neck-miner
              sighup-pass specialization-pass tabacco-helpers;
          };

          # Documentation
          doc = pkgs.callPackage ./doc { };
          doc-tarball = pkgs.runCommandNoCC "lmcas-doc-tarball" { } ''
            cp --dereference --recursive ${doc} lmcas-doc
            tar czf $out lmcas-doc
          '';

          # TaBaCCo stdenv
          gllvm =
            pkgs.callPackage ./musl-gclang/gllvm.nix { inherit llvmPkgs; };
          musl-gclang =
            pkgs.callPackage ./musl-gclang { inherit gllvm llvmPkgs; };
          musl-gclang-stdenv = pkgs.callPackage ./musl-gclang/stdenv.nix {
            inherit fix-bitcode-paths musl-gclang;
          };

          # TaBaCCo Components
          annotation-pass =
            pkgs.callPackage ./annotation-pass { inherit llvmPkgs; };
          sighup-pass = pkgs.callPackage ./sighup-pass { inherit llvmPkgs; };
          instrumentation-libc = pkgs.callPackage ./instrumentation-libc {
            inherit annotation-pass instrumentation-pass instrumentation-runtime
              llvmPkgs musl-gclang musl-gclang-stdenv;
          };
          instrumentation-parent =
            pkgs.callPackage ./instrumentation-parent { };
          instrumentation-pass =
            pkgs.callPackage ./instrumentation-pass { inherit llvmPkgs; };
          instrumentation-runtime =
            pkgs.callPackage ./instrumentation-runtime { inherit llvmPkgs; };
          make-external-list =
            pkgs.python310Packages.callPackage ./make-external-list { };
          neck-miner =
            pkgs.callPackage ./neck-miner { inherit llvmPkgs phasar; };
          phasar =
            pkgs.callPackage ./neck-miner/phasar.nix { inherit llvmPkgs; };
          specialization-pass =
            pkgs.callPackage ./specialization-pass { inherit llvmPkgs; };
          tabacco-helpers = pkgs.callPackage ./tabacco-helpers {
            inherit llvmPkgs musl-gclang;
          };

          # Targets
          targets = pkgs.recurseIntoAttrs {
            curl = pkgs.callPackage ./test/tabacco-targets/curl {
              inherit musl-gclang-stdenv;
            };
            dropbear = pkgs.callPackage ./test/tabacco-targets/dropbear {
              inherit musl-gclang-stdenv;
            };
            mongoose = pkgs.callPackage ./targets/mongoose {
              inherit musl-gclang-stdenv;
            };
            wget = pkgs.callPackage ./test/tabacco-targets/wget {
              inherit musl-gclang-stdenv;
            };
            tabacco-cps = pkgs.callPackage ./targets/tabacco-cps {
              inherit musl-gclang-stdenv;
            };
            uwisc =
              pkgs.callPackage ./targets/uwisc { inherit musl-gclang-stdenv; };
          };
          targets-debloated = pkgs.lib.pipe lmcasPkgs.targets [
            debloatBinaries
            (builtins.map (pkg: {
              name = pkg.name;
              value = pkg;
            }))
            builtins.listToAttrs
            pkgs.recurseIntoAttrs
          ];

          # Tools
          # TO DO: incorporate into nightly
          tabacco-stats =
            pkgs.python39Packages.callPackage ./tools/tabacco-stats {
              inherit lmcas-metrics;
            };
          # Utils
          fix-bitcode-paths =
            pkgs.python39Packages.callPackage ./utils/fix-bitcode-paths {
              inherit lmcas-sh;
            };
          lddcollect = pkgs.python39Packages.callPackage ./utils/lddcollect { };
          libsegfault = pkgs.callPackage ./utils/libsegfault.nix { };
          lmcas-sh = pkgs.python39Packages.callPackage ./utils/sh { };
          lmcas-dejson = pkgs.python39Packages.callPackage ./utils/dejson { };
          lmcas-utils = pkgs.python39Packages.callPackage ./utils/utils { };
          lmcas-metrics = pkgs.python39Packages.callPackage ./utils/metrics {
            inherit lmcas-sh;
          };
          # Delivery image
          deliv = pkgs.callPackage ./deliv { inherit lmcasPkgs self targets; };
        };

        fmt = pkgs.callPackage ./nix/fmt.nix { inherit llvmPkgs; };

        # Functions used by debloatTargets and checksForTests
        toFilterOn = vals: builtins.split "," vals;
        filterOut = val: vals: builtins.any (x: x == val) vals;
        removeVal = removeFunc: vals:
          pkgs.lib.filterAttrs (val: _: !(removeFunc val)) vals;

        # Defines the targets that should be tested with written
        # scripts, commonly pytest.
        debloatTargets = args:
          let
            targets = lmcasPkgs.targets;
            # The targets that should not be tests
            targetsToRemove = toFilterOn (args.noTarget or "");
            # A predicate to check if a target is in targetsToRemove
            targetShouldBeRemoved = name: filterOut name targetsToRemove;
            # Removes all targets from given target attrset
            targetsToDebloat = removeVal targetShouldBeRemoved targets;
            # The debloated targets
            debloatedTargets = debloatBinaries targetsToDebloat;
          in pkgs.linkFarmFromDrvs "debloated-targets" debloatedTargets;

        debloatBinaries = pkgs.callPackage ./test/debloat.nix {
          inherit (lmcasPkgs) musl-gclang tabacco-driver;
        };

        # This function defines the tests that are run by `nix flake check` and
        # `lmcas-run-tests`. It accepts the arguments to `lmcas-run-tests` as
        # an attribute set (or an empty attribute set, for `nix flake check`),
        # and returns an attribute set of checks to build.
        checksForTests = args:
          let
            # The tests that should not be run.
            testsToRemove = toFilterOn (args.noTest or "");
            # A predicate to check if a test is in testsToRemove.
            testShouldBeRemoved = name: filterOut name testsToRemove;
            # A function that removes all the tests that should be removed from
            # an attribute set.
            removeTests = removeVal testShouldBeRemoved;
          in removeTests {
            lint = fmt.lint;
            build-neck-miner = lmcasPkgs.neck-miner;
            check-bind = pkgs.callPackage ./unit-tests/check_bind.nix {
              inherit (lmcasPkgs) musl-gclang-stdenv tabacco-driver;
            };
            check-determinism =
              pkgs.callPackage ./unit-tests/check-determinism.nix {
                inherit (lmcasPkgs)
                  musl-gclang-stdenv tabacco-driver instrumentation-parent;
                inherit (pkgs) jq;
              };
            check-robustness =
              pkgs.callPackage ./unit-tests/check-robustness.nix {
                inherit (lmcasPkgs)
                  musl-gclang-stdenv tabacco-driver instrumentation-parent;
                inherit (lmcasPkgs.targets) wget;
              };
            check-connect = pkgs.callPackage ./unit-tests/check_connect.nix {
              inherit (lmcasPkgs) musl-gclang-stdenv tabacco-driver;
            };
            check-mock = pkgs.callPackage ./unit-tests/check_mock.nix {
              inherit (lmcasPkgs) musl-gclang-stdenv tabacco-driver;
            };
            check-preneck-noexec =
              pkgs.callPackage ./unit-tests/check_preneck_noexec.nix {
                inherit (lmcasPkgs) musl-gclang-stdenv tabacco-driver;
              };
            check-sigaction = pkgs.callPackage ./unit-tests/check-sigaction {
              inherit (lmcasPkgs) musl-gclang-stdenv tabacco-driver;
            };
            check-term-size =
              pkgs.callPackage ./unit-tests/check_term_size.nix {
                inherit (lmcasPkgs) musl-gclang-stdenv tabacco-driver;
              };
            neck-locations-json =
              pkgs.callPackage ./unit-tests/neck-locations-json {
                inherit (lmcasPkgs) musl-gclang-stdenv tabacco-driver;
              };
          };
      in {
        apps = {
          fmt = {
            type = "app";
            program = "${fmt.fix}";
          };

          get-bc = {
            type = "app";
            program = "${lmcasPkgs.gllvm}/bin/get-bc";
          };

          instrumentation-parent = {
            type = "app";
            program =
              "${lmcasPkgs.instrumentation-parent}/bin/instrumentation-parent";
          };

          tape-determinism-checker = {
            type = "app";
            program =
              "${lmcasPkgs.instrumentation-parent}/bin/tape-determinism-checker";
          };

          lmcas-stats = {
            type = "app";
            program = "${lmcasPkgs.lmcas-stats}/bin/lmcas-stats";
          };

          musl-gclang = {
            type = "app";
            program = "${lmcasPkgs.musl-gclang}/bin/musl-gclang";
          };
          tabacco = {
            type = "app";
            program = "${lmcasPkgs.tabacco-driver}/bin/tabacco";
          };
        };

        checks = checksForTests { };

        defaultPackage = pkgs.runCommandNoCC "fail-to-build" { } ''
          echo "Use \`nix build -L \$LMCAS_CODE#tabacco-driver' (or another package), or \`nix flake show' to see all packages."
          false
        '';

        legacyPackages = { inherit checksForTests debloatTargets; };

        packages = flake-utils.lib.flattenTree lmcasPkgs;
      });
}
