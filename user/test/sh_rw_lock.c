#include "types.h"
#include "stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    sh_var_write(0);
    int id = sem_create(1); // 创建信号量
    int pid = fork();
    int i, n;
    for (i = 0; i < 10000; i++)
    {
        sem_p(id);
        n = sh_var_read();
        sh_var_write(n + 1);
        sem_v(id);
    }
    if (pid > 0)
    {
        wait(0);
        sem_free(id);
    }
    printf("pid = %d ,sum = %d\n", pid, sh_var_read());
    exit(0);

    return 0;
}