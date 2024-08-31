// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

__attribute__((noinline)) void _lmcas_neck(void) {}

static int flag = 0;

void handler(int sig) { flag = sig; }

int main(void) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  struct sigaction sa = {
      .sa_handler = handler,
      .sa_mask = mask,
      .sa_restorer = (void (*)(void))0x9abcdef0,
  };
  sigaction(SIGINT, &sa, NULL);

  _lmcas_neck();

  raise(SIGINT);
  printf("%d\n", flag);
  return 0;
}
