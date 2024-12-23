#include "param.h"
struct stat;
struct rtcdate;
struct sysinfo;
// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int *);
int pipe(int *);
int write(int, const void *, int);
int read(int, void *, int);
int close(int);
int kill(int);
int exec(char *, char **);
int open(const char *, int);
int mknod(const char *, short, short);
int unlink(const char *);
int fstat(int fd, struct stat *);
int link(const char *, const char *);
int mkdir(const char *);
int chdir(const char *);
int dup(int);
int getpid(void);
char *sbrk(int);
int sleep(int);
int uptime(void);
int cps(void);
int trace(int);
int sysinfo(struct sysinfo *);
int setPriority(int pid, int priority);
int execve(const char *path, char *argv[], char *envp[]);
int getparentpid(void);
int print_pgtable(void);
void *mmap(void *addr, int length, int prot, int flags, int fd, int offset);
int munmap(void *addr, int length);
int sh_var_read(void);
void sh_var_write(int n);
int symlink(const char*,const char*);
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
int connect(uint32, uint16, uint16);
//恢复被删除的文件
int geti(const char*,uint64);
int recoveri(uint,uint64);
// 信号量
int sem_create(int);
int sem_free(int);
int sem_p(int);
int sem_v(int);
int mkf(char *);
// 共享内存
uint64 shmgetat(int, int);
int shmrefcount(int);
// 消息队列
int mqget(uint);               // 申请使用某个消息队列
int msgsnd(uint, void *, int); // 发送消息
int msgrcv(uint, void *, int); // 接收消息
// 文件权限
int chmod(const char*,char);
// 内核线程
int clone(uint64,uint64,uint64);
int join(uint64);

// ulib.c
int stat(const char *, struct stat *);
char *strcpy(char *, const char *);
void *memmove(void *, const void *, int);
char *strchr(const char *, char c);
int strcmp(const char *, const char *);
void fprintf(int, const char *, ...);
void printf(const char *, ...);
char *gets(char *, int);
uint strlen(const char *);
void *memset(void *, int, uint);
void *malloc(uint);
void free(void *);
int atoi(const char *);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
int statistics(void *buf, int sz);
// uthread.c
int thread_join(void);
int thread_create(void(*start_routine)(void*),void*arg);

