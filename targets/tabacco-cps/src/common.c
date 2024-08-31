#include "common.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef MUST_AUTO_NECK_ID
void _lmcas_neck(void) {}
#endif

int read_file(const char *path, unsigned char **out_buf, size_t *out_len) {
  FILE *file = fopen(path, "r");
  if (!file)
    return errno;
  size_t len = 0, cap = 1;
  unsigned char *buf = malloc(cap);

  while (1) {
    assert(len < cap);
    size_t count = fread(&buf[len], 1, cap - len, file);
    if (ferror(file))
      return ferror(file);
    len += count;
    if (len < cap)
      break;
    cap *= 2;
    buf = realloc(buf, cap);
  }

  *out_buf = buf;
  *out_len = len;
  return 0;
}
