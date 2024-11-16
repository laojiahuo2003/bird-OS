#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void main()
{
  if(cpuid() == 0){//查询TP寄存器，返回运行着当前代码的核心编号，如果是0(第一个核心)，则执行下列初始化
    consoleinit();
    printfinit();
    printf("\n");
    printf("bird-os is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // 该函数通过将之前定义的全局变量kernel_pagetable写入到satp寄存器中来开启分页功能
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode cache
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();//告诉编译器在将started赋值为1之前，完成上面的所有初始化，防止编译器优化而扰乱多核代码执行
    started = 1;
  } else {//除第一个核心之外的将执行下列代码
    while(started == 0);//执行空循环直到核心0完成初始化
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
