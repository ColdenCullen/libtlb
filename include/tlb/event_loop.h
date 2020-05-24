#ifndef TLB_EVENT_LOOP_H
#define TLB_EVENT_LOOP_H

#include "tlb/allocator.h"

struct tlb_event_loop;

enum tlb_events {
  TLB_EV_READ = TLB_BIT(0),
  TLB_EV_WRITE = TLB_BIT(1),
  TLB_EV_CLOSE = TLB_BIT(2),
  TLB_EV_ERROR = TLB_BIT(3),
};

typedef void *tlb_handle;

typedef void tlb_on_event(tlb_handle handle, int events, void *userdata);

TLB_EXTERN_C_BEGIN

/** Event loop lifecycle management */
struct tlb_event_loop *tlb_evl_new(struct tlb_allocator *alloc);
void tlb_evl_destroy(struct tlb_event_loop *loop);

/** Subscribe a file descriptor to the event loop. */
tlb_handle tlb_evl_add_fd(struct tlb_event_loop *loop, int fd, int events, tlb_on_event *on_event, void *userdata);

/** Add a user trigger to the event loop */
tlb_handle tlb_evl_add_trigger(struct tlb_event_loop *loop, tlb_on_event *trigger, void *userdata);
int tlb_evl_trigger_fire(struct tlb_event_loop *loop, tlb_handle trigger);

/** Add a sub-loop */
tlb_handle tlb_evl_add_evl(struct tlb_event_loop *loop, struct tlb_event_loop *sub_loop);

/* All unsubscribe implementations are the same */
int tlb_evl_remove(struct tlb_event_loop *loop, tlb_handle subscription);

/** Handles up to budget pending events without waiting. */
int tlb_evl_handle_events(struct tlb_event_loop *loop, size_t budget);

TLB_EXTERN_C_END

#endif /* TLB_EVENT_LOOP_H */
