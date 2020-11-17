#include "tlb/tlb.h"

#include "tlb/core.h"

#include "tlb/allocator.h"
#include "tlb/pipe.h"
#include "tlb/private/event_loop.h"

enum {
  TLB_MAX_THREADS = 128,
};

struct tlb_thread {
  thrd_t id;
  struct tlb *tlb;
  struct tlb_event_loop loop;
  struct tlb_pipe thread_stop_pipe;
  struct tlb_subscription thread_stop_sub;
  volatile bool should_stop;
};

struct tlb {
  struct tlb_allocator *alloc;
  struct tlb_options options;

  struct tlb_event_loop super_loop;

  /* Synced resources */
  mtx_t thread_mutex;
  size_t active_threads;
  struct tlb_thread threads[];
};

static const uint64_t s_thread_stop_value = 0xBADC0FFEE;
static int s_thread_start(void *arg);
static tlb_on_event s_thread_stop;

struct tlb *tlb_new(struct tlb_allocator *alloc, struct tlb_options options) {
  const size_t alloc_size = sizeof(struct tlb) + (options.max_thread_count * sizeof(struct tlb_thread));
  struct tlb *tlb = TLB_CHECK(NULL !=, tlb_calloc(alloc, 1, alloc_size));
  tlb->alloc = alloc;
  tlb->options = options;

  TLB_CHECK_GOTO(0 ==, tlb_evl_init(&tlb->super_loop, alloc), cleanup);
  tlb->super_loop.super_loop = true;

  TLB_CHECK_GOTO(thrd_success ==, mtx_init(&tlb->thread_mutex, mtx_plain), cleanup);

  return tlb;

cleanup:
  tlb_destroy(tlb);
  return NULL;
}

void tlb_destroy(struct tlb *tlb) {
  /* Stop all of the threads */
  tlb_stop(tlb);

  mtx_destroy(&tlb->thread_mutex);

  tlb_evl_cleanup(&tlb->super_loop);

  tlb_free(tlb->alloc, tlb);
}

int tlb_start(struct tlb *tlb) {
  mtx_lock(&tlb->thread_mutex);

  const size_t target_threads = tlb->options.max_thread_count;
  for (size_t ii = 0; ii < target_threads; ++ii) {
    struct tlb_thread *thread = &tlb->threads[ii];
    thread->tlb = tlb;
    thread->should_stop = false;
    thrd_create(&thread->id, s_thread_start, thread);
    tlb->active_threads++;
  }

  mtx_unlock(&tlb->thread_mutex);
  return 0;
}

int tlb_stop(struct tlb *tlb) {
  mtx_lock(&tlb->thread_mutex);

  const size_t num_threads = tlb->active_threads;
  for (size_t ii = 0; ii < num_threads; ++ii) {
    /* Fire the trigger, and wait for the event to be handled */
    struct tlb_thread *thread = &tlb->threads[ii];
    thread->should_stop = true;
    tlb_pipe_write(&thread->thread_stop_pipe, s_thread_stop_value);
  }

  for (size_t ii = 0; ii < num_threads; ++ii) {
    /* Join the threads */
    int result = 0;
    thrd_join(tlb->threads[ii].id, &result);
    TLB_ASSERT(thrd_success == result);
    tlb->active_threads--;
  }

  mtx_unlock(&tlb->thread_mutex);
  return 0;
}

static int s_thread_start(void *arg) {
  struct tlb_thread *thread = arg;
  struct tlb *tlb = thread->tlb;

  tlb_evl_init(&thread->loop, tlb->alloc);

  /* Subscribe the super loop to the thread loop */
  struct tlb_subscription super_loop_sub = {
      .ident = {.fd = tlb->super_loop.fd},
      .events = TLB_EV_READ,
      .sub_mode = TLB_SUB_EDGE,
      .on_event = tlb_evl_sub_loop_on_event,
      .userdata = &tlb->super_loop,
      .name = "super-loop",
  };
  tlb_evl_impl_fd_init(&super_loop_sub);
  TLB_CHECK_GOTO(0 ==, tlb_evl_impl_subscribe(&thread->loop, &super_loop_sub), sub_failed);

  tlb_pipe_open(&thread->thread_stop_pipe);
  thread->thread_stop_sub = (struct tlb_subscription){
      .ident = {.fd = thread->thread_stop_pipe.fd_read},
      .events = TLB_EV_READ,
      .sub_mode = TLB_SUB_EDGE,
      .on_event = s_thread_stop,
      .userdata = thread,
      .name = "tlb-thread-stop",
  };
  tlb_evl_impl_fd_init(&thread->thread_stop_sub);
  tlb_evl_impl_subscribe(&thread->loop, &thread->thread_stop_sub);

  /* Wait for events */
  while (!thread->should_stop) {
    tlb_evl_handle_events(&thread->loop, 0, TLB_WAIT_INDEFINITE);
  }

  tlb_evl_impl_unsubscribe(&thread->loop, &thread->thread_stop_sub);
  tlb_evl_impl_unsubscribe(&thread->loop, &super_loop_sub);

  tlb_pipe_close(&thread->thread_stop_pipe);
  tlb_evl_cleanup(&thread->loop);

  return thrd_success;

sub_failed:
  TLB_LOG("Failed to subscribe super loop to thread loop!");
  return thrd_error;
}

static void s_thread_stop(tlb_handle subscription, int events, void *userdata) {
  (void)subscription;
  (void)events;
  (void)userdata;

  struct tlb_thread *thread = userdata;

  /* Just read the data sent to clear the buffer */
  uint64_t value = 0;
  tlb_pipe_read(&thread->thread_stop_pipe, &value);
  TLB_ASSERT(value == s_thread_stop_value);
}

struct tlb_event_loop *tlb_get_evl(struct tlb *tlb) {
  return &tlb->super_loop;
}
