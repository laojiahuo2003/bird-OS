#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Fetch the uint64 at addr from the current process.
// 实现安全的参数传递机制
int fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if (addr >= p->sz || addr + sizeof(uint64) > p->sz)
    return -1;
  if (copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
// 实现安全的参数传递机制
int fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  
  int err = copyinstr(p->pagetable, buf, addr, max);
  if (err < 0)
    return err;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n)
  {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
int argint(int n, int *ip)
{
  *ip = argraw(n);
  return 0;
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  struct proc *p = myproc();

  // 处理向系统调用传入lazy allocation地址的情况
  if (walkaddr(p->pagetable, *ip) == 0)
  {
    if (PGROUNDUP(p->trapframe->sp) - 1 < *ip && *ip < p->sz)
    {
      char *pa = kalloc();
      if (pa == 0)
        return -1;
      memset(pa, 0, PGSIZE);

      if (mappages(p->pagetable, PGROUNDDOWN(*ip), PGSIZE, (uint64)pa, PTE_R | PTE_W | PTE_X | PTE_U) != 0)
      {
        kfree(pa);
        return -1;
      }
    }
    else
    {
      return -1;
    }
  }
  return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int argstr(int n, char *buf, int max)
{
  uint64 addr;
  if (argaddr(n, &addr) < 0)
    return -1;
  
  return fetchstr(addr, buf, max);
}

extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_mknod(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_unlink(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);
extern uint64 sys_cps(void);
extern uint64 sys_trace(void);
extern uint64 sys_sysinfo(void);
extern uint64 sys_setPriority(void);
extern uint64 sys_execve(void);
extern uint64 sys_getparentpid(void);
extern uint64 sys_print_pgtable(void);
extern uint64 sys_mmap(void);
extern uint64 sys_munmap(void);
extern uint64 sys_sh_var_read(void);  // 信号量
extern uint64 sys_sh_var_write(void); // 信号量
extern uint64 sys_sem_create(void);   // 信号量
extern uint64 sys_sem_free(void);     // 信号量
extern uint64 sys_sem_p(void);        // 信号量
extern uint64 sys_sem_v(void);        // 信号量
extern uint64 sys_symlink(void);
extern uint64 sys_mkf(void);
extern uint64 sys_shmgetat(void);    // 共享内存
extern uint64 sys_shmrefcount(void); // 共享内存
extern uint64 sys_mqget(void);
extern uint64 sys_msgsnd(void);
extern uint64 sys_msgrcv(void);
extern uint64 sys_getcwd(void);
extern uint64 sys_dup_new(void);
extern uint64 sys_shmgetat(void);     // 共享内存
extern uint64 sys_shmrefcount(void);  // 共享内存
extern uint64 sys_sigalarm(void);
extern uint64 sys_sigreturn(void);
extern uint64 sys_connect(void);
extern uint64 sys_chmod(void);
extern uint64 sys_geti(void);
extern uint64 sys_recoveri(void);
extern uint64 sys_clone(void);
extern uint64 sys_join(void);
static uint64 (*syscalls[])(void) = {
    [SYS_fork] sys_fork,
    [SYS_exit] sys_exit,
    [SYS_wait] sys_wait,
    [SYS_pipe] sys_pipe,
    [SYS_read] sys_read,
    [SYS_kill] sys_kill,
    [SYS_exec] sys_exec,
    [SYS_fstat] sys_fstat,
    [SYS_chdir] sys_chdir,
    [SYS_dup] sys_dup,
    [SYS_getpid] sys_getpid,
    [SYS_sbrk] sys_sbrk,
    [SYS_sleep] sys_sleep,
    [SYS_uptime] sys_uptime,
    [SYS_open] sys_open,
    [SYS_write] sys_write,
    [SYS_mknod] sys_mknod,
    [SYS_unlink] sys_unlink,
    [SYS_link] sys_link,
    [SYS_mkdir] sys_mkdir,
    [SYS_close] sys_close,
    [SYS_cps] sys_cps,
    [SYS_trace] sys_trace,
    [SYS_sysinfo] sys_sysinfo,
    [SYS_setPriority] sys_setPriority,
    [SYS_execve] sys_execve,
    [SYS_getparentpid] sys_getparentpid,
    [SYS_print_pgtable] sys_print_pgtable,
    [SYS_mmap] sys_mmap,
    [SYS_munmap] sys_munmap,
    [SYS_sh_var_read] sys_sh_var_read,   // 信号量
    [SYS_sh_var_write] sys_sh_var_write, // 信号量
    [SYS_sem_create] sys_sem_create,
    [SYS_sem_free] sys_sem_free,
    [SYS_sem_p] sys_sem_p,
    [SYS_sem_v] sys_sem_v,
    [SYS_symlink] sys_symlink,
    [SYS_mkf] sys_mkf,
    [SYS_shmgetat] sys_shmgetat,
    [SYS_shmrefcount] sys_shmrefcount,
    [SYS_getcwd] sys_getcwd,
    [SYS_dup_new] sys_dup_new,
    [SYS_sigalarm] sys_sigalarm,
    [SYS_sigreturn] sys_sigreturn,
    [SYS_connect] sys_connect,
    [SYS_mqget] sys_mqget,
    [SYS_msgsnd] sys_msgsnd,
    [SYS_msgrcv] sys_msgrcv,
    [SYS_chmod] sys_chmod,
    [SYS_geti] sys_geti,
    [SYS_recoveri] sys_recoveri,
    [SYS_clone] sys_clone,
    [SYS_join] sys_join,
}; // 这些索引会从1开始，不是从0开始
static char *syscall_names[] = {
    [SYS_fork] "fork",
    [SYS_exit] "exit",
    [SYS_wait] "wait",
    [SYS_pipe] "pipe",
    [SYS_read] "read",
    [SYS_kill] "kill",
    [SYS_exec] "exec",
    [SYS_fstat] "fstat",
    [SYS_chdir] "chdir",
    [SYS_dup] "dup",
    [SYS_getpid] "getpid",
    [SYS_sbrk] "sbrk",
    [SYS_sleep] "sleep",
    [SYS_uptime] "uptime",
    [SYS_open] "open",
    [SYS_write] "write",
    [SYS_mknod] "mknod",
    [SYS_unlink] "unlink",
    [SYS_link] "link",
    [SYS_mkdir] "mkdir",
    [SYS_close] "close",
    [SYS_cps] "sys_cps",
    [SYS_trace] "trace",
    [SYS_sysinfo] "sys_sysinfo",
    [SYS_setPriority] "setPriority",
    [SYS_execve] "sys_execve",
    [SYS_getparentpid] "sys_getparentpid",
    [SYS_print_pgtable] "sys_print_pgtable",
    [SYS_mmap] "sys_mmap",
    [SYS_munmap] "sys_munmap",
    [SYS_sh_var_read] "sys_sh_var_read",   // 信号量
    [SYS_sh_var_write] "sys_sh_var_write", // 信号量
    [SYS_sem_create] "sys_sem_create",
    [SYS_sem_free] "sys_sem_free",
    [SYS_sem_p] "sys_sem_p",
    [SYS_sem_v] "sys_sem_v",
    [SYS_symlink] "sys_symlink",
    [SYS_mkf] "sys_mkf",
    [SYS_shmgetat] "sys_shmgetat",
    [SYS_shmrefcount] "sys_shmrefcount",
    [SYS_getcwd] "sys_getcwd",
    [SYS_dup_new] "sys_dup_new",
    [SYS_sigalarm] "sys_sigalarm",
    [SYS_sigreturn] "sys_sigreturn",
    [SYS_connect] "sys_connect",
    [SYS_mqget] "sys_mqget",
    [SYS_msgsnd] "sys_msgsnd",
    [SYS_msgrcv] "sys_msgrcv",
    [SYS_chmod] "sys_chmod",
    [SYS_geti] "sys_geti",
    [SYS_recoveri] "sys_recoveri",
    [SYS_clone] "sys_clone",
    [SYS_join] "sys_join",
};
void syscall(void) // 在usys.s中系统调用的参数放在a0与a1中，系统调用号放在a7
{
  int num;
  struct proc *p = myproc();
  char *syscall_name;
  num = p->trapframe->a7; // 从当前进程的trampoline页中的a7中得到系统调用号
  if (num > 0 && num < NELEM(syscalls) && syscalls[num])
  {
    p->trapframe->a0 = syscalls[num](); // 执行相应的系统调用函数并将返回值会存储在p->trapframe->a0中
    if ((p->trace_mask & (1 << num)) != 0)
    {
      syscall_name = syscall_names[num];
      printf("%d: syscall %s -> %d", p->pid, syscall_name, p->trapframe->a0);
    }
  }
  else
  {
    printf("%d %s: unknown sys call %d\n",
           p->pid, p->name, num);
    p->trapframe->a0 = -1; // 系统调用成功返回0或正数，返回负数表示错误。
  }
}
