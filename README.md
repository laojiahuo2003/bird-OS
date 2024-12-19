# BirdOS（参赛方向：OS原理赛道——小型内核实现）

## **项目简介**

本项目是一个基于xv6-RISCV实现的小型内核操作系统，旨在开发过程中对xv6的各个模块进行改进和优化。在原有基础上，我们分别在进程调度、内存管理、文件管理几个方面完善了功能。截至目前一共实现了XX个系统调用，为用户提供了更丰富 的系统服务。

**开发过程**：已记录在项目根目录下的[开发日志](https://gitlab.eduxiji.net/T202410336994266/project2608132-272904/-/blob/master/%E5%BC%80%E5%8F%91%E6%97%A5%E5%BF%97.md)中。

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

<img src="https://gitlab.eduxiji.net/T202410336994266/project2608132-272904/-/raw/master/docs/img/%E5%B1%8F%E5%B9%95%E6%88%AA%E5%9B%BE_2024-12-19_114254.png" style="zoom: 80%;" />

## 内核各模块设计综述

在xv6原有基础上，我们针对内核各模块进行了相关改进与创新（详细文档在最后），添加如下功能：

- **进程管理**

基于动态优先级的进程调度器

共享内存的进程通信方式 

用于进程同步与互斥的记录型信号量

- **内存管理**

写时复制Copy On Write

懒分配

基于VMA的虚拟地址空间管理

空闲页面链表互斥锁的细粒度化

- **文件系统**

三级间接块的混合索引分配方式

buffer cache互斥锁的细粒度化

基于VMA的文件内存映射

- **网络设备**

e1000网卡驱动程序

UDP/IP协议通信的支持



## 文档

内核各模块的详细设计文档如下：

[系统调用](https://gitlab.eduxiji.net/T202410336994266/project2608132-272904/-/blob/master/docs/document/%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8.md)

进程管理

内存管理

文件系统

网络设备

