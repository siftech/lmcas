# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ annotation-pass, binutils, instrumentation-pass, instrumentation-runtime
, llvmPkgs, musl, musl-gclang, musl-gclang-stdenv, recurseIntoAttrs
, runCommandNoCC }:

recurseIntoAttrs rec {
  # A build of musl with bitcode embedded in the same way it is for target
  # programs.
  with-embedded-bitcode =
    (musl.override { stdenv = musl-gclang-stdenv; }).overrideAttrs (oldAttrs: {
      name = "musl-with-embedded-bitcode-${oldAttrs.version}";
      pname = "musl-with-embedded-bitcode";

      patches = oldAttrs.patches ++ [
        ./0001-disable-vdso.patch
        ./0002-use-stdatomic.patch
        ./0003-remove-isatty-ioctl-in-stdout.patch
        ./0004-wipe-at_random.patch
      ];

      prePatch = ''
        patchShebangs configure
      '';
      configureFlags = oldAttrs.configureFlags;

      postUnpack = ''
        mv $sourceRoot/src/malloc/mallocng/{free,freeng}.c
        mv $sourceRoot/src/malloc/mallocng/{realloc,reallocng}.c
      '';
    });

  # Bitcode extracted from the with-embedded-bitcode build of musl. Includes:
  #
  # - the original bitcode, annotated with the annotation pass, in annotated.bc
  # - the assembly code (which can't be instrumented), in asm-objects.a
  # - the instrumented bitcode plus the runtime, in instrumented.bc
  # - a static library containing the instrumented bitcode, the runtime, and
  #   the assembly code, in libc-instrumented.a
  bitcode = runCommandNoCC "musl-bitcode" {
    nativeBuildInputs = [ binutils llvmPkgs.llvm musl-gclang ];
  } ''
    # Loop over all the object files in libc.a, extracting bitcode from the
    # ones written in C and saving the ones written in assembly off to the side.
    # The assembly ones get handled specially later.
    mkdir asm-objects bitcode-files object-files
    cd object-files
    ar x ${with-embedded-bitcode}/lib/libc.a
    for file in *; do
      get-bc -o "../bitcode-files/$file" "$file" \
        || cp "$file" "../asm-objects/$file"
    done
    cd ..

    # We combine all the bitcode to a single large bitcode file.
    llvm-link -o libc-unmerged.bc bitcode-files/*
    opt -constmerge -mergefunc -o libc.bc libc-unmerged.bc

    # Once we have the bitcode in hand, we can annotate it. This gives numeric
    # IDs to each basic block (technically, the basic block's terminator) so
    # that we can uniquely identify them without relying on names (which are
    # not necessarily unique thanks to unnamed functions in LLVM IR).
    mkdir $out
    opt \
      -load ${annotation-pass}/lib/libLmcasAnnotationPass.so \
      -annotate \
      -o $out/annotated.bc libc.bc

    # We also bundle up the assembly code to a static library, for linking the
    # final binary with.
    ar rcs $out/asm-objects.a asm-objects/*

    # Next, we run the instrumentation pass on the annotated bitcode. The
    # output isn't complete yet, since it has unresolved calls to the
    # instrumentation runtime, and none of the syscall wrappers been replaced.
    opt \
      -load ${instrumentation-pass}/lib/libLmcasInstrumentationPass.so \
      -instrument \
      -instrument-libc \
      $out/annotated.bc \
      -o instrumented-without-runtime.bc

    # Once the libc has been annotated, we can link in the instrumentation
    # runtime, replacing the syscall wrappers (which we're intentionally
    # overriding).
    llvm-link \
      -o $out/instrumented.bc \
      instrumented-without-runtime.bc \
      --override=${instrumentation-runtime}

    # Finally, we can compile the instrumented libc bitcode to an object file,
    # and bundle it with the hacked up assembly object files to make a new
    # static library.
    llc -o instrumented.s $out/instrumented.bc
    as -o instrumented.o instrumented.s
    ar rcs $out/libc-instrumented.a instrumented.o asm-objects/*
  '';
}
