#include <tlb/event_loop.h>

#include <gtest/gtest.h>

#include "test_helpers.h"
#include "tlb/allocator.h"

namespace tlb {

class EventLoopTest : public ::testing::Test {
 public:
  void SetUp() override {
    alloc = tlb::test_allocator();
  }

 protected:
  tlb_allocator *alloc = nullptr;
};

TEST_F(EventLoopTest, CreateDestroy) {
  tlb_event_loop *loop = tlb_event_loop_new(alloc);
  tlb_event_loop_destroy(loop);
}

}  // namespace tlb
