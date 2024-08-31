// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

__attribute__((noinline, weak)) void _lmcas_neck(void) {}

int main(int argc, char **argv) {
  int sfd;
  struct sockaddr_in6 my_addr;

  sfd = socket(AF_INET6, SOCK_STREAM, 0);
  if (sfd == -1)
    handle_error("socket");

  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin6_family = AF_INET6;
  my_addr.sin6_port = 0;
  my_addr.sin6_addr = in6addr_any;

  if (bind(sfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1)
    handle_error("bind");

  if (listen(sfd, 50) == -1)
    handle_error("listen");

  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);
  if (getsockname(sfd, (struct sockaddr *)&sin, &len) == -1)
    perror("getsockname");

  _lmcas_neck();

  /* Code to deal with incoming connection(s)... */

  if (close(sfd) == -1)
    handle_error("close");

  return 0;
}
