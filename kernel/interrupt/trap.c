#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "proc.h"
#include "file.h"
#include "defs.h"
#include "fcntl.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void usertrap(void)
{
  int which_dev = 0;

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // save user program counter.
  p->trapframe->epc = r_sepc();
  uint64 cause = r_scause();
  if (cause == 8)
  {
    // system call

    if (p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  }
  else if ((which_dev = devintr()) != 0)
  {
    // ok
  }
  else if (cause == 13 || cause == 15)
  {
    uint64 fault_va = r_stval(); // 获取出错的虚拟地址
    if (cowpage(p->pagetable, fault_va) == 0)
    { // 如果是cow页出错
      if (fault_va >= p->sz || cowalloc(p->pagetable, PGROUNDDOWN(fault_va)) == 0)
        p->killed = 1;
    }
    else if (PGROUNDUP(p->trapframe->sp) - 1 < fault_va && fault_va < p->sz && mmap_handler(r_stval(), cause) == 0)
    {
    } //  缺页异常(内存映射文件引起的)
    else
    {           //  缺页异常(懒分配引起的)
      char *pa; // 分配的物理地址
      if (PGROUNDUP(p->trapframe->sp) - 1 < fault_va && fault_va < p->sz && (pa = kalloc()) != 0)
      {
        memset(pa, 0, PGSIZE);
        if (mappages(p->pagetable, PGROUNDDOWN(fault_va), PGSIZE, (uint64)pa, PTE_R | PTE_W | PTE_X | PTE_U) != 0)
        {
          kfree(pa);
          p->killed = 1;
        }
      }
      else if (PGROUNDUP(p->trapframe->sp) - 1 < fault_va && fault_va < p->shm && (pa = kalloc()) != 0) // 共享内存p->shm
      {
        memset(pa, 0, PGSIZE);
        if (mappages(p->pagetable, PGROUNDDOWN(fault_va), PGSIZE, (uint64)pa, PTE_R | PTE_W | PTE_X | PTE_U) != 0)
        {
          kfree(pa);
          p->killed = 1;
        }
      }
      else
      {
        printf("usertrap(): out of memory!\n"); // 已经没有可分配的空闲页面
        p->killed = 1;
      }
    }
  }
  else
  {
    printf("usertrap(): unexpected scause %p pid=%d\n", cause, p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if (p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
  {
    if (p->alarm_interval != 0)
    { // 如果设定了时钟事件
      if (--p->alarm_ticks <= 0)
      { // 时钟倒计时 -1 tick，如果已经到达或超过设定的 tick 数
        if (!p->alarm_goingoff)
        { // 确保没有时钟正在运行
          p->alarm_ticks = p->alarm_interval;
          // jump to execute alarm_handler
          *p->alarm_trapframe = *p->trapframe; // backup trapframe
          p->trapframe->epc = (uint64)p->alarm_handler;
          p->alarm_goingoff = 1;
        }
        // 如果一个时钟到期的时候已经有一个时钟处理函数正在运行，则会推迟到原处理函数运行完成后的下一个 tick 才触发这次时钟
      }
    }
    yield();
  }

  usertrapret();
}

//
// return to user space
//
void usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.

  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr()) == 0)
  {
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);

  // struct proc *p;
  //  // 遍历进程表，更新每个进程的等待时间和CPU时间
  //  for (p = proc; p < &proc[NPROC]; p++)
  //  {

  //   acquire(&p->lock);
  //   if (p->state == RUNNABLE)
  //   {
  //     p->wait_time++; // 增加等待时间
  //     p->dyn_priority = p->priority + (p->wait_time / 5) - (p->cpu_time / 5);
  //     // printf("PID: %d, wait_time: %d, cpu_time: %d, dyn_priority: %d\n", p->pid, p->wait_time, p->cpu_time, p->dyn_priority);
  //   }
  //   else if (p->state == RUNNING)
  //   {
  //     p->cpu_time++; // 增加CPU时间
  //     p->dyn_priority = p->priority + (p->wait_time / 5) - (p->cpu_time / 5);
  //     // printf("PID: %d, wait_time: %d, cpu_time: %d, dyn_priority: %d\n", p->pid, p->wait_time, p->cpu_time, p->dyn_priority);
  //   }
  //   release(&p->lock);
  // }
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr()
{
  uint64 scause = r_scause();

  if ((scause & 0x8000000000000000L) &&
      (scause & 0xff) == 9)
  {
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();
    if (irq == UART0_IRQ)
    {
      uartintr();
    }
    else if (irq == VIRTIO0_IRQ)
    {
      virtio_disk_intr();
    }
    else if (irq == E1000_IRQ)
    {
      e1000_intr();
    }
    else if (irq)
    {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if (irq)
      plic_complete(irq);

    return 1;
  }
  else if (scause == 0x8000000000000001L)
  {
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if (cpuid() == 0)
    {
      clockintr();
    }

    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  }
  else
  {
    return 0;
  }
}
/**
 * @brief mmap_handler 处理mmap惰性分配导致的页面错误
 * @param va 页面故障虚拟地址
 * @param cause 页面故障原因
 * @return 0成功，-1失败
 */
int mmap_handler(int va, int cause)
{
  int i;
  struct proc *p = myproc();
  // 根据地址查找属于哪一个VMA
  for (i = 0; i < NVMA; ++i)
  {
    if (p->vma[i].used && p->vma[i].addr <= va && va <= p->vma[i].addr + p->vma[i].len - 1)
    {
      break;
    }
  }
  if (i == NVMA)
    return -1;

  int pte_flags = PTE_U;
  if (p->vma[i].prot & PROT_READ)
    pte_flags |= PTE_R;
  if (p->vma[i].prot & PROT_WRITE)
    pte_flags |= PTE_W;
  if (p->vma[i].prot & PROT_EXEC)
    pte_flags |= PTE_X;

  struct file *vf = p->vma[i].vfile;
  // 读导致的页面错误
  if (cause == 13 && vf->readable == 0)
    return -1;
  // 写导致的页面错误
  if (cause == 15 && vf->writable == 0)
    return -1;

  void *pa = kalloc();
  if (pa == 0)
    return -1;
  memset(pa, 0, PGSIZE);

  // 读取文件内容
  ilock(vf->ip);
  // 计算当前页面读取文件的偏移量，实验中p->vma[i].offset总是0
  // 要按顺序读读取，例如内存页面A,B和文件块a,b
  // 则A读取a，B读取b，而不能A读取b，B读取a
  int offset = p->vma[i].offset + PGROUNDDOWN(va - p->vma[i].addr);
  int readbytes = readi(vf->ip, 0, (uint64)pa, offset, PGSIZE);
  // 什么都没有读到
  if (readbytes == 0)
  {
    iunlock(vf->ip);
    kfree(pa);
    return -1;
  }
  iunlock(vf->ip);

  // 添加页面映射
  if (mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)pa, pte_flags) != 0)
  {
    kfree(pa);
    return -1;
  }

  return 0;
}
int sigalarm(int ticks, void (*handler)())
{
  // 设置 myproc 中的相关属性
  struct proc *p = myproc();
  p->alarm_interval = ticks;
  p->alarm_handler = handler;
  p->alarm_ticks = ticks;
  return 0;
}

int sigreturn()
{
  // 将 trapframe 恢复到时钟中断之前的状态，恢复原本正在执行的程序流
  struct proc *p = myproc();
  *p->trapframe = *p->alarm_trapframe;
  p->alarm_goingoff = 0;
  return 0;
}