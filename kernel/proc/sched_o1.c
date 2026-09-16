// sched_o1.c —— O(1) 多级就绪队列（参考 Linux O(1) 调度器思想的简化实现）
//
// 动态优先级 dyn_priority 的取值范围为 [0, 20]，对应 RUNQ_LEN=21 个优先级桶。
// 每个桶是一个双向链表；runq_bitmap 的第 i 位表示第 i 个桶是否非空。
// 调度决策时，用 ctz（count trailing zeros）位运算在常数时间内找到
// 最高优先级非空桶，并取出队头进程 —— 进程选择的时间复杂度为 O(1)，
// 且单次决策只需获取 1 次就绪队列锁（原 O(n) 调度器每次需遍历 NPROC=64
// 个进程并逐一获取进程锁）。
//
// 锁协议：就绪队列操作须持有 runq_lock；凡同时修改进程状态的调用点
// 还应持有 p->lock，并保持「先 p->lock、后 runq_lock」的一致加锁顺序，
// 避免死锁。

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define RUNQ_LEN 21 // 动态优先级 0..20，共 21 个优先级桶

struct spinlock runq_lock;
struct proc *runq_head[RUNQ_LEN];
struct proc *runq_tail[RUNQ_LEN];
uint64 runq_bitmap;

// 常数时间返回 x 最低位 1 的位置。
// 使用 de Bruijn 序列算法，仅依赖移位与乘法，不依赖硬件指令或 libgcc，
// 适合 freestanding 内核环境。
static int ctz64(uint64 x)
{
  static const unsigned char tbl[64] = {
      0,  1,  48, 2,  57, 49, 28, 3,  61, 58, 50, 42, 38, 29, 17, 4,
      62, 55, 59, 36, 53, 51, 43, 22, 45, 39, 33, 30, 24, 18, 12, 5,
      63, 47, 56, 27, 60, 41, 37, 16, 54, 35, 52, 21, 44, 32, 23, 11,
      46, 26, 40, 15, 34, 20, 31, 10, 25, 14, 19, 9,  13, 8,  7,  6};
  return tbl[((uint64)((x & -x) * 0x03f79d71b4cb0a89UL)) >> 58];
}

// 经典 aging 公式：dyn = base_priority + wait_time/5 - cpu_time/5，限制在 [0,20]
int runq_dyn(struct proc *p)
{
  int d = p->priority + p->wait_time / 5 - p->cpu_time / 5;
  if (d < 0)
    d = 0;
  if (d > RUNQ_LEN - 1)
    d = RUNQ_LEN - 1;
  return d;
}

// 初始化就绪队列（procinit 调用）
void runq_init(void)
{
  initlock(&runq_lock, "runq");
  for (int i = 0; i < RUNQ_LEN; i++)
    runq_head[i] = runq_tail[i] = 0;
  runq_bitmap = 0;
}

// 入队（队尾插入）。须持有 runq_lock；建议同时持有 p->lock。
void runq_enqueue(struct proc *p)
{
  if (p->on_runq)
    return; // 防御：已在队列中
  int i = runq_dyn(p);
  p->dyn_priority = i;
  p->run_next = 0;
  p->run_prev = runq_tail[i];
  if (runq_tail[i])
    runq_tail[i]->run_next = p;
  else
    runq_head[i] = p;
  runq_tail[i] = p;
  runq_bitmap |= (1UL << i);
  p->on_runq = 1;
}

// 出队。须持有 runq_lock。
void runq_remove(struct proc *p)
{
  if (!p->on_runq)
    return;
  int i = p->dyn_priority;
  struct proc *nx = p->run_next;
  struct proc *pr = p->run_prev;
  if (pr)
    pr->run_next = nx;
  else
    runq_head[i] = nx;
  if (nx)
    nx->run_prev = pr;
  else
    runq_tail[i] = pr;
  p->run_next = p->run_prev = 0;
  p->on_runq = 0;
  if (runq_head[i] == 0)
    runq_bitmap &= ~(1UL << i);
}

// O(1) 选取：位图定位最高优先级非空桶，取出队头。须持有 runq_lock。
struct proc *runq_pick(void)
{
  if (runq_bitmap == 0)
    return 0;
  struct proc *p = runq_head[ctz64(runq_bitmap)];
  runq_remove(p);
  return p;
}

// 每个时钟节拍调用一次（约 100Hz）：
// 1) 就绪队列中所有进程等待时间 +1（老化，防止低优先级进程饥饿）；
// 2) 动态优先级改变后，将进程搬移到正确的优先级桶；
// 3) 当前 CPU 上运行进程的 CPU 时间 +1（用于优先级衰减）。
// 该函数运行在时钟中断上下文，O(n) 扫描的开销可忽略，
// 且不在调度决策的关键路径上（决策本身是 O(1)）。
void sched_tick(void)
{
  acquire(&runq_lock);

  // 1) aging
  for (int i = 0; i < RUNQ_LEN; i++)
    for (struct proc *p = runq_head[i]; p != 0; p = p->run_next)
      p->wait_time++;

  // 2) 重新分桶
  for (int i = 0; i < RUNQ_LEN; i++)
  {
    struct proc *p = runq_head[i];
    while (p != 0)
    {
      struct proc *nx = p->run_next;
      if (runq_dyn(p) != i)
      {
        runq_remove(p);
        runq_enqueue(p);
      }
      p = nx;
    }
  }
  release(&runq_lock);

  // 3) 当前运行进程 CPU 时间 +1
  struct proc *r = mycpu()->proc;
  if (r != 0)
  {
    acquire(&r->lock);
    r->cpu_time++;
    release(&r->lock);
  }
}
