#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {

  int fd;
  mode_t mode, old;

  struct stat fileStat;

  printf("Old mask is: %i\n", old = umask(S_IRWXG));

  int opt;
  while ((opt = getopt(argc, argv, "rw")) != -1) {
    switch (opt) {
    case 'r':
      mode = S_IRUSR;
      break;
    case 'w':
      mode = S_IWUSR;
      break;
    default:
      fprintf(stderr, "Usage: %s <-r|-w>\n", argv[0]);
      return -1;
    }
  }

  int nurls = argc - optind;

  if ((fd = creat("new.file", mode)) < 0) {
    perror("create() error");
  } else {
    if (fstat(fd, &fileStat) < 0)
      return 1;

    printf("File Permissions: \t");
    printf((S_ISDIR(fileStat.st_mode)) ? "d" : "-");
    printf((fileStat.st_mode & S_IRUSR) ? "r" : "-");
    printf((fileStat.st_mode & S_IWUSR) ? "w" : "-");
    printf((fileStat.st_mode & S_IXUSR) ? "x" : "-");
    printf((fileStat.st_mode & S_IRGRP) ? "r" : "-");
    printf((fileStat.st_mode & S_IWGRP) ? "w" : "-");
    printf((fileStat.st_mode & S_IXGRP) ? "x" : "-");
    printf((fileStat.st_mode & S_IROTH) ? "r" : "-");
    printf((fileStat.st_mode & S_IWOTH) ? "w" : "-");
    printf((fileStat.st_mode & S_IXOTH) ? "x" : "-");
    printf("\n\n");

    close(fd);
    unlink("new.file");
  }
  umask(old);

  return 0;
}
