#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

__attribute__((noinline, weak)) void _lmcas_neck(void) {}

struct config {
  uint16_t port;
  char *header;
  char *text;
};

// takes ownership, nullable
static struct config load_config(char *config_path);

static int setup_sock(const struct config *config);

int main(int argc, char **argv) {
  char *config_path = NULL;
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [CONFIG_FILE]\n", argc ? argv[0] : "cp8");
    return EINVAL;
  } else if (argc == 2) {
    config_path = strdup(argv[1]);
  }

  struct config config = load_config(config_path);

  // _lmcas_neck();

  int sock = setup_sock(&config);

  while (1) {
    int conn = accept(sock, NULL, NULL);
    if (conn == -1) {
      int err = errno;
      perror("accept");
      exit(err);
    }

    write(conn, "# ", 2);
    write(conn, config.header, strlen(config.header));
    write(conn, "\n\n", 2);
    write(conn, config.text, strlen(config.text));

    if (shutdown(conn, SHUT_RDWR)) {
      int err = errno;
      perror("shutdown");
      exit(err);
    }

    close(conn);
  }
  return 0;
}

// free() me
static char *get_config_path(void) {
  errno = 0;
  // writes to static buffer, don't free()
  struct passwd *pwd = getpwuid(getuid());
  if (!pwd) {
    int err = errno;
    perror("getpwuid");
    exit(err);
  }

  const char *homedir = pwd->pw_dir;
  size_t homedir_len = strlen(homedir);
  if (!homedir_len) {
    fprintf(stderr, "failed to get home directory\n");
    exit(ENOENT);
  }

  // remove trailing slash; safe since homedir_len > 0
  if (homedir[homedir_len - 1] == '/')
    homedir_len--;

  size_t file_path_len = strlen("/.cp8.conf");

  size_t config_path_len = homedir_len + file_path_len;
  char *config_path = malloc(config_path_len + 1);
  memcpy(config_path, homedir, homedir_len);
  memcpy(config_path + homedir_len, "/.cp8.conf", file_path_len);
  config_path[config_path_len] = '\0';
  return config_path;
}

static struct config load_config(char *config_path) {
  if (!config_path)
    config_path = get_config_path();

  int config_file = open(config_path, O_RDONLY);
  if (config_file == -1) {
    int err = errno;
    perror(config_path);
    free(config_path);
    exit(err);
  }

  char *buf = malloc(4096);
  size_t buf_len = 0, buf_cap = 4096;
  while (1) {
    ssize_t count = read(config_file, &buf[buf_len], buf_cap - buf_len);
    if (count == -1) {
      int err = errno;
      perror(config_path);
      close(config_file);
      free(config_path);
      exit(err);
    } else if (count == 0) {
      break;
    } else {
      buf_len += count;
      if (buf_len == buf_cap) {
        buf_cap *= 2;
        buf = realloc(buf, buf_cap);
      }
    }
  }

  struct config out = {0};

  if (buf_len == 0)
    return out;
  if (buf[buf_len - 1] != '\n') {
    if (buf_cap == buf_len) {
      buf_cap *= 2;
      buf = realloc(buf, buf_cap);
    }
    buf[buf_len++] = '\n';
  }

  enum state {
    KEY,
    PORT,
    HEADER,
    TEXT,
  } state = KEY;
  char *start = buf;
  for (size_t i = 0; i < buf_len; i++) {
    char ch = buf[i];
    if (ch == '=' && state == KEY) {
      size_t key_len = &buf[i] - start;
      if (key_len == 4 && memcmp(start, "port", 4) == 0) {
        if (out.port) {
          fprintf(stderr, "Duplicate key: `%.*s'\n", (int)key_len, start);
          exit(EINVAL);
        }
        state = PORT;
      } else if (key_len == 6 && memcmp(start, "header", 6) == 0) {
        if (out.header) {
          fprintf(stderr, "Duplicate key: `%.*s'\n", (int)key_len, start);
          exit(EINVAL);
        }
        state = HEADER;
      } else if (key_len == 4 && memcmp(start, "text", 4) == 0) {
        if (out.text) {
          fprintf(stderr, "Duplicate key: `%.*s'\n", (int)key_len, start);
          exit(EINVAL);
        }
        state = TEXT;
      } else {
        fprintf(stderr, "Unknown key: `%.*s'\n", (int)key_len, start);
        exit(EINVAL);
      }
      start = &buf[i + 1];
    } else if (ch == '\n') {
      size_t val_len = &buf[i] - start;

      unsigned long parsed_value;
      char *str;
      switch (state) {
      case KEY:
        fprintf(stderr, "Invalid line: `%.*s'\n", (int)(&buf[i] - start),
                start);
        exit(EINVAL);
        break;

      case PORT:
        buf[i] = '\0';
        parsed_value = strtoul(start, &str, 10);
        if (*str || parsed_value > 0xffff) {
          fprintf(stderr, "Invalid value: `%.*s'\n", (int)(&buf[i] - start),
                  start);
          exit(EINVAL);
        }
        out.port = parsed_value;
        break;

      case HEADER:
      case TEXT:
        str = memcpy(malloc(val_len + 1), start, val_len);
        str[val_len] = '\0';
        if (state == HEADER)
          out.header = str;
        else
          out.text = str;
        break;
      }

      start = &buf[i + 1];
      state = KEY;
    }
  }

  return out;
}

static int setup_sock(const struct config *config) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    int err = errno;
    perror("socket");
    exit(err);
  }

  // needed for testing; we kill the subprocess with a timeout, so the kernel
  // doesn't like to let anyone bind to the socket too soon after...
  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) ==
      -1) {
    int err = errno;
    perror("setsockopt");
    exit(err);
  }

  struct sockaddr_in listen_addr = {0};
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons(config->port);

  if (bind(sock, (const struct sockaddr *)&listen_addr, sizeof(listen_addr))) {
    int err = errno;
    perror("bind");
    exit(err);
  }

  if (listen(sock, 10)) {
    int err = errno;
    perror("listen");
    exit(err);
  }

  return sock;
}
