struct buf {
  int valid;   // 记录是否数据从磁盘读取到内存，首次记录目标块时会标记为0
  int disk;    // 用于磁盘层驱动和中断之间作为消息
  uint dev;    // 设备号
  uint blockno; // 缓冲对应的硬盘块号
  struct sleeplock lock;  // 睡眠锁
  uint refcnt;  //记录有多少进程在使用该缓冲块
  struct buf *prev; // 双向循环链表，方便LRU算法处理
  struct buf *next;
  uchar data[BSIZE];//硬盘数据存储位置
  uint timestamp;  // 时间戳
};

