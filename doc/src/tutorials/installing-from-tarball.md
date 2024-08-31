# Tutorial: Installing TaBaCCo from a tarball

## Prerequisites

TaBaCCo is delivered as a Docker image in a `.tar.gz` archive.
In the below instructions, we assume that it's named `lmcas-deliv.tar.gz`, but the exact file name doesn't matter.

If you have access to a Docker registry that has TaBaCCo's Docker image, see the [Installing TaBaCCo from a registry] tutorial instead of this one.

You will also need rootless Docker installed.
If you would like to check if Docker is installed in rootless mode, run `docker info | grep rootless`; if there are any results, Docker is installed with rootless mode.

If you don't currently have rootless Docker, [the upstream Docker instructions](https://docs.docker.com/engine/security/rootless/) may be useful.
However, if your machine is administered by your organization, it's likely your system administrator will need to be involved in the installation.

Finally, you will need to be on an x86_64-linux machine.
To check this, run `uname -msr`.
The output should be something like `Linux 5.15.67 x86_64`.

[Installing TaBaCCo from a registry]: ./installing-from-registry.md

## Screencast

{{#asciinema installing}}

## Instructions

The Docker image can be loaded into Docker with `docker load`, and run with `docker run`:

```shell
$ docker load < lmcas-deliv.tar.gz
...
Loaded image: lmcas-deliv:v0.1.0
$ docker run --hostname lmcas --name lmcas -it lmcas-deliv:v0.1.0
lmcas ~ #
```

This is all that's necessary to get a basic container started.
If you exit the container, you can restart it with `docker start`:

```shell
lmcas ~ # exit
$ docker start -a lmcas
lmcas ~ #
```

Once stopped, the container can be deleted with `docker rm`.

```shell
lmcas ~ # exit
$ docker rm lmcas
```

It can then be recreated with `docker run`, possibly with different flags.
For example, I typically mount my working directory to `/here`, to make it convenient to move files between the container's filesystem and the system's filesystem.

```shell
$ docker run --hostname lmcas --name lmcas -itv $(pwd):/here lmcas-deliv:v0.1.0
lmcas ~ #
```

The command `tabacco` within the container is used to run TaBaCCo.

```shell
lmcas ~ # tabacco --help
Usage: tabacco [OPTIONS] DEBLOATING_SPEC_FILE [INPUT_PROGRAM]

  Debloats according to the debloating specification using TaBaCCo.

Options:
  -o, --output FILE               The path the debloated binary will be
                                  written to  [required]
  --tmpdir DIRECTORY              The path temporary files will be written to
  --tabacco-safe-external-function-regex TEXT
                                  Additional safe external function regexes to
                                  use during the specialization pass
  -O, --optimize [0|1|2|3|s|z]    Flags controlling how much optimization
                                  should be performed
  -v, --verbose                   Flag controlling verbose flags passed to
                                  components.
  --debloat / -N, --no-debloat    Don't debloat, just optimize and whole-
                                  program compile
  --input-type [binary|bitcode|llvm-ir-src]
                                  type of input-program
  --use-neck-miner / -M, --no-use-neck-miner
                                  Don't use neck-miner, assume the neck is
                                  already marked
  --help                          Show this message and exit.
```
