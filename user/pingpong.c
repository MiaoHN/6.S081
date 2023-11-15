#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];
  char byte = 'a';
  pipe(p);

  if (fork() == 0) {
    // child
    int pid = getpid();

    read(p[0], &byte, 1);
    printf("%d: received ping\n", pid);
    close(p[0]);
    write(p[1], &byte, 1);
    close(p[1]);
  } else {
    // parent
    int pid = getpid();

    write(p[1], &byte, 1);
    close(p[1]);
    read(p[0], &byte, 1);
    printf("%d: received pong\n", pid);
    close(p[0]);

  }

  exit(0);
}