#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define stat xv6_stat // avoid clash with host struct stat
#include "kernel/include/types.h"
#include "kernel/include/fs.h"
#include "kernel/include/stat.h"
#include "kernel/include/param.h"

#ifndef static_assert
#define static_assert(a, b) \
	do                      \
	{                       \
		switch (0)          \
		case 0:             \
		case (a):;          \
	} while (0)
#endif

#define NINODES 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE / (BSIZE * 8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;	 // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks; // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1; // 当前可用的第一个空闲 inode 的编号
uint freeblock;

void balloc(int);
void wsect(uint, void *);
void winode(uint, struct dinode *);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

// convert to intel byte order
ushort
xshort(ushort x)
{
	ushort y;
	uchar *a = (uchar *)&y;
	a[0] = x;
	a[1] = x >> 8;
	return y;
}
// 将一个 32 位无符号整数 x 按字节分解并重新组合成 y。
// 如果目标是调整字节序，可能会根据系统的字节序来进行相应的字节重排。
uint xint(uint x)
{
	uint y;
	uchar *a = (uchar *)&y;
	a[0] = x;
	a[1] = x >> 8;
	a[2] = x >> 16;
	a[3] = x >> 24;
	return y;
}

int main(int argc, char *argv[])
{
	int i, cc, fd;			 // 分别用于循环控制、文件大小计数、文件描述符
	uint rootino, inum, off; // 分别用于存储根目录的 inode 索引、inode 编号以及偏移量
	struct dirent de;		 // 目录项数据结构，表示目录中的每一项文件或目录,包含文件名和inode 编号
	char buf[BSIZE];		 // 缓冲区
	struct dinode din;		 // 磁盘上的inode，包含文件元数据，例如类型、大小、数据块位置等）

	static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

	if (argc < 2)
	{
		fprintf(stderr, "Usage: mkfs fs.img files...\n");
		exit(1);
	}
	// 确保 BSIZE（块大小）能被 struct dinode 和 struct dirent 的大小整除。
	assert((BSIZE % sizeof(struct dinode)) == 0);
	assert((BSIZE % sizeof(struct dirent)) == 0);

	// 这里打开（或创建）一个文件系统映像文件 argv[1]
	// 并以读写模式 (O_RDWR)、创建模式 (O_CREAT)，以及截断模式 (O_TRUNC) 打开它。
	// 这意味着如果文件已存在，它会被截断到零大小；如果不存在，则创建一个新文件。
	// 0666 表示新文件的权限，即所有用户都可以读写。
	// 如果 open 函数返回一个负值，说明打开文件失败,输出错误信息并退出。
	fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fsfd < 0)
	{
		perror(argv[1]);
		exit(1);
	}

	// 1 fs block = 1 disk sector
	// nmeta 是文件系统的元数据所占的块数
	// 元数据块包括：引导块 超级块 日志块 i节点块 位图块
	// nlog：日志块的数量 ninodeblocks：i节点块的数量 nbitmap：位图块的数量
	nmeta = 2 + nlog + ninodeblocks + nbitmap;
	// 表示可用的数据块数量，即文件系统总大小 FSSIZE 减去元数据块 nmeta 后剩余的块数。
	nblocks = FSSIZE - nmeta;
	sb.magic = FSMAGIC;							  // 设置超级块的魔数，魔数用来标识文件系统的格式
	sb.size = xint(FSSIZE);						  // 文件系统的总块数
	sb.nblocks = xint(nblocks);					  // 可用的块数（数据块）
	sb.ninodes = xint(NINODES);					  //	i节点的数量，表示文件系统中可以存储的文件数量。
	sb.nlog = xint(nlog);						  // 日志块的数量。
	sb.logstart = xint(2);						  // 日志块的起始位置。从第 2 块开始。
	sb.inodestart = xint(2 + nlog);				  // i节点块的起始位置。i节点块通常紧随日志块之后。
	sb.bmapstart = xint(2 + nlog + ninodeblocks); // 位图块的起始位置，紧接着 i节点块之后。
	// 打印每个分区的大小
	printf("nmeta:%d (boot:1, super:1, log blocks:%u inode blocks:%u, bitmap blocks:%u) blocks:%d total:%d\n",
		   nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);
	// 文件系统中第一个可以分配的空闲块:元数据块
	freeblock = nmeta; // the first free block that we can allocate

	for (i = 0; i < FSSIZE; i++)
		wsect(i, zeroes); // 写磁盘扇区的函数，将第 i 个块写为零

	memset(buf, 0, sizeof(buf));   // 缓冲区 buf 初始化为零
	memmove(buf, &sb, sizeof(sb)); // 将已经设置好的超级块 sb 的内容复制到缓冲区 buf 中
	wsect(1, buf);				   // 将超级块写入到磁盘映像文件的第 1 块
	/*下面的代码为根目录分配了一个新的 inode。
	然后，它创建了两个特殊的目录项：
	"."：指向根目录的 inode，表示当前目录。
	".."：指向根目录的 inode，表示父目录。
	最后，将这两个目录项通过 iappend 写入根目录的 inode，完成了根目录的初始化。
	*/
	rootino = ialloc(T_DIR);	// ialloc 申请一个 inode 节点，T_DIR表示分配的是根目录的inode
	assert(rootino == ROOTINO); // 确保根目录的 inode 节点号为 ROOTINO

	bzero(&de, sizeof(de));			   // 将目录项 de 清零
	de.inum = xshort(rootino);		   // 将根目录的 inode号存储在目录项中
	strcpy(de.name, ".");			   // 设置目录项名称为 "."
	iappend(rootino, &de, sizeof(de)); // 将目录项 de 写入磁盘，关联到根目录 inode

	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, "..");

	iappend(rootino, &de, sizeof(de));
	// 创建好根目录后，就将fs.img后面跟着的app 文件写入磁盘
	for (i = 2; i < argc; i++)
	{
		// 去除 "user/" 和 "kernel/include/"
		char *shortname;
		if (strncmp(argv[i], "user/", 5) == 0)
			shortname = argv[i] + 5;
		else if (strncmp(argv[i], "kernel/include/", 15) == 0)
			shortname = argv[i] + 15;
		else
			shortname = argv[i];
		printf("%s\n", shortname);
		assert(index(shortname, '/') == 0);
		if ((fd = open(argv[i], 0)) < 0)
		{
			perror(argv[i]);
			exit(1);
		}

		// Skip leading _ in name when writing to file system.
		// The binaries are named _rm, _cat, etc. to keep the
		// build operating system from trying to execute them
		// in place of system binaries like rm and cat.
		if (shortname[0] == '_')
			shortname += 1;

		inum = ialloc(T_FILE);

		bzero(&de, sizeof(de));
		de.inum = xshort(inum);
		strncpy(de.name, shortname, DIRSIZ);
		iappend(rootino, &de, sizeof(de));

		while ((cc = read(fd, buf, sizeof(buf))) > 0)
			iappend(inum, buf, cc);

		close(fd);
	}

	// fix size of root inode dir
	rinode(rootino, &din);
	off = xint(din.size);
	off = ((off / BSIZE) + 1) * BSIZE;
	din.size = xint(off);
	winode(rootino, &din);

	balloc(freeblock); // 0-freeblock-1表示已经使用的磁盘块

	exit(0);
}

