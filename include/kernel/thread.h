//
// Created by Aaron Gill-Braun on 2021-03-18.
//

#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include <base.h>
#include <queue.h>
#include <mutex.h>

#define ERRNO (PERCPU_THREAD->errno)

#define USER_VSTACK_SIZE  0x20000  // 128 KiB
#define USER_PSTACK_SIZE  0x20000  // 128 KiB
#define TLS_SIZE          0x2000   // 8 KiB
#define DEFAULT_RFLAGS 0x246

typedef struct process process_t;
typedef struct page page_t;
typedef struct sched_stats sched_stats_t;

// thread flags
#define F_THREAD_OWN_BLOCKQ 0x1 // thread uses external blocked queue
#define F_THREAD_JOINING    0x2 // thread will join
#define F_THREAD_DETATCHING 0x4 // thread will detatch
#define F_THREAD_CREATED    0x8 // thread was just created

typedef enum thread_status {
  THREAD_READY,
  THREAD_RUNNING,
  THREAD_BLOCKED,
  THREAD_SLEEPING,
  THREAD_TERMINATED,
  THREAD_KILLED
} thread_status_t;

typedef struct tls_block {
  uintptr_t addr; // base address of tls
  size_t size;    // tls memory size
  page_t *pages;  // pages used for tls
} tls_block_t;

typedef struct thread_ctx {
  uint64_t rax;    // 0x00
  uint64_t rbx;    // 0x00
  uint64_t rbp;    // 0x08
  uint64_t r12;    // 0x10
  uint64_t r13;    // 0x18
  uint64_t r14;    // 0x20
  uint64_t r15;    // 0x28
  //
  uint64_t rip;    // 0x30
  uint64_t cs;     // 0x38
  uint64_t rflags; // 0x40
  uint64_t rsp;    // 0x48
  uint64_t ss;     // 0x50
} thread_ctx_t;

typedef struct thread_meta_ctx {
  thread_ctx_t ctx;            // thread contex
  uintptr_t kernel_sp;         // kernel stack pointer
  uintptr_t user_sp;           // user stack pointer
  thread_status_t status;      // thread status
  int errno;                   // thread local errno
} thread_meta_ctx_t;

typedef struct thread {
  id_t tid;                    // thread id
  uint32_t reserved;           // reserved
  thread_ctx_t *ctx;           // thread context
  thread_meta_ctx_t *mctx;     // thread meta context
  process_t *process;          // owning process
  tls_block_t *tls;            // thread local storage
  uintptr_t kernel_sp;         // kernel stack pointer
  uintptr_t user_sp;           // user stack pointer
  // !!! DO NOT CHANGE ABOVE HERE !!!
  // assembly code in thread.asm accesses these fields using known offsets

  uint8_t cpu_id;              // current/last cpu used
  uint8_t policy;              // thread scheduling policy
  uint16_t priority;           // thread priority
  thread_status_t status;      // thread status
  sched_stats_t *stats;        // scheduling stats
  int affinity;                // thread cpu affinity
  clockid_t alarm_id;          // wakeup alarm id if sleeping

  spinlock_t lock;             // thread spinlock
  mutex_t mutex;               // thread mutex
  cond_t data_ready;           // thread data ready condition
  sigset_t signal;             // signal mask
  uint32_t flags;              // thread flags

  char *name;                  // thread name or description (owning)
  int errno;                   // thread local errno
  int preempt_count;           // preempt disable counter
  void *data;                  // thread data pointer

  page_t *kernel_stack;        // kernel stack pages
  page_t *user_stack;          // user stack pages

  LIST_ENTRY(thread_t) group;  // thread group (threads from same process)
  LIST_ENTRY(thread_t) list;   // generic thread list (used by scheduler, mutex, cond, etc)
} thread_t;
static_assert(offsetof(thread_t, tid) == 0x00);
static_assert(offsetof(thread_t, process) == 0x18);
static_assert(offsetof(thread_t, user_sp) == 0x30);

thread_t *thread_alloc(id_t tid, void *(start_routine)(void *), void *arg, bool user);
thread_t *thread_copy(thread_t *other);
void thread_free(thread_t *thread);

thread_t *thread_create(void *(start_routine)(void *), void *arg);
thread_t *thread_create_n(char *name, void *(start_routine)(void *), void *arg);
void thread_exit(void *retval);
int thread_join(thread_t *thread, void **retval);
int thread_send(void *data);
int thread_receive(thread_t *thread, void **data);
void thread_sleep(uint64_t us);
void thread_yield();
void thread_block();

int thread_setpolicy(uint8_t policy);
int thread_setpriority(uint16_t priority);
int thread_setaffinity(uint8_t affinity);
int thread_setsched(uint8_t policy, uint16_t priority);

void preempt_disable();
void preempt_enable();

int thread_alloc_stack(thread_t *thread, bool user);

void print_debug_thread(thread_t *thread);

#endif
