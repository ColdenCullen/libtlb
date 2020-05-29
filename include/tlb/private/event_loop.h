#ifndef EVENT_LOOP_H /* NOLINT(llvm-header-guard) */
#define EVENT_LOOP_H

#include "tlb/event_loop.h"

enum tlb_sub_mode {
  TLB_SUB_ONESHOT = TLB_BIT(1),
  TLB_SUB_EDGE = TLB_BIT(2),
};

enum tlb_sub_state {
  TLB_STATE_SUBBED,
  TLB_STATE_RUNNING,
  TLB_STATE_UNSUBBED,
};

struct tlb_event_loop {
  struct tlb_allocator *alloc;
  int fd;
};

struct tlb_subscription {
  union {
    int fd;
    uintptr_t ident;
  } ident;

  tlb_on_event *on_event;
  void *userdata;

  uint8_t events;         /* enum enum tlb_events */
  uint8_t sub_mode;       /* enum tlb_sub_flags */
  volatile uint8_t state; /* enum tlb_sub_state */

  /* Reserved for each platform to use */
  union {
    struct tlb_evl_epoll {
      bool close;             /* Whether this fd should be closed on removal (timers & triggers) */
      tlb_on_event *on_event; /* Some events need to wrap on_event, this keeps track of the original */
    } epoll;
    struct tlb_evl_kqueue {
      int16_t filters[2];
      uintptr_t data;
    } kqueue;
  } platform;

  const char *name;
};

#define TLB_LOG_EVENT(sub, text) TLB_LOGF("[%s:%p] %s", ((struct tlb_subscription *)(sub))->name, (void *)(sub), text)
#define TLB_LOGF_EVENT(sub, format, ...) \
  TLB_LOGF("[%s:%p] " format, ((struct tlb_subscription *)(sub))->name, (void *)(sub), __VA_ARGS__)

enum {
  TLB_EV_EVENT_BATCH = 100,
};

TLB_EXTERN_C_BEGIN

/* Implemented per platform */

int tlb_evl_init(struct tlb_event_loop *loop, struct tlb_allocator *alloc);
void tlb_evl_cleanup(struct tlb_event_loop *loop);

/* Initializes specific types to the loop */
void tlb_evl_impl_fd_init(struct tlb_subscription *sub);
void tlb_evl_impl_trigger_init(struct tlb_subscription *sub);
void tlb_evl_impl_timer_init(struct tlb_subscription *sub, int timeout);

/* All subscribe/unsubscribe implementations are the same */
int tlb_evl_impl_subscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub);
int tlb_evl_impl_unsubscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub);

TLB_EXTERN_C_END

#endif /* EVENT_LOOP_H */
