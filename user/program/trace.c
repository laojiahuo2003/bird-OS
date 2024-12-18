#include "param.h"
#include "types.h"
#include "stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  if (argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9'))
  {
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  if (trace(atoi(argv[1])) < 0)
  { // 参数用trapframe传入内核态
    /*
    (trace函数已添加到 user/user.h, 对此用户程序可见)
    所有的系统调用都首先经过 kernel/syscall.c 的 syscall()【系统调用入口函数】,
    此处调用trace系统调用, 转到 syscall(), 后转到kernel/sysproc.c 的sys_trace()函数,
    接收“掩码”并设置状态到当前进程的数据结构中,syscall()返回;
    因此在之后的系统调用发生时, 在syscall()函数中系统调用返回后,
    根据进程传入的参数“掩码”和系统调用号关系, 判断系统调用是否应该被跟踪并输出.

    需要注意, 要求满足子进程的系统调用依然被跟踪, 对于 kernel/proc.c 的fork()函数,
    子进程继承(复制)父进程的“掩码”.
    */
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }

  for (i = 2; i < argc && i < MAXARG; i++)
  {
    nargv[i - 2] = argv[i];
  }
  exec(nargv[0], nargv);
  exit(0);
}
