
#include "tlb/pipe.h"

#include "tlb/event_loop.h"
#include "tlb/private/event_loop.h"

#include <gtest/gtest.h>
#include <string.h>

#include "test_helpers.h"
#include <array>
#include <chrono>
#include <ratio>

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

  template <typename T>
  T Read() {
    T out;
    static constexpr size_t size = sizeof(T);
    EXPECT_EQ(size, tlb_pipe_read_buf(&pipe, &out, size));

    Log() << "Reading from pipe " << &pipe << ": " << out << std::endl;
    return out;
  }

  template <typename T>
  void Write(const T &in) {
    Log() << "Writing to pipe " << &pipe << ": " << in << std::endl;

    static constexpr size_t size = sizeof(T);
    EXPECT_EQ(size, tlb_pipe_write_buf(&pipe, &in, size));
  }

  tlb_pipe pipe;
};

TEST_P(PipeTest, OpenClose) {
}

TEST_P(PipeTest, SubscribeUnsubscribe) {
  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {}, nullptr);
  ASSERT_NE(nullptr, sub);

  EXPECT_EQ(0, tlb_evl_remove(loop(), sub)) << strerror(errno);
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
        const uint64_t value = state->test->Read<uint64_t>();
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
        const size_t value = state->test->Read<size_t>();
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
        uint64_t value = state->test->Read<uint64_t>();
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
  EXPECT_EQ(0, tlb_evl_remove(loop(), sub)) << strerror(errno);
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
        tlb_evl_remove(state->test->loop(), handle);

        auto lock = state->test->lock();
        state->read = true;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  Write(s_test_value);
  wait([&]() { return state.read; });
}

TEST_P(PipeTest, DoubleReadable) {
  struct TestState {
    PipeTest *test = nullptr;
    int read_count = 0;
  } state;
  state.test = this;

  tlb_handle sub = SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        const uint64_t value = state->test->Read<uint64_t>();
        EXPECT_EQ(s_test_value, value);

        auto lock = state->test->lock();
        state->read_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  Write(s_test_value);
  Write(s_test_value);

  wait([&]() { return 2 == state.read_count; });
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
        tlb_evl_remove(state->test->loop(), handle);

        auto lock = state->test->lock();
        state->wrote = true;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, sub);

  wait([&]() { return state.wrote; });

  const uint64_t value = Read<uint64_t>();
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
        const uint64_t value = state->test->Read<uint64_t>();
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
        tlb_evl_remove(state->test->loop(), handle);
      },
      &state);
  ASSERT_NE(nullptr, write_sub);

  // Run the events
  wait([&]() { return state.read_count == 1; });
}

TLB_INSTANTIATE_TEST(PipeTest);

class MultiPipeTest : public TlbTest {
 public:
  static constexpr size_t kPipeCount = 100;

  void SetUp() override {
    TlbTest::SetUp();

    for (auto &pipe : pipes) {
      ASSERT_EQ(0, tlb_pipe_open(&pipe));
    }
  }

  void TearDown() override {
    for (auto &pipe : pipes) {
      tlb_pipe_close(&pipe);
    }

    TlbTest::TearDown();
  }

  void SubscribeRead(tlb_on_event *on_event, void *userdata) {
    for (const auto &pipe : pipes) {
      ASSERT_NE(nullptr, tlb_evl_add_fd(loop(), pipe.fd_read, TLB_EV_READ, on_event, userdata));
    }
  }

  void SubscribeWrite(tlb_on_event *on_event, void *userdata) {
    for (const auto &pipe : pipes) {
      ASSERT_NE(nullptr, tlb_evl_add_fd(loop(), pipe.fd_write, TLB_EV_WRITE, on_event, userdata));
    }
  }

  template <typename T>
  T Read(tlb_handle handle) {
    auto *sub = static_cast<tlb_subscription *>(handle);

    T out;
    static constexpr size_t size = sizeof(T);

    tlb_pipe fake_pipe;
    fake_pipe.fd_read = sub->ident.fd;
    EXPECT_EQ(size, tlb_pipe_read_buf(&fake_pipe, &out, size));

    Log() << "Reading from pipe " << sub->ident.fd << ": " << out << std::endl;
    return out;
  }

  template <typename T>
  void Write(const T &in) {
    auto &pipe = pipes[pipe_index];
    Log() << "Writing to pipe " << &pipe << ": " << in << std::endl;

    static constexpr size_t size = sizeof(T);
    EXPECT_EQ(size, tlb_pipe_write_buf(&pipe, &in, size));

    pipe_index = (pipe_index + 6) % pipes.size();
  }

  std::array<tlb_pipe, kPipeCount> pipes;

  size_t pipe_index = 0;
};

TEST_P(MultiPipeTest, Readible) {
  static constexpr size_t kTargetReadCount = 5000;
  struct TestState {
    MultiPipeTest *test = nullptr;
    std::atomic<size_t> read_count{0};
  } state;
  state.test = this;

  SubscribeRead(
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        const uint64_t value = state->test->Read<uint64_t>(handle);

        const auto new_read_count = state->read_count.fetch_add(1) + 1;
        if (new_read_count == kTargetReadCount) {
          auto lock = state->test->lock();
          state->test->notify();
        } else {
          state->test->Write(new_read_count);
        }
      },
      &state);

  /* Start by writing to a pipe per thread */
  for (size_t i = 0; i < thread_count(); ++i) {
    Write(s_test_value);
  }
  wait([&]() { return state.read_count >= kTargetReadCount && state.read_count <= kTargetReadCount + thread_count(); },
       std::chrono::milliseconds(7500) / thread_count());
}

TLB_INSTANTIATE_TEST(MultiPipeTest);

}  // namespace
}  // namespace tlb_test
