#include "tlb/tlb.h"

#include "tlb/core.h"

#include "tlb/allocator.h"
#include "tlb/pipe.h"
#include "tlb/private/event_loop.h"

#include <stdatomic.h>

enum {
  TLB_MAX_THREADS = 128,
};

struct tlb {
  struct tlb_allocator *alloc;
  struct tlb_options options;

  struct tlb_event_loop super_loop;

  struct tlb_pipe thread_stop_pipe;
  struct tlb_subscription thread_stop_sub;

  atomic_size_t active_threads;

  /* Used to sync the start and stop routines */
  mtx_t mtx;
  cnd_t cnd;

  thrd_t threads[];
};

static _Thread_local bool s_should_stop;
static const uint64_t s_thread_stop_value = 0xBADC0FFEE;

static int s_thread_start(void *arg);
static tlb_on_event s_thread_stop;

struct tlb *tlb_new(struct tlb_allocator *alloc, struct tlb_options options) {
  const size_t alloc_size = sizeof(struct tlb) + (options.max_thread_count * sizeof(thrd_t));
  struct tlb *tlb = TLB_CHECK(NULL !=, tlb_calloc(alloc, 1, alloc_size));
  tlb->alloc = alloc;
  tlb->options = options;

  TLB_CHECK_GOTO(0 ==, tlb_evl_init(&tlb->super_loop, alloc), evl_init_failed);
  tlb->super_loop.super_loop = true;

  /* Setup the pipe used to stop threads. */
  TLB_CHECK_GOTO(0 ==, tlb_pipe_open(&tlb->thread_stop_pipe), pipe_open_failed);
  tlb->thread_stop_sub = (struct tlb_subscription){
      .ident = {.fd = tlb->thread_stop_pipe.fd_read},
      .on_event = s_thread_stop,
      .userdata = tlb,
      .events = TLB_EV_READ,
      .sub_mode = TLB_SUB_EDGE, /* Don't use oneshot here, or rapidly stopping threads will be racey */
      .name = "tlb_thread_stop_pipe",
  };
  tlb_evl_impl_fd_init(&tlb->thread_stop_sub);
  TLB_CHECK_GOTO(0 ==, tlb_evl_impl_subscribe(&tlb->super_loop, &tlb->thread_stop_sub), thread_stop_sub_failed);

  TLB_CHECK_GOTO(thrd_success ==, mtx_init(&tlb->mtx, mtx_plain), mtx_init_failed);
  TLB_CHECK_GOTO(thrd_success ==, cnd_init(&tlb->cnd), cnd_init_failed);
  atomic_init(&tlb->active_threads, 0);

  return tlb;

cnd_init_failed:
  mtx_destroy(&tlb->mtx);
mtx_init_failed:
  tlb_evl_impl_unsubscribe(&tlb->super_loop, &tlb->thread_stop_sub);
thread_stop_sub_failed:
  tlb_pipe_close(&tlb->thread_stop_pipe);
pipe_open_failed:
  tlb_evl_cleanup(&tlb->super_loop);
evl_init_failed:
  tlb_free(alloc, tlb);
  return NULL;
}

void tlb_destroy(struct tlb *tlb) {
  /* Stop all of the threads */
  tlb_stop(tlb);

  cnd_destroy(&tlb->cnd);
  mtx_destroy(&tlb->mtx);

  tlb_evl_cleanup(&tlb->super_loop);

  tlb_free(tlb->alloc, tlb);
}

int tlb_start(struct tlb *tlb) {
  mtx_lock(&tlb->mtx);

  const size_t target_threads = tlb->options.max_thread_count;
  for (size_t ii = 0; ii < target_threads; ++ii) {
    thrd_create(&tlb->threads[ii], s_thread_start, tlb);
    cnd_wait(&tlb->cnd, &tlb->mtx);
  }

  mtx_unlock(&tlb->mtx);

  TLB_ASSERT(atomic_load(&tlb->active_threads) == target_threads);

  return 0;
}

int tlb_stop(struct tlb *tlb) {
  mtx_lock(&tlb->mtx);

  const size_t active_threads = tlb->active_threads;
  for (size_t ii = active_threads; ii > 0; --ii) {
    TLB_LOGF("Stopping thread %zu", ii);

    /* Fire the trigger, and wait for the event to be handled */
    tlb_pipe_write(&tlb->thread_stop_pipe, s_thread_stop_value);

    /* Wait for a thread to exit */
    cnd_wait(&tlb->cnd, &tlb->mtx);

    const size_t current_threads = atomic_load(&tlb->active_threads);
    if (current_threads != ii - 1) {
      TLB_LOGF("Thread count doesn't match! Started iteration with %zu threads, now only have %zu.", ii,
               current_threads);
    }
  }

  for (size_t ii = 0; ii < active_threads; ++ii) {
    /* Join the threads */
    int result = 0;
    thrd_join(tlb->threads[ii], &result);
    TLB_ASSERT(thrd_success == result);
  }

  mtx_unlock(&tlb->mtx);

  return 0;
}

static int s_thread_start(void *arg) {
  struct tlb *tlb = arg;
  s_should_stop = false;

  atomic_fetch_add(&tlb->active_threads, 1);

  /* Acquire the lock so we know the stop thread is waiting */
  mtx_lock(&tlb->mtx);
  mtx_unlock(&tlb->mtx);
  cnd_signal(&tlb->cnd);

  /* Wait for events */
  while (!s_should_stop) {
    tlb_evl_handle_events(&tlb->super_loop, 0, TLB_WAIT_INDEFINITE);
  }

  atomic_fetch_sub(&tlb->active_threads, 1);

  /* Acquire the lock so we know the stop thread is waiting */
  mtx_lock(&tlb->mtx);
  mtx_unlock(&tlb->mtx);
  cnd_signal(&tlb->cnd);

  return thrd_success;
}

static void s_thread_stop(tlb_handle subscription, int events, void *userdata) {
  (void)subscription;
  (void)events;

  struct tlb *tlb = userdata;

  s_should_stop = true;

  /* Just read the data sent to clear the buffer */
  uint64_t value = 0;
  tlb_pipe_read(&tlb->thread_stop_pipe, &value);
  TLB_ASSERT(value == s_thread_stop_value);
}

struct tlb_event_loop *tlb_get_evl(struct tlb *tlb) {
  return &tlb->super_loop;
}
