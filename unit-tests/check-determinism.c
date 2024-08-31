// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>

__attribute__((noinline, weak)) void _lmcas_neck(void) {}

int main(int argc, char *argv[]) {
  const uint16_t *ptr = (const uint16_t *)getauxval(AT_RANDOM);
  printf("value of getauxval -> (%" PRIu16 ")\n", *ptr);
  // uses it in a call to sys_open.
  char str[30];
  sprintf(str, "/tmp/%" PRIu16, *ptr);
  fopen(str, "r");

  _lmcas_neck();

  return 0;
}
