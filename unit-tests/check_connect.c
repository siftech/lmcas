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

int test_connect() {
  struct sockaddr_un sa;
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    fputs("Failed socket call", stderr);
    return -2;
  }
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, "TEST_ADDR_PATH", 15);
  if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    switch (errno) {
    case ENOENT:
      fputs("Got ENOENT back from connect.", stderr);
      return 0;
    default:
      fputs("Incorrect error from connect:", stderr);
      return -4;
    }
  } else {
    close(fd);
    return -1;
  }
}

int main(int argc, char **argv) {
  fputs("entering main.\n", stderr);
  int status = test_connect();
  if (status < 0) {
    fputs("test_connect failed to get the right error.", stderr);
  } else {
    fputs("test_connect got the right error back.", stderr);
  }
  _lmcas_neck();
  return 0;
}
