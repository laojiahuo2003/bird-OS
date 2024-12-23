#include "types.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "proc.h"
#include "memlayout.h"

#define MAX_SHM_PGNUM (4)

struct sharemem
{
    int refcount;                  // 共享内存引用数量
    int pagenum;                   // 占用的页数(0~4)
    void *physaddr[MAX_SHM_PGNUM]; // 对应每页的物理地址
};
struct spinlock shmlock;   // 互斥访问的锁
struct sharemem shmtab[8]; // 整个系统最多8个共享内存
// 内存区初始化
void sharememinit()
{
    initlock(&shmlock, "shmlock"); // 初始化锁
    for (int i = 0; i < 8; i++)
    {
        shmtab[i].refcount = 0; // 引用数初始化
    }
     printf("Shared memory area initialization completed\n");
}
// 判断内存区是否已经启用
int shmkeyused(uint64 key, uint64 mask)
{
    if (key < 0 || key > 8)
    {
        return 0;
    }
    return (mask >> key) & 0x1; // 判断对应的系统共享内存区是否已经启用
}

// 将共享内存映射到进程空间
void *shmgetat(uint64 key, uint64 num)
{
    pagetable_t pagetable;
    void *phyaddr[MAX_SHM_PGNUM];
    uint64 shm = 0;

    // 检查输入是否合法
    if (key < 0 || key >= 8 || num < 0 || MAX_SHM_PGNUM < num)
        return (void *)-1;

    acquire(&shmlock);
    pagetable = proc->pagetable; // 获取当前进程的页表
    shm = proc->shm;

    // 情况1：如果当前进程已经映射了该key的共享内存，直接返回地址
    if (proc->shmkeymask >> key & 1)
    {

        // printf("qingkuang1\n");
        // release(&shmlock);
        // return proc->shmva[key];
    }

    // 情况2：如果系统还未创建此key对应的共享内存，则分配内存并映射
    if (shmtab[key].refcount == 0)
    {
        // printf("qingkuang2\n");
        shm = allocshm(pagetable, shm, shm - num * PGSIZE, proc->sz, phyaddr);
        if (shm == 0)
        {
            release(&shmlock);
            return (void *)-1;
        }
        // 新分配的内存映射到进程空间
        proc->shmva[key] = (void *)shm;
        shmadd(key, num, phyaddr); // 将新内存区信息填入shmtab[8]数组
    }
    else
    {
        // printf("qingkuang3\n");
        // 情况3：如果未持有且已经在系统中分配此key对应的共享内存，则直接映射
        for (int i = 0; i < num; i++)
        {
            phyaddr[i] = shmtab[key].physaddr[i];
        }

        num = shmtab[key].pagenum;
        // mapshm方法新建映射
        if ((shm = mapshm(pagetable, shm, shm, proc->sz, phyaddr)) == 0)
        {
            release(&shmlock);
            return (void *)-1;
        }

        proc->shmva[key] = (void *)shm;
        shmtab[key].refcount++; // 引用计数+1
    }

    proc->shm = shm;
    proc->shmkeymask |= 1 << key; // 更新共享内存键的标志
    // printf("proc->shmkeymask:%d\n", proc->shmkeymask);
    release(&shmlock);
    // printf("分配到shm:%d\n", shm);
    return (void *)shm; // 返回共享内存的地址
}

// 分配
int allocshm(pagetable_t pagetable, uint64 oldshm, uint64 newshm, uint64 sz, void *phyaddr[MAX_SHM_PGNUM])
{
    char *mem;
    uint64 a;

    // 检查 oldshm 和 newshm 是否按页对齐，以及它们是否在有效的内存范围内
    if (oldshm & 0xFFF || newshm & 0xFFF || oldshm > KERNBASE || newshm < sz)
        return 0;

    a = newshm;

    // 循环分配内存并映射
    for (int i = 0; a < oldshm; a += PGSIZE, i++)
    {
        mem = kalloc(); // 分配物理页帧
        if (mem == 0)
        {
            deallocshm(pagetable, newshm, oldshm); // 清理已分配的内存
            return 0;
        }

        memset(mem, 0, PGSIZE); // 清零内存

        // 修改这里，传递正确的参数给 mappages
        if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W | PTE_U) != 0)
        {
            kfree(mem);                            // 如果映射失败，释放内存
            deallocshm(pagetable, newshm, oldshm); // 清理已分配的内存
            return 0;
        }

        // 存储物理地址
        phyaddr[i] = (void *)mem; // 直接使用 mem 作为物理地址
    }

    return newshm;
}

