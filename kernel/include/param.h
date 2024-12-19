#define NPROC 64  // maximum number of processes
#define NCPU 8    // maximum number of CPUs
#define NOFILE 16 // open files per process
#define NFILE 100 // open files per system
#define NINODE 50 // maximum number of active i-nodes
#define NDEV 10   // maximum major device number
#define ROOTDEV 1 // device number of file system root disk
#define MAXARG 32 // max exec arguments
// 指定了文件系统（FS）操作中每次可以写入的最大磁盘块数
// 这意味着任何一个文件系统操作（如读写文件、修改文件系统结构等）
// 最多会涉及 10 个磁盘块。
#define MAXOPBLOCKS 10
#define LOGSIZE (MAXOPBLOCKS * 3) // 指定日志区域的大小为每次可以写入的最大磁盘块数的3倍
#define NBUF (MAXOPBLOCKS * 3)    // size of disk block cache
#define FSSIZE 100000             // size of file system in blocks
#define MAXPATH 128               // maximum file path name
#define MQMAX 8                   // 消息队列数量