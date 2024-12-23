#include "types.h"
#include "riscv.h"
#include "defs.h" //存放函数声明
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

int sh_var_for_sem_demo = 0; // 信号量；共享变量

uint64 sys_setPriority(void)
{
  int pid, priority;

  // 从用户栈中读取 pid 和 priority 参数
  if (argint(0, &pid) < 0 || argint(1, &priority) < 0)
  {
    return -1; // 参数读取失败，返回错误
  }

  // 调用 setPriority 函数设置进程优先级
  return setPriority(pid, priority);
}

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;

  struct proc *p = myproc();
  addr = p->sz;
  uint64 sz = p->sz;

  if (n > 0)
  {
    // 懒分配
    p->sz += n;
  }
  else if (sz + n > 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
    p->sz = sz;
  }
  else
  {
    return -1;
  }
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_cps(void)
{
  return cps();
}

uint64 sys_trace(void)
{ // 为当前进程的trace_mask赋值
  int n;
  if (argint(0, &n) < 0)
  { // n赋值为p->trapframe->a0，a0来自于进程用户空间，用与传参
    return -1;
  }
  myproc()->trace_mask = n; // trace_mask保存了a0的信息，用于调试
  return 0;
}

uint64 sys_sysinfo(void)
{
  struct sysinfo info;
  freebytes(&info.freemem);
  procnum(&info.nproc);

  // 获取虚拟地址
  uint64 dstaddr;
  argaddr(0, &dstaddr);

  // 从内核空间拷贝数据到用户空间
  if (copyout(myproc()->pagetable, dstaddr, (char *)&info, sizeof info) < 0)
    return -1;

  return 0;
}

uint64
sys_execve(void)
{
  char path[260], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;
  // 获取路径和参数地址
  if (argstr(0, path, 260) < 0 || argaddr(1, &uargv) < 0)
  {
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  // 设置 argv[0] 为程序的路径
  argv[0] = path;
  // 从用户空间获取其他参数
  for (i = 1;; i++)
  {
    if (i >= NELEM(argv))
      goto bad;
    // 获取每个参数的地址
    if (fetchaddr(uargv + sizeof(uint64) * (i - 1), (uint64 *)&uarg) < 0)
      goto bad;
    // 如果参数为空，结束循环
    if (uarg == 0)
    {
      argv[i] = 0;
      break;
    }
    // 为参数分配内存
    argv[i] = kalloc();
    if (argv[i] == 0)
      goto bad;
    // 获取字符串内容
    if (fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }
  // 打印参数（调试用）
  // for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
  // {
  //   printf("argv[%d]: %s\n", i, argv[i]);
  // }
  // 调用 exec 执行程序
  // printf("%s", path);
  int ret = exec(path, argv);
  // 清理已分配的内存
  for (i = 1; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return ret;
bad:
  // 清理已分配的内存
  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64 sys_getparentpid(void)
{
  struct proc *p = myproc();
  return p->parent->pid;
}

uint64 sys_print_pgtable(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  vmprint(p->pagetable);
  release(&p->lock);
  return 0;
}

// 信号量
int sys_sh_var_read()
{
  return sh_var_for_sem_demo;
}
// 信号量
int sys_sh_var_write()
{
  int n;
  if (argint(0, &n) < 0)
  {
    return -1;
  }
  sh_var_for_sem_demo = n;
  return sh_var_for_sem_demo;
}

// 信号量：创建信号量
int sys_sem_create()
{
  int n_sem, id;
  if (argint(0, &n_sem) < 0 || n_sem <= 0) // 参数必须合法且大于0
  {
    return -1;
  }

  for (id = 0; id < SEM_MAX_NUM; id++)
  {
    acquire(&sems[id].lock);
    if (sems[id].allocated == 0)
    {
      sems[id].allocated = 1;
      sems[id].resource_count = n_sem; // 分配资源
      sem_used_count++;
      printf("创建了 %d sem\n", id);
      release(&sems[id].lock);
      return id; // 返回信号量索引
    }
    release(&sems[id].lock);
  }

  return -1; // 没有可用的信号量
}
// 信号量：释放信号量
int sys_sem_free()
{
  int id;
  if (argint(0, &id) < 0 || id < 0 || id >= SEM_MAX_NUM) // 检查参数范围
  {
    return -1;
  }

  acquire(&sems[id].lock);
  if (sems[id].allocated == 1)
  {
    sems[id].allocated = 0;
    sems[id].resource_count = 0; // 清除资源计数
    sem_used_count--;
    printf("释放 %d sem\n", id);
  }
  release(&sems[id].lock);

  return 0;
}
// 信号量：P操作，获取资源
int sys_sem_p()
{
  int id;
  struct proc *p = myproc();

  if (argint(0, &id) < 0 || id < 0 || id >= SEM_MAX_NUM) // 参数合法性检查
  {
    return -1;
  }

  // printf("sem_p: 尝试获取信号量 id = %d\n", id);

  acquire(&p->lock);       // 获取当前进程的锁
  acquire(&sems[id].lock); // 获取信号量锁

  sems[id].resource_count--;
  if (sems[id].resource_count < 0)
  {
    release(&sems[id].lock);    // 释放信号量锁
    sleep(&sems[id], &p->lock); // 使用进程锁进行休眠
  }
  else
  {
    release(&sems[id].lock); // 如果资源足够，释放信号量锁
  }

  release(&p->lock); // 释放进程锁
  return 0;
}

// 信号量：V操作，释放资源
int sys_sem_v()
{
  int id;
  if (argint(0, &id) < 0 || id < 0 || id >= SEM_MAX_NUM) // 参数合法性检查
  {
    return -1;
  }

  // printf("sem_v: 尝试释放信号量 id = %d\n", id);

  acquire(&sems[id].lock); // 获取信号量锁

  sems[id].resource_count++;
  if (sems[id].resource_count <= 0)
  {
    wakeup(&sems[id]); // 唤醒等待的进程
  }

  release(&sems[id].lock); // 释放信号量锁
  return 0;
}

uint64 sys_shmgetat(void)
{
  int key, num;
  if (argint(0, &key) < 0 || argint(1, &num) < 0)
    return -1;
  return (uint64)shmgetat(key, num);
}

int sys_shmrefcount(void)
{
  int key;
  if (argint(0, &key) < 0)
  {
    return -1;
  }
  return shmrefcount(key);
}
// sysproc.c
uint64 sys_sigalarm(void) {
  int n;
  uint64 fn;
  if(argint(0, &n) < 0)
    return -1;
  if(argaddr(1, &fn) < 0)
    return -1;
  
  return sigalarm(n, (void(*)())(fn));
}

uint64 sys_sigreturn(void) {
	return sigreturn();
}
int sys_clone(void)
{
  uint64 fcn;
  uint64 arg;
  uint64 stack;
  argaddr(0,&fcn);
  argaddr(1,&arg);
  argaddr(2,&stack);
  return clone(fcn,arg,stack);
}
int sys_join(void)
{
  uint64 stackaddr;
  argaddr(0,&stackaddr);
  return join(stackaddr);
}