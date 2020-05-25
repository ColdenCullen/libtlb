#ifndef EVENT_LOOP_H /* NOLINT(llvm-header-guard) */
#define EVENT_LOOP_H

#include "tlb/event_loop.h"

#include "tlb/core.h"

enum tlb_sub_flags {
  TLB_SUB_ONESHOT = TLB_BIT(0),
  TLB_SUB_EDGE = TLB_BIT(1),
};

enum tlb_sub_state {
  TLB_STATE_SUBBED,
  TLB_STATE_RUNNING,
  TLB_STATE_UNSUBBED,
};

union tlb_ident {
  int fd;
  uintptr_t ident;
};

struct tlb_event_loop {
  struct tlb_allocator *alloc;
  int fd;
};

struct tlb_subscription {
  union tlb_ident ident;
  int events;
  int flags; /* enum tlb_sub_flags */

  tlb_on_event *on_event;
  void *userdata;

  volatile uint8_t state; /* enum tlb_sub_state */

  /* Reserved for each platform to use */
  union {
    struct {
    } epoll;
    int16_t kqueue[2];
  } platform;
};

enum {
  TLB_EV_EVENT_BATCH = 100,
};

TLB_EXTERN_C_BEGIN

/* Implemented per platform */

int tlb_evl_init(struct tlb_event_loop *loop);
void tlb_evl_cleanup(struct tlb_event_loop *loop);

/* Initializes specific types to the loop */
void tlb_evl_impl_fd_init(struct tlb_subscription *sub);
void tlb_evl_impl_trigger_init(struct tlb_subscription *sub);

/* All subscribe/unsubscribe implementations are the same */
int tlb_evl_impl_subscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub);
int tlb_evl_impl_unsubscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub);

TLB_EXTERN_C_END

#endif /* EVENT_LOOP_H */
