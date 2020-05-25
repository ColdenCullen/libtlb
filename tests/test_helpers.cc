#include "test_helpers.h"

#include "tlb/allocator.h"

namespace tlb_test {

static void *s_malloc(void *userdata, size_t size) {
  return malloc(size);
}

static void *s_calloc(void *userdata, size_t num, size_t size) {
  return calloc(num, size);
}

static void s_free(void *userdata, void *buffer) {
  free(buffer);
}

extern tlb_allocator *test_allocator() {
  static tlb_allocator::tlb_allocator_vtable vtable = {
      .malloc = s_malloc,
      .calloc = s_calloc,
      .free = s_free,
  };
  static tlb_allocator alloc = {
      &vtable,
      nullptr,
  };

  return &alloc;
}
}  // namespace tlb_test