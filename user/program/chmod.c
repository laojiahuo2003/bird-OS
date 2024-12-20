#include "types.h"
#include "stat.h"
#include "user/user.h"

int main(int argc,char*argv[])
{
    if(argc<=2)
    {
        printf("format: chmod pathname mode\n");
        exit(0);
    }
    chmod(argv[1],atoi(argv[2]));
    exit(0);
}