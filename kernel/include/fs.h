// On-disk file system format.
// Both the kernel and user programs use this header file.

#define ROOTINO 1  // root i-number
#define BSIZE 1024 // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock
{
  uint magic;      // Must be FSMAGIC
  uint size;       // Size of file system image (blocks)
  uint nblocks;    // Number of data blocks
  uint ninodes;    // Number of inodes.
  uint nlog;       // Number of log blocks
  uint logstart;   // Block number of first log block
  uint inodestart; // Block number of first inode block
  uint bmapstart;  // Block number of first free map block
};

#define FSMAGIC 0x10203040
// 直接地址指针是指向文件数据块的指针，
// 在 addrs[] 数组的前 11 个位置存储直接地址指针。
#define NDIRECT 11
// 一个间接块是一个数据块，存储了指向其他数据块的地址。
#define NINDIRECT (BSIZE / sizeof(uint))
// 二级间接块
#define NDINDIRECT ((BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint)))
// 一个文件最大可以使用的块数量， 包含了文件的直接块和间接块
#define MAXFILE (NDIRECT + NINDIRECT+NDINDIRECT)
// 一个块中的地址数量
#define NADDR_PER_BLOCK (BSIZE / sizeof(uint))  

// On-disk inode structure
struct dinode
{
  char mode;                // 文件权限
  char type;              // 文件类型
  short major;             // 主设备号
  short minor;             // 从设备号
  short nlink;             // 引用计数
  uint size;               // 文件大小
  uint addrs[NDIRECT + 2];   // 数据块地址
};

// Inodes per block.
#define IPB (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14
#define MAX_SYMLINK_DEPTH 10
struct dirent
{
  ushort inum;       // 文件或目录的 inode 编号
  char name[DIRSIZ]; //  文件或目录的名称（以 null 结尾的字符串）
};
