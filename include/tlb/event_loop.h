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

#define TLB_WAIT_NONE ((int)0)
#define TLB_WAIT_INDEFINITE ((int)-1)

TLB_EXTERN_C_BEGIN

/** Event loop lifecycle management */
struct tlb_event_loop *tlb_evl_new(struct tlb_allocator *alloc);
void tlb_evl_destroy(struct tlb_event_loop *loop);

/** Subscribe a file descriptor to the event loop. */
tlb_handle tlb_evl_add_fd(struct tlb_event_loop *loop, int fd, int events, tlb_on_event *on_event, void *userdata);

/** Add a timer to fire in timeout milliseconds */
tlb_handle tlb_evl_add_timer(struct tlb_event_loop *loop, int timeout, tlb_on_event *trigger, void *userdata);

/** Add a sub-loop */
tlb_handle tlb_evl_add_evl(struct tlb_event_loop *loop, struct tlb_event_loop *sub_loop);

/** Remove a subscription from the loop */
int tlb_evl_remove(struct tlb_event_loop *loop, tlb_handle subscription);

/** Handles up to budget events, waiting for up to timeout milliseconds (or 0 to not wait, or -1 to wait forever) */
int tlb_evl_handle_events(struct tlb_event_loop *loop, size_t budget, int timeout);

TLB_EXTERN_C_END

#endif /* TLB_EVENT_LOOP_H */
