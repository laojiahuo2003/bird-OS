#define SEM_MAX_NUM 128 // 信号量：总量
// Mutual exclusion lock.
struct spinlock
{
  uint locked; // Is the lock held?

  // For debugging:
  char *name;      // Name of lock.
  struct cpu *cpu; // The cpu holding the lock.
};
extern int sh_var_for_sem_demo; // 信号量；共享变量
extern int sem_used_count;      // 信号量：当前在用信号量数量
struct sem
{
  struct spinlock lock; // 内核自旋锁
  int resource_count;   // 资源计数
  int allocated;        // 是否被分配 1已分配，0未分配
};
extern struct sem sems[SEM_MAX_NUM]; // 系统最多有128个信号量