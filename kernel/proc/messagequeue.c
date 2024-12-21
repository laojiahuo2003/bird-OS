#include "types.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "proc.h"
struct msg
{                     // 消息结构体
    struct msg *next; // 下一个消息
    long type;        // 消息类型
    char *dataaddr;   // 数据地址
    int datasize;     // 消息长度
};

struct mq
{                     // 消息队列
    int key;          // 对应的key
    int status;       // 0代表未使用，1代表已使用
    struct msg *msgs; // 指向msg链表
    int maxbytes;     // 一个消息队列最大为4k
    int curbytes;     // 当前已使用字节数
    int refcount;     // 引用数（进程数）
};

struct spinlock mqlock;     // 消息队列锁
struct mq mqs[MQMAX];       // 默认系统最多8个消息队列
struct proc *wqueue[NPROC]; // 写阻塞队列
int wstart = 0;             // 写阻塞队列指示下标
struct proc *rqueue[NPROC]; // 读阻塞队列
int rstart = 0;             // 读阻塞队列指示下标

int findkey(int key)
{
    int idx = -1;
    for (int i = 0; i < MQMAX; ++i)
    {
        if (mqs[i].status != 0 && mqs[i].key == key)
        {
            idx = i;
            break;
        }
    }
    return idx;
}

void mqinit()
{
    printf("Message queue initialization completed\n");
    initlock(&mqlock, "mqlock");
    for (int i = 0; i < MQMAX; ++i)
    {
        mqs[i].status = 0;
    }
}
int newmq(int key)
{
    int idx = -1;
    for (int i = 0; i < MQMAX; i++)
    {
        if (mqs[i].status == 0)
        {
            idx = i;
            break;
        }
    }
    if (idx == -1)
    {
        printf("newmq 失败:分配新的消息队列失败，无法获得idx\n");
        return -1;
    }
    mqs[idx].msgs = (struct msg *)kalloc(); // 为消息池分配一个页
    if (mqs[idx].msgs == 0)
    {
        printf("newmq 失败:无法分配新的页面\n");
        return -1;
    }
    mqs[idx].key = key;               // 为该消息队列设置key值
    mqs[idx].status = 1;              // 标示为已启用
    memset(mqs[idx].msgs, 0, PGSIZE); // 清空消息池
    mqs[idx].msgs->next = 0;          // 接下来都是初始化消息队列
    mqs[idx].msgs->datasize = 0;
    mqs[idx].maxbytes = PGSIZE;
    mqs[idx].curbytes = 16;
    mqs[idx].refcount = 1;
    proc->mqmask |= 1 << idx; // 修改当前进程的mqmask，表示使用中
    return idx;
}

void addmqcount(uint mask)
{
    acquire(&mqlock);
    for (int key = 0; key < MQMAX; key++)
    {
        if (mask >> key & 1)
        {
            mqs[key].refcount++;
        }
    }
    release(&mqlock);
}

int sys_mqget(uint key)
{
    acquire(&mqlock);
    int idx = findkey(key);
    if (idx != -1)
    { // 如果key对应的消息队列已经创建
        if (!(proc->mqmask >> idx & 1))
        {
            proc->mqmask |= 1 << idx; // 标记该进程使用该消息队列
            mqs[idx].refcount++;      // 消息队列的引用计数+1
        }
        release(&mqlock);
        return idx;
    }

    // 对应key消息队列未创建则newmg创建
    idx = newmq(key); // 创建消息队列
    release(&mqlock);
    return idx; // 返回该消息队列在mqs[]中的下标
}

// 消除内存碎片
int reloc(int mqid)
{
    struct msg *pages = mqs[mqid].msgs;
    struct msg *m = pages;
    struct msg *t;
    struct msg *pre = pages;
    while (m != 0)
    {
        t = m->next;
        memmove(pages, m, m->datasize + 16);
        pages->next = (struct msg *)((char *)pages + pages->datasize + 16);
        pages->dataaddr = ((char *)pages + 16);
        pre = pages;
        pages = pages->next;
        m = t;
    }
    pre->next = 0;
    return 0;
}

