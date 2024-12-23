#include "types.h"
#include "stat.h"
#include "user/user.h"
#include "fcntl.h"

// 创建一个文件
int create_file(char *filename) {
    int fd;

    // 创建文件，返回文件描述符
    fd = open(filename, O_CREATE);
    if (fd < 0) {
        printf("create_file: failed to create file %s\n", filename);
        return -1;
    }
    printf("File %s created successfully.\n", filename);
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    // 调用 create_file 函数创建文件
    if (create_file(argv[1]) < 0) {
        printf("Error: Could not create file %s\n", argv[1]);
        exit(1);
    }
    exit(0);
}
