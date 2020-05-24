#ifndef EVENT_LOOP_H /* NOLINT(llvm-header-guard) */
#define EVENT_LOOP_H

#include "tlb/event_loop.h"

#include "tlb/core.h"

enum tlb_sub_flags {
  TLB_SUB_ONESHOT = TLB_BIT(0),
  TLB_SUB_EDGE = TLB_BIT(1),
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
};

enum {
  TLB_EV_EVENT_BATCH = 100,
};

TLB_EXTERN_C_BEGIN

/* Implemented per platform */
int tlb_ev_init(struct tlb_event_loop *loop);
void tlb_ev_cleanup(struct tlb_event_loop *loop);

int tlb_fd_subscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub);
int tlb_fd_unsubscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub);

int tlb_trigger_add(struct tlb_event_loop *loop, struct tlb_subscription *sub);
int tlb_trigger_remove(struct tlb_event_loop *loop, struct tlb_subscription *sub);

TLB_EXTERN_C_END

#endif /* EVENT_LOOP_H */
