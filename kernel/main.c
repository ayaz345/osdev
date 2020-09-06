//
// Created by Aaron Gill-Braun on 2019-04-17.
//

#include <drivers/ata_pio.h>
#include <drivers/keyboard.h>
#include <drivers/screen.h>
#include <drivers/serial.h>
#include <kernel/cpu/asm.h>
#include <multiboot.h>

#include <fs/ext2/ext2.h>
#include <fs/fs.h>

#include <kernel/cpu/gdt.h>
#include <kernel/cpu/idt.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <kernel/bus/pci.h>
#include <kernel/cpu/rtc.h>
#include <kernel/mem/heap.h>
#include <kernel/mem/mm.h>
#include <kernel/mem/paging.h>
#include <kernel/time.h>

void main(multiboot_info_t *mbinfo) {
  kclear();
  kprintf("Kernel loaded!\n");

  install_gdt();
  install_idt();

  enable_interrupts();

  init_serial(COM1);
  init_keyboard();
  init_ata();

  kprintf("\n");
  kprintf("Kernel Start: %p (%p)\n", kernel_start, virt_to_phys(kernel_start));
  kprintf("Kernel End: %p (%p)\n", kernel_end, virt_to_phys(kernel_end));
  kprintf("Kernel Size: %d KiB\n", (kernel_end - kernel_start) / 1024);
  kprintf("\n");
  kprintf("Lower Memory: %d KiB\n", mbinfo->mem_lower);
  kprintf("Upper Memory: %d KiB\n", mbinfo->mem_upper);
  kprintf("\n");

  paging_init();

  uintptr_t mem_start = 0x100000;
  // align the kernel_end address
  uintptr_t kernel_aligned = (virt_to_phys(kernel_end) + 0x1000) & 0xFFFF000;
  kprintf("kernel_aligned %p\n", phys_to_virt(kernel_aligned));
  // set aside one mb for the watermark allocator heap
  uintptr_t base_addr = kernel_aligned + 0x100000;
  kprintf("start_addr %p\n", phys_to_virt(base_addr));
  size_t mem_size = (mbinfo->mem_upper * 1024) - (base_addr - mem_start);
  kprintf("base address: %p | size: %u MiB\n", base_addr, mem_size / (1024 * 1024));

  // Initialize memory
  mem_init(base_addr, mem_size);
  kheap_init();

  kprintf("\n");

  rtc_init(RTC_MODE_CLOCK);

  rtc_time_t time;
  rtc_get_time(&time);
  rtc_print_debug_time(&time);

  // rtc_init(RTC_MODE_TIMER);
  // rtc_print_debug_status();

  // ata_t disk = ATA_DRIVE_PRIMARY;
  // ata_info_t disk_info;
  // ata_identify(&disk, &disk_info);
  // ata_print_debug_ata_disk(&disk);
  // ata_print_debug_ata_info(&disk_info);

  // pci_enumerate_busses();
  // pci_device_t *device = pci_locate_device(PCI_STORAGE_CONTROLLER, PCI_IDE_CONTROLLER);
  // pci_print_debug_device(device);
}
