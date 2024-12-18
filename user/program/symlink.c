#include "param.h"
#include "types.h"
#include "stat.h"
#include "riscv.h"
#include "fcntl.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "user/user.h"
int main(int argc, char *argv[])
{
    int i=symlink(argv[1],argv[2]);
    if(i) {printf("Failed to link"); exit(1);}
    else exit(0);
}
