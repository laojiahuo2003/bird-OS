#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void main()
{
  if (cpuid() == 0)
  {
    consoleinit();
    printfinit();
    printf("\033[1;32m");
    printf("\n");
    printf(" ______  _________ _______  ______          _______  _______ \n");
    printf("(  ___ \\ \\__   __/(  ____ )(  __  \\        (  ___  )(  ____ \\\n");
    printf("| (   ) )   ) (   | (    )|| (  \\  )       | (   ) || (    \\/\n");
    printf("| (__/ /    | |   | (____)|| |   ) | _____ | |   | || (_____ \n");
    printf("|  __ (     | |   |     __)| |   | |(_____)| |   | |(_____  )\n");
    printf("| (  \\ \\    | |   | (\\ (   | |   ) |       | |   | |      ) |\n");
    printf("| )___) )___) (___| ) \\ \\__| (__/  )       | (___) |/\\____) |\n");
    printf("|/ \\___/ \\_______/|/   \\__/(______/        (_______)\\_______)\n");
    printf("\n");
    printf("Wellcome to bird-os!\n");
    printf("\n");
    kinit();            // 初始化内存，将所有可用内存切碎
    kvminit();          // 创建内核页表，完成内核虚拟地址映射
    kvminithart();      // 把内核页表物理地址放入当前CPU核的页表基地寄存器(satp)中
    procinit();         // process table
    trapinit();         // trap vectors
    trapinithart();     // install kernel trap vector
    plicinit();         // set up interrupt controller
    plicinithart();     // ask PLIC for device interrupts
    binit();            // buffer cache
    iinit();            // inode cache
    fileinit();         // file table
    virtio_disk_init(); // emulated hard disk
    pci_init();
    sockinit();
    initsem();          // 信号量数组初始化
    sharememinit();
    mqinit();
    printf("\033[0m");
    userinit();           // first user process
    __sync_synchronize(); // 防止编译器优化，确保后续的任何操作都是初始化之后进行
    started = 1;
  }
  else
  {
    while (started == 0)
      ;
    __sync_synchronize();
    printf("\033[1;32mhart %d starting\033[0m\n", cpuid());
    kvminithart();  // turn on paging
    trapinithart(); // install kernel trap vector
    plicinithart(); // ask PLIC for device interrupts
  }

  scheduler();
}
