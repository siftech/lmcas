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

## How this documentation is organized

There are four sections to the documentation, available from the hamburger menu ([&#9776;](#sidebar-toggle)) in the upper-left corner.
You may need to scroll up to see it.

- **Tutorials** walk you through the installation and the steps of debloating an application.
  Start here if you've never used TaBaCCo before.
- **Topic Guides** describe how TaBaCCo works and provide useful background information that you may wish to know before using TaBaCCo on your own applications.
- **How-To Guides** explain how to do some of the more complicated tasks that you may need to do when using TaBaCCo.
- **References** are technical references that exhaustively describe the user-exposed parts of TaBaCCo.

## High-level overview of LMCAS TaBaCCo

TaBaCCo runs much faster than some other debloating tools by taking advantage of a common program structure.

TaBaCCo assumes that the program targeted for debloating can be split into two parts at a point called the **neck**.
The portion of the program before the neck processes the command line arguments and configuration files.
The portion of the program after the neck performs the actual work of the program.

TaBaCCo works by executing the program up to the neck, instrumenting the control flow and I/O the program performs onto a **tape**.
The tape records the value of configuration variables, ensuring they cannot change, locking in the desired behavior.
A residual program is then specialized from the input program using the tape.
TaBaCCo locks the control-flow and I/O recorded into the tape, and removes code corresponding to the removed options.

Using TaBaCCo requires the following:

- The ability to build the program you want to specialize from source code.
- The ability to build any required libraries from source code.
- The target program to be specialized, and all of its library dependencies, must be POSIX C code compilable by the [Clang] 12.0.1 compiler.
- The target program to be specialized, and all of its library dependencies, must be capable of being built against the [musl] libc.
- The target program to be specialized, and all of its library dependencies, must run on an x86_64-linux machine.
- The target program must only use [system calls we support] before the neck.
  Any system call can be used after the neck.
- Your system must be a x86_64-linux machine with rootless Docker.

This material is based on research sponsored by the Office of Naval Research via contract number N00014-21-C-1032, a prime contract to Grammatech.
The views and conclusions contained herein are those of the authors and should not be interpreted as necessarily representing the official policies or endorsements, either expressed or implied, of the Office of Naval Research.

## Known Issues with TaBaCCo

We have documented known issues that do not work in the current version.   Please check [known issues] to make sure the application you want to debloat will work with this version TaBaCCo.    Most of these issues simply will be addressed in future versions of TaBaCCo.

[Clang]: https://clang.llvm.org/
[cURL]: https://curl.se/
[known issues]: ./references/known-issues.md
[musl]: https://musl.libc.org/
[system calls we support]: ./references/syscalls.md
