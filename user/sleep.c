#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: sleep seconds\n");
    exit(1);
  }

  // see kenel/start.c:69
  int sec = atoi(argv[1]);
  int tick = sec * 10;
  sleep(tick);

  exit(0);
}
