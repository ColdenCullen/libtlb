#ifndef TLB_TLB_H
#define TLB_TLB_H

#include "tlb/allocator.h"
#include "tlb/event_loop.h"

struct tlb;

struct tlb_options {
  size_t max_thread_count;
};

TLB_EXTERN_C_BEGIN

struct tlb *tlb_new(struct tlb_allocator *alloc, struct tlb_options options);
void tlb_destroy(struct tlb *tlb);

TLB_EXTERN_C_END

#endif /* TLB_TLB_H */
