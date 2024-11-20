#include "kernel/types.h"
#include "user/user.h"
 
int main(int argc, char *argv[])
{
	if(argc!=1)
	printf("Usage: zrzps\n");
	else
	cps();
	exit(0);
}
