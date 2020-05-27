#include "tlb/event_loop.h"
#include "tlb/pipe.h"

#include <gtest/gtest.h>
#include <string.h>

#include "test_helpers.h"
#include <array>
#include <chrono>
#include <condition_variable>
#include <thread>

namespace tlb_test {
namespace {
class TriggerTest : public TlbTest {};

TEST_P(TriggerTest, Trigger) {
  struct TestState {
    TriggerTest *test = nullptr;
    size_t trigger_count = 0;
  } state;
  state.test = this;

  tlb_handle trigger = tlb_evl_add_trigger(
      loop(),
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        auto lock = state->test->lock();
        state->trigger_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, trigger);

  EXPECT_EQ(0, state.trigger_count);

  ASSERT_EQ(0, tlb_evl_trigger_fire(loop(), trigger));
  wait([&]() { return 1 == state.trigger_count; });

  ASSERT_EQ(0, tlb_evl_trigger_fire(loop(), trigger));
  wait([&]() { return 2 == state.trigger_count; });
}

TEST_P(TriggerTest, MultiTrigger) {
  struct TestState {
    TriggerTest *test = nullptr;
    size_t trigger_count = 0;
  } state;
  state.test = this;

  tlb_handle trigger = tlb_evl_add_trigger(
      loop(),
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        auto lock = state->test->lock();
        state->trigger_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, trigger);

  EXPECT_EQ(0, state.trigger_count);

  Stop();

  // Fire trigger twice while the loop is not active
  ASSERT_EQ(0, tlb_evl_trigger_fire(loop(), trigger));
  ASSERT_EQ(0, tlb_evl_trigger_fire(loop(), trigger));

  Restart();
  wait([&]() { return 1 == state.trigger_count; });

  // Make sure only one event goes
  EXPECT_EQ(1, state.trigger_count);
}

TEST_P(TriggerTest, RecursiveTrigger) {
  struct TestState {
    TriggerTest *test = nullptr;
    size_t target_triggers = std::max(thread_count(), 1UL) * 2;
    size_t trigger_count = 0;
  } state;
  state.test = this;

  tlb_handle trigger = tlb_evl_add_trigger(
      loop(),
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        if (state->trigger_count < state->target_triggers) {
          auto lock = state->test->lock();
          state->trigger_count++;
          ASSERT_EQ(0, tlb_evl_trigger_fire(state->test->loop(), handle));
          state->test->notify();
        }
      },
      &state);
  ASSERT_NE(nullptr, trigger);

  EXPECT_EQ(0, state.trigger_count);

  // Fire the trigger
  ASSERT_EQ(0, tlb_evl_trigger_fire(loop(), trigger));

  // Wait until all the triggers have fired
  wait([&]() { return state.trigger_count == state.target_triggers; });
}

TEST_P(TriggerTest, RecursiveTriggerRemove) {
  struct TestState {
    TriggerTest *test = nullptr;
    size_t trigger_count = 0;
  } state;
  state.test = this;

  tlb_handle trigger = tlb_evl_add_trigger(
      loop(),
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        auto lock = state->test->lock();
        state->trigger_count++;
        // Remove the trigger
        ASSERT_EQ(0, tlb_evl_remove(state->test->loop(), handle));
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, trigger);

  ASSERT_EQ(0, tlb_evl_handle_events(loop(), s_event_budget, TLB_WAIT_NONE));
  EXPECT_EQ(0, state.trigger_count);

  // Fire the trigger
  ASSERT_EQ(0, tlb_evl_trigger_fire(loop(), trigger));

  // Make sure only one event goes
  wait([&]() { return 1 == state.trigger_count; });
}

TEST_P(TriggerTest, TriggerUnsubscribe) {
  struct TestState {
    TriggerTest *test = nullptr;
    size_t trigger_count = 0;
  } state;
  state.test = this;

  tlb_handle trigger = tlb_evl_add_trigger(
      loop(),
      +[](tlb_handle handle, int events, void *userdata) {
        TestState *state = static_cast<TestState *>(userdata);
        auto lock = state->test->lock();
        state->trigger_count++;
        state->test->notify();
      },
      &state);
  ASSERT_NE(nullptr, trigger);

  EXPECT_EQ(0, state.trigger_count);

  ASSERT_EQ(0, tlb_evl_trigger_fire(loop(), trigger));
  wait([&]() { return state.trigger_count == 1; });

  // Make sure no more events show up after unsubscribing
  ASSERT_EQ(0, tlb_evl_remove(loop(), trigger));

  wait([&]() { return state.trigger_count == 1; });
}

TLB_INSTANTIATE_TEST(TriggerTest);

}  // namespace
}  // namespace tlb_test