// 映射
int mapshm(pagetable_t pagetable, uint64 oldshm, uint64 newshm, uint64 sz, void **physaddr)
{
    uint64 a;

    // 参数验证：检查地址对齐性和地址范围的合法性
    if ((oldshm & 0xFFF) || (newshm & 0xFFF) || oldshm > KERNBASE || newshm < sz)
    {
        return 0;
    }

    // 起始地址为 newshm
    a = newshm;

    // 逐页映射共享内存
    for (int i = 0; a < oldshm; a += PGSIZE, i++)
    {
        // 调用 mappages 将共享内存的物理地址映射到虚拟地址
        if (mappages(pagetable, a, PGSIZE, (uint64)physaddr[i], PTE_W | PTE_U) < 0)
        {
            return 0; // 映射失败，返回 0
        }
    }

    return newshm; // 映射成功，返回新地址
}

// 新内存区信息填入数组
int shmadd(uint64 key, uint64 pagenum, void *physaddr[MAX_SHM_PGNUM])
{
    // 参数验证：检查 key 和 pagenum 是否在合法范围内
    if (key < 0 || key >= 8 || pagenum <= 0 || pagenum > MAX_SHM_PGNUM)
    {
        return -1; // 参数不合法
    }

    // 初始化共享内存表项
    shmtab[key].refcount = 1;      // 设置引用计数为 1
    shmtab[key].pagenum = pagenum; // 设置页数

    // 保存物理地址信息
    for (int i = 0; i < pagenum; ++i)
    {
        shmtab[key].physaddr[i] = physaddr[i];
    }

    return 0; // 添加成功
}
// 引用计数修改
void shmaddcount(uint64 mask)
{
    acquire(&shmlock); // 加锁，确保共享内存表的线程安全

    for (int key = 0; key < 8; key++)
    {
        // 检查该 key 是否被当前进程引用
        if (shmkeyused(key, mask))
        {
            // 对共享内存引用计数加 1
            shmtab[key].refcount++;
        }
    }

    release(&shmlock); // 解锁
}

int deallocshm(pagetable_t pagetable, uint64 oldshm, uint64 newshm)
{
    pte_t *pte;
    uint64 a, pa;

    // 如果 newshm 小于等于 oldshm，则无需释放
    if (newshm <= oldshm)
    {
        return oldshm;
    }

    // 计算从 newshm 开始需要释放的内存范围
    a = (uint64)PGROUNDDOWN(newshm - PGSIZE);

    // 遍历并释放每一页
    for (; oldshm <= a; a -= PGSIZE)
    {
        // 使用 walk 查找页表条目
        pte = walk(pagetable, a, 0);

        // 如果页表条目存在且有效
        if (pte && (*pte & PTE_V) != 0)
        {                      // 检查有效性
            pa = PTE2PA(*pte); // 获取物理地址

            if (pa == 0)
            {
                panic("kfree: found a null physical address");
            }

            // 释放物理内存
            kfree((void *)pa);

            // 清空页表条目
            *pte = 0;
        }
    }

    return newshm; // 返回新的共享内存起始地址
}

int shmrelease(pagetable_t pagetable, uint64 shm, uint64 keymask)
{
    // cprintf("shmrelease: shm is %x, keymask is %x.\n", shm, keymask);

    acquire(&shmlock); // 获取共享内存锁

    // 释放共享内存对应的用户空间
    deallocshm(pagetable, shm, KERNBASE);

    // 对共享内存表中的所有共享内存键进行操作
    for (int k = 0; k < 8; k++)
    {
        if (shmkeyused(k, keymask))
        {
            shmtab[k].refcount--; // 引用计数减1
            if (shmtab[k].refcount == 0)
            {
                // 如果引用计数为0，释放该共享内存的物理内存
                shmrm(k);
            }
        }
    }

    release(&shmlock); // 释放共享内存锁

    return 0;
}

int shmrm(int key)
{
    if (key < 0 || key >= 8)
    {
        return -1; // 如果 key 不在有效范围内，返回 -1
    }

    // 获取共享内存条目
    struct sharemem *shmem = &shmtab[key];

    // 逐个页帧回收共享内存
    for (int i = 0; i < shmem->pagenum; i++)
    {
        // 这里调用 PA2PTE 来获取页表项，再根据需要进行处理
        pte_t *pte = (pte_t *)PA2PTE(shmem->physaddr[i]);
        if (pte && (*pte & PTE_V)) // 确保该页有效
        {
            // 将物理地址传递给 kfree 进行回收
            kfree((void *)PTE2PA(*pte)); // 回收每一页物理内存
        }
    }

    // 清空共享内存引用计数
    shmem->refcount = 0;

    return 0; // 成功回收共享内存
}

int shmrefcount(uint64 key)
{
    acquire(&shmlock);
    int count;

    // 如果 key 不在有效范围内，返回 -1，否则返回对应的 refcount
    count = (key < 0 || key >= 8) ? -1 : shmtab[key].refcount;

    release(&shmlock);
    return count;
}
