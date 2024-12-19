#include "param.h"
#include "fcntl.h"
#include "types.h"
#include "stat.h"
#include "riscv.h"
#include "fs.h"
#include "user/user.h"

struct msg
{
    long type; // 使用 long 类型来符合消息队列要求
    char *dataaddr;
} s1, s2, g;

void msg_test()
{
    // 创建消息队列
    int mqid = mqget(123);

    int pid = fork();
    if (pid == 0)
    { // 子进程
        s1.type = 1;
        s1.dataaddr = "This is the first message!";
        msgsnd(mqid, &s1, 27); // 发送消息1

        s1.type = 2;
        s1.dataaddr = "Hello, another message comes!";
        msgsnd(mqid, &s1, 30); // 发送消息2

        s1.type = 3;
        s1.dataaddr = "This is the third message, and this message has great characters!";
        msgsnd(mqid, &s1, 70); // 发送消息3

        printf("All messages have been sent.\n");
        exit(0); // 子进程结束
    }
    else if (pid > 0)
    {              // 父进程
        sleep(10); // sleep 保证子进程消息写入之后才读入

        g.dataaddr = malloc(70);

        g.type = 2;
        msgrcv(mqid, &g, 30); // 读入消息2
        printf("Received the %ldth message: Hello, another message comes!\n", g.type);

        g.type = 1;
        msgrcv(mqid, &g, 27); // 读入消息1
        printf("Received the %ldth message: This is the first message!\n", g.type, g.dataaddr);

        g.type = 3;
        msgrcv(mqid, &g, 70); // 读入消息3
        printf("Received the %ldth message: This is the third message, and this message has great characters!\n", g.type, g.dataaddr);

        wait(0); // 等待子进程结束
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    msg_test();
    return 0;
}
