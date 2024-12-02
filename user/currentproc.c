#include "types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
	if (argc != 1)
		printf("Usage: currentproc\n");
	else
		cps();
	exit(0);
}
