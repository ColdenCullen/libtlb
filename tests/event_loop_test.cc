#include <tlb/event_loop.h>
#include <tlb/pipe.h>

#include <gtest/gtest.h>

#include "test_helpers.h"

namespace tlb {
namespace {

constexpr size_t s_event_budget = 100;

class EventLoopTest : public ::testing::Test {
 public:
  void SetUp() override {
    alloc = tlb::test_allocator();
    loop = tlb_evl_new(alloc);
  }

  void TearDown() override {
    tlb_evl_destroy(loop);
  }

  tlb_allocator *alloc = nullptr;
  tlb_event_loop *loop = nullptr;
};

TEST_F(EventLoopTest, CreateDestroy) {
}

class EventLoopPipeTest : public EventLoopTest {
 public:
  void SetUp() override {
    tlb_pipe_open(&pipe);

    EventLoopTest::SetUp();
  }

  void TearDown() override {
    tlb_pipe_close(&pipe);

    EventLoopTest::TearDown();
  }

  tlb_pipe pipe;
};

TEST_F(EventLoopPipeTest, PipeReadable) {
  static const uint64_t s_test_value = 0x0BADFACE;

  struct TestState {
    EventLoopPipeTest *test = nullptr;
    bool read = false;
  } state;
  state.test = this;

  tlb_handle sub = tlb_evl_fd_add(
      loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        state->read = true;
      },
      &state);
  ASSERT_NE(nullptr, sub);

  write(pipe.fd_write, &s_test_value, sizeof(s_test_value));

  EXPECT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_TRUE(state.read);
}

TEST_F(EventLoopPipeTest, PipeWritable) {
  static const uint64_t s_test_value = 0x0BADFACE;

  struct TestState {
    EventLoopPipeTest *test = nullptr;
    bool wrote = false;
  } state;
  state.test = this;

  tlb_handle sub = tlb_evl_fd_add(
      loop, pipe.fd_write, TLB_EV_WRITE,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        tlb_pipe_write(&state->test->pipe, &s_test_value, sizeof(s_test_value));
        state->wrote = true;
      },
      &state);
  ASSERT_NE(nullptr, sub);

  EXPECT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));

  uint64_t value = 0;
  read(pipe.fd_read, &value, sizeof(value));
  EXPECT_EQ(s_test_value, value);

  EXPECT_TRUE(state.wrote);
}

TEST_F(EventLoopPipeTest, PipeReadableWritable) {
  static const uint64_t s_test_value = 0x0BADFACE;

  struct TestState {
    EventLoopPipeTest *test = nullptr;
    bool wrote = false;
    bool read = false;
  } state;
  state.test = this;

  tlb_handle read_sub = tlb_evl_fd_add(
      loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        state->read = true;
      },
      &state);
  ASSERT_NE(nullptr, read_sub);

  tlb_handle write_sub = tlb_evl_fd_add(
      loop, pipe.fd_write, TLB_EV_WRITE,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        if (!state->wrote) {
          tlb_pipe_write(&state->test->pipe, &s_test_value, sizeof(s_test_value));
          state->wrote = true;
        }
      },
      &state);
  ASSERT_NE(nullptr, write_sub);

  // Run the write event
  EXPECT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_TRUE(state.wrote);
  EXPECT_FALSE(state.read);
  // Run the read event (and re-writable event)
  EXPECT_EQ(2, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_TRUE(state.wrote);
  EXPECT_TRUE(state.read);
}

TEST_F(EventLoopPipeTest, Trigger) {
  static const uint64_t s_test_value = 0x0BADFACE;

  struct TestState {
    EventLoopPipeTest *test = nullptr;
    bool triggered = false;
  } state;
  state.test = this;

  tlb_handle trigger = tlb_evl_trigger_add(
      loop,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        state->triggered = true;
      },
      &state);
  ASSERT_NE(nullptr, trigger);

  EXPECT_EQ(0, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_FALSE(state.triggered);

  ASSERT_EQ(0, tlb_evl_trigger_fire(loop, trigger));
  EXPECT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_TRUE(state.triggered);
}

}  // namespace
}  // namespace tlb
