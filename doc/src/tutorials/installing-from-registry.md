# Installing TaBaCCo from a registry

## Prerequisites

TaBaCCo is delivered as a Docker image to the GitHub container registries.
You need access to one of these registries, or to another container registry that has the TaBaCCo image.

If you cannot get access to one, you may be able to get a tarball from a coworker using `docker save`.
If you have one of those instead, see the [Installing TaBaCCo from a tarball] tutorial instead of this one.

You will also need rootless Docker installed.
If you would like to check if Docker is installed in rootless mode, run `docker info | grep rootless`; if reports `rootless: true`, Docker is installed with rootless mode.

If you don't currently have rootless Docker, [the upstream Docker instructions](https://docs.docker.com/engine/security/rootless/) may be useful.
However, if your machine is administered by your organization, it's likely your system administrator will need to be involved in the installation.

Finally, you will need to be on an x86_64-linux machine.
To check this, run `uname -msr`.
The output should be something like `Linux 5.15.67 x86_64`.

[Installing TaBaCCo from a tarball]: ./installing-from-tarball.md

## Instructions

First, figure out the image name to be used for the TaBaCCo image.

- If you're getting it from the GitHub container registry, this will be of the form:
  `ghcr.io/siftech/lmcas:VERSION`

The available versions can be seen from the web UI [here](https://github.com/siftech/lmcas/pkgs/container/lmcas).

You may need to log in to the container registry.
See [the GitHub documentation on this](https://docs.github.com/en/packages/working-with-a-github-packages-registry/working-with-the-container-registry) for more information.

The image can be pulled with `docker pull`:

```shell
$ docker pull ghcr.io.siftech/lmcas:v0.1.0
Trying to pull ghcr.io.siftech/lmcas:v0.1.0...
Getting image source signatures
Copying blob da31d7211ca0 done  
Copying blob cec3a11a7569 done  
Copying config 402ea30f4b done  
Writing manifest to image destination
Storing signatures
402ea30f4b8d6d0b52ce3aded7c2706e3d5efc875bf9b7bcb84322386429e9d9
```

A container can be made from the image with `docker run`:

```shell
$ docker run --hostname lmcas --name lmcas -it ghcr.io.siftech/lmcas:v0.1.0
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
$ docker run --hostname lmcas --name lmcas -itv $(pwd):/here ghcr.io.siftech/lmcas:v0.1.0
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
