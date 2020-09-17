NAME = osdev
VERSION = 1.0
TARGET = i686

BUILD = build
BUILD_DIR = $(BUILD)/$(NAME)

CFLAGS  = -g -Wall -Wextra
LDFLAGS = -T linker.ld
ASFLAGS =

INCLUDE = -Iinclude -Ilib -Ilibc

QEMUFLAGS = -usb \
             -vga std \
             -cpu Nehalem \
             -smp 2 \
             -m 256M \
             -rtc base=localtime,clock=host \
             -serial file:$(BUILD)/stdio \
             -drive file=$(BUILD)/disk.img,format=raw,if=ide \
             -drive file=$(BUILD)/disk.img,format=raw,if=none,id=disk \
             -device ahci,id=ahci \
             -device ide-hd,drive=disk,bus=ahci.0 \
             -drive file=$(BUILD)/osdev.iso,media=cdrom

include scripts/Makefile.toolchain

# --------- #
#  Sources  #
# --------- #

kernel = \
	boot.asm \
	kernel/task.asm \
	kernel/main.c \
	kernel/panic.c \
	kernel/task.c \
	kernel/time.c \
	kernel/bus/pci.c \
	kernel/cpu/asm.asm \
	kernel/cpu/exception.asm \
	kernel/cpu/interrupt.asm \
	kernel/cpu/exception.c \
	kernel/cpu/asm.c \
	kernel/cpu/gdt.c \
	kernel/cpu/idt.c \
	kernel/cpu/interrupt.c \
	kernel/cpu/pdt.c \
	kernel/cpu/rtc.c \
	kernel/cpu/timer.c \
	kernel/mem/paging.asm \
	kernel/mem/cache.c \
	kernel/mem/heap.c \
	kernel/mem/mm.c \
	kernel/mem/paging.c \
	kernel/vga/vga.c

kernel-y := $(call objects,$(kernel))


drivers = \
	drivers/ahci.c \
	drivers/ata_dma.c \
	drivers/ata_pio.c \
	drivers/keyboard.c \
	drivers/screen.c \
	drivers/serial.c

drivers-y = $(call objects,$(drivers))


fs = \
	fs/fs.c \
	fs/super.c \
	fs/ext2/ext2.c \
	fs/ext2/dir.c \
	fs/ext2/inode.c \
	fs/ext2/super.c \
	fs/rd/rd.c

fs-y = $(call objects,$(fs))


lib = \
	lib/hash_table.c \
	lib/segment_tree.c

lib-y = $(call objects,$(lib))


libc = \
	libc/builtins/divdi3.s \
	libc/builtins/udivdi3.s \
	libc/math/math.c \
	libc/stdio/printf.c \
	libc/stdio/stdio.c \
	libc/stdlib/stdlib.c \
	libc/string/string.c \
	libc/libgen.c

libc-y = $(call objects,$(libc))


# --------- #
#  Targets  #
# --------- #

all: $(BUILD)/osdev.iso

run: $(BUILD)/osdev.iso $(BUILD)/disk.img
	$(QEMU) $(QEMUFLAGS)

debug: $(BUILD)/osdev.iso $(BUILD)/disk.img
	$(QEMU) -s -S $(QEMUFLAGS) &
	$(GDB) -w \
		-ex "target remote localhost:1234" \
		-ex "add-symbol $(BUILD)/osdev.bin"

run-debug: $(BUILD)/osdev.iso $(BUILD)/disk.img
	$(QEMU) -s -S $(QEMUFLAGS) &

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	mkdir $(BUILD_DIR)
	$(MAKE) -C tools clean

# Tools

.PHONY: tools
tools:
	$(MAKE) -C tools all

.PHONY: initrd
initrd:
	$(MAKE) -C tools initrd

ramdisk: initrd $(BUILD)/initrd.img

# -------------- #
#  Dependencies  #
# -------------- #

# Kernel

$(BUILD)/osdev.iso: $(BUILD)/osdev.bin $(BUILD)/initrd.img grub.cfg
	mkdir -p $(BUILD)/iso/boot/grub
	mkdir -p $(BUILD)/iso/modules
	cp $(BUILD)/osdev.bin $(BUILD)/iso/boot/osdev
	cp $(BUILD)/initrd.img $(BUILD)/iso/modules/initrd
	cp grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	$(MKRESCUE) -o $@ $(BUILD)/iso &> /dev/null

$(BUILD)/osdev.bin: $(kernel-y) $(drivers-y) $(fs-y) $(lib-y) $(libc-y)
	$(LD) $(LDFLAGS) $^ -o $@

# External Data

$(BUILD)/disk.img:
	scripts/create-disk.sh $@

$(BUILD)/initrd.img: $(BUILD)/initrd
	scripts/create-initrd.sh $< $@

# Tools


# ------------------- #
#  Compilation Rules  #
# ------------------- #

$(BUILD_DIR)/%_c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%_cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(INCLUDE) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%_s.o: %.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%_asm.o: %.asm
	@mkdir -p $(@D)
	$(NASM) $(NASMFLAGS) $< -o $@
