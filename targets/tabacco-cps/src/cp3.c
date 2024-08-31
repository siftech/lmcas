#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

__attribute__((noinline, weak)) void _lmcas_neck(void) {}
struct config {
  int foo;
  int bar;
};

void check_home_dir(const char *homedir, int isRoot) {
  struct stat statbuf;
  int status;
  const char *cfgPath = "/.config/cp3/cp3rc";
  int lenHomeDirString = strlen(homedir);
  int lenHomeCfgPath = strlen(cfgPath);
  int lenCombinedPath = lenHomeDirString + 1 + lenHomeCfgPath + 1;
  char *buf = malloc(lenCombinedPath);
  memcpy(buf, homedir, lenHomeDirString);
  memcpy(buf + lenHomeDirString, cfgPath, lenHomeCfgPath);
  buf[lenCombinedPath] = '\0';
  status = stat(buf, &statbuf);
  if (status != -1 && isRoot) {
    fprintf(
        stderr,
        "Note that file %s exists but I am explicitly not reading from it.\n",
        buf);
  }
}

int parse_config_file(char *fname, struct config *init_config, int verbose) {
  FILE *file = fopen(fname, "r");
  struct stat bufstat;
  if (file == NULL) {
    return 0;
  }
  if (verbose) {
    printf("parsing config file @ %s\n", fname);
    if (fstat(fileno(file), &bufstat) != -1) {
      printf("config file has size:%ld\n", bufstat.st_size);
    }
  }
  char *buf = malloc(16);
  size_t len = 16;
  while (getline(&buf, &len, file) != -1) {
    if (sscanf(buf, "foo = %d", &init_config->foo) == 1) {
      if (verbose) {
        printf("foo is now %d\n", init_config->foo);
      }
      continue;
    } else if (sscanf(buf, "bar = %d", &init_config->bar) == 1) {
      if (verbose) {
        printf("bar is now %d\n", init_config->bar);
      }
      continue;
    } else {
      fprintf(stderr, "Unable to recognize line! %s\n", buf);
      fclose(file);
      exit(1);
    }
  }
  fclose(file);
  return 0;
}

int main(int argc, char *argv[]) {

  printf("Entering main.\n");
  struct config *cfg = malloc(sizeof(struct config));
  cfg->foo = 0;
  cfg->bar = 0;
  int verbose = 0;

  int opt;
  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
    case 'v':
      verbose = 1;
      break;
    default:
      fprintf(stderr, "Usage: %s [-v]\n", argv[0]);
      return -1;
    }
  }

  uid_t uid = getuid();
  struct passwd *pw = getpwuid(uid);
  const char *homedir = pw->pw_dir;
  // Running as root
  int is_root = uid == 0;
  char *fname = "/etc/cp3rc";
  check_home_dir(homedir, is_root);
  if (is_root) {
    parse_config_file(fname, cfg, verbose);
  } else {
    const char *cfgPath = "/.config/cp3/cp3rc";
    int lenHomeDirString = strlen(homedir);
    int lenHomeCfgPath = strlen(cfgPath);
    int lenCombinedPath = lenHomeDirString + 1 + lenHomeCfgPath + 1;
    char *buf = malloc(lenCombinedPath);
    memcpy(buf, homedir, lenHomeDirString);
    memcpy(buf + lenHomeDirString, cfgPath, lenHomeCfgPath);
    buf[lenCombinedPath] = '\0';
    parse_config_file(buf, cfg, verbose);
    free(buf);
  }
  _lmcas_neck();

  printf("foo = %d\n", cfg->foo);
  printf("bar = %d\n", cfg->bar);

  return 0;
}
