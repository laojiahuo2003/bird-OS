K=kernel
U=user

OBJS = \
  $K/asm/entry.o \
  $K/start.o \
  $K/driver/console.o \
  $K/lib/printf.o \
  $K/driver/uart.o \
  $K/mm/kalloc.o \
  $K/lock/spinlock.o \
  $K/lib/string.o \
  $K/main.o \
  $K/mm/vm.o \
  $K/proc/proc.o \
  $K/asm/swtch.o \
  $K/asm/trampoline.o \
  $K/interrupt/trap.o \
  $K/syscall.o \
  $K/sysproc.o \
  $K/filesystem/bio.o \
  $K/filesystem/fs.o \
  $K/filesystem/log.o \
  $K/lock/sleeplock.o \
  $K/filesystem/file.o \
  $K/proc/pipe.o \
  $K/proc/exec.o \
  $K/sysfile.o \
  $K/asm/kernelvec.o \
  $K/interrupt/plic.o \
  $K/driver/virtio_disk.o \
  $K/proc/messagequeue.o \
  $K/mm/sharemem.o \
  $K/network/net.o\
  $K/network/e1000.o\
  $K/network/pci.o\
  $K/sysnet.o


# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I. -Ikernel/include
# CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += -DNET_TESTS_PORT=$(SERVERPORT)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

$K/kernel: $(OBJS) $K/kernel.ld $U/program/initcode
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS) 
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

$U/program/initcode: $U/program/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/program/initcode.S -o $U/program/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/program/initcode.out $U/program/initcode.o
	$(OBJCOPY) -S -O binary $U/program/initcode.out $U/program/initcode
	$(OBJDUMP) -S $U/program/initcode.o > $U/program/initcode.asm

tags: $(OBJS) _init
	etags *.S *.c

ULIB = $U/program/ulib.o $U/usys.o $U/program/printf.o $U/program/umalloc.o $U/program/uthread.o $U/program/statistics.o

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$U/usys.S : $U/usys.pl
	perl $U/usys.pl > $U/usys.S

$U/usys.o : $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S

$U/test/_forktest: $U/test/forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/test/_forktest $U/test/forktest.o $U/program/ulib.o $U/usys.o
	$(OBJDUMP) -S $U/test/_forktest > $U/test/forktest.asm

mkfs/mkfs: mkfs/mkfs.c $K/include/fs.h $K/include/param.h
	gcc -Werror -Wall -I. -o mkfs/mkfs mkfs/mkfs.c


.PRECIOUS: %.o

UPROGS=\
	$U/program/_cat\
	$U/program/_echo\
	$U/test/_forktest\
	$U/program/_grep\
	$U/program/_init\
	$U/program/_kill\
	$U/program/_ln\
	$U/program/_ls\
	$U/program/_mkdir\
	$U/program/_rm\
	$U/program/_sh\
	$U/program/_stressfs\
	$U/test/_usertests\
	$U/program/_grind\
	$U/program/_wc\
	$U/program/_zombie\
	$U/program/_sleep\
	$U/program/_currentproc\
	$U/program/_trace\
	$U/program/_sysinfo\
	$U/test/_cowtest\
	$U/program/_setp\
	$U/test/_lazytest\
	$U/program/_execve\
	$U/program/_getparentpid\
	$U/program/_print_pgtable\
	$U/test/_mmaptest\
	$U/test/_sh_rw_nolock\
	$U/test/_sh_rw_lock\
	$U/test/_symlinktest\
	$U/test/_bigfiletest\
	$U/program/_symlink\
	$U/program/_readfile\
	$U/program/_writefile\
	$U/program/_mkf\
	$U/test/_sharemm\
	$U/test/_alarmtest\
	$U/test/_nettest\
	$U/test/_msgtest\
	$U/program/_chmod\
	$U/test/_chmodtest\
	$U/program/_savei\
	$U/program/_recoveri\
	$U/test/_recoveritest\
	$U/program/_uthread\
	$U/test/_kalloctest\
	$U/test/_bcachetest\
	$U/program/_statistics



UEXTRA = $(wildcard kernel/include/*.h)
fs.img: mkfs/mkfs README.md $(UEXTRA) $(UPROGS)
	mkfs/mkfs fs.img README.md $(UEXTRA) $(UPROGS)

-include kernel/*.d user/*.d

clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*/*.o */*.d */*.asm */*.sym \
	*/*/*.o */*/*.d */*/*.asm */*/*.sym \
	$U/initcode $U/initcode.out $K/kernel fs.img \
	mkfs/mkfs .gdbinit \
        $U/usys.S \
	$(UPROGS)

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 3
endif
FWDPORT = $(shell expr `id -u` % 5000 + 25999)
QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
QEMUOPTS += -netdev user,id=net0,hostfwd=udp::$(FWDPORT)-:2000 -object filter-dump,id=net0,netdev=net0,file=packets.pcap
QEMUOPTS += -device e1000,netdev=net0,bus=pcie.0
qemu: $K/kernel fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $K/kernel .gdbinit fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)
SERVERPORT = $(shell expr `id -u` % 5000 + 25099)

server:
	python3 ./user/test/server.py $(SERVERPORT)

ping:
	python3 ./user/test/ping.py $(FWDPORT)