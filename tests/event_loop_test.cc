#include "tlb/event_loop.h"

#include "tlb/pipe.h"

#include <gtest/gtest.h>

#include "test_helpers.h"

namespace tlb {
namespace {

constexpr size_t s_event_budget = 100;
constexpr uint64_t s_test_value = 0x0BADFACE;

class EventLoopTest : public ::testing::Test {
 public:
  void SetUp() override {
    alloc = tlb::test_allocator();
    ASSERT_NE(nullptr, alloc);
    loop = tlb_evl_new(alloc);
    ASSERT_NE(nullptr, loop);
  }

  void TearDown() override {
    // Make sure there are no leftover events
    EXPECT_EQ(0, tlb_evl_handle_events(loop, s_event_budget));

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
  struct TestState {
    EventLoopPipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = tlb_evl_fd_add(
      loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        state->read_count++;
      },
      &state);
  ASSERT_NE(nullptr, sub);

  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));

  EXPECT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_EQ(1, state.read_count);
}

TEST_F(EventLoopPipeTest, PipeRereadable) {
  struct TestState {
    EventLoopPipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = tlb_evl_fd_add(
      loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        state->read_count++;
      },
      &state);
  ASSERT_NE(nullptr, sub);

  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));
  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));

  // Handle read
  ASSERT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_EQ(1, state.read_count);
  // Handle re-read
  EXPECT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_EQ(2, state.read_count);
}

TEST_F(EventLoopPipeTest, PipeWritable) {
  struct TestState {
    EventLoopPipeTest *test = nullptr;
    bool wrote = false;
  } state;
  state.test = this;

  tlb_handle sub = tlb_evl_fd_add(
      loop, pipe.fd_write, TLB_EV_WRITE,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        if (!state->wrote) {
          tlb_pipe_write(&state->test->pipe, &s_test_value, sizeof(s_test_value));
          state->wrote = true;
        }
      },
      &state);
  ASSERT_NE(nullptr, sub);

  EXPECT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  ASSERT_TRUE(state.wrote);

  uint64_t value = 0;
  read(pipe.fd_read, &value, sizeof(value));
  EXPECT_EQ(s_test_value, value);
}

TEST_F(EventLoopPipeTest, PipeReadableWritable) {
  struct TestState {
    EventLoopPipeTest *test = nullptr;
    bool wrote = false;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle read_sub = tlb_evl_fd_add(
      loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        state->read_count++;
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
  EXPECT_FALSE(state.read_count);
  // Run the read event (and re-writable event)
  EXPECT_EQ(2, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_TRUE(state.wrote);
  EXPECT_EQ(1, state.read_count);

  // Run the re-readable event
  EXPECT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
}

TEST_F(EventLoopPipeTest, Trigger) {
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

  ASSERT_EQ(0, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_FALSE(state.triggered);

  ASSERT_EQ(0, tlb_evl_trigger_fire(loop, trigger));
  EXPECT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_TRUE(state.triggered);
}

class EventLoopSubLoopTest : public EventLoopPipeTest {
 public:
  void SetUp() override {
    EventLoopPipeTest::SetUp();

    sub_loop = tlb_evl_new(alloc);
    sub_loop_handle = tlb_evl_evl_add(loop, sub_loop);
  }

  void TearDown() override {
    // Ensure there aren't leftover events
    ASSERT_EQ(0, tlb_evl_handle_events(sub_loop, s_event_budget));

    tlb_evl_evl_remove(loop, sub_loop_handle);
    tlb_evl_destroy(sub_loop);

    EventLoopPipeTest::TearDown();
  }

  struct tlb_event_loop *sub_loop;
  tlb_handle sub_loop_handle;
};

TEST_F(EventLoopSubLoopTest, CreateDestroy) {
  EXPECT_NE(nullptr, sub_loop_handle);
}

TEST_F(EventLoopSubLoopTest, PipeReadable) {
  struct TestState {
    EventLoopPipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = tlb_evl_fd_add(
      sub_loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        state->read_count++;
      },
      &state);
  ASSERT_NE(nullptr, sub);

  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));

  // Handle read
  ASSERT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_EQ(1, state.read_count);
}

TEST_F(EventLoopSubLoopTest, PipeRereadable) {
  struct TestState {
    EventLoopPipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = tlb_evl_fd_add(
      sub_loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        state->read_count++;
      },
      &state);
  ASSERT_NE(nullptr, sub);

  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));
  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));

  // Handle read
  ASSERT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_EQ(1, state.read_count);
  // Handle reread
  ASSERT_EQ(1, tlb_evl_handle_events(loop, s_event_budget));
  EXPECT_EQ(2, state.read_count);
}

}  // namespace
}  // namespace tlb
