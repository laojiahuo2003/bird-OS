struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;
struct sharemem;
// messagequeue.c
void mqinit();                 // 初始化系统消息队列
int mqget(uint);               // 申请使用某个消息队列
int msgsnd(uint, void *, int); // 发送消息
int msgrcv(uint, void *, int); // 接收消息
void releasemq(uint);          // 释放消息队列
void releasemq2(int);
void addmqcount(uint); // 增加消息队列的引用计数

struct mbuf;
struct sock;

// sharemem.c
void sharememinit();
void *shmgetat(uint64, uint64);
int shmrefcount(uint64 key);
void shmaddcount(uint64 mask);
int shmkeyused(uint64, uint64);
int shmrelease(pagetable_t pagetable, uint64 shm, uint64 keymask);
int allocshm(pagetable_t pagetable, uint64 oldshm, uint64 newshm, uint64 sz, void *phyaddr[]);
int shmadd(uint64, uint64, void *physaddr[]);
int deallocshm(pagetable_t pagetable, uint64 oldshm, uint64 newshm);
int mapshm(pagetable_t pagetable, uint64 oldshm, uint64 newshm, uint64 sz, void **physaddr);
int shmrm(int key);
// bio.c
void binit(void);
struct buf *bread(uint, uint);
void brelse(struct buf *);
void bwrite(struct buf *);
void bpin(struct buf *);
void bunpin(struct buf *);

// console.c
void consoleinit(void);
void consoleintr(int);
void consputc(int);

// exec.c
int exec(char *, char **);

// file.c
struct file *filealloc(void);
void fileclose(struct file *);
struct file *filedup(struct file *);
void fileinit(void);
int fileread(struct file *, uint64, int n);
int filestat(struct file *, uint64 addr);
int filewrite(struct file *, uint64, int n);

// fs.c
void fsinit(int);
int dirlink(struct inode *, char *, uint);
struct inode *dirlookup(struct inode *, char *, uint *);
struct inode *ialloc(uint, char);
struct inode *idup(struct inode *);
void iinit();
void ilock(struct inode *);
void iput(struct inode *);
void iunlock(struct inode *);
void iunlockput(struct inode *);
void iupdate(struct inode *);
int namecmp(const char *, const char *);
struct inode *namei(char *);
struct inode *nameiparent(char *, char *);
int readi(struct inode *, int, uint64, uint, uint);
void stati(struct inode *, struct stat *);
int writei(struct inode *, int, uint64, uint, uint);
void itrunc(struct inode *);

// ramdisk.c
void ramdiskinit(void);
void ramdiskintr(void);
void ramdiskrw(struct buf *);

// kalloc.c
void *kalloc(void);
void kfree(void *);
void kinit(void);
void freebytes(uint64 *);
int cowpage(pagetable_t, uint64);
void *cowalloc(pagetable_t, uint64);
int krefcnt(void *);
int kaddrefcnt(void *);
// log.c
void initlog(int, struct superblock *);
void log_write(struct buf *);
void begin_op(void);
void end_op(void);

// pipe.c
int pipealloc(struct file **, struct file **);
void pipeclose(struct pipe *, int);
int piperead(struct pipe *, uint64, int);
int pipewrite(struct pipe *, uint64, int);

// printf.c
void printf(char *, ...);
void panic(char *) __attribute__((noreturn));
void printfinit(void);

// proc.c
int setPriority(int pid, int priority);
int cpuid(void);
void exit(int);
int fork(void);
int growproc(int);
pagetable_t proc_pagetable(struct proc *);
void proc_freepagetable(pagetable_t, uint64);
int kill(int);
struct cpu *mycpu(void);
struct cpu *getmycpu(void);
struct proc *myproc();
void procinit(void);
void scheduler(void) __attribute__((noreturn));
void sched(void);
void setproc(struct proc *);
void sleep(void *, struct spinlock *);
void userinit(void);
int wait(uint64);
void wakeup(void *);
void wakeupOneProc(void *chan); // 信号量机制需要
void yield(void);
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void procdump(void);
int cps(void);
void procnum(uint64 *dst);
int clone(uint64 ,uint64 ,uint64 );
int join(uint64);
// swtch.S
void swtch(struct context *, struct context *);

// spinlock.c
void acquire(struct spinlock *);
int holding(struct spinlock *);
void initlock(struct spinlock *, char *);
void release(struct spinlock *);
void push_off(void);
void pop_off(void);
void initsem(void); // 信号量：初始化

// sleeplock.c
void acquiresleep(struct sleeplock *);
void releasesleep(struct sleeplock *);
int holdingsleep(struct sleeplock *);
void initsleeplock(struct sleeplock *, char *);

// string.c
int memcmp(const void *, const void *, uint);
void *memmove(void *, const void *, uint);
void *memset(void *, int, uint);
char *safestrcpy(char *, const char *, int);
int strlen(const char *);
int strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);

// syscall.c
int argint(int, int *);
int argstr(int, char *, int);
int argaddr(int, uint64 *);
int fetchstr(uint64, char *, int);
int fetchaddr(uint64, uint64 *);
void syscall();

// trap.c
extern uint ticks;
void trapinit(void);
void trapinithart(void);
extern struct spinlock tickslock;
void usertrapret(void);
int mmap_handler(int va, int cause);
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
// uart.c
void uartinit(void);
void uartintr(void);
void uartputc(int);
void uartputc_sync(int);
int uartgetc(void);

// vm.c
void kvminit(void);
void kvminithart(void);
uint64 kvmpa(uint64);
void kvmmap(uint64, uint64, uint64, int);
int mappages(pagetable_t, uint64, uint64, uint64, int);
pagetable_t uvmcreate(void);
void uvminit(pagetable_t, uchar *, uint);
uint64 uvmalloc(pagetable_t, uint64, uint64);
uint64 uvmdealloc(pagetable_t, uint64, uint64);
int uvmcopy(pagetable_t, pagetable_t, uint64);
void uvmfree(pagetable_t, uint64);
void uvmunmap(pagetable_t, uint64, uint64, int);
void uvmclear(pagetable_t, uint64);
uint64 walkaddr(pagetable_t, uint64);
int copyout(pagetable_t, uint64, char *, uint64);
int copyin(pagetable_t, char *, uint64, uint64);
int copyinstr(pagetable_t, char *, uint64, uint64);
pte_t *walk(pagetable_t, uint64, int);
void vmprint_helper(pagetable_t, int);
void vmprint(pagetable_t);
// plic.c
void plicinit(void);
void plicinithart(void);
int plic_claim(void);
void plic_complete(int);

// virtio_disk.c
void virtio_disk_init(void);
void virtio_disk_rw(struct buf *, int);
void virtio_disk_intr(void);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
// pci.c
void            pci_init();
// e1000.c
void            e1000_init(uint32 *);
void            e1000_intr(void);
int             e1000_transmit(struct mbuf*);

// net.c
void            net_rx(struct mbuf*);
void            net_tx_udp(struct mbuf*, uint32, uint16, uint16);
struct mbuf * mbufalloc(unsigned int headroom);


// sysnet.c
void            sockinit(void);
int             sockalloc(struct file **, uint32, uint16, uint16);
void            sockclose(struct sock *);
int             sockread(struct sock *, uint64, int);
int             sockwrite(struct sock *, uint64, int);
void            sockrecvudp(struct mbuf*, uint32, uint16, uint16);
