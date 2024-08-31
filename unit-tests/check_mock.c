// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

__attribute__((noinline, weak)) void _lmcas_neck(void) {}

int main(int argc, char **argv) {
  fputs("entering main.\n", stderr);
  uid_t uid = getuid();
  uid_t euid = geteuid();
  gid_t gid = getgid();
  pid_t pid = getpid();
  pid_t ppid = getppid();
  printf("uid=%lu, euid=%lu, gid=%lu, pid=%lu, ppid=%lu\n",
         (unsigned long int)uid, (unsigned long int)euid,
         (unsigned long int)gid, (unsigned long int)pid,
         (unsigned long int)ppid);

  _lmcas_neck();
  return 0;
}
