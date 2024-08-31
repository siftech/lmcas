# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ callPackage, gllvm, llvmPkgs, makeWrapper, musl, runCommandNoCC }:

runCommandNoCC "musl-gclang" {
  meta.mainProgram = "musl-gclang";

  passthru = {
    inherit gllvm;
    targetPrefix = "";
  };
} ''
  substitute ${./musl-gclang.in} musl-gclang \
    --subst-var-by CC ${gllvm}/bin/gclang \
    --subst-var-by CLANG_LIB ${llvmPkgs.clang-unwrapped.lib} \
    --subst-var-by COMPILER_RT ${llvmPkgs.compiler-rt} \
    --subst-var-by MUSL ${musl} \
    --subst-var-by MUSL_DEV ${musl.dev} \
    --subst-var-by OUT $out
  substitute ${./ld.musl-gclang.in} ld.musl-gclang \
    --subst-var-by LIBUNWIND ${llvmPkgs.libunwind} \
    --subst-var-by MUSL ${musl} \
    --subst-var-by WRAPPED_CC ${llvmPkgs.clangUseLLVM}/bin/clang

  install -Dt $out/bin musl-gclang ld.musl-gclang
  ln -st $out/bin ${gllvm}/bin/get-bc ${gllvm}/bin/gsanity-check
''
