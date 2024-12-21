// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "user/user.h"
#include "fcntl.h"

char *argv[] = {"sh", 0};

int main(void)
{
  int pid, wpid;

  if (open("console", O_RDWR) < 0)
  {
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0); // stdout
  dup(0); // stderr

  for (;;)
  {
    printf("\033[1;32minit: starting sh\033[0m\n");
    pid = fork();
    if (pid < 0)
    {
      printf("init: fork failed\n");
      exit(1);
    }
    if (pid == 0)
    {
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }

    for (;;)
    {
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      wpid = wait((int *)0);
      if (wpid == pid)
      {
        // the shell exited; restart it.
        break;
      }
      else if (wpid < 0)
      {
        printf("init: wait returned an error\n");
        exit(1);
      }
      else
      {
        // it was a parentless process; do nothing.
      }
    }
  }
}
