#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__attribute__((noinline, weak)) void _lmcas_neck(void) {}

volatile sig_atomic_t stay_alive = 1;
volatile sig_atomic_t reload_config = 0;

void load_config() {
  printf("Load config\n");
  fflush(stdout);
}

void handle_reload_config(int signo, siginfo_t *info, void *context) {
  reload_config = 1;
}

void handle_interrupt(int signo, siginfo_t *info, void *context) {
  stay_alive = 0;
}

int main(int argc, char **argv) {

  struct sigaction hup;
  memset(&hup, 0, sizeof hup);
  hup.sa_sigaction = handle_reload_config;
  int sig = SIGHUP;
  if (sigaction(sig, &hup, NULL) != 0) {
    perror("sigaction () failed installing SIGHUP handler");
    return EXIT_FAILURE;
  }

  struct sigaction intr;
  memset(&intr, 0, sizeof intr);
  intr.sa_sigaction = handle_interrupt;
  if (sigaction(SIGINT, &intr, NULL) != 0) {
    perror("sigaction () failed installing SIGINT handler");
    return EXIT_FAILURE;
  }

  load_config();
  _lmcas_neck();
  int i = 0;
  while (1) {
    fflush(stdout);
    if (reload_config) {
      reload_config = 0;
      printf("Reload config\n");
      fflush(stdout);
      load_config();
    }

    if (!stay_alive) {
      printf("Interrupt\n");
      fflush(stdout);
      break;
    }

    usleep(100000);
    i += 1;
    if (i >= 100) {
      printf("Reached max count\n");
      fflush(stdout);
      break;
    }
  }
  return EXIT_SUCCESS;
}
