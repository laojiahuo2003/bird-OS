#include "types.h"
#include "stat.h"
#include "user/user.h"

void handler()
{
    printf("Alarm triggered!\n");
    sigreturn();
}

int main()
{
    printf("Starting alarm test...\n");

    // 注册 sigalarm，每隔 2 个时钟周期触发 handler
    sigalarm(2, handler); // 确保传递函数地址

    // 模拟一些长时间运行的工作
    int i = 0;
    while (i < 5)
    {
        printf("Working... %d\n", i);
        sleep(50); // 每次睡眠 50 时钟周期
        i++;
    }

    printf("Alarm test completed!\n");
    exit(0);
}