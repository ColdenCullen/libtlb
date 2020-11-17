#ifndef TESTS_TEST_HELPERS_H
#define TESTS_TEST_HELPERS_H

#include "tlb/allocator.h"
#include "tlb/event_loop.h"
#include "tlb/tlb.h"

#include <gtest/gtest.h>

#include <atomic>
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

std::ostream &Log();

void PrintTo(const LoopMode &mode, std::ostream *out);

constexpr size_t s_event_budget = 100;
constexpr uint64_t s_test_value = 0x0BADFACE;
constexpr size_t s_thread_count = 5;

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

  void wait(const std::function<bool()> &predicate);

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
