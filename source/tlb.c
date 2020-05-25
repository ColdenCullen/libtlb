#include "tlb/tlb.h"

#include "tlb/core.h"

#include "tlb/allocator.h"
#include "tlb/event_loop.h"

enum {
  TLB_MAX_THREADS = 128,
};

struct tlb {
  struct tlb_allocator *alloc;
  struct tlb_options options;

  struct tlb_event_loop *super_loop;

  /* Trigger used to wake a thread */
  tlb_handle thread_stop;

  /* Synced resources */
  mtx_t thread_mutex;
  cnd_t thread_waiter;
  size_t active_threads;

  thrd_t threads[];
};

static int s_thread_start(void *arg);
static tlb_on_event s_thread_stop;

struct tlb *tlb_new(struct tlb_allocator *alloc, struct tlb_options options) {
  const size_t alloc_size = sizeof(struct tlb) + (options.max_thread_count * sizeof(thrd_t));
  struct tlb *tlb = TLB_CHECK(NULL !=, tlb_calloc(alloc, 1, alloc_size));
  tlb->alloc = alloc;
  tlb->options = options;

  tlb->super_loop = TLB_CHECK_GOTO(NULL !=, tlb_evl_new(alloc), cleanup);
  tlb->thread_stop = TLB_CHECK_GOTO(NULL !=, tlb_evl_add_trigger(tlb->super_loop, s_thread_stop, tlb), cleanup);

  TLB_CHECK_GOTO(thrd_success ==, mtx_init(&tlb->thread_mutex, mtx_plain), cleanup);
  TLB_CHECK_GOTO(thrd_success ==, cnd_init(&tlb->thread_waiter), cleanup);

  return tlb;

cleanup:
  tlb_destroy(tlb);
  return NULL;
}

void tlb_destroy(struct tlb *tlb) {
  /* Stop all of the threads */
  tlb_stop(tlb);

  mtx_destroy(&tlb->thread_mutex);
  cnd_destroy(&tlb->thread_waiter);

  if (tlb->super_loop) {
    if (tlb->thread_stop) {
      tlb_evl_remove(tlb->super_loop, tlb->thread_stop);
    }
    tlb_evl_destroy(tlb->super_loop);
  }

  tlb_free(tlb->alloc, tlb);
}

int tlb_start(struct tlb *tlb) {
  mtx_lock(&tlb->thread_mutex);
  const size_t target_threads = tlb->options.max_thread_count;
  for (size_t ii = 0; ii < target_threads; ++ii) {
    thrd_create(&tlb->threads[ii], s_thread_start, tlb);
    cnd_wait(&tlb->thread_waiter, &tlb->thread_mutex);
  }
  TLB_ASSERT(target_threads == tlb->active_threads);
  mtx_unlock(&tlb->thread_mutex);

  return 0;
}

int tlb_stop(struct tlb *tlb) {
  mtx_lock(&tlb->thread_mutex);
  const size_t num_threads = tlb->active_threads;
  for (size_t ii = 0; ii < num_threads; ++ii) {
    /* Fire the trigger, and wait for the event to be handled */
    tlb_evl_trigger_fire(tlb->super_loop, tlb->thread_stop);
    cnd_wait(&tlb->thread_waiter, &tlb->thread_mutex);
  }
  mtx_unlock(&tlb->thread_mutex);

  for (size_t ii = 0; ii < num_threads; ++ii) {
    /* Join the threads */
    int result = 0;
    thrd_join(tlb->threads[ii], &result);
    TLB_ASSERT(thrd_success == result);
  }

  return 0;
}

static tss_t s_should_run;
static int s_thread_start(void *arg) {
  struct tlb *tlb = arg;

  TLB_CHECK(thrd_success ==, tss_create(&s_should_run, NULL));
  TLB_CHECK(thrd_success ==, tss_set(s_should_run, (void *)1));

  /* Signal startup is complete */
  mtx_lock(&tlb->thread_mutex);
  tlb->active_threads++;
  cnd_signal(&tlb->thread_waiter);
  mtx_unlock(&tlb->thread_mutex);

  /* Wait for events */
  while (tss_get(s_should_run)) {
    tlb_evl_handle_events(tlb->super_loop, 0, TLB_WAIT_INDEFINITE);
  }

  mtx_lock(&tlb->thread_mutex);
  tlb->active_threads--;
  cnd_signal(&tlb->thread_waiter);
  mtx_unlock(&tlb->thread_mutex);

  tss_delete(s_should_run);

  return thrd_success;
}

static void s_thread_stop(tlb_handle subscription, int events, void *userdata) {
  (void)subscription;
  (void)events;
  (void)userdata;

  tss_set(s_should_run, (void *)0);
}
