#ifndef TLB_TLB_H
#define TLB_TLB_H

#include "tlb/allocator.h"
#include "tlb/event_loop.h"

/**
 * NOTE: This structure is THREAD-COMPATIBLE, not thread-safe. All operations on the underlying event loop are thread
 * safe, but you should not call multiple functions declared in this file from multiple threads at the same time.
 */
struct tlb;

struct tlb_options {
  size_t max_thread_count;
};

TLB_EXTERN_C_BEGIN

struct tlb *tlb_new(struct tlb_allocator *alloc, struct tlb_options options);
void tlb_destroy(struct tlb *tlb);

int tlb_start(struct tlb *tlb);
int tlb_stop(struct tlb *tlb);

/** Gets the event loop that things may be subscribed to */
struct tlb_event_loop *tlb_get_evl(struct tlb *tlb);

TLB_EXTERN_C_END

#endif /* TLB_TLB_H */
