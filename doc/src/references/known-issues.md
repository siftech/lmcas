# Known issues

These are situations that TaBaCCo currently cannot handle.
In most cases, these are solvable with sufficient engineering effort rather than being fundamental issues of the approach, and so may be resolved in a future version.

<!-- toc -->

## Compiling specific libraries for use with TaBaCCo

Issues with libraries are typically due to peculiarities of their build systems.
We don't want to recommend that users start modifying the non-standard internals of projects' build systems, since this easily results in a broken build and the details of the needed modifications can easily change between versions.
As a result, we don't have a standard procedure for these libraries.
If desired, the delivery container could begin shipping precompiled copies of these libraries that avoid the need for a user to understand the peculiarities of their build systems.

- [gnulib](https://www.gnu.org/software/gnulib/) is a bit of a strange case.
  Versions of gnulib from the last decade should work with musl-gclang (and therefore with TaBaCCo).
  However, copies that don't include commit [2b14f42b44](https://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=commit;h=2b14f42b44f04bce5631269ed46cd8be2413ccec) (committed on 19 Jun 2012) will not work, because patches necessary to support musl were added then.

  Unlike most libraries, which are installed system-wide, gnulib is installed by copying its sources into the project being compiled.
  Making things worse, individual source files are typically copied, rather than using a Git submodule, SVN external, or other similar mechanism.
  As a result, updating gnulib is a task that requires some knowledge about how the project uses GNU Autotools, and is difficult to fully automate.

- [ncurses](https://invisible-island.net/ncurses/) appears to want a C++ compiler for the build machine.
  Unfortunately, because `musl-gclang` pretends to be a native compiler rather than a cross-compiler, if ncurses' configure script finds a C++ compiler, it will assume that it can link object files produced by it to those produced by `musl-gclang`.
  This is not the case.

- [OpenSSL](https://www.openssl.org/) is partially written in C and partially in assembly.
  Currently, TaBaCCo cannot handle this situation automatically.
  In principle, a sufficiently expert user could break the library up to separate the two parts, and modify TaBaCCo to link in the assembly part where appropriate.

## Container features

- We don't ship a package manager in the container, because there are very few packages that can be safely installed without perturbing the development environment.
  This is because any libraries that are installed from an off-the-shelf package manager will not have been compiled with musl-gclang, and installing them may make other packages' configure scripts confused.

## Language support

- Currently, TaBaCCo only supports POSIX C, with some extensions supported by Clang.
  In particular, software that contains components written in assembly or C++ may pose trouble.

  Inline assembly can be handled as long as it does not get called before the neck, however.

## System calls

- Currently, TaBaCCo only supports certain address families in networking-related syscalls before the neck.
  The specific address families are given in the [Supported system calls](../references/syscalls.md) reference page.

  More address families could potentially be supported later, but in practice, very few programs e.g. create AX.25 sockets before the neck.

- Currently, TaBaCCo requires many system calls performed before the neck to succeed.
  This restriction could be lifted later on a system call by system call basis, but it's critical to inspect the reasons a particular system call may fail to ensure that they have no observable effects on the process.

  In practice, most programs do only minimal I/O before the neck, so this restriction does not prevent the successful debloating of programs encountered during the development of TaBaCCo.

## Programs that use PID

- TaBaCCo does not support programs that manipulate and store the process identifier (PID) of themselves before the neck. In many systems, PID will always vary on each new execution of a binary, and this means that the PID that is frozen in specialization will be a different, and therefore incorrect, PID than the actual PID assigned to the binary in execution. In addition, because PIDs are often written to files that are read by other programs (e.g `init` systems that manage daemons and other long running processes), the wrong PID might cause these systems to unintentionally spawn thousands of unnecessary duplicate processes.

  As a result, TaBaCCo does not support any binary that calls `sys_getpid` or `sys_getppid` before the neck, as calls to those syscalls are definite signs that the program intends to manipulate the PID of itself.

  TaBaCCo is still able to debloat binaries that manipulate PID after the neck, as the PID is no longer being frozen by the process of specialization.
