#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#define errExit(msg)                                                           \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

__attribute__((noinline, weak)) void _lmcas_neck(void) {}

int main(int argc, char *argv[]) {
  struct rlimit old, new;
  struct rlimit *newp;
  pid_t pid;

  pid = atoi(argv[1]); /* PID of target process */

  newp = NULL;
  if (argc == 4) {
    new.rlim_cur = atoi(argv[1]);
    new.rlim_max = atoi(argv[2]);
    newp = &new;
  }

  /* Set CPU time limit of target process; retrieve and display
      previous limit */

  if (prlimit(pid, RLIMIT_CPU, newp, &old) == -1)
    printf("Previous limits: soft=%jd; hard=%jd\n", (intmax_t)old.rlim_cur,
           (intmax_t)old.rlim_max);

  /* Retrieve and display new CPU time limit */

  if (prlimit(pid, RLIMIT_CPU, NULL, &old) == -1)
    printf("New limits: soft=%jd; hard=%jd\n", (intmax_t)old.rlim_cur,
           (intmax_t)old.rlim_max);

  _lmcas_neck();

  exit(EXIT_SUCCESS);
}
