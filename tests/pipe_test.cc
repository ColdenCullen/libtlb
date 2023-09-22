
#include "tlb/pipe.h"

#include "tlb/event_loop.h"

#include <gtest/gtest.h>
#include <string.h>

#include "test_helpers.h"
#include <unordered_set>

namespace tlb_test {
namespace {

class PipeTest : public TlbTest {
 public:
  void SetUp() override {
    TlbTest::SetUp();

    ASSERT_EQ(0, tlb_pipe_open(&pipe));
  }

  void TearDown() override {
    auto open_subs_copy = open_subscriptions;
    for (tlb_handle sub : open_subs_copy) {
      Unsubscribe(sub);
    }
    open_subs_copy.clear();

    tlb_pipe_close(&pipe);

    TlbTest::TearDown();
  }

  tlb_handle SubscribeRead(tlb_on_event *on_event, void *userdata) {
    const tlb_handle handle = tlb_evl_add_fd(loop(), pipe.fd_read, TLB_EV_READ, on_event, userdata);
    open_subscriptions.emplace(handle);
    return handle;
  }

  tlb_handle SubscribeWrite(tlb_on_event *on_event, void *userdata) {
    const tlb_handle handle = tlb_evl_add_fd(loop(), pipe.fd_write, TLB_EV_WRITE, on_event, userdata);
    open_subscriptions.emplace(handle);
    return handle;
  }

  void Unsubscribe(tlb_handle subscription) {
    open_subscriptions.erase(subscription);
    EXPECT_EQ(0, tlb_evl_remove(loop(), subscription)) << strerror(errno);
  }

  template <typename T>
  bool Read(T &data) {
    data = {};
    static constexpr size_t expected_size = sizeof(T);
    const ssize_t actual_size = tlb_pipe_read_buf(&pipe, &data, expected_size);
    // Check for no waiting data
    if (actual_size == -1) {
      return false;
    }
    EXPECT_EQ(expected_size, actual_size);

    Log() << "Reading from pipe " << &pipe << ": " << data << std::endl;
    return true;
  }

  template <typename T>
  void Write(const T &data) {
    Log() << "Writing to pipe " << &pipe << ": " << data << std::endl;

    static constexpr size_t size = sizeof(T);
    EXPECT_EQ(size, tlb_pipe_write_buf(&pipe, &data, size));
  }

  tlb_pipe pipe;
  std::unordered_set<tlb_handle> open_subscriptions;
};

TEST_P(PipeTest, OpenClose) {
}

TEST_P(PipeTest, SubscribeUnsubscribe) {
  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {}, nullptr);
  ASSERT_NE(nullptr, sub);

  Unsubscribe(sub);
}

TEST_P(PipeTest, Readable) {
  struct TestState {
    PipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value;
        ASSERT_TRUE(state->test->Read(value));
        EXPECT_EQ(s_test_value, value);

        auto lock = state->test->lock();
        state->read_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  Write(s_test_value);
  wait([&]() { return 1 == state.read_count; });
}

TEST_P(PipeTest, ReadableRecursive) {
  static constexpr size_t kTargetReadCount = 100;
  struct TestState {
    PipeTest *test = nullptr;
    size_t read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        size_t value;
        ASSERT_TRUE(state->test->Read<size_t>(value));
        EXPECT_EQ(state->read_count, value);

        if (++state->read_count == kTargetReadCount) {
          auto lock = state->test->lock();
          state->test->notify();
        } else {
          state->test->Write(state->read_count);
        }
      },
      &state);
  ASSERT_NE(nullptr, sub);

  Write(state.read_count);
  wait([&]() { return kTargetReadCount == state.read_count; });
}

TEST_P(PipeTest, ReadableUnsubscribe) {
  struct TestState {
    PipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value;
        ASSERT_TRUE(state->test->Read(value));
        EXPECT_EQ(s_test_value, value);

        auto lock = state->test->lock();
        state->read_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  Write(s_test_value);

  // Handle read
  wait([&]() { return 1 == state.read_count; });

  // Unsubscribe and ensure no more events show up
  Unsubscribe(sub);
  Write(s_test_value);
  wait([&]() { return 1 == state.read_count; });
}

TEST_P(PipeTest, RecursiveUnsubscribe) {
  struct TestState {
    PipeTest *test = nullptr;
    bool read = false;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        state->test->Unsubscribe(handle);

        auto lock = state->test->lock();
        state->read = true;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  Write(s_test_value);
  wait([&]() { return state.read; });
}

TEST_P(PipeTest, Writable) {
  struct TestState {
    PipeTest *test = nullptr;
    bool wrote = false;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeWrite(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        state->test->Write(s_test_value);
        state->test->Unsubscribe(handle);

        auto lock = state->test->lock();
        state->wrote = true;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  wait([&]() { return state.wrote; });

  uint64_t value;
  ASSERT_TRUE(Read(value));
  EXPECT_EQ(s_test_value, value);
}

TEST_P(PipeTest, ReadableWritable) {
  struct TestState {
    PipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle read_sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        uint64_t value;
        ASSERT_TRUE(state->test->Read(value));
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
        state->test->Write(s_test_value);
        // Make sure we don't get writable again
        state->test->Unsubscribe(handle);
      },
      &state);
  ASSERT_NE(nullptr, write_sub);

  // Run the events
  wait([&]() { return state.read_count == 1; });
}

TLB_INSTANTIATE_TEST(PipeTest);

}  // namespace
}  // namespace tlb_test
