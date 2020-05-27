#include "tlb/tlb.h"

#include "tlb/event_loop.h"
#include "tlb/pipe.h"

#include <gtest/gtest.h>

#include "test_helpers.h"
#include <array>
#include <condition_variable>
#include <mutex>

namespace tlb_test {
namespace {

constexpr size_t s_event_budget = 100;
constexpr uint64_t s_test_value = 0x0BADFACE;
constexpr size_t s_thread_count = 2;
constexpr auto s_default_timeout = std::chrono::seconds(10);

class TlbTest : public ::testing::Test {
 public:
  void SetUp() override {
    alloc = tlb_test::test_allocator();
    ASSERT_NE(nullptr, alloc);
    boi = tlb_new(alloc, {.max_thread_count = 2});
    ASSERT_NE(nullptr, boi);
    loop = tlb_get_evl(boi);
    ASSERT_NE(nullptr, loop);

    EXPECT_EQ(0, tlb_start(boi));
  }

  void TearDown() override {
    EXPECT_EQ(0, tlb_stop(boi));

    tlb_destroy(boi);
  }

  tlb_allocator *alloc = nullptr;
  tlb *boi = nullptr;
  tlb_event_loop *loop = nullptr;
};

class TlbPipeTest : public TlbTest {
 public:
  void SetUp() override {
    TlbTest::SetUp();

    tlb_pipe_open(&pipe);
  }

  void TearDown() override {
    tlb_pipe_close(&pipe);

    TlbTest::TearDown();
  }

  tlb_pipe pipe;
};

TEST_F(TlbPipeTest, PipeReadableTest) {
  struct TestState {
    TlbPipeTest *test = nullptr;
    int read_count = 0;

    std::mutex lock;
    std::condition_variable read_done;
  } state;
  state.test = this;

  tlb_handle sub = tlb_evl_add_fd(
      loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        state->read_count++;

        std::unique_lock<std::mutex> lock(state->lock);
        state->read_done.notify_one();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  std::unique_lock<std::mutex> lock(state.lock);
  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));

  EXPECT_EQ(std::cv_status::no_timeout, state.read_done.wait_for(lock, s_default_timeout));
  EXPECT_EQ(1, state.read_count);
}

}  // namespace
}  // namespace tlb_test
