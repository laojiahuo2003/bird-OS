#include "types.h"
#include "stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        // 如果命令行参数不正确，打印帮助信息
        printf("Usage: setpriority_test pid priority\n");
        exit(1);
    }

    int pid = atoi(argv[1]);      // 获取 PID 参数
    int priority = atoi(argv[2]); // 获取优先级参数

    // 调用 sys_setPriority 系统调用
    int result = setPriority(pid, priority);

    if (result == 0)
    {
        printf("Successfully set priority of process %d to %d\n", pid, priority);
    }
    else
    {
        printf("Failed to set priority of process %d\n", pid);
    }

    exit(0);
}
