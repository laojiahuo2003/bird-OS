// System call numbers
#define SYS_fork 1
#define SYS_exit 2
#define SYS_wait 3
#define SYS_pipe 4
#define SYS_read 5
#define SYS_kill 6
#define SYS_exec 7
#define SYS_fstat 8
#define SYS_chdir 9
#define SYS_dup 10
#define SYS_getpid 11
#define SYS_sbrk 12
#define SYS_sleep 13
#define SYS_uptime 14 // 获取系统的启动时间
#define SYS_open 15
#define SYS_write 16
#define SYS_mknod 17
#define SYS_unlink 18
#define SYS_link 19
#define SYS_mkdir 20
#define SYS_close 21
#define SYS_cps 22
#define SYS_trace 23
#define SYS_sysinfo 24
#define SYS_setPriority 25
#define SYS_execve 26
#define SYS_getparentpid 27  // 获取当前进程的父进程的pid
#define SYS_print_pgtable 28 // 打印当前进程的页表
#define SYS_mmap 29          // 建立内存文件映射
#define SYS_munmap 30        // 取消内存文件映射
#define SYS_sh_var_read 31   // 信号量：访问共享变量
#define SYS_sh_var_write 32  // 信号量：修改共享变量
#define SYS_sem_create 33    // 信号量：创建信号量
#define SYS_sem_free 34      // 信号量：释放信号量
#define SYS_sem_p 35         // 信号量：P操作，获取资源
#define SYS_sem_v 36         // 信号量：V操作，释放资源
#define SYS_symlink 37       // 创建软链接
#define SYS_mkf 38           // 创建文件
#define SYS_shmgetat 39      // 共享内存
#define SYS_shmrefcount 40   // 共享内存
#define SYS_getcwd 41
#define SYS_dup_new 42
#define SYS_sigalarm 43
#define SYS_sigreturn 44
#define SYS_connect 45
#define SYS_mqget 46
#define SYS_msgsnd 47
#define SYS_msgrcv 48
#define SYS_chmod 49       // 修改文件权限
#define SYS_geti 50         // 保存文件的索引信息
#define SYS_recoveri 51     // 根据文件的索引信息恢复文件
#define SYS_clone 52        // 创建线程
#define SYS_join 53         // 回收线程