
#include "tlb/pipe.h"

#include "tlb/event_loop.h"

#include <gtest/gtest.h>
#include <string.h>

#include "test_helpers.h"
#include <array>
#include <chrono>

namespace tlb_test {
namespace {

class PipeTest : public TlbTest {
 public:
  void SetUp() override {
    TlbTest::SetUp();

    ASSERT_EQ(0, tlb_pipe_open(&pipe));
  }

  void TearDown() override {
    tlb_pipe_close(&pipe);

    TlbTest::TearDown();
  }

  tlb_handle SubscribeRead(tlb_on_event *on_event, void *userdata) {
    return tlb_evl_add_fd(loop(), pipe.fd_read, TLB_EV_READ, on_event, userdata);
  }

  tlb_handle SubscribeWrite(tlb_on_event *on_event, void *userdata) {
    return tlb_evl_add_fd(loop(), pipe.fd_write, TLB_EV_WRITE, on_event, userdata);
  }

  tlb_pipe pipe;
};

TEST_P(PipeTest, OpenClose) {
}

TEST_P(PipeTest, PipeReadable) {
  struct TestState {
    PipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);

        auto lock = state->test->lock();
        state->read_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));
  wait([&]() { return 1 == state.read_count; });
}

TEST_P(PipeTest, PipeReadableUnsubscribe) {
  struct TestState {
    PipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);

        auto lock = state->test->lock();
        state->read_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));

  // Handle read
  wait([&]() { return 1 == state.read_count; });

  // Unsubscribe and ensure no more events show up
  EXPECT_EQ(0, tlb_evl_remove(loop(), sub));
  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));
  wait([&]() { return 1 == state.read_count; });
}

TEST_P(PipeTest, PipeRecursiveUnsubscribe) {
  struct TestState {
    PipeTest *test = nullptr;
    bool read = false;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        tlb_evl_remove(state->test->loop(), handle);

        auto lock = state->test->lock();
        state->read = true;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));
  wait([&]() { return state.read; });
}

TEST_P(PipeTest, PipeDoubleReadable) {
  struct TestState {
    PipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);

        auto lock = state->test->lock();
        state->read_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));
  tlb_pipe_write(&pipe, &s_test_value, sizeof(s_test_value));

  // Handle read
  wait([&]() { return 1 == state.read_count; });
}

TEST_P(PipeTest, PipeWritable) {
  struct TestState {
    PipeTest *test = nullptr;
    bool wrote = false;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeWrite(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        tlb_pipe_write(&state->test->pipe, &s_test_value, sizeof(s_test_value));
        tlb_evl_remove(state->test->loop(), handle);

        auto lock = state->test->lock();
        state->wrote = true;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  wait([&]() { return state.wrote; });

  uint64_t value = 0;
  tlb_pipe_read(&pipe, &value, sizeof(value));
  EXPECT_EQ(s_test_value, value);
}

TEST_P(PipeTest, PipeReadableWritable) {
  struct TestState {
    PipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle read_sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value = 0;
        tlb_pipe_read(&state->test->pipe, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);

        auto lock = state->test->lock();
        state->read_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, read_sub);

  tlb_handle write_sub = SubscribeWrite(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        tlb_pipe_write(&state->test->pipe, &s_test_value, sizeof(s_test_value));
        // Make sure we don't get writable again
        tlb_evl_remove(state->test->loop(), handle);

        auto lock = state->test->lock();
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, write_sub);

  // Run the events
  wait([&]() { return state.read_count == 1; });
}

TLB_INSTANTIATE_TEST(PipeTest);

}  // namespace
}  // namespace tlb_test