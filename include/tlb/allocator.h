#ifndef TLB_ALLOCATOR_H
#define TLB_ALLOCATOR_H

#include <tlb/core.h>

struct tlb_allocator {
  struct tlb_allocator_vtable {
    /* Required */
    void *(*malloc)(size_t size);
    /* Optional */
    void *(*calloc)(size_t num, size_t size);

    /* Required */
    void (*free)(void *buffer);
  } * vtable;

  void *userdata;
};

TLB_EXTERN_C_BEGIN

inline void *tlb_malloc(struct tlb_allocator *alloc, size_t size) {
  TLB_ASSERT(alloc);
  TLB_ASSERT(alloc->vtable->malloc);
  TLB_ASSERT(size > 0);

  return alloc->vtable->malloc(size);
}

inline void *tlb_calloc(struct tlb_allocator *alloc, size_t num, size_t size) {
  TLB_ASSERT(alloc);
  TLB_ASSERT(alloc->vtable->calloc || alloc->vtable->malloc);
  TLB_ASSERT(num > 0);
  TLB_ASSERT(size > 0);

  if (alloc->vtable->calloc) {
    return alloc->vtable->calloc(num, size);
  }

  const size_t alloc_size = num * size;
  void *buffer = alloc->vtable->malloc(alloc_size);
  memset(buffer, 0, alloc_size);
  return buffer;
}

inline void tlb_free(struct tlb_allocator *alloc, void *buffer) {
  TLB_ASSERT(alloc);
  TLB_ASSERT(alloc->vtable->free);
  TLB_ASSERT(buffer != NULL);

  alloc->vtable->free(buffer);
}

TLB_EXTERN_C_END

#endif /* TLB_ALLOCATOR_H */