int sys_msgsnd(uint mqid, void *msg, int sz)
{
    // 校验消息队列的合法性
    if (mqid < 0 || mqid >= MQMAX || mqs[mqid].status == 0)
    {
        return -1;
    }

    // 解析传入的消息数据
    char *data = (char *)(((int *)(msg + 4)));

    int *type = ((int *)msg); // 获取消息类型

    // 如果队列为空，打印错误信息
    if (mqs[mqid].msgs == 0)
    {
        printf("msgsnd failed: msgs == 0.\n");
        return -1;
    }

    acquire(&mqlock);

    while (1)
    { // 一直循环直到发送成功
        if (mqs[mqid].curbytes + sz + 16 <= mqs[mqid].maxbytes)
        { // 如果剩余空间充裕
            struct msg *m = mqs[mqid].msgs;

            // 找到队尾最后一个空闲消息区
            while (m->next != 0)
            {
                m = m->next;
            }
            // 退出循环时，m->next == 0，表示空闲消息区

            m->next = (void *)m + m->datasize + 16; // 计算用于存储消息的起始位置
            m = m->next;                            // m为本消息存储空间起点

            m->type = *(type);            // 填写本消息的类型
            m->next = 0;                  // 本消息暂无后续消息
            m->dataaddr = (void *)m + 16; // 数据区的起始位置
            m->datasize = sz;             // 数据长度

            // 复制消息数据到消息区
            memmove(m->dataaddr, data, sz);

            mqs[mqid].curbytes += (sz + 16); // 可用空间缩减

            // 唤醒所有读阻塞进程
            for (int i = 0; i < rstart; i++)
            {
                wakeup(rqueue[i]);
            }

            rstart = 0; // 读阻塞队列置空
            release(&mqlock);
            return 0; // 成功发送消息
        }
        else
        { // 如果空间不足，进程睡眠在wqueue阻塞队列
            printf("msgsnd: cannot alloc: pthread: %d sleep.\n", myproc()->pid);
            wqueue[wstart++] = proc; // 将当前进程加入等待队列

            sleep(proc, &mqlock); // 进入休眠状态，等待唤醒
        }
    }
    return -1; // 发送失败
}

int sys_msgrcv(uint mqid, void *msg, int sz)
{
    // 校验消息队列的合法性
    if (mqid < 0 || mqid >= MQMAX || mqs[mqid].status == 0)
    {
        return -1;
    }

    int *type = msg;     // 待读取消息类型
    int *data = msg + 4; // 待读取消息目标位置

    acquire(&mqlock);

    while (1)
    {
        struct msg *m = mqs[mqid].msgs->next;
        struct msg *pre = mqs[mqid].msgs;

        while (m != 0)
        {
            if (m->type == *type)
            { // 找到要读取的消息类型
                // 复制消息数据到目标位置
                memmove(data, m->dataaddr, sz);

                // 从队列中删除已读取的消息
                pre->next = m->next;

                // 释放消息占用的空间
                mqs[mqid].curbytes -= (m->datasize + 16);

                // 重新整理内存
                reloc(mqid);

                // 唤醒所有写阻塞进程
                for (int i = 0; i < wstart; i++)
                {
                    wakeup(wqueue[i]);
                }
                wstart = 0; // 写阻塞队列置空

                release(&mqlock);
                return 0; // 成功读取消息
            }

            pre = m;
            m = m->next;
        }

        // 如果没有找到匹配的消息类型，当前进程进入休眠状态
        printf("msgrcv: can not read: pthread: %d sleep.\n", proc->pid);

        // 将当前进程加入读阻塞队列
        rqueue[rstart++] = proc;

        sleep(proc, &mqlock); // 进入休眠状态，等待唤醒
        return -1;            // 未能成功读取消息
    }
}

void rmmq(int mqid)
{
    kfree((char *)mqs[mqid].msgs);
    mqs[mqid].status = 0;
}

void releasemq(uint key)
{
    int idx = findkey(key);
    if (idx != -1)
    {
        acquire(&mqlock);
        mqs[idx].refcount--;
        if (mqs[idx].refcount == 0)
        {
            rmmq(idx);
        }
        release(&mqlock);
    }
}

void releasemq2(int mask)
{
    acquire(&mqlock);
    for (int id = 0; id < MQMAX; ++id)
    {
        if (mask >> id & 0x1)
        {
            mqs[id].refcount--;
            if (mqs[id].refcount == 0)
            {
                rmmq(id);
            }
        }
    }
    release(&mqlock);
}