# musl-gclang

The musl-gclang compiler needs to be used in order to build programs and libraries used as inputs to TaBaCCo.

This is necessary because TaBaCCo needs to do whole-program analysis on the [LLVM bitcode] in order to function.

The [Clang] C compiler is used to compile the C source code to LLVM bitcode.

A modified version of [GLLVM] is also used to wrap Clang to store the bitcode in the programs and libraries it outputs.

The [glibc] implementation of the C standard library is not easily able to be compiled to LLVM bitcode at the time of writing, so `musl-gclang` instead links programs to the [musl] C standard library instead.
A second layer of wrapping is used to achieve this.

The compiler is wrapped instead of modified so that an off-the-shelf copy of Clang can be used, making it faster and cheaper to upgrade it.

[LLVM bitcode]: https://releases.llvm.org/12.0.1/docs/BitCodeFormat.html
[Clang]: https://clang.llvm.org/
[GLLVM]: https://github.com/SRI-CSL/gllvm
[glibc]: https://www.gnu.org/software/libc/
[musl]: https://musl.libc.org/
