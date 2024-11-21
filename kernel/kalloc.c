// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#define PA2INDEX(pa) (((uint64)pa)/PGSIZE)//计算物理页的下标
void freerange(void *pa_start, void *pa_end);
int cowcount[PHYSTOP/PGSIZE];//保存每个物理页的映射数量
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");// 初始化锁
  //end()表示是内核区域后第一个可用的地址，(void*)PHYSTOP表示的是物理地址的结束地址
  freerange(end, (void*)PHYSTOP);
  /*  将第一个可用的内存到最后一个可用的内存分成一页一页的
    * 并将这些页添加到空闲页链表中
  */
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);//向下对齐
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    cowcount[PA2INDEX(p)] = 1; // 初始化的时候把每个物理页都加入freelist
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  // 需要加锁保证原子性
  acquire(&kmem.lock);
  int remain = --cowcount[PA2INDEX(pa)];
  release(&kmem.lock);

  if (remain > 0) {
    // 只有最后1个reference被删除时需要真正释放这个物理页
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void * kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char *)r, 5, PGSIZE); // fill with junk
    int idx = PA2INDEX(r);
    if (cowcount[idx] != 0) {
      panic("kalloc: cowcount[idx] != 0");
    }
    cowcount[idx] = 1; // 新allocate的物理页的计数器为1
  }
  return (void*)r;
}
void adjustref(uint64 pa, int num) {//增加物理页的cowcount
    if (pa >= PHYSTOP) {
        panic("addref: pa too big");
    }
    acquire(&kmem.lock);
    cowcount[PA2INDEX(pa)] += num;
    release(&kmem.lock);
}
void freebytes(uint64 *dst)//获取空闲内存量
{
  *dst = 0;
  struct run *p = kmem.freelist; // 用于遍历

  acquire(&kmem.lock);
  while (p) {
    *dst += PGSIZE;
    p = p->next;
  }
  release(&kmem.lock);
}

