#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

__attribute__((noinline, weak)) void _lmcas_neck(void) {}

int main(int argc, char **argv) {
  char *config_path = NULL;
  if (argc != 2) {
    fprintf(stderr, "Usage: %s FILE\n", argc ? argv[0] : "cp9");
    return EINVAL;
  }

  int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    int err = errno;
    perror("open");
    return err;
  }

  size_t len = 0, cap = 4096;
  unsigned char *buf = malloc(cap);
  while (1) {
    if (len == cap) {
      cap = len * 2;
      buf = realloc(buf, cap);
    }
    ssize_t count = read(fd, &buf[len], cap - len);
    if (count < 0) {
      int err = errno;
      perror("read");
      return err;
    } else if (count == 0) {
      break;
    } else {
      len += count;
    }
  }

  if (close(fd) == -1) {
    int err = errno;
    perror("close");
    return err;
  }

  size_t line_count = 0;
  for (size_t i = 0; i < len; i++)
    if (buf[i] == '\n')
      line_count++;
  printf("There were %zu lines.\n", line_count);

  _lmcas_neck();

  size_t off = 0;
  while (off < len) {
    ssize_t count = write(STDOUT_FILENO, &buf[off], len - off);
    if (count < 0) {
      int err = errno;
      perror("write");
      return err;
    }
    off += count;
  }

  return 0;
}
