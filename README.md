# BirdOS（参赛方向：OS原理赛道——小型内核实现）

## **项目简介**

本项目是一个基于xv6-RISCV实现的小型内核操作系统，旨在开发过程中对xv6的各个模块进行改进和优化。在原有基础上，我们分别在进程调度、内存管理、文件管理几个方面完善了功能。截至目前一共XX个系统调用，为用户提供了更丰富 的系统服务。

**参考项目与书籍：**

[mit-pdos/xv6-public: xv6 OS](https://github.com/mit-pdos/xv6-public)


[介紹 | xv6 中文文档](https://th0ar.gitbooks.io/xv6-chinese/content/)

**开发过程：** 已记录在项目根目录下的[开发日志](https://gitlab.eduxiji.net/T202410336994266/project2608132-272904/-/blob/master/%E5%BC%80%E5%8F%91%E6%97%A5%E5%BF%97.md)中。

## 内核架构





## 项目组织

```
.
├── Makefile
├── README.md
├── docs			# 说明文档
├── kernel			# 内核代码
│   ├── asm			# 汇编
│   ├── driver		# 磁盘驱动以及uart驱动
│   ├── filesystem	# 文件系统
│   ├── include		# 内核头文件
│   ├── interrupt	# 中断
│   ├── kernel.ld	# 内核链接
│   ├── lib			# 库函数相关
│   ├── lock		# 锁
│   ├── main.c		# 主函数
│   ├── mm			# 内存管理
│   ├── network		# 网卡驱动
│   ├── proc		# 进程管理
│   ├── start.c		
│   ├── syscall.c	# 系统调用接口
│   ├── sysfile.c	# 文件相关系统调用
│   ├── sysnet.c	# 网络相关系统调用
│   └── sysproc.c	# 进程相关系统调用
├── mkfs			# 磁盘分区初始化
└── user
    ├── program		# 用户命令与程序
    ├── test		# 测试用例
    ├── user.h		# 用户头文件
    └── usys.pl		# 用户调用系统调用
```



## 项目运行

### 环境依赖

Ubuntu 20.04	

qemu-5.1.0

RISC-V GNU 编译器工具链



### 运行命令

- 在项目根目录下通过以下命令构建并运行OS

```
make qemu
```

- 清理内核镜像以及用户编译结果

```
make clean
```

### 运行效果

<img src="https://gitlab.eduxiji.net/T202410336994266/project2608132-272904/-/blob/test/docs/img/BirdOS-init.png" style="zoom: 80%;" />

## 内核各模块设计综述

在xv6原有基础上，我们针对内核各模块进行了相关改进与创新（详细文档在最后），添加如下功能：

- **系统调用：** 用于支持相应功能以及提供用户接口，共XX个（xv6自带21个）

- **进程管理**

基于动态优先级的进程调度器

共享内存的进程通信方式

消息队列的进程通信方式

基于中断的定时提醒机制

用于进程同步与互斥的记录型信号量

内核多线程与用户线程库（开发中，暂未测试）

- **内存管理**

写时复制(Copy On Write)

懒分配

基于VMA的文件内存映射(MMAP)

空闲页面链表互斥锁的细粒度化

- **文件系统**

三级间接块的混合索引分配方式

buffer cache互斥锁的细粒度化

文件访问控制权限

基于索引信息的文件恢复策略

- **网络设备**

e1000网卡驱动程序

UDP/IP协议通信的支持

- **系统测试：** 我们在本项目/user/test下添加了对各功能的相关测试

## 文档

模块的设计文档如下：

[系统调用](https://gitlab.eduxiji.net/T202410336994266/project2608132-272904/-/blob/master/docs/document/%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8.md)

[进程管理](https://gitlab.eduxiji.net/T202410336994266/project2608132-272904/-/blob/master/docs/document/%E8%BF%9B%E7%A8%8B%E7%AE%A1%E7%90%86.md)

[内存管理](https://gitlab.eduxiji.net/T202410336994266/project2608132-272904/-/blob/master/docs/document/%E5%86%85%E5%AD%98%E7%AE%A1%E7%90%86.md)

文件系统

网络设备

