# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

CC = clang
CFLAGS = -ffreestanding -std=c11 -Wall -Werror

all: lmcas_instrumentation_runtime.bc
.PHONY: all

lmcas_instrumentation_runtime.bc: lmcas_instrumentation_runtime.c lmcas_instrumentation_runtime.h
	$(CC) $(CFLAGS) -c -D_DEFAULT_SOURCE -emit-llvm -fno-stack-protector -U_FORTIFY_SOURCE -o $@ $<

example/main: example/main.c lmcas_instrumentation_runtime.bc
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
