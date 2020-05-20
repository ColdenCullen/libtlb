#include <tlb/allocator.h>

void *tlb_malloc(struct tlb_allocator *alloc, size_t size) {
  TLB_ASSERT(alloc);
  TLB_ASSERT(alloc->vtable->malloc);
  TLB_ASSERT(size > 0);

  return alloc->vtable->malloc(alloc->userdata, size);
}

void *tlb_calloc(struct tlb_allocator *alloc, size_t num, size_t size) {
  TLB_ASSERT(alloc);
  TLB_ASSERT(alloc->vtable->calloc || alloc->vtable->malloc);
  TLB_ASSERT(num > 0);
  TLB_ASSERT(size > 0);

  if (alloc->vtable->calloc) {
    return alloc->vtable->calloc(alloc->userdata, num, size);
  }

  const size_t alloc_size = num * size;
  void *buffer = alloc->vtable->malloc(alloc->userdata, alloc_size);
  memset(buffer, 0, alloc_size);
  return buffer;
}

void tlb_free(struct tlb_allocator *alloc, void *buffer) {
  TLB_ASSERT(alloc);
  TLB_ASSERT(alloc->vtable->free);
  TLB_ASSERT(buffer != NULL);

  alloc->vtable->free(alloc->userdata, buffer);
}
