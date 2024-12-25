#include "types.h"
#include "riscv.h"
#include "sysinfo.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  struct sysinfo info;
  sysinfo(&info);
  printf("freemem:%db,procnum:%d\n",info.freemem,info.nproc);
  exit(0);
}
