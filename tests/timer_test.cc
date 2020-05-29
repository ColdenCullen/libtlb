#include "tlb/event_loop.h"
#include "tlb/pipe.h"

#include <gtest/gtest.h>
#include <string.h>

#include "test_helpers.h"
#include <chrono>

namespace tlb_test {
namespace {
constexpr auto duration = std::chrono::seconds(1);
constexpr auto s_timer_epsilon = std::chrono::milliseconds(50);

class TimerTest : public TlbTest {};

TEST_P(TimerTest, Timer) {
  struct TestState {
    TimerTest *test = nullptr;
    size_t trigger_count = 0;
  } state;
  state.test = this;

  tlb_handle timer = tlb_evl_add_timer(
      loop(), std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(),
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        state->trigger_count++;
      },
      &state);
  ASSERT_NE(nullptr, timer);

  // Sleep, and include a bit of extra time for safety
  std::this_thread::sleep_for(duration + s_timer_epsilon);

  wait([&]() { return state.trigger_count == 1; });
}

TEST_P(TimerTest, TimerUnsubscribe) {
  struct TestState {
    TimerTest *test = nullptr;
    size_t trigger_count = 0;
  } state;
  state.test = this;

  tlb_handle timer = tlb_evl_add_timer(
      loop(), std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(),
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        auto lock = state->test->lock();
        state->trigger_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, timer);

  EXPECT_EQ(0, state.trigger_count);

  // Make sure no more events show up after unsubscribing
  ASSERT_EQ(0, tlb_evl_remove(loop(), timer));

  // Sleep, and include a bit of extra time for safety
  std::this_thread::sleep_for(duration + s_timer_epsilon);

  wait([&]() { return state.trigger_count == 0; });
}

TLB_INSTANTIATE_TEST(TimerTest);

}  // namespace
}  // namespace tlb_test
