// Mutual exclusion spin locks.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

int sem_used_count = 0;       // 信号量：当前在用信号量数量
struct sem sems[SEM_MAX_NUM]; // 信号量：系统最多有128个信号量

void initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}
// 信号量：初始化信号量数组
void initsem()
{
  for (int i = 0; i < SEM_MAX_NUM; i++)
  {
    initlock(&sems[i].lock, "semaphore");
    sems[i].allocated = 0;      // 标记为未分配
    sems[i].resource_count = 0; // 初始化资源计数
  }
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void acquire(struct spinlock *lk)
{
  push_off();         // 关中断，避免死锁
  if (holding(lk))    // 检查是否已经持有锁
    panic("acquire"); // 如果已有，则处理错误

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ; // 原子操作

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize(); // 防止编译器优化，确保后续的任何操作都是上锁之后进行

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}

// Release the lock.
void release(struct spinlock *lk)
{
  if (!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked); // 原子操作

  pop_off(); // 开中断
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void push_off(void)
{
  int old = intr_get();   // 获取关中断之前的中断状态
  intr_off();             // 关中断
  if (mycpu()->noff == 0) // 第一次上锁，保存关中断之前的中断状态
    mycpu()->intena = old;
  mycpu()->noff += 1; // 锁的个数增加
}

void pop_off(void)
{
  struct cpu *c = mycpu();
  if (intr_get())
    panic("pop_off - interruptible");
  if (c->noff < 1)
    panic("pop_off");
  c->noff -= 1;                  // 锁的个数减少
  if (c->noff == 0 && c->intena) // 如果当前所有锁已释放而且上锁之前是开中断
    intr_on();
}
