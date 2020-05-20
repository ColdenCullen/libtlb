#ifndef TLB_ALLOCATOR_H
#define TLB_ALLOCATOR_H

#include <tlb/core.h>

struct tlb_allocator {
  struct tlb_allocator_vtable {
    /* Required */
    void *(*malloc)(void *userdata, size_t size);
    /* Optional */
    void *(*calloc)(void *userdata, size_t num, size_t size);

    /* Required */
    void (*free)(void *userdata, void *buffer);
  } * vtable;

  void *userdata;
};

TLB_EXTERN_C_BEGIN

void *tlb_malloc(struct tlb_allocator *alloc, size_t size);

void *tlb_calloc(struct tlb_allocator *alloc, size_t num, size_t size);

void tlb_free(struct tlb_allocator *alloc, void *buffer);

TLB_EXTERN_C_END

#endif /* TLB_ALLOCATOR_H */
