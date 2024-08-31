// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

__attribute__((noinline, weak)) void _lmcas_neck(void) {}

int main(void) {
  if (!isatty(0))
    fprintf(stderr, "stdin is not a tty\n");

  struct winsize ws = {0};
  if (ioctl(0, TIOCGWINSZ, &ws))
    perror("ioctl failed");

  _lmcas_neck();

  printf("%dx%d\n", ws.ws_col, ws.ws_row);

  return 0;
}
