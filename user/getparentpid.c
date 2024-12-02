#include "param.h"
#include "types.h"
#include "stat.h"
#include "user/user.h"

int main(void)
{
    int ppid = getparentpid();
    printf("Parent process ID: %d\n", ppid);
    exit(0);
}
