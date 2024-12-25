//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "buf.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if (argint(n, &fd) < 0)
    return -1;
  if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
    return -1;
  if (pfd)
    *pfd = fd;
  if (pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for (fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd] == 0)
    {
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if (argfd(0, 0, &f) < 0)
    return -1;
  if ((fd = fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if (argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if (argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if (argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if ((ip = namei(old)) == 0)
  {
    end_op();
    return -1;
  }

  ilock(ip);
  if (ip->type == T_DIR)
  {
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if ((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if (dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0)
  {
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for (off = 2 * sizeof(de); off < dp->size; off += sizeof(de))
  {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if (de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if (argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op();
  if ((dp = nameiparent(path, name)) == 0)
  {
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if ((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if (ip->nlink < 1)
    panic("unlink: nlink < 1");
  if (ip->type == T_DIR && !isdirempty(ip))
  {
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if (ip->type == T_DIR)
  {
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}


static struct inode *create(char *path, char type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if ((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if ((ip = dirlookup(dp, name, 0)) != 0)
  {
    
    iunlockput(dp);
    ilock(ip);
    if (type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }
  if ((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");
  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  ip->type=type;
  iupdate(ip);

  if (type == T_DIR)
  {              // Create . and .. entries.
    
    dp->nlink++; // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if (dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);
  
  return ip;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  if ((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if (omode & O_CREATE)
  {
    ip = create(path, T_FILE, 0, 0);
    if (ip == 0)
    {
      end_op();
      return -1;
    }
  }
  else
  {
    if ((ip = namei(path)) == 0)
    {
      end_op();
      return -1;
    }
    ilock(ip);
    if (ip->type == T_DIR && omode != O_RDONLY)
    {
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if (ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV))
  {
    iunlockput(ip);
    end_op();
    return -1;
  }
  // 处理符号链接
  if(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)) {
    // 若符号链接指向的仍然是符号链接，则递归的跟随它
    // 直到找到真正指向的文件
    // 但深度不能超过MAX_SYMLINK_DEPTH
    for(int i = 0; i <MAX_SYMLINK_DEPTH; ++i) {
      // 读出符号链接指向的路径
      if(readi(ip, 0, (uint64)path, 0, MAXPATH) != MAXPATH) {
        iunlockput(ip);
        end_op();
        return -1;
      }
      iunlockput(ip);
      ip = namei(path);
      if(ip == 0) {
        end_op();
        return -1;
      }
      ilock(ip);
      if(ip->type != T_SYMLINK)
        break;
    }
    // 超过最大允许深度后仍然为符号链接，则返回错误
    if(ip->type == T_SYMLINK) {
      iunlockput(ip);
      end_op();
      return -1;
    }
  }
  if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0)
  {
    if (f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if (ip->type == T_DEVICE)
  {
    f->type = FD_DEVICE;
    f->major = ip->major;
  }
  else
  {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if ((omode & O_TRUNC) && ip->type == T_FILE)
  {
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op();
  if (argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0)
  {
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op();
  if ((argstr(0, path, MAXPATH)) < 0 ||
      argint(1, &major) < 0 ||
      argint(2, &minor) < 0 ||
      (ip = create(path, T_DEVICE, major, minor)) == 0)
  {
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();

  begin_op();
  if (argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0)
  {
    end_op();
    return -1;
  }
  ilock(ip);
  if (ip->type != T_DIR)
  {
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op();
  p->cwd = ip;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if (argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0)
  {
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for (i = 0;; i++)
  {
    if (i >= NELEM(argv))
    {
      goto bad;
    }
    if (fetchaddr(uargv + sizeof(uint64) * i, (uint64 *)&uarg) < 0)
    {
      goto bad;
    }
    if (uarg == 0)
    {
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if (argv[i] == 0)
      goto bad;
    if (fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

bad:
  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  if (argaddr(0, &fdarray) < 0)
    return -1;
  if (pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0)
  {
    if (fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if (copyout(p->pagetable, fdarray, (char *)&fd0, sizeof(fd0)) < 0 ||
      copyout(p->pagetable, fdarray + sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0)
  {
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

uint64
sys_mmap(void)
{
  uint64 addr;
  int length;
  int prot;
  int flags;
  int vfd;
  struct file *vfile;
  int offset;
  uint64 err = 0xffffffffffffffff;

  // 获取系统调用参数
  if (argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 ||
      argint(3, &flags) < 0 || argfd(4, &vfd, &vfile) < 0 || argint(5, &offset) < 0)
    return err;

  // 实验提示中假定addr和offset为0，简化程序可能发生的情况
  if (addr != 0 || offset != 0 || length < 0)
    return err;

  // 文件不可写则不允许拥有PROT_WRITE权限时映射为MAP_SHARED
  if (vfile->writable == 0 && (prot & PROT_WRITE) != 0 && flags == MAP_SHARED)
    return err;

  struct proc *p = myproc();
  // 没有足够的虚拟地址空间
  if (p->sz + length > MAXVA)
    return err;

  // 遍历查找未使用的VMA结构体
  for (int i = 0; i < NVMA; ++i)
  {
    if (p->vma[i].used == 0)
    {
      p->vma[i].used = 1;
      p->vma[i].addr = p->sz;
      p->vma[i].len = length;
      p->vma[i].flags = flags;
      p->vma[i].prot = prot;
      p->vma[i].vfile = vfile;
      p->vma[i].vfd = vfd;
      p->vma[i].offset = offset;

      // 增加文件的引用计数
      filedup(vfile);

      p->sz += length;
      return p->vma[i].addr;
    }
  }

  return err;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  int length;
  if (argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;

  int i;
  struct proc *p = myproc();
  for (i = 0; i < NVMA; ++i)
  {
    if (p->vma[i].used && p->vma[i].len >= length)
    {
      // 根据提示，munmap的地址范围只能是
      // 1. 起始位置
      if (p->vma[i].addr == addr)
      {
        p->vma[i].addr += length;
        p->vma[i].len -= length;
        break;
      }
      // 2. 结束位置
      if (addr + length == p->vma[i].addr + p->vma[i].len)
      {
        p->vma[i].len -= length;
        break;
      }
    }
  }
  if (i == NVMA)
    return -1;

  // 将MAP_SHARED页面写回文件系统
  if (p->vma[i].flags == MAP_SHARED && (p->vma[i].prot & PROT_WRITE) != 0)
  {
    filewrite(p->vma[i].vfile, addr, length);
  }

  // 判断此页面是否存在映射
  uvmunmap(p->pagetable, addr, length / PGSIZE, 1);

  // 当前VMA中全部映射都被取消
  if (p->vma[i].len == 0)
  {
    fileclose(p->vma[i].vfile);
    p->vma[i].used = 0;
  }

  return 0;
}
uint64
sys_symlink(void) {
  char target[MAXPATH], path[MAXPATH];
  struct inode* ip_path;

  if(argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0) {
    return -1;
  }
  begin_op();
  // 分配一个inode结点，create返回锁定的inode
  ip_path = create(path, T_SYMLINK, 0, 0);
  
  if(ip_path == 0) {
    end_op();
    return -1;
  }
  // 向inode数据块中写入target路径
  if(writei(ip_path, 0, (uint64)target, 0, MAXPATH) < MAXPATH) {
    iunlockput(ip_path);
    end_op();
    return -1;
  }

  iunlockput(ip_path);
  end_op();
  return 0;
}

uint64 sys_mkf(void) {
    char path[MAXPATH];  // 用于存储文件路径
    struct inode *ip;    // 用于返回创建的 inode
    // 获取系统调用参数：路径
    if (argstr(0, path, MAXPATH) < 0) {          
        return -1;  // 如果参数获取失败，返回错误
    }
    // 调用 create 函数创建文件
    begin_op();
    ip = create(path, T_FILE, 0, 0);
    
    // 如果文件创建失败，则返回错误
    if (ip == 0)
    {
      end_op();
      return -1;
    }
    // 如果创建成功，返回 inode 的编号
    end_op();
    return ip->inum;
}
int sys_connect(void)
{
  struct file *f;
  int fd;
  uint32 raddr;
  uint32 rport;
  uint32 lport;
  if (argint(0, (int*)&raddr) < 0 ||
      argint(1, (int*)&lport) < 0 ||
      argint(2, (int*)&rport) < 0) {
    return -1;
  }
  if(sockalloc(&f, raddr, lport, rport) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0){
    fileclose(f);
    return -1;
  }
  return fd;
}
int sys_chmod(void)
{
  char pathname[MAXPATH];
  int mode;
  struct inode*ip;
  
  if(argstr(0,pathname,MAXPATH)<0||argint(1,&mode)<0)
    return -1;
  begin_op();
  if((ip=namei(pathname))==0)
  {
    end_op();
    return -1;
  }
  
  ilock(ip);
  ip->mode=(char)mode;
  iupdate(ip);
  iunlock(ip);
  end_op();
  return 0;
}
int sys_geti()  //保存文件索引信息
{
  char pathname[MAXPATH];
  uint64 addrsout;
  uint addrsin[14];
  struct inode*ip;
  if(argstr(0,pathname,MAXPATH)<0||argaddr(1,&addrsout)<0) return -1;
  begin_op();
  if((ip=namei(pathname))==0)	// 得不到对应索引节点
  {
    end_op();
    return -1;
  }
  ilock(ip);	// 同步inode和dinode
  for(int i=0;i<13;i++)
    addrsin[i]=ip->addrs[i];	// 复制索引
  addrsin[13]=ip->size;
  iunlock(ip);
  end_op();
   // 将内核中的 addrsin 写入到用户空间的 addrsout
    if (copyout(myproc()->pagetable, addrsout, (char *)addrsin, sizeof(addrsin)) < 0) {
        return -1; // 如果写入失败，返回错误
    }
  return 0;
}

int sys_recoveri() //根据文件索引信息恢复文件
{
    uint blockno;  // 用户传入的块号（可能是直接块、间接块或二级间接块）
    uint64 bufout; // 用户缓冲区地址
    char bufin[BSIZE]; // 缓冲区大小
    struct buf *b;
    // 获取用户传入的参数
    if (argint(0, (int *)&blockno) < 0 || argaddr(1, &bufout) < 0) {
        return -1;
    }
    b = bread(1, blockno); // 直接读取块
    // 将块内容复制到用户缓冲区
    memmove(bufin, b->data, BSIZE);
    if (copyout(myproc()->pagetable, bufout, bufin, BSIZE) < 0) {
        brelse(b);
        return -1;
    }
    brelse(b);
    return 0; // 成功
}

/*
uint64 sys_getcwd(void) {
  uint64 addr;
  
  // 获取用户传入的地址参数，如果失败则返回-1
  if (argaddr(0, &addr) < 0)
    return -1;

  struct dirent *de = myproc()->cwd; // 获取当前进程的当前工作目录
  char path[FAT32_MAX_PATH];         // 用于存储路径的缓冲区
  char *s = path + FAT32_MAX_PATH - 1;  // 指向路径的末尾
  int len;
  
  // 初始化路径缓冲区
  *s = '\0';
  
  // 处理根目录（没有父目录的情况）
  if (de->parent == NULL) {
    s = "/";
  } else {
    // 遍历父目录，构建路径
    while (de->parent) {
      len = strlen(de->name);
      
      // 检查是否有足够的空间来存储目录名和斜杠
      s -= len;
      if (s <= path) {
        // 如果路径超出了缓冲区，返回-1，表示路径无法构造
        return -1;
      }

      // 将当前目录名复制到缓冲区
      strncpy(s, de->name, len);
      s -= 1; // 为目录名添加斜杠
      *s = '/';

      de = de->parent; // 移动到父目录
    }
  }

  // 检查是否提供了有效的地址，如果地址为0则分配内存
  if (addr == 0) {
    addr = (uint64)kalloc();
    if (addr == 0) {
      return -1; // 内存分配失败
    }

    mappages(myproc()->pagetable, addr, PGSIZE, addr, PTE_R | PTE_W);
  }

  // 将路径字符串从内核空间复制到用户空间
  if (copyout2(addr, s, strlen(s) + 1) < 0) {
    return -1; // 如果复制失败，返回-1
  }

  return addr; // 返回路径字符串的用户空间地址
}

*/
uint64
sys_getcwd(void)
{
  uint64 addr;
  if (argaddr(0, &addr) < 0)
    return -1;

  char *mp = "/";

  if (copyout(myproc()->pagetable, addr, mp, strlen(mp) + 1) < 0)
    return -1;
  // printf("[sys_getcwd] cwd: %s, cwd_len: %d, addr: %p\n", mp, strlen(mp) + 1, addr);
  return addr;
}

uint64 sys_dup_new(void) // 复制文件描述符到指定的新文件描述符的系统调用
{
    struct file *f; // 文件指针
    int newfd; // 新的文件描述符

    // 获取文件指针
    if(argfd(0, 0, &f) < 0) {
        return -1;
    }

    // 获取新文件描述符
    if(argint(1, &newfd) < 0 || newfd < 0 || newfd >= NOFILE) {
        return -1;
    }

    // 如果新文件描述符已占用，则先关闭它
    if (myproc()->ofile[newfd] != ((void*)0)) {
        fileclose(myproc()->ofile[newfd]);  // 关闭已占用的文件描述符
    }

    // 将新文件描述符指向文件指针
    if ((newfd = fdalloc(f)) < 0)
      return -1;
    filedup(f);  // 增加文件指针的引用计数

    return newfd; // 返回新的文件描述符
}
