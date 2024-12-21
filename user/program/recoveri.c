#include "types.h"
#include "stat.h"
#include "user/user.h"
#include "fcntl.h"
#include "fs.h"

char buf[BSIZE];
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("format: savei filename temp\n");
        exit(1);
    }
    uint addrs[NDIRECT + 2]; // 包含直接块、一级间接块和二级间接块的索引
    int fd;

    // 从之前保存的 temp 文件中读出索引块
    fd = open("temp", O_RDONLY);
    read(fd, addrs, sizeof(addrs));
    close(fd);
    // 打开目标文件
    fd = open(argv[1], O_CREATE | O_RDWR);

    // **处理直接块**
    for (int i = 0; i < NDIRECT && addrs[i] != 0; i++) {
        recoveri(addrs[i], (uint64)buf); // 从块中恢复数据到缓冲区
        write(fd, buf, BSIZE);     // 写到新文件中
    }
    // **处理一级间接块**
    if (addrs[NDIRECT] != 0) {
        uint indirect_blocks[NINDIRECT]; // 一级间接块中的块地址表
        recoveri(addrs[NDIRECT], (uint64)indirect_blocks);
        for (int i = 0; i < NINDIRECT && indirect_blocks[i] != 0; i++) {
            recoveri(indirect_blocks[i], (uint64)buf);
            write(fd, buf, BSIZE);
        }
    }
    // **处理二级间接块**
    if (addrs[NDIRECT + 1] != 0) {
        uint second_level_blocks[NINDIRECT]; // 二级间接块中的一级间接块地址表
        recoveri(addrs[NDIRECT + 1], (uint64)second_level_blocks);

        for (int i = 0; i < NINDIRECT && second_level_blocks[i] != 0; i++) {
            uint indirect_blocks[NINDIRECT]; // 每个一级间接块中的块地址表
            recoveri(second_level_blocks[i], (uint64)indirect_blocks);

            for (int j = 0; j < NINDIRECT && indirect_blocks[j] != 0; j++) {
                recoveri(indirect_blocks[j], (uint64)buf);
                write(fd, buf, BSIZE);
            }
        }
    }
    close(fd);
    exit(0);
}
