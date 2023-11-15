#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
filter(int* pl)
{
  int nums[35];
  int cnt = 0;
  char buf[1];
  int pr[2];
  pipe(pr);

  for (;;) {
    int r = read(pl[0], buf, sizeof(buf));
    if (r == 0) break;
    nums[cnt++] = buf[0];
  }
  close(pl[0]);

  if (cnt == 0) return;
  int first = nums[0];
  printf("prime %d\n", first);

  for (int i = 0; i < cnt; ++i) {
    if (nums[i] % first != 0) {
      buf[0] = nums[i];
      write(pr[1], buf, sizeof(buf));
    }
  }
  close(pr[1]);

  int pid = fork();
  if (pid == 0) {
    filter(pr);
  }
}

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);

  for (int i = 2; i < 35; ++i) {
    char c = i;
    write(p[1], &c, 1);
  }
  close(p[1]);

  filter(p);

  wait(0);

  exit(0);
}