#include "../neck-annotation.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int iterative_factorial(int N) {
  if (N <= 1) {
    return 1;
  }
  int fact = 1;
  for (int i = 1; i <= N; i++) {

    fact = fact * i;
  }
  return fact;
}

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

  klee_dump_memory();
  iterative_factorial(aflag);

  if (argc < optind + 1) {
    return -1;
  }

  return 0;
}
