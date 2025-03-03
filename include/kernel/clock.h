//
// Created by Aaron Gill-Braun on 2022-06-29.
//

#ifndef KERNEL_CLOCK_H
#define KERNEL_CLOCK_H

#include <base.h>
#include <queue.h>
#include <spinlock.h>

typedef struct clock_source {
  const char *name;
  void *data;

  uint32_t scale_ns;
  uint64_t last_tick;
  uint64_t value_mask;
  spinlock_t lock;

  int (*enable)(struct clock_source *);
  int (*disable)(struct clock_source *);
  uint64_t (*read)(struct clock_source *);

  LIST_ENTRY(struct clock_source) list;
} clock_source_t;

void register_clock_source(clock_source_t *source);

clock_t clock_now();
clock_t clock_kernel_time_ns();
uint64_t clock_current_ticks();
void clock_update_ticks();

static inline clock_t clock_future_time(uint64_t ns) {
  return clock_now() + ns;
}

static inline bool is_future_time(clock_t future) {
  return clock_now() >= future;
}

#endif
