#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/stat.h"
#include "user/user.h"

void
run(int argc, char *argv[], char* left)
{
  argv[argc++] = left;
  argv[argc] = 0;
  int pid = fork();
  if (pid == 0) {
    exec(argv[1], argv + 1);
  }
  wait(0);
}

int
main(int argc, char *argv[])
{
  char buf[100];
  int r;
  int len = 0;

  while ((r = read(0, buf + len, sizeof(buf + len))) != 0) {
    len += r;
  }
  buf[len] = 0;

  char *p = buf;
  for (int i = 0; i < len; ++i) {
    if (buf[i] == '\n' || buf[i] == ' ') {
      buf[i] = 0;
      run(argc, argv, p);
      p = buf + i + 1;
    }
  }

  exit(0);
}
