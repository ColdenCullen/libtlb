#ifndef TLB_EVENT_LOOP_H
#define TLB_EVENT_LOOP_H

#include <tlb/core.h>

#include <tlb/allocator.h>

struct tlb_event_loop;

enum tlb_events {
  TLB_EV_READ = TLB_BIT(0),
  TLB_EV_WRITE = TLB_BIT(1),
  TLB_EV_CLOSE = TLB_BIT(2),
  TLB_EV_ERROR = TLB_BIT(3),
};

typedef void tlb_on_event(int events, void *userdata);

typedef void *tlb_handle;

TLB_EXTERN_C_BEGIN

/** Event loop lifecycle management */
struct tlb_event_loop *tlb_event_loop_new(struct tlb_allocator *alloc);
void tlb_event_loop_destroy(struct tlb_event_loop *loop);

/** Subscribe a file descriptor to the event loop. */
tlb_handle tlb_event_loop_subscribe(struct tlb_event_loop *loop, int fd, int events, tlb_on_event *on_event,
                                    void *userdata);
int tlb_event_loop_unsubscribe(struct tlb_event_loop *loop, tlb_handle subscription);

/** Add a user trigger to the event loop */
tlb_handle tlb_event_loop_trigger_add(struct tlb_event_loop *loop, tlb_on_event *trigger, void *userdata);
int tlb_event_loop_trigger_remove(struct tlb_event_loop *loop, tlb_handle trigger);
int tlb_trigger_fire(struct tlb_event_loop *loop, tlb_handle trigger);

/** Handles up to budget pending events without waiting. */
int tlb_event_loop_handle_events(struct tlb_event_loop *loop, size_t budget);

TLB_EXTERN_C_END

#endif /* TLB_EVENT_LOOP_H */
