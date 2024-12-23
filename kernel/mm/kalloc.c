// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
struct ref_stru
{
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE]; // 引用计数
} ref;
struct run
{
  struct run *next;
};
struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU]; // 每个CPU对应一个kmem

void kinit()
{
  char lockname[7] = "kmem_"; // 为每个CPU分配的kmem锁分配一个name
  initlock(&ref.lock, "ref");
  for (int i = 0; i < NCPU; i++)
  {
    lockname[5] = '0' + i;
    lockname[6] = '\0';
    initlock(&kmem[i].lock, lockname); // 初始化锁
  }
  // end()表示是内核区域后第一个可用的地址，(void*)PHYSTOP表示的是物理地址的结束地址
  freerange(end, (void *)PHYSTOP);
  //  将第一个可用的内存到最后一个可用的内存分成一页一页的
  // 并将这些页添加到空闲页链表中
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
    // 在kfree中将会对cnt[]减1，这里要先设为1，否则就会减成负数
    ref.cnt[(uint64)p / PGSIZE] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;
  // printf("%d", (uint64)pa % PGSIZE);
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  // 只有当引用计数为0了才回收空间
  // 否则只是将引用计数减1
  acquire(&ref.lock);
  if (--ref.cnt[(uint64)pa / PGSIZE] == 0)
  {
    release(&ref.lock);
    r = (struct run *)pa;
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    // 使用cpuid()和它返回的结果时必须关中断
    push_off();
    int id = cpuid();
    acquire(&kmem[id].lock);
    r->next = kmem[id].freelist;
    kmem[id].freelist = r;
    release(&kmem[id].lock);
    pop_off();
  }
  else
  {
    release(&ref.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void)
{
  struct run *r;
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if (r)
  { // 当前CPU的空闲列表有空闲内存就分配
    kmem[id].freelist = r->next;
    acquire(&ref.lock);
    ref.cnt[(uint64)r / PGSIZE] = 1; // 将引用计数初始化为1
    release(&ref.lock);
  }
  else
  { // 当前CPU的空闲列表没有可分配内存时窃取其他内存的
    int antid;
    // 遍历所有CPU的空闲列表
    for (antid = 0; antid < NCPU; antid++)
    {
      if (antid == id)
        continue;
      acquire(&kmem[antid].lock);
      r = kmem[antid].freelist;
      if (r)
      {
        kmem[antid].freelist = r->next;
        acquire(&ref.lock);
        ref.cnt[(uint64)r / PGSIZE] = 1;
        release(&ref.lock);
        release(&kmem[antid].lock);
        break;
      }
      release(&kmem[antid].lock);
    }
  }
  release(&kmem[id].lock);
  pop_off();
  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

void freebytes(uint64 *dst) // 获取空闲内存量
{
  *dst = 0;
  for (int i = 0; i < 8; i++)
  {
    struct run *p = kmem[i].freelist; // 用于遍历
    acquire(&kmem[i].lock);
    while (p)
    {
      *dst += PGSIZE;
      p = p->next;
    }
    release(&kmem[i].lock);
  }
}

/**
 * cowpage 判断一个页面是否为COW页面
 * pagetable 指定查询的页表
 * va 虚拟地址
 * 0 是 -1 不是
 */
int cowpage(pagetable_t pagetable, uint64 va)
{
  if (va >= MAXVA)
    return -1;
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    return -1;
  if ((*pte & PTE_V) == 0)
    return -1;
  return (*pte & PTE_F ? 0 : -1);
}

/**
 * cowalloc copy-on-write分配器
 * pagetable 指定页表
 * va 指定的虚拟地址,必须页面对齐
 * 分配后va对应的物理地址，如果返回0则分配失败
 */
void *cowalloc(pagetable_t pagetable, uint64 va)
{
  if (va % PGSIZE != 0)
    return 0;

  uint64 pa = walkaddr(pagetable, va); // 获取对应的物理地址
  if (pa == 0)
    return 0;

  pte_t *pte = walk(pagetable, va, 0); // 获取对应的PTE

  if (krefcnt((char *)pa) == 1)
  {
    // 只剩一个进程对此物理地址存在引用
    // 则直接修改对应的PTE即可
    *pte |= PTE_W;
    *pte &= ~PTE_F;
    return (void *)pa;
  }
  else
  {
    // 多个进程对物理内存存在引用
    // 需要分配新的页面，并拷贝旧页面的内容
    char *mem = kalloc();
    if (mem == 0)
      return 0;

    // 复制旧页面内容到新页
    memmove(mem, (char *)pa, PGSIZE);

    // 清除PTE_V，否则在mappagges中会判定为remap
    *pte &= ~PTE_V;

    // 为新页面添加映射
    if (mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_F) != 0)
    {
      kfree(mem);
      *pte |= PTE_V;
      return 0;
    }

    // 将原来的物理内存引用计数减1
    kfree((char *)PGROUNDDOWN(pa));
    return mem;
  }
}

/**
 * krefcnt 获取内存的引用计数
 * pa 指定的内存地址
 * 引用计数
 */
int krefcnt(void *pa)
{
  return ref.cnt[(uint64)pa / PGSIZE];
}

/**
 * kaddrefcnt 增加内存的引用计数
 * pa 指定的内存地址
 *  0:成功 -1:失败
 */
int kaddrefcnt(void *pa)
{
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    return -1;
  acquire(&ref.lock);
  ++ref.cnt[(uint64)pa / PGSIZE];
  release(&ref.lock);
  return 0;
}
