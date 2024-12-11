#include "types.h"
#include "stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    sh_var_write(0);
    int pid = fork();
    int i, n;
    for (i = 0; i < 10000; i++)
    {
        n = sh_var_read();
        sh_var_write(n + 1);
    }
    if (pid > 0)
    {
        wait(0);
    }
    printf("pid = %d ,sum = %d\n", pid, sh_var_read());
    exit(0);
}