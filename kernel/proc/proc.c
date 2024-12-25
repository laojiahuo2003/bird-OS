#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fcntl.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void wakeup1(struct proc *chan);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

int setPriority(int pid, int priority)
{
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      acquire(&p->lock); // 获取进程的锁
      p->priority = priority;
      release(&p->lock); // 修改完成后释放锁
      return 0;
    }
  }
  // 找不到该pid，错误
  return -1; // 这里不需要释放锁，因为从未获取过
}

// initialize the proc table at boot time.
void procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");
    // 为每个进程分配一个内核栈。它将每个栈映射在KSTACK生成的虚拟地址上，这就为栈守护页留下了空间
    //  Allocate a page for the process's kernel stack.
    //  Map it high in memory, followed by an invalid
    //  guard page.
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W); // 将对应的页表项加入内核页表
    p->kstack = va;
  }
  kvminithart(); // 调用kvminithart将内核页表重新加载到satp中
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *allocproc(void)
{
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&p->lock);
    }
  }
  return 0;
found:

  p->pid = allocpid();
  p->mqmask = 0;
  p->priority = 10; // 设定优先级为10
  p->cpu_time = 0;
  p->wait_time = 0;
  p->dyn_priority = 10;
  p->trace_mask = 0; // 设定掩码为0
  p->shm = KERNBASE; // 初始化shm，刚开始shm与kernbase重合
  p->shmkeymask = 0; // 初始化shmkeymask
  // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0)
  {
    release(&p->lock);
    return 0;
  }
  // Allocate a trapframe page for alarm_trapframe.
  if((p->alarm_trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }
  p->alarm_interval = 0;
  p->alarm_handler = 0;
  p->alarm_ticks = 0;
  p->alarm_goingoff = 0;
  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0)
  {
    printf("fuck3\n");
    freeproc(p);
    release(&p->lock);

    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;
  memset(&p->vma, 0, sizeof(p->vma));
  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void freeproc(struct proc *p)
{
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  if(p->alarm_trapframe)
    kfree((void*)p->alarm_trapframe);
  p->alarm_trapframe = 0;
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->alarm_interval = 0;
  p->alarm_handler = 0;
  p->alarm_ticks = 0;
  p->alarm_goingoff = 0;
  p->state = UNUSED;
  // 释放进程
  shmrelease(p->pagetable, p->shm, p->shmkeymask);
  p->shm = KERNBASE;
  p->shmkeymask = 0;
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
               (uint64)trampoline, PTE_R | PTE_X) < 0)
  {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
               (uint64)(p->trapframe), PTE_R | PTE_W) < 0)
  {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// Set up first user process.
void userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;     // user program counter
  p->trapframe->sp = PGSIZE; // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0)
  {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    freeproc(np);
    release(&np->lock);

    return -1;
  }
  // 共享内存区信息复制
  np->shm = proc->shm;
  np->shmkeymask = proc->shmkeymask;
  // printf("shmkeymask:%d\n", np->shmkeymask);
  for (int i = 0; i < 8; ++i)
  {
    if (shmkeyused(i, np->shmkeymask)) // 只复制已启用的共享内存区
    {
      np->shmva[i] = proc->shmva[i];
    }
  }
  shmaddcount(proc->shmkeymask); // fork新进程，所以共享内存引用数量加一

  addmqcount(p->mqmask);  // 消息队列引用数量+1
  np->mqmask = p->mqmask; // 掩码复制
  np->sz = p->sz;

  np->parent = p;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);
  // 复制父进程的VMA
  for (i = 0; i < NVMA; ++i)
  {
    if (p->vma[i].used)
    {
      memmove(&np->vma[i], &p->vma[i], sizeof(p->vma[i]));
      filedup(p->vma[i].vfile);
    }
  }
  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  np->state = RUNNABLE;
  np->trace_mask = p->trace_mask; // 从父进程复制trace mask到子进程
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold p->lock.
void reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++)
  {
    // this code uses pp->parent without holding pp->lock.
    // acquiring the lock first could cause a deadlock
    // if pp or a child of pp were also in exit()
    // and about to try to lock p.
    if (pp->parent == p)
    {
      // pp->parent can't change between the check and the acquire()
      // because only the parent changes it, and we're the parent.
      acquire(&pp->lock);
      pp->parent = initproc;
      // we should wake up init here, but that would require
      // initproc->lock, which would be a deadlock, since we hold
      // the lock on one of init's children (pp). this is why
      // exit() always wakes init (before acquiring any locks).
      release(&pp->lock);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }
  // 将进程的已映射区域取消映射
  for (int i = 0; i < NVMA; ++i)
  {
    if (p->vma[i].used)
    {
      if (p->vma[i].flags == MAP_SHARED && (p->vma[i].prot & PROT_WRITE) != 0)
      {
        filewrite(p->vma[i].vfile, p->vma[i].addr, p->vma[i].len);
      }
      fileclose(p->vma[i].vfile);
      uvmunmap(p->pagetable, p->vma[i].addr, p->vma[i].len / PGSIZE, 1);
      p->vma[i].used = 0;
    }
  }
  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  // we might re-parent a child to init. we can't be precise about
  // waking up init, since we can't acquire its lock once we've
  // acquired any other proc lock. so wake up init whether that's
  // necessary or not. init may miss this wakeup, but that seems
  // harmless.
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);

  // grab a copy of p->parent, to ensure that we unlock the same
  // parent we locked. in case our parent gives us away to init while
  // we're waiting for the parent lock. we may then race with an
  // exiting parent, but the result will be a harmless spurious wakeup
  // to a dead or wrong process; proc structs are never re-allocated
  // as anything else.
  acquire(&p->lock);
  struct proc *original_parent=0;
  if(p->parent==0&&p->pthread!=0)
    original_parent=p->pthread;
  else
    original_parent=p->parent;
  release(&p->lock);

  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acquire(&original_parent->lock);

  acquire(&p->lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup1(original_parent);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&original_parent->lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  // hold p->lock for the whole time to avoid lost
  // wakeups from a child's exit().
  acquire(&p->lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (np = proc; np < &proc[NPROC]; np++)
    {
      // this code uses np->parent without holding np->lock.
      // acquiring the lock first would cause a deadlock,
      // since np might be an ancestor, and we already hold p->lock.
      if (np->parent == p)
      {
        // np->parent can't change between the check and the acquire()
        // because only the parent changes it, and we're the parent.
        acquire(&np->lock);
        havekids = 1;
        if (np->state == ZOMBIE)
        {
          // Found one.
          pid = np->pid;
          releasemq2(p->mqmask);
          p->mqmask = 0;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                   sizeof(np->xstate)) < 0)
          {
            release(&np->lock);
            release(&p->lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&p->lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || p->killed)
    {
      release(&p->lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &p->lock); // DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler2(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for (;;)
  {
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    int found = 0;
    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;

        found = 1;
      }
      release(&p->lock);
    }
    if (found == 0)
    {
      intr_on();
      asm volatile("wfi");
    }
  }
}
void scheduler1(void)
{
  struct proc *p;
  struct proc *pmax = 0; // 优先级最高的进程
  int priority_max;      // 记录最大优先级
  struct cpu *c = mycpu();

  c->proc = 0;

  for (;;)
  {
    // 避免死锁，确保设备中断
    intr_on();

    pmax = 0;
    priority_max = -1;

    // 查找优先级最高的进程并持有其锁
    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE && p->priority > priority_max)
      {
        // 如果找到更高优先级的进程，则释放上一个高优先级进程的锁
        if (pmax != 0)
        {
          release(&pmax->lock);
        }
        pmax = p; // 更新优先级最高的进程
        priority_max = p->priority;
      }
      else
      {
        release(&p->lock); // 非 RUNNABLE 或优先级不够，直接释放锁
      }
    }

    if (pmax != 0)
    {
      // 找到优先级最高的进程，调度它
      pmax->state = RUNNING;
      c->proc = pmax;

      swtch(&c->context, &pmax->context);

      // 进程运行结束后，释放锁
      c->proc = 0;
      release(&pmax->lock);
    }
    else
    {
      // 没有可运行的进程，进入低功耗模式
      intr_on();
      asm volatile("wfi");
    }
  }
}
void scheduler(void)
{
  struct proc *p;
  struct proc *pmax; // 优先级最高的进程
  struct cpu *c = mycpu();
  int priority_max; // 当前最高优先级

  c->proc = 0;

  for (;;)
  {
    intr_on();         // 启用中断
    pmax = 0;          // 重置优先级最高的进程
    priority_max = -1; // 重置最高优先级

    // 遍历进程表
    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        // 更新动态优先级
        p->wait_time++; // 增加等待时间
        p->dyn_priority = p->priority + (p->wait_time / 5) - (p->cpu_time / 5);

        // 限制动态优先级范围在 [0, 20]
        if (p->dyn_priority < 0)
          p->dyn_priority = 0;
        if (p->dyn_priority > 20)
          p->dyn_priority = 20;

        // 找到优先级更高的进程
        if (p->dyn_priority > priority_max)
        {
          priority_max = p->dyn_priority;
          pmax = p;
        }
      }

      release(&p->lock);
    }

    // 如果没有找到 RUNNABLE 的进程，则进入低功耗等待模式
    if (pmax == 0)
    {
      intr_on();
      asm volatile("wfi");
      continue;
    }

    // 调度优先级最高的进程
    acquire(&pmax->lock);
    if (pmax->state == RUNNING)
    {
      pmax->cpu_time = 0;
    }
    if (pmax->state == RUNNABLE) // 再次确认状态
    {
      pmax->state = RUNNING;              // 设置为运行状态
      pmax->wait_time = 0;                // 清零等待时间
      c->proc = pmax;                     // 当前 CPU 正在运行的进程
      swtch(&c->context, &pmax->context); // 切换到目标进程

      // 切换回来时，重置 CPU 的当前运行进程
      c->proc = 0;
    }
    release(&pmax->lock);
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first)
  {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
  if (lk != &p->lock)
  {                    // DOC: sleeplock0
    acquire(&p->lock); // DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &p->lock)
  {
    release(&p->lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
    }
    release(&p->lock);
  }
}

// 只唤醒一个等待资源的进程
void wakeupOneProc(void *chan)
{
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      break; // 多加了这一步
    }
    release(&p->lock);
  }
}

