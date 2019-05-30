KERNEL_CS equ 0x08
KERNEL_DS equ 0x10


global outb
outb:
  mov al, [esp + 8]
  mov dx, [esp + 4]
  out dx, al
  ret

global inb
inb:
  mov dx, [esp + 4]
  in  al, dx
  ret

global outw
outw:
  mov ax, [esp + 8]
  mov dx, [esp + 4]
  out dx, ax
  ret

global inw
inw:
  mov dx, [esp + 4]
  in  ax, dx
  ret

global load_gdt
load_gdt:
  mov eax, [esp + 4]
  lgdt [eax]
  mov ax, KERNEL_DS
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  jmp KERNEL_CS:.flush
.flush:
  ret

global  load_idt
load_idt:
  mov eax, [esp + 4]
  lidt [eax]
  ret

global interrupt
interrupt:
  push ebp
  mov ebp, esp
  int 49
  pop ebp
  ret


global interrupt_out_of_memory
interrupt_out_of_memory:
  push ebp
  mov ebp, esp
  int 50
  pop ebp
  ret


global enable_hardware_interrupts
enable_hardware_interrupts:
  sti
  ret


global disable_hardware_interrupts
disable_hardware_interrupts:
  cli
  ret
