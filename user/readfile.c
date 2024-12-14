#include "types.h"
#include "stat.h"
#include "user/user.h"
#include "fcntl.h"

#define BUF_SIZE 512  // 缓冲区大小

// 用户级函数：读取文件并打印到标准输出
void read_file(const char *filename) {
    int fd;              // 文件描述符
    int bytes_read;      // 每次读取的字节数
    char buf[BUF_SIZE];  // 用于存储读取的文件内容

    // 打开文件
    fd = open(filename, O_RDONLY);  // 以只读模式打开文件
    if (fd < 0) {
        printf("Error: Could not open file %s\n", filename);
        exit(1);
    }

    // 循环读取文件内容，直到读取完
    while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
        // 打印读取的内容
        write(1, buf, bytes_read);  // 将读取的内容输出到标准输出
    }
    write(1, "\n", 1);
    if (bytes_read < 0) {
        printf("Error: Could not read file %s\n", filename);
    }

    // 关闭文件
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    // 调用函数读取文件并打印内容
    read_file(argv[1]);

    exit(0);
}
