#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__attribute__((noinline, weak)) void _lmcas_neck(void) {}
struct config {
  int foo;
  int bar;
};

// Signals are numbered from 1, signal 0 doesn't exist
volatile sig_atomic_t last_received_signal = 0;

// Signal handler, will set the global variable
// to indicate what is the last signal received.
// There should be as less work as possible inside
// signal handler routine, and one must take care only
// to call reentrant functions (in case of signal arriving
// while program is already executing same function)
void signal_catcher(int signo, siginfo_t *info, void *context) {
  last_received_signal = info->si_signo;
}

int main(int argc, char **argv) {
  // Setup a signal handler for SIGUSR1 and SIGUSR2
  struct sigaction act;
  memset(&act, 0, sizeof act);

  // sigact structure holding old configuration
  // (will be filled by sigaction):
  struct sigaction old_1;
  memset(&old_1, 0, sizeof old_1);
  struct sigaction old_2;
  memset(&old_2, 0, sizeof old_2);

  act.sa_sigaction = signal_catcher;
  // When passing sa_sigaction, SA_SIGINFO flag
  // must be specified. Otherwise, function pointed
  // by act.sa_handler will be invoked
  act.sa_flags = SA_SIGINFO;

  if (0 != sigaction(SIGUSR1, &act, &old_1)) {
    perror("sigaction () failed installing SIGUSR1 handler");
    return EXIT_FAILURE;
  }

  if (0 != sigaction(SIGUSR2, &act, &old_2)) {
    perror("sigaction() failed installing SIGUSR2 handler");
    return EXIT_FAILURE;
  }

  _lmcas_neck();

  // Main body of "work" during which two signals
  // will be raised, after 5 and 10 seconds, and which
  // will print last received signal
  for (int i = 1; i <= 15; i++) {
    if (i == 5) {
      if (0 != raise(SIGUSR1)) {
        perror("Can't raise SIGUSR1");
        return EXIT_FAILURE;
      }
    }

    if (i == 10) {
      if (0 != raise(SIGUSR2)) {
        perror("Can't raise SIGUSR2");
        return EXIT_FAILURE;
      }
    }

    printf("Tick #%d, last caught signal: %d\n", i, last_received_signal);

    sleep(1);
  }

  // Restore old signal handlers
  if (0 != sigaction(SIGUSR1, &old_1, NULL)) {
    perror("sigaction() failed restoring SIGUSR1 handler");
    return EXIT_FAILURE;
  }

  if (0 != sigaction(SIGUSR2, &old_2, NULL)) {
    perror("sigaction() failed restoring SIGUSR2 handler");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
