#include "test_helpers.h"

#include "tlb/allocator.h"
#include "tlb/event_loop.h"
#include "tlb/tlb.h"

namespace tlb_test {

void PrintTo(const LoopMode &mode, std::ostream *out) {
  switch (mode) {
    case LoopMode::RawLoop:
      *out << "Raw";
      break;
    case LoopMode::NestedLoop:
      *out << "Nested";
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

void TlbTest::SetUp() {
  // This permutation is not supported.
  ASSERT_FALSE(mode() == LoopMode::TlbLoop && thread_count() == 0);

  Test::SetUp();

  switch (mode()) {
    case LoopMode::RawLoop:
      evl = tlb_evl_new(alloc());
      break;

    case LoopMode::NestedLoop:
      evl = tlb_evl_new(alloc());
      super_loop.super_loop = tlb_evl_new(alloc());
      super_loop.loop_sub = tlb_evl_add_evl(super_loop.super_loop, evl);
      break;

    case LoopMode::TlbLoop:
      tlb.tlb = tlb_new(alloc(), {.max_thread_count = thread_count()});
      evl = tlb_get_evl(tlb.tlb);
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

    case LoopMode::NestedLoop:
      tlb_evl_remove(super_loop.super_loop, super_loop.loop_sub);
      tlb_evl_destroy(evl);
      tlb_evl_destroy(super_loop.super_loop);
      break;

    case LoopMode::TlbLoop:
      tlb_destroy(tlb.tlb);
      break;
  }

  Test::TearDown();
}

void TlbTest::Stop() {
  running = false;

  switch (mode()) {
    case LoopMode::TlbLoop:
      tlb_stop(tlb.tlb);
      break;

    case LoopMode::RawLoop:
    case LoopMode::NestedLoop:
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
    tlb_start(tlb.tlb);
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
    case LoopMode::NestedLoop:
      return super_loop.super_loop;
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