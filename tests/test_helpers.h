#ifndef TESTS_TEST_HELPERS_H
#define TESTS_TEST_HELPERS_H

#include "tlb/allocator.h"
#include "tlb/private/event_loop.h"
#include "tlb/tlb.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace tlb_test {
extern tlb_allocator *test_allocator();

enum class LoopMode {
  RawLoop,
  NestedLoop,
  TlbLoop,
};

constexpr size_t s_event_budget = 100;
constexpr uint64_t s_test_value = 0x0BADFACE;
constexpr size_t s_thread_count = 5;
constexpr auto s_timer_epsilon = std::chrono::milliseconds(50);

class TlbTest : public ::testing::TestWithParam<std::tuple<LoopMode, size_t>> {
 public:
  TlbTest() : unique_lock(mutex) {
  }

  static tlb_allocator *alloc();

  static LoopMode mode() {
    return std::get<0>(GetParam());
  }

  static size_t thread_count() {
    return std::get<1>(GetParam());
  }

  tlb_event_loop *loop() const {
    return evl;
  }

  std::unique_lock<std::mutex> lock() {
    return std::unique_lock<std::mutex>(mutex);
  }

  void notify() {
    if (thread_count() > 0) {
      on_event.notify_one();
    }
  }

  template <typename PredT>
  void wait(const PredT &predicate) {
    const auto run_until = std::chrono::steady_clock::now() + s_timer_epsilon;
    bool passed = false;
    // Fire the loop if necessary
    if (thread_count() == 0) {
      unique_lock.unlock();
      while (std::chrono::steady_clock::now() < run_until) {
        tlb_evl_handle_events(handled_loop(), 100, 0);
        if (predicate()) {
          passed = true;
          break;
        }
      }
      unique_lock.lock();
    } else {
      passed = on_event.wait_until(unique_lock, run_until, predicate);
    }

    if (!passed) {
      ADD_FAILURE() << "Wait operation timed out";
    } else {
      // If it passed, wait a second (if multithreaded) and try again
      if (thread_count() == 0) {
        unique_lock.unlock();
        tlb_evl_handle_events(handled_loop(), 100, 0);
        unique_lock.lock();
      } else {
        std::this_thread::sleep_for(s_timer_epsilon);
      }
      if (!predicate()) {
        ADD_FAILURE() << "Test passed, but predicate returned false after waiting";
      }
    }
  }

  void HandleEvents();

  void SetUp() override;
  void TearDown() override;

  void Stop();
  void Restart();

 private:
  tlb_event_loop *handled_loop();

  // Public loop tests will subscribe on
  tlb_event_loop *evl;

  union {
    struct {
      tlb *tlb;
    } tlb;
    struct {
      tlb_event_loop *super_loop;
      tlb_handle loop_sub;
    } super_loop;
  };

  std::atomic<bool> running = {false};
  std::vector<std::thread> threads;

  std::mutex mutex;
  std::unique_lock<std::mutex> unique_lock;
  std::condition_variable on_event;
};

#define TLB_INSTANTIATE_TEST(suite)                                                                           \
  INSTANTIATE_TEST_SUITE_P(                                                                                   \
      RawLoop, suite,                                                                                         \
      ::testing::Combine(::testing::Values(LoopMode::RawLoop), ::testing::Values<size_t>(0, 1, 2, 4, 8)));    \
  INSTANTIATE_TEST_SUITE_P(                                                                                   \
      NestedLoop, suite,                                                                                      \
      ::testing::Combine(::testing::Values(LoopMode::NestedLoop), ::testing::Values<size_t>(0, 1, 2, 4, 8))); \
  INSTANTIATE_TEST_SUITE_P(                                                                                   \
      TlbLoop, suite, ::testing::Combine(::testing::Values(LoopMode::TlbLoop), ::testing::Values<size_t>(1, 2, 4, 8)))
}  // namespace tlb_test

#endif /* TESTS_TEST_HELPERS_H */
