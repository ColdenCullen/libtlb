#include "test_helpers.h"

#include "tlb/allocator.h"
#include "tlb/event_loop.h"
#include "tlb/tlb.h"

#include <iostream>

namespace tlb_test {

std::ostream &Log() {
  return std::cerr;
}

void PrintTo(const LoopMode &mode, std::ostream *out) {
  switch (mode) {
    case LoopMode::RawLoop:
      *out << "Raw";
      break;
    case LoopMode::TlbLoop:
      *out << "TLB";
      break;
  }
}

static void *s_malloc(void *userdata, size_t size) {
  return malloc(size);
}

static void *s_calloc(void *userdata, size_t num, size_t size) {
  return calloc(num, size);
}

static void s_free(void *userdata, void *buffer) {
  free(buffer);
}

tlb_allocator *test_allocator() {
  return TlbTest::alloc();
}

tlb_allocator *TlbTest::alloc() {
  static tlb_allocator::tlb_allocator_vtable vtable = {
      .malloc = s_malloc,
      .calloc = s_calloc,
      .free = s_free,
  };
  static tlb_allocator alloc = {
      &vtable,
      nullptr,
  };

  return &alloc;
}

void TlbTest::wait(const std::function<bool()> &predicate) {
  const auto timeout = std::chrono::milliseconds(50 * std::max<size_t>(thread_count(), 1));
  const auto run_until = std::chrono::steady_clock::now() + timeout;
  bool passed = false;
  // Fire the loop if necessary
  if (thread_count() == 0) {
    unique_lock.unlock();
    do {
      tlb_evl_handle_events(handled_loop(), 100, 0);
      if (predicate()) {
        passed = true;
        break;
      }
    } while (std::chrono::steady_clock::now() < run_until);
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
      std::this_thread::sleep_for(timeout);
    }
    if (!predicate()) {
      ADD_FAILURE() << "Test passed, but predicate returned false after waiting";
    }
  }
}

void TlbTest::SetUp() {
  // This permutation is not supported.
  ASSERT_FALSE(mode() == LoopMode::TlbLoop && thread_count() == 0);

  Test::SetUp();

  switch (mode()) {
    case LoopMode::RawLoop:
      evl = tlb_evl_new(alloc());
      break;

    case LoopMode::TlbLoop:
      tlb_inst = tlb_new(alloc(), {.max_thread_count = thread_count()});
      ASSERT_NE(nullptr, tlb_inst);
      evl = tlb_get_evl(tlb_inst);
      break;
  }

  Restart();
}

void TlbTest::TearDown() {
  Stop();

  switch (mode()) {
    case LoopMode::RawLoop:
      tlb_evl_destroy(evl);
      break;

    case LoopMode::TlbLoop:
      tlb_destroy(tlb_inst);
      break;
  }

  Test::TearDown();
}

void TlbTest::Stop() {
  running = false;

  switch (mode()) {
    case LoopMode::TlbLoop:
      tlb_stop(tlb_inst);
      break;

    case LoopMode::RawLoop:
      for (auto &thread : threads) {
        thread.join();
      }
      threads.clear();
      break;
  }
}

void TlbTest::Restart() {
  running = true;

  if (mode() == LoopMode::TlbLoop) {
    tlb_start(tlb_inst);
  } else {
    for (size_t i = 0; i < thread_count(); ++i) {
      threads.emplace_back([=]() {
        while (running) {
          tlb_evl_handle_events(handled_loop(), 100, 20);
        }
      });
    }
  }
}

tlb_event_loop *TlbTest::handled_loop() {
  switch (mode()) {
    case LoopMode::RawLoop:
      return evl;
    case LoopMode::TlbLoop:
      return nullptr;
  }
}

namespace {
class FixtureTest : public TlbTest {};

TEST_P(FixtureTest, StartStop) {
}

TEST_P(FixtureTest, StopRestart) {
  Stop();
  Restart();
}

TLB_INSTANTIATE_TEST(FixtureTest);
}  // namespace
}  // namespace tlb_test