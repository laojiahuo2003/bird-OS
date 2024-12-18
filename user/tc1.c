#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){	
	int i;
	int j = 0;
	int k;
	int pid;
	sett();
	tc1();	//system call for testing
	for (i = 0; i < 30; i++) {
		pid = fork();
		if (pid == 0) {  //child
			j = (getpid()) % 2; // ensures independence from the first son's pid when gathering the results in the second part of the program
			switch(j) {
				case 0: //CPU‐bound process (CPU):
					for (k = 0; k < 200; k++){
						for (j = 0; j < 1000000; j++){}
					}
					break;
				
				case 1:// simulate I/O bound process (IO)
					for(k = 0; k < 800; k++){
						sleep(1);
					}
					break;
			}
			fexit();  //system call to toggle firstexit 
			exit(0); // children exit here
		}
		continue; // father continues to spawn the next child
	}
	setflag();
	while((pid=wait(0))>=0) {}
	pfreset();
	resetflag();
	resett();
	refexit();
	exit(0);
}
