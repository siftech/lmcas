# LMCAS TaBaCCo

Lightweight Multi-Stage Compiler Assisted Application Specialization, or LMCAS, is a software tool that hardens and debloats Linux programs by removing unused functionality.
Given an application's source code and specific functionality to preserve, as selected by command line arguments or a configuration file, LMCAS generates a specialized version of the application supporting only that functionality.
LMCAS prevents the other options from executing, and integrates with standard tools that detect unreachable code to remove the unsupported options' code from the specialized binary.

The original version of LMCAS was developed by the University of Wisconsin, and is available [here](https://github.com/Mohannadcse/LMCAS).
See [doi:10.1109/EuroSP53844.2022.00024](https://doi.org/10.1109/EuroSP53844.2022.00024) for more information.

This version of LMCAS, called TaBaCCo (Tape-Based Control-flow Concretization), replaces the original UWisc LMCAS algorithms with a more robust and easier to use approach.
TaBaCCo incorporates new algorithms to handle more applications, such as applications that use configuration files to control functionality.
TaBaCCo automatically analyzes the source code to automate some tasks that required human intervention in the original LMCAS tool.

TaBaCCo runs on a target program and creates a new binary specialized for a specific purpose, with some of its functionality removed.
TaBaCCo requires applications be compiled into LLVM bitcode.
The standard Clang compiler supports LLVM bitcode, making it easy to recompile most C programs from source code to apply TaBaCCo.
TaBaCCo locks the command line options and configuration files to a specific configuration and removes the code supporting the unused functionality.
The debloated program will not accept any more command-line *options*, but may accept other arguments.

## Development shell

The [Nix](https://nixos.org/) package manager is used to acquire dependencies and orchestrate builds.
To avoid the need to install it, a Docker image is provided.
This image is based on `ghcr.io/siftech/nix-base-image`.

If you haven't pulled images from the GitHub container registry before, see [these instructions](https://docs.github.com/en/packages/working-with-a-github-packages-registry/working-with-the-container-registry).
Otherwise, a normal `docker pull ghcr.io/siftech/nix-base-image` is sufficient.

Running `./tools/host/lmcas-dev-shell` will start the dev shell.

## Test suite

Our test suite may be run from `code/` on your host machine with

```
./test/lmcas-run-tests
```

This script will trigger both the debloating and test scripts to run in sequence.
Once the lmcas-run-tests script begins, it launches a dev shell where targets are first debloated if the current set up is not cached in the nix store.
Additional tests like the linter and unit tests will also run during this stage.
After the debloating process, we then launch a docker container for every target and every test under that target (exccept the tabacco cps and uwisc targets where all tests are run in the same container).

Some target tests require IPv6 support for Docker, which is not enabled by default.
This can be enabled locally by following the [Enable IPv6 support](https://docs.docker.com/config/daemon/ipv6/) guide.

### Testing options

Specific targets may be excluded from a test run with the flag `--no-target` followed by a comma separated list of targets.
The legal names for targets can be found in `flake.nix` in the targets attrset.

```
./test/lmcas-run-tests --no-target dropbear,mongoose
```

Similarly, specific tests run in the dev shell during the first testing stage may also be turned off.

```
./test/lmcas-run-tests --no-test lint,check-bind
```

### Adding new targets

New targets can be added under `test/tabacco-targets`.
Create a new directory there for the target. This should include:

1. A patch which manually places the neck into the source (this may change once neck miner has been fully integrated)
2. `default.nix` to build and install the target. This should also include two passthrus:
    - `passthru.bin-paths` to specify the path to the binary or binaries you intend to debloat
    - `passhtru.specs` to specify all the tabacco specifications that will be used during testing
3. All tabacco specs you intend to debloat with.
These specs are given as nix attrsets but later converted to json during the debloating stage of the tests.
4. A directory for each test you would like to run in a separate docker container.
This must contain a script `test.sh`, but this script can be used to dispatch e.g. `pytest` or as the test itself.
5. Any extra configuration files you might need.

5.5. You will also need to add it to the `targets` attrset in `flake.nix`.

The directories `curl`, `dropbear`, `mongoose`, etc. in `test/tabacco-targets` are all good examples of this and their directory structure should be closely followed.
You may reference their contents for how to set up your own tests.

You can add targets for debloating before writing more elaborate test scripts for them, but you will still need to implement steps 1-3 and 5.5 above for your new target.

The file `test/Dockerfile` sets up the docker image that all target test scripts are run in. If your test requires additional dependencies (that do not need to be built from source), you can add them here.

### Adding other tests

Non-target specific tests are also run during the debloating stage.
This includes the linter and unit tests.
New unit tests should be written as `.nix` files and placed in `unit-tests/`.
This directory might be restructured as additional unit tests are added.

You can include your unit tests by adding them to flake.nix in the attrset under the function `checksForTests`.

## Developing the passes

Run e.g. `nix develop .#instrumentation-pass` in the root directory of the project to get into the correct environment, then `cd` into the `instrumentation-pass` directory.

The first time you do this (or if a `build` directory does not exist), run `cmake -B build -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo` to generate build scripts in `build`.

This is also necessary for the language server plugin for your editor to pick up the locations of libraries.

To build the pass (e.g. for a test compile), run `ninja -C build`.
In general, however, passes should be built through the Nix infrastructure to ensure a clean build and to replicate the conditions used to prepare release builds.

## Funding Acknowledgement

This material is based on research sponsored by the Office of Naval Research via contract number N00014-21-C-1032, a prime contract to Grammatech.
The views and conclusions contained herein are those of the authors and should not be interpreted as necessarily representing the official policies or endorsements, either expressed or implied, of the Office of Naval Research.
