#include "tlb/tlb.h"

#include "tlb/core.h"

#include "tlb/allocator.h"
#include "tlb/private/event_loop.h"

enum {
  TLB_MAX_THREADS = 128,
};

struct tlb_thread {
  thrd_t id;
  struct tlb *tlb;
  struct tlb_event_loop loop;
  struct tlb_subscription thread_stop;
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

static int s_thread_start(void *arg);
static tlb_on_event s_thread_stop;

struct tlb *tlb_new(struct tlb_allocator *alloc, struct tlb_options options) {
  const size_t alloc_size = sizeof(struct tlb) + (options.max_thread_count * sizeof(struct tlb_thread));
  struct tlb *tlb = TLB_CHECK(NULL !=, tlb_calloc(alloc, 1, alloc_size));
  tlb->alloc = alloc;
  tlb->options = options;

  TLB_CHECK_GOTO(0 ==, tlb_evl_init(&tlb->super_loop, alloc), cleanup);

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
    tlb_evl_trigger_fire(&thread->loop, &thread->thread_stop);
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
  tlb_handle super_loop = tlb_evl_add_evl(&thread->loop, &tlb->super_loop);

  thread->thread_stop = (struct tlb_subscription){
      .on_event = s_thread_stop,
      .userdata = thread,
  };
  tlb_evl_impl_trigger_init(&thread->thread_stop);
  tlb_evl_impl_subscribe(&thread->loop, &thread->thread_stop);

  /* Wait for events */
  while (!thread->should_stop) {
    tlb_evl_handle_events(&thread->loop, 0, TLB_WAIT_INDEFINITE);
  }

  tlb_evl_impl_subscribe(&thread->loop, &thread->thread_stop);
  tlb_evl_remove(&thread->loop, super_loop);
  tlb_evl_cleanup(&thread->loop);

  return thrd_success;
}

static void s_thread_stop(tlb_handle subscription, int events, void *userdata) {
  (void)subscription;
  (void)events;
  (void)userdata;

  /* no-op */
}

struct tlb_event_loop *tlb_get_evl(struct tlb *tlb) {
  return &tlb->super_loop;
}