// Wake up p if it is sleeping in wait(); used by exit().
// Caller must hold p->lock.
static void
wakeup1(struct proc *p)
{
  if (!holding(&p->lock))
    panic("wakeup1");
  if (p->chan == p && p->state == SLEEPING)
  {
    p->state = RUNNABLE;
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst)
  {
    return copyout(p->pagetable, dst, src, len);
  }
  else
  {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src)
  {
    return copyin(p->pagetable, dst, src, len);
  }
  else
  {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

int cps(void)
{
  struct proc *p = proc; // 定义一个结构体(进程控制块)
  // stati();	// 中断
  printf("name \t pid \t state \t \t priority \tdyn_priority\n"); // 罗列所有的pid
  for (p = proc; p < &proc[NPROC]; p++)                           // NPROC为64
  {
    if (p->state == SLEEPING) // 睡眠
      printf("%s \t %d \t SLEEPING \t %d\t \t%d\n", p->name, p->pid, p->priority, p->dyn_priority);
    else if (p->state == RUNNING) // 正在执行
      printf("%s \t %d \t RUNNING \t %d\t \t%d\n", p->name, p->pid, p->priority, p->dyn_priority);
    else if (p->state == RUNNABLE) // 可运行队列
      printf("%s \t %d \t RUNNABLE \t %d\t \t%d\n", p->name, p->pid, p->priority, p->dyn_priority);
  }
  return 22; // 返回22
}

void procnum(uint64 *dst) // 获取进程数
{
  *dst = 0;
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state != UNUSED)
      (*dst)++;
  }
}
int clone(uint64 fcn, uint64 arg, uint64 stack)
{
  struct proc *curproc = myproc();  // 获取当前进程的结构体指针
  struct proc *np = 0;              // 创建新的进程指针
  // 分配新的进程结构体，如果失败则返回-1
  if ((np = allocproc()) == 0) return -1;
  np->pagetable = curproc->pagetable;   // 子进程继承父进程的页表
  np->sz = curproc->sz;                 // 子进程继承父进程的地址空间大小
  np->pthread = curproc;                // 子进程关联到父进程，表示它是一个线程
  np->ustack = (void *)stack;           // 设置子进程的用户堆栈地址
  np->parent = 0;                       // 目前没有父进程（这里应该指向父进程，可能是逻辑问题）
  // 复制父进程的 trapframe（陷阱帧），包含上下文信息（如寄存器值）
  *np->trapframe = *curproc->trapframe;
  // 为新的进程分配内核栈空间
  void *stackin = kalloc();
  // 设置栈指针（从栈顶开始，倒推16字节位置）
  uint64 *sp = stackin + 4096 - 16;
  // 在内核栈中伪造现场，假装子进程的返回地址是 fcn 函数，并将堆栈指针指向线程栈
  np->trapframe->epc = fcn;             // 设置程序计数器为目标函数地址
  np->trapframe->sp = stack + 4096 - 16; // 设置栈指针为线程栈的起始位置
  np->trapframe->s0 = stack + 4096 - 16; // 设置帧指针为线程栈的起始位置
  np->trapframe->a0 = 0;                // 设置返回值寄存器为 0
  // 在内核栈中伪造函数参数
  *(sp + 1) = arg;  // 将参数 `arg` 存入栈中
  *sp = 0xffffffffffffffff;  // 设置一个伪造的返回地址，表示函数调用的结束
  // 将内核栈的内容复制到用户栈中
  copyout(curproc->pagetable, stack, stackin, PGSIZE);
  // 复制文件描述符：子进程继承父进程的打开文件
  for (int i = 0; i < NOFILE; i++) {
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  }
  // 复制父进程的当前工作目录
  np->cwd = idup(curproc->cwd);
  // 复制父进程的进程名
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  // 释放新进程的锁
  release(&np->lock);
  int pid = np->pid;  // 获取新进程的 PID
  // 重新获取新进程的锁并将状态设置为 RUNNABLE，表示它可以被调度
  acquire(&np->lock);
  np->state = RUNNABLE;
  // 释放进程锁
  release(&np->lock);
  return pid;  // 返回新进程的 PID
}

