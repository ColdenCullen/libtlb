#include "tlb/tlb.h"

#include "tlb/allocator.h"
#include "tlb/event_loop.h"

struct tlb {
  struct tlb_allocator *alloc;
  struct tlb_options options;

  struct tlb_event_loop *super_loop;
};

struct tlb *tlb_new(struct tlb_allocator *alloc, struct tlb_options options) {
  struct tlb *tlb = TLB_CHECK(NULL !=, tlb_calloc(alloc, 1, sizeof(struct tlb)));
  tlb->alloc = alloc;
  tlb->options = options;

  tlb->super_loop = TLB_CHECK_GOTO(NULL !=, tlb_evl_new(alloc), cleanup);

  return tlb;

cleanup:
  tlb_destroy(tlb);
  return NULL;
}

void tlb_destroy(struct tlb *tlb) {
  tlb_evl_destroy(tlb->super_loop);
  tlb_free(tlb->alloc, tlb);
}
