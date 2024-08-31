// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  enum { MODE_INVALID, MODE_HELLO, MODE_LS } mode = MODE_INVALID;

  int opt;
  while ((opt = getopt(argc, argv, "hl")) != -1) {
    switch (opt) {
    case 'h':
      mode = MODE_HELLO;
      break;
    case 'l':
      mode = MODE_LS;
      break;
    default:
      fprintf(stderr, "Usage: %s <-h|-l>\n", argv[0]);
      return -1;
    }
  }

  switch (mode) {
  case MODE_HELLO:
    puts("Hello, world!");
    break;
  case MODE_LS:
    return system("ls");
  default:
    fprintf(stderr, "Error: invalid mode\n");
    return 1;
  }

  return 0;
}
