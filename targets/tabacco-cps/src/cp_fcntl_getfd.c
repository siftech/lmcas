#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {

  enum { MODE_INVALID, MODE_ONE, MODE_TWO } mode = MODE_INVALID;
  int rVal, fd;

  if ((fd = open("file1", O_CLOEXEC)) < 0) {
    printf("error opening file.\n");
    return -1;
  } else {
    printf("File opened.\n");
  }

  if ((rVal = fcntl(fd, F_GETFD)) < 0) {
    printf("Error getting file status flags.\n");
    return -1;
  } else {
    printf("File status flags retrieved.\n");
  }

  int opt;
  while ((opt = getopt(argc, argv, "ot")) != -1) {
    switch (opt) {
    case 'o':
      mode = MODE_ONE;
      break;
    case 't':
      mode = MODE_TWO;
      break;
    default:
      fprintf(stderr, "Usage: %s <-o|-t>\n", argv[0]);
      return -1;
    }
  }

  switch (mode) {
  case MODE_ONE:
    printf("Option 1 is selected and return of fcntl is: %d", rVal);
    break;
  case MODE_TWO:
    printf("Option 2 is selected and return of fcntl is: %d", rVal);
    break;
  default:
    fprintf(stderr, "Error: invalid mode\n");
    return 1;
  }

  close(fd);
  unlink("file1");

  return 0;
}
