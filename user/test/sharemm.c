#include "types.h"
#include "stat.h"
#include "user/user.h"
#include "fs.h"

int main(void)
{
    char *shm;
    int pid = fork(); // 创建子进程

    if (pid == 0)
    {                                 // 子进程
        sleep(3);                     // 子进程等待父进程映射共享内存
        shm = (char *)shmgetat(1, 2); // key为1，大小为3页的共享内存
        printf("子进程 PID: %d，共享内存内容: %s，共享内存引用计数: %d\n", getpid(), shm, shmrefcount(1));
        strcpy(shm, "hello_world!"); // 向共享内存写入数据
        printf("子进程 shm%d", shm);
        printf("子进程 PID: %d，向共享内存写入内容: %s\n", getpid(), shm);
    }
    else if (pid > 0)
    {                                 // 父进程
        shm = (char *)shmgetat(1, 2); // key为1，大小为3页的共享内存
        printf("父进程 PID: %d，调用 wait() 前，共享内存内容: %s，共享内存引用计数: %d\n", getpid(), shm, shmrefcount(1));
        strcpy(shm, "share_memory!"); // 向共享内存写入数据
        printf("父进程 PID: %d，向共享内存写入内容: %s\n", getpid(), shm);

        wait(0); // 等待子进程结束
        printf("父进程 shm%d", shm);
        // 检查共享内存中的内容
        printf("父进程 PID: %d，调用 wait() 后，共享内存内容: hello_world!，共享内存引用计数: %d\n", getpid(), shmrefcount(1));
    }

    exit(0);
}
