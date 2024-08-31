#include "../neck-annotation.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int aflag = 0;
  int bflag = 0;
  char *cvalue = NULL;
  int index;
  int c;

  opterr = 0;

  while ((c = getopt(argc, argv, "abc:")) != -1)
    switch (c) {
    case 'a':
      aflag = 1;

      for (size_t i = 0; i < argc; i++) {
        printf("inner loop %zu \n", i);
        // should not be here
        // klee_dump_memory();
      }
      // should not be here
      // klee_dump_memory();
      printf("outer loop %d \n", argc);

      break;
    case 'b':
      bflag = 1;
      break;
    case 'c':
      cvalue = optarg;
      break;
    case '?':
      if (optopt == 'c')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default:
      abort();
    }

  // should be here
  klee_dump_memory();

  if (argc < optind + 1) {
    return -1;
  }

  return 0;
}
