#include "types.h"
#include "riscv.h"
#include "sysinfo.h"
#include "user/user.h"

void sinfo(struct sysinfo *info)
{
  if (sysinfo(info) < 0)
  {
    printf("FAIL: sysinfo failed");
    exit(1);
  }
}

void testmem()
{
  struct sysinfo info;
  sinfo(&info);
  printf("freemem:%db\t", info.freemem);
}

void testcall()
{
  struct sysinfo info;

  if (sysinfo(&info) < 0)
  {
    printf("FAIL: sysinfo failed\n");
    exit(1);
  }

  if (sysinfo((struct sysinfo *)0xeaeb0b5b00002f5e) != 0xffffffffffffffff)
  {
    printf("FAIL: sysinfo succeeded with bad argument\n");
    exit(1);
  }
  // printf("testcall OK\n");
}

void testproc()
{
  struct sysinfo info;
  uint64 nproc;
  int status;
  int pid;

  sinfo(&info);
  nproc = info.nproc;

  pid = fork();
  if (pid < 0)
  {
    printf("sysinfotest: fork failed\n");
    exit(1);
  }
  if (pid == 0)
  {
    sinfo(&info);
    if (info.nproc != nproc + 1)
    {
      printf("sysinfotest: FAIL nproc is %d instead of %d\n", info.nproc, nproc + 1);
      exit(1);
    }
    exit(0);
  }
  wait(&status);
  sinfo(&info);
  if (info.nproc != nproc)
  {
    printf("sysinfotest: FAIL nproc is %d instead of %d\n", info.nproc, nproc);
    exit(1);
  }
  printf("nproc:%d\n", info.nproc);
}

int main(int argc, char *argv[])
{
  printf("sysinfotest: start\n");
  testcall();
  testmem();
  testproc();
  printf("sysinfotest: OK\n");
  exit(0);
}
