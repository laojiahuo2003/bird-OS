#include "types.h"
#include "user/user.h"
#include "fcntl.h"
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("format:savei filename temp\n");
        exit(1);
    }
    uint addrs[14];                           // 存储索引信息
    geti(argv[1], (uint64)addrs);             // 获取索引信息
    int fd = open("temp", O_CREATE | O_RDWR); // temp保存索引信息
    write(fd, addrs, sizeof(addrs));
    close(fd);
    exit(0);
}