int join(uint64 stackaddrout)
{
  uint64 stackaddrin;  // 用于存储子进程的堆栈地址
  struct proc *curproc = myproc();  // 获取当前进程
  struct proc *p;
  int havekids;
  acquire(&curproc->lock);  // 获取当前进程的锁，防止其状态被修改
  //循环等待子进程结束
  for(;;)
  {
    havekids = 0;  // 重置是否有子进程的标志
    for(p = proc; p < &proc[NPROC]; p++)  // 遍历所有进程
    {
      if(p->pthread == curproc)  // 判断该进程是否为当前进程的子进程（线程）
      {
        acquire(&p->lock);  // 获取子进程的锁
        havekids = 1;  // 当前进程有子进程
        if(p->state == ZOMBIE)  // 如果子进程已经结束
        {
          stackaddrin = (uint64)p->ustack;  // 获取子进程的堆栈地址
          int pid = p->pid;  // 获取子进程的PID
          // 释放子进程的内核栈
          kfree((void*)p->kstack);
          p->kstack = 0;
          // 重置子进程的状态和相关字段
          p->state = UNUSED;
          p->pid = 0;
          p->parent = 0;
          p->pthread = 0;
          p->name[0] = 0;
          p->killed = 0;
          // 将子进程的堆栈地址返回给父进程
          copyout(p->pagetable, stackaddrout, (char*)&stackaddrin, 8);
          release(&p->lock);  // 释放子进程的锁
          release(&curproc->lock);  // 释放当前进程的锁
          return pid;  // 返回子进程的PID，表示成功回收
        }
        release(&p->lock);  // 如果子进程不是ZOMBIE，释放子进程锁
      }
    }
    // 如果没有子进程或者当前进程已被杀死，返回失败
    if(!havekids || curproc->killed) {
      release(&curproc->lock);
      return -1;
    }
    // 如果有子进程但未结束，进入休眠
    sleep(curproc, &curproc->lock);
  }
  return 0;  // 这是一个永远不会到达的地方
}