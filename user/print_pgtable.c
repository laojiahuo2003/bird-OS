#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"
int main(int argc, char *argv[])
{
    print_pgtable();
    exit(0);
}