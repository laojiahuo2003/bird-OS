#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_nice(void){
  int nice_value;
    if(argint(0,&nice_value) < 0)
      return -1;
    if(nice_value < -20 || nice_value > 19 )
      return -1;
  myproc()->pri = nice_value;
  myproc()->prevweight = pri_to_weight[nice_value+20];
  return 1;
}

uint64
sys_getpidd(void){
  return myproc()->pid;
}

uint64
sys_sett(void){
  timerflag=1;
  flagval=0;
  int i=0;
  for(i=0;i<NPROC;i++){
    counter[i]=0;
  }
  return 1;
}

uint64
sys_wait2(void){
  uint64 p;
  if(argaddr(3, &p) < 0)
    return -1;
  return wait(p);
}

uint64 
sys_setflag(void){
  flag=1;
  return 1;
}

uint64 
sys_resetflag(void){
  flag=0;
  return 1;
}

uint64
sys_resett(void){
  timerflag=0;
  return 1;
}

uint64 
sys_fexit(void){
  if(!firstexit)  firstexit=1;
  return 1;
}

uint64 
sys_refexit(void){
  firstexit=0;
  return 1;
}

uint64 
sys_tc1(void){
  printflag=1;
  return 1;
}

uint64 
sys_tc2(void){
  printflag=2;
  return 1;
}

uint64
sys_pfreset(void){
  printflag=0;
  return 1;
}
