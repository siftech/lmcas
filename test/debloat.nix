# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ symlinkJoin, lib, linkFarmFromDrvs, runCommandNoCC, musl-gclang, pkgs
, tabacco-driver }:

let
  # Python script for comparing neck locations to rubric
  test-neck-locations =
    pkgs.python39Packages.callPackage ./test-neck-locations/default.nix { };
  # Debloat all targets given source targets
  # Input: an attrset of all targets
  # Output: a list of derivations
in targets:
let
  /* Debloat target w/ given spec
     Input:
        - target: a source target derivation
        - spec: a spec represented as an attrset
     Output: a derivation w/ both bloated & debloated binaries & all
      associated temporary files
  */
  debloatTargetFromSpec = target: spec-name: spec:
    let
      bin = spec.binary;
      relative-path = "${target.passthru.bin-paths.${bin}}";
      bin-path = "${target}/${relative-path}";
      use-neck-miner = if (spec ? use_neck_miner) then "false" else "true";
      specfile = pkgs.writeText "${bin}.json"
        (builtins.toJSON (spec // { binary = bin-path; }));
    in runCommandNoCC "${bin}" {
      inherit specfile;
      nativeBuildInputs = [ musl-gclang tabacco-driver ];
    } ''
      dir=${spec-name}
      mkdir -p $dir
      cd $dir
      mkdir -p bloated
      mkdir -p bloated/tmp
      mkdir -p debloated
      mkdir -p debloated/tmp

      echo "DEBLOATING BINARY: ${bin}"
      tabacco ${specfile} -o debloated/${spec-name} --tmpdir debloated/tmp
      tabacco ${specfile} --no-debloat -o bloated/${spec-name} --tmpdir bloated/tmp

      mkdir -p $out/$dir
      mkdir -p $out/$dir/debloated
      mkdir -p $out/$dir/bloated
      mkdir -p $out/$dir/debloated/tmp
      mkdir -p $out/$dir/bloated/tmp

      install -DT debloated/${spec-name} $out/$dir/debloated/${spec-name}
      install -DT bloated/${spec-name} $out/$dir/bloated/${spec-name}
      cp -r bloated/tmp $out/$dir/bloated
      cp -r debloated/tmp $out/$dir/debloated

      echo "debloated ${bin} installed in $out/${spec-name}"

      if [ "${use-neck-miner}" = "true" ]; then
        ${test-neck-locations}/bin/compare_neck_locations "${spec-name}" "$out/$dir/debloated/tmp/neck_miner_neck.json"
      fi
    '';
  # If multiple specs exist for one target, debloat each target once for each spec
  # Input: target, a derivation
  # Output: a derivation of all debloated & bloated bins for one target (may be
  # multiple if there are multiple specs)
  debloatTargetFromSpecs = target:
    let
      derivations =
        lib.mapAttrsToList (name: spec: debloatTargetFromSpec target name spec)
        target.passthru.specs;
    in symlinkJoin {
      name = "${target.pname or target.name}";
      paths = derivations;
      postBuild = ''echo "target links added in $out"'';
    };
in lib.pipe targets [
  builtins.attrValues
  (builtins.filter lib.isDerivation)
  (map debloatTargetFromSpecs)
]
