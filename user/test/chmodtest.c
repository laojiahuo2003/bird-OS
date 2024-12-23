#include "param.h"
#include "types.h"
#include "stat.h"
#include "riscv.h"
#include "fcntl.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "user/user.h"

#define BUF_SIZE 512  // 缓冲区大小

void read_file(const char *filename) {
    int fd;
    int bytes_read;
    char buf[BUF_SIZE];

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Error: Could not open file %s\n", filename);
        exit(1);
    }

    while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
        write(1, buf, bytes_read);
    }

    if (bytes_read < 0) {
        printf("Error: Could not read file %s\n", filename);
    }

    close(fd);
}

int main() {
    char buf[BUF_SIZE];
    int bytes_read;
    int fd;

    // 创建目录和文件
    mkdir("/test");
    fd = open("/test/a", O_RDWR | O_CREATE);
    if (fd < 0) {
        printf("Error: Could not create file /test/a\n");
        exit(1);
    }
    printf("Created file /test/a\n");
    printf("权限为可读可写:\n");
    // 写入数据
    if(write(fd, "hellow", 6)>0) printf("write \"hellow\" to /test/a successed\n");
    else printf("write \"hellow\" to /test/a failed\n");
    close(fd);
    // 读取文件内容并打印
    fd = open("/test/a", O_RDWR);
    while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
        write(1, buf, bytes_read);
    }
    if(bytes_read<0) printf("read from /test/a failed\n");
    else printf("\nread from /test/a successed\n");
    close(fd);

    // 修改文件权限为只读
    chmod("/test/a", 1);  // 只读权限
    printf("\n\n修改权限为只读:\n");
    fd = open("/test/a", O_RDWR);
    if(write(fd, "hellow1", 7)>0) printf("write \"hellow1\" to /test/a successed\n");
    else printf("write \"hellow1\" to /test/a failed\n");
    close(fd);
    fd = open("/test/a", O_RDWR);
    while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
        write(1, buf, bytes_read);
    }
    if(bytes_read<0) printf("read from /test/a failed\n");
    else printf("\nread from /test/a successed\n");
    close(fd);

    // 修改文件权限为只写
    chmod("/test/a", 2);  // 只写权限
    printf("\n\n修改权限为只写:\n");
    fd = open("/test/a", O_RDWR);
    if(write(fd, "hellow2", 7)>0) printf("write \"hellow2\" to /test/a successed\n");
    else printf("write \"hellow2\" to /test/a failed\n");
    close(fd);
    fd = open("/test/a", O_RDWR);
    while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
        write(1, buf, bytes_read);
    }
    if(bytes_read<0) printf("read from /test/a failed\n");
    else printf("\nread from /test/a successed\n");
    close(fd);
    exit(0);
}
