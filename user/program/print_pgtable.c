#include "types.h"
#include "riscv.h"
#include "sysinfo.h"
#include "user/user.h"
int main(int argc, char *argv[])
{
    print_pgtable();
    exit(0);
}