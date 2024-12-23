#include "param.h"
#include "types.h"
#include "stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    // 传递给 execve 的参数
    // char *args[] = {"echo", "Hello", "world", 0};

    // 调用 execve 系统调用，执行 /bin/echo 程序
    printf("%s\n", argv[0]);
    int ret = execve(argv[1], &argv[2], 0);

    // 如果 execve 调用成功，下面的代码不会被执行
    if (ret < 0)
    {
        printf("execve failed!\n");
        exit(1);
    }

    // 代码不会执行到这里，因为 execve 会替换当前进程
    return 0;
}
