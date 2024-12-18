#include "types.h"
#include "stat.h"
#include "user/user.h"
#include "fcntl.h"

#define BUF_SIZE 512  // 缓冲区大小

// 用户级函数：写内容到文件
void write_file(const char *filename, const char *content) {
    int fd;            // 文件描述符
    int content_len;   // 内容的长度

    // 打开文件
    fd = open(filename, O_WRONLY);  // 只写模式打开文件
    if (fd < 0) 
    {
        printf("Error: Could not open file %s\n", filename);
        exit(1);
    }
    

    // 获取要写入的内容的长度
    content_len = strlen(content);

    // 写入文件
    if (write(fd, content, content_len) != content_len) {
        printf("Error: Write to file %s failed\n", filename);
        close(fd);
        exit(1);
    }

    // 关闭文件
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <filename> <content>\n", argv[0]);
        exit(1);
    }

    // 调用函数将内容写入文件
    write_file(argv[1], argv[2]);

    printf("Content written to %s successfully.\n", argv[1]);

    exit(0);
}