void wsect(uint sec, void *buf)
{
	if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
	{
		perror("lseek");
		exit(1);
	}
	if (write(fsfd, buf, BSIZE) != BSIZE)
	{
		perror("write");
		exit(1);
	}
}

void winode(uint inum, struct dinode *ip)
{
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb);
	rsect(bn, buf);
	dip = ((struct dinode *)buf) + (inum % IPB);
	*dip = *ip;
	wsect(bn, buf);
}

void rinode(uint inum, struct dinode *ip)
{
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb);
	rsect(bn, buf);
	dip = ((struct dinode *)buf) + (inum % IPB);
	*ip = *dip;
}

void rsect(uint sec, void *buf)
{
	if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
	{
		perror("lseek");
		exit(1);
	}
	if (read(fsfd, buf, BSIZE) != BSIZE)
	{
		perror("read");
		exit(1);
	}
}

uint ialloc(ushort type)
{
	uint inum = freeinode++;
	struct dinode din;

	bzero(&din, sizeof(din));
	din.type = xshort(type);
	din.nlink = xshort(1);
	din.size = xint(0);
	winode(inum, &din);
	return inum;
}
// bitmap分区总共只有一块，也就是1024字节，块号为45，
// bitmap每一位表示一个block，1024 * 8 = 8192bit，
// 该位为1表示这个块被使用了，为0表示这个块没有被使用。
void balloc(int used)
{
	uchar buf[BSIZE];
	int i;

	printf("balloc: first %d blocks have been allocated\n", used);
	assert(used < BSIZE * 8);
	bzero(buf, BSIZE);
	for (i = 0; i < used; i++)
	{
		buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
	}
	printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
	wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint inum, void *xp, int n)
{
	char *p = (char *)xp;	  // 数据指针，将数据作为字符流来处理
	uint fbn, off, n1;		  // fbn: 文件块号，off: 偏移量，n1: 当前写入的数据量
	struct dinode din;		  // 文件的 inode
	char buf[BSIZE];		  // 数据缓冲区，用于读写文件数据块
	uint indirect[NINDIRECT]; // 缓冲区，用于存储间接块地址
	uint x;					  // 临时变量

	rinode(inum, &din);
	off = xint(din.size);
	// printf("append inum %d at off %d sz %d\n", inum, off, n);
	while (n > 0)
	{
		fbn = off / BSIZE;	   // 计算当前偏移量对应的文件块号
		assert(fbn < MAXFILE); // 确保文件块号在最大文件大小范围内
		// 如果文件块号小于直接块数，则使用直接块
		if (fbn < NDIRECT)
		{
			// 如果该直接块地址为 0，分配一个新块
			if (xint(din.addrs[fbn]) == 0)
			{
				din.addrs[fbn] = xint(freeblock++);
			}
			x = xint(din.addrs[fbn]);
		}
		else // 超过直接块数，使用间接块
		{
			// 如果没有间接块，分配一个新的间接块
			if (xint(din.addrs[NDIRECT]) == 0)
			{
				din.addrs[NDIRECT] = xint(freeblock++);
			}
			rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
			// 如果该间接块没有对应的块，分配一个新的空闲块
			if (indirect[fbn - NDIRECT] == 0)
			{
				indirect[fbn - NDIRECT] = xint(freeblock++);
				wsect(xint(din.addrs[NDIRECT]), (char *)indirect); // 写回间接块
			}
			x = xint(indirect[fbn - NDIRECT]); // 获取间接块中的实际块地址
		}
		// 计算当前写入块的字节数
		n1 = min(n, (fbn + 1) * BSIZE - off);
		rsect(x, buf);							 // 读取块数据到缓冲区
		bcopy(p, buf + off - (fbn * BSIZE), n1); // 计算出缓冲区中的具体位置，即当前文件块的偏移位置,拷贝数据到缓冲区
		wsect(x, buf);							 // 写回数据块
		// 更新剩余数据大小、偏移量和数据指针
		n -= n1;
		off += n1;
		p += n1;
	}
	// 更新 inode 的文件大小，并将其写回
	din.size = xint(off);
	winode(inum, &din);
}
