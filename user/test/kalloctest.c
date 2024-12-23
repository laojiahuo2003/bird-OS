#include "param.h"
#include "types.h"
#include "stat.h"
#include "riscv.h"
#include "memlayout.h"
#include "fcntl.h"
#include "user/user.h"

#define NCHILD 2
#define N 100000
#define SZ 4096

void test(void);
char buf[SZ];

int
main(int argc, char *argv[])
{
  test();
  exit(0);
}

int ntas(int print)
{
  int n;
  char *c;
  if (statistics(buf, SZ) <= 0) {
    fprintf(2, "ntas: no stats\n");
  }
  c = strchr(buf, '=');
  n = atoi(c+2);
  if(print)
    printf("%s", buf);
  return n;
}

void test(void)
{
  void *a, *a1;
  int n, m;
  printf("start test\n");  
  m = ntas(0);
  for(int i = 0; i < NCHILD; i++){
    int pid = fork();
    if(pid < 0){
      printf("fork failed");
      exit(-1);
    }
    if(pid == 0){
      for(i = 0; i < N; i++) {
        a = sbrk(4096);
        *(int *)(a+4) = 1;
        a1 = sbrk(-4096);
        if (a1 != a + 4096) {
          printf("wrong sbrk\n");
          exit(-1);
        }
      }
      exit(-1);
    }
  }
  for(int i = 0; i < NCHILD; i++){
    wait(0);
  }
  n = ntas(1);
  if(n-m < 10) 
    printf("test OK\n");
  else
    printf("test FAIL\n");
}