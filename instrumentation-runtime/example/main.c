// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../lmcas_instrumentation_runtime.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *read_to_end(const char *path) {
  lmcas_instrumentation_bb_start(4);

  int fd = open("/etc/passwd", O_RDONLY);
  if (fd == -1) {
    lmcas_instrumentation_bb_start(5);
    int err = errno;
    perror("open");
    exit(err);
  }

  lmcas_instrumentation_bb_start(6);
  char *buf = malloc(16);
  size_t len = 0, cap = 16;

  while (1) {
    lmcas_instrumentation_bb_start(7);
    if (cap == len) {
      lmcas_instrumentation_bb_start(8);
      cap *= 2;
      buf = realloc(buf, cap);
    }

    lmcas_instrumentation_bb_start(9);
    ssize_t ret = read(fd, &buf[len], cap - len);
    if (ret == -1 && errno == EINTR) {
      lmcas_instrumentation_bb_start(10);
      continue;
    } else if (ret == -1) {
      lmcas_instrumentation_bb_start(11);
      int err = errno;
      perror("read");
      exit(err);
    } else if (ret == 0) {
      lmcas_instrumentation_bb_start(12);
      if (close(fd) == -1) {
        lmcas_instrumentation_bb_start(13);
        int err = errno;
        perror("close");
        exit(err);
      }
      lmcas_instrumentation_bb_start(14);
      buf[len] = 0;
      return buf;
    } else {
      lmcas_instrumentation_bb_start(15);
      len += ret;
    }
  }
}

int main(void) {
  lmcas_instrumentation_setup();

  lmcas_instrumentation_bb_start(0);
  int x = 2 + 2;
  int y;
  lmcas_instrumentation_record_cond_br(x > 3);
  if (x > 3) {
    lmcas_instrumentation_bb_start(1);
    y = 1;
  } else {
    lmcas_instrumentation_bb_start(2);
    y = 2;
  }
  lmcas_instrumentation_bb_start(3);

  (void)write(STDOUT_FILENO, &y, 4);

  char *buf = read_to_end("/etc/passwd");
  printf("%s", buf);

  lmcas_instrumentation_done();
  return 0;
}
