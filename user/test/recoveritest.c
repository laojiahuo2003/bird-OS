#include "types.h"
#include "stat.h"
#include "user/user.h"
#include "fcntl.h"

#define TEST_FILE "testfile"
#define TEMP_FILE "temp"
#define RECOVERED_FILE "recovered"

void create_test_file() {
    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("Error: Could not create test file.\n");
        exit(1);
    }
    // 写入测试数据
    char *data = "This is a test file for recoveri.\n";
    write(fd, data, strlen(data));
    close(fd);
}

int compare_files(const char *file1, const char *file2) {
    int fd1 = open(file1, O_RDONLY);
    int fd2 = open(file2, O_RDONLY);
    if (fd1 < 0 || fd2 < 0) {
        printf("Error: Could not open files for comparison.\n");
        return 0;
    }
    char buf1[512], buf2[512];
    int n1;
    while ((n1 = read(fd1, buf1, sizeof(buf1))) > 0) 
    {
        read(fd2, buf2, sizeof(buf2));
        if (memcmp(buf1, buf2, n1) != 0) 
        {
            close(fd1);
            close(fd2);
            return 0;
        }
    }
    close(fd1);
    close(fd2);
    return 1;
}

int main() {
    printf("Starting recoveri test...\n");
    // Step 1: Create a test file
    create_test_file();
    // Step 2: Save the file's inode information
    if (fork() == 0) {
        char *args[] = {"savei", TEST_FILE, TEMP_FILE, 0};
        exec("savei", args);
        exit(0);
    }
    wait(0);
    // Step 3: Recover the file using inode information
    if (fork() == 0) {
        char *args[] = {"recoveri", RECOVERED_FILE, TEMP_FILE, 0};
        exec("recoveri", args);
        exit(0);
    }
    wait(0);
    // Step 4: Compare the original and recovered files
    if (compare_files(TEST_FILE, RECOVERED_FILE)) {
        printf("Test passed: Recovered file matches original.\n");
    } else {
        printf("Test failed: Recovered file does not match original.\n");
    }

    // Cleanup
    unlink(TEST_FILE);
    unlink(TEMP_FILE);
    unlink(RECOVERED_FILE);

    exit(0);
}
