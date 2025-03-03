//
// Created by Aaron Gill-Braun on 2020-11-10.
//

#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <base.h>
#include <common/syscalls.h>

#define IA32_STAR_MSR   0xC0000081 // Ring 0 and ring 3 segment bases (and syscall eip)
#define IA32_LSTAR_MSR  0xC0000082 // rip syscall entry for 64-bit software
#define IA32_CSTAR_MSR  0xC0000083 // rip syscall entry for compatibility mode
#define IA32_SFMASK_MSR 0xC0000084 // syscall flag mask

void syscalls_init();

#endif
