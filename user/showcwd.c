// showcwd.c

#include "types.h"
#include "user.h"

#define MAX_PATH_LEN 128

int main(void) {
    char cwd[MAX_PATH_LEN];
    
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf(1, "当前工作目录: %s\n", cwd);
    } else {
        printf(2, "getcwd 失败\n");
    }

    exit();
}
