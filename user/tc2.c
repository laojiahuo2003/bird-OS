#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
	int k, j, pid;
	int niceval;
	tc2();
	for(niceval=19; niceval >= -20; niceval--){
		pid=fork();
		
		if(pid<0){printf("fork failed\n");}
		if(pid==0){
			nice(niceval);
			for (k = 0; k < 250; k++){
				for (j = 0; j < 5000000; j++){}
			}
			exit(0);
		}
	}

	setflag();
	while((pid=wait(0))>=0){}

	pfreset();
	resetflag();
	refexit();
	exit(0);
}
