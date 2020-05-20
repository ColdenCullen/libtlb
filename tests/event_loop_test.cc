#include <tlb/event_loop.h>
#include <tlb/pipe.h>

#include <gtest/gtest.h>

#include "test_helpers.h"

namespace tlb {

class EventLoopTest : public ::testing::Test {
 public:
  void SetUp() override {
    alloc = tlb::test_allocator();
    loop = tlb_event_loop_new(alloc);
    tlb_pipe_open(&pipe);
  }

  void TearDown() override {
    tlb_pipe_close(&pipe);
    tlb_event_loop_destroy(loop);
  }

  tlb_allocator *alloc = nullptr;
  tlb_event_loop *loop = nullptr;
  tlb_pipe pipe;

  bool completed = false;
};

TEST_F(EventLoopTest, CreateDestroy) {
}

TEST_F(EventLoopTest, PipeReadable) {
  static const uint64_t s_test_value = 0x0BADFACE;

  tlb_handle sub = tlb_event_loop_subscribe(
      loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        EventLoopTest *t = static_cast<EventLoopTest *>(userdata);
        uint64_t value = 0;
        read(t->pipe.fd_read, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        t->completed = true;
      },
      this);

  write(pipe.fd_write, &s_test_value, sizeof(s_test_value));

  tlb_event_loop_handle_events(loop, 1);

  EXPECT_TRUE(completed);

  tlb_event_loop_unsubscribe(loop, sub);
}

TEST_F(EventLoopTest, PipeWritable) {
  static const uint64_t s_test_value = 0x0BADFACE;

  tlb_handle sub = tlb_event_loop_subscribe(
      loop, pipe.fd_write, TLB_EV_WRITE,
      +[](tlb_handle handle, int events, void *userdata) {
        EventLoopTest *t = static_cast<EventLoopTest *>(userdata);
        write(t->pipe.fd_write, &s_test_value, sizeof(s_test_value));
        t->completed = true;
      },
      this);

  tlb_event_loop_handle_events(loop, 1);

  uint64_t value = 0;
  read(pipe.fd_read, &value, sizeof(value));
  EXPECT_EQ(s_test_value, value);

  EXPECT_TRUE(completed);

  tlb_event_loop_unsubscribe(loop, sub);
}

TEST_F(EventLoopTest, PipeReadableWritable) {
  static const uint64_t s_test_value = 0x0BADFACE;

  tlb_handle read_sub = tlb_event_loop_subscribe(
      loop, pipe.fd_read, TLB_EV_READ,
      +[](tlb_handle handle, int events, void *userdata) {
        EventLoopTest *t = static_cast<EventLoopTest *>(userdata);
        uint64_t value = 0;
        read(t->pipe.fd_read, &value, sizeof(value));
        EXPECT_EQ(s_test_value, value);
        t->completed = true;
      },
      this);

  tlb_handle write_sub = tlb_event_loop_subscribe(
      loop, pipe.fd_write, TLB_EV_WRITE,
      +[](tlb_handle handle, int events, void *userdata) {
        EventLoopTest *t = static_cast<EventLoopTest *>(userdata);
        write(t->pipe.fd_write, &s_test_value, sizeof(s_test_value));
      },
      this);

  // Run the write event
  tlb_event_loop_handle_events(loop, 1);
  // Run the read event
  tlb_event_loop_handle_events(loop, 1);

  EXPECT_TRUE(completed);

  tlb_event_loop_unsubscribe(loop, read_sub);
  tlb_event_loop_unsubscribe(loop, write_sub);
}

TEST_F(EventLoopTest, Trigger) {
  static const uint64_t s_test_value = 0x0BADFACE;

  tlb_handle trigger = tlb_event_loop_trigger_add(
      loop,
      +[](tlb_handle handle, int events, void *userdata) {
        EventLoopTest *t = static_cast<EventLoopTest *>(userdata);
        t->completed = true;
      },
      this);

  tlb_event_loop_handle_events(loop, 1);

  EXPECT_FALSE(completed);

  tlb_trigger_fire(loop, trigger);
  tlb_event_loop_handle_events(loop, 1);

  EXPECT_TRUE(completed);

  tlb_event_loop_trigger_remove(loop, trigger);
}

}  // namespace tlb
