#include "tlb/tlb.h"

#include "tlb/pipe.h"

#include <gtest/gtest.h>

#include "test_helpers.h"
#include <array>

namespace tlb_test {
namespace {

constexpr size_t s_event_budget = 100;
constexpr uint64_t s_test_value = 0x0BADFACE;
constexpr size_t s_thread_count = 2;

class TlbTest : public ::testing::Test {
 public:
  void SetUp() override {
    alloc = tlb_test::test_allocator();
    ASSERT_NE(nullptr, alloc);
    boi = tlb_new(alloc, {.max_thread_count = 2});
    ASSERT_NE(nullptr, boi);
  }

  void TearDown() override {
    tlb_destroy(boi);
  }

  tlb_allocator *alloc = nullptr;
  tlb *boi = nullptr;
};  // namespace

TEST_F(TlbTest, CreateDestroy) {
}

TEST_F(TlbTest, StartStop) {
  EXPECT_EQ(0, tlb_start(boi));
  EXPECT_EQ(0, tlb_stop(boi));
}

}  // namespace
}  // namespace tlb_test
