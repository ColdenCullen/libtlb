#include "tlb/private/event_loop.h"

#include <errno.h>

/**********************************************************************************************************************
 * Life cycle                                                                                                         *
 **********************************************************************************************************************/

struct tlb_event_loop *tlb_evl_new(struct tlb_allocator *alloc) {
  struct tlb_event_loop *loop = TLB_CHECK(NULL !=, tlb_calloc(alloc, 1, sizeof(struct tlb_event_loop)));

  TLB_CHECK_GOTO(0 ==, tlb_evl_init(loop, alloc), cleanup);

  return loop;

cleanup:
  tlb_evl_destroy(loop);
  return NULL;
}

void tlb_evl_destroy(struct tlb_event_loop *loop) {
  tlb_evl_cleanup(loop);

  tlb_free(loop->alloc, loop);
}

static struct tlb_subscription *s_sub_new(struct tlb_event_loop *loop, tlb_on_event *on_event, void *userdata,
                                          const char *name) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, tlb_malloc(loop->alloc, sizeof(struct tlb_subscription)));
  *sub = (struct tlb_subscription){
      .on_event = on_event,
      .userdata = userdata,
      .name = name,
  };

  return sub;
}

/**********************************************************************************************************************
 * File descriptor                                                                                                    *
 **********************************************************************************************************************/

tlb_handle tlb_evl_add_fd(struct tlb_event_loop *loop, int fd, int events, tlb_on_event *on_event, void *userdata) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, s_sub_new(loop, on_event, userdata, "fd"));
  sub->ident.fd = fd;
  sub->events = events;
  sub->sub_mode |= TLB_SUB_EDGE;

  tlb_evl_impl_fd_init(sub);
  TLB_CHECK_GOTO(0 ==, tlb_evl_impl_subscribe(loop, sub), sub_failed);

  return sub;

sub_failed:
  tlb_free(loop->alloc, sub);
  return NULL;
}

/**********************************************************************************************************************
 * Timer                                                                                                              *
 **********************************************************************************************************************/

tlb_handle tlb_evl_add_timer(struct tlb_event_loop *loop, int timeout, tlb_on_event *trigger, void *userdata) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, s_sub_new(loop, trigger, userdata, "timer"));
  tlb_evl_impl_timer_init(sub, timeout);
  TLB_CHECK_GOTO(0 ==, tlb_evl_impl_subscribe(loop, sub), sub_failed);
  return sub;

sub_failed:
  tlb_free(loop->alloc, sub);
  return NULL;
}

/**********************************************************************************************************************
 * Sub-loop                                                                                                           *
 **********************************************************************************************************************/

tlb_handle tlb_evl_add_evl(struct tlb_event_loop *loop, struct tlb_event_loop *sub_loop) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, s_sub_new(loop, tlb_evl_sub_loop_on_event, sub_loop, "sub-loop"));
  sub->ident.fd = sub_loop->fd;
  sub->events = TLB_EV_READ;
  sub->sub_mode |= 0;

  tlb_evl_impl_fd_init(sub);
  TLB_CHECK_GOTO(0 ==, tlb_evl_impl_subscribe(loop, sub), sub_failed);
  return sub;

sub_failed:
  tlb_free(loop->alloc, sub);
  return NULL;
}

void tlb_evl_sub_loop_on_event(tlb_handle subscription, int events, void *userdata) {
  (void)subscription;
  (void)events;
  struct tlb_event_loop *sub_loop = userdata;

  int handled = tlb_evl_handle_events(sub_loop, TLB_EV_EVENT_BATCH, 0);
  if (handled > 0) {
    TLB_LOGF_EVENT(subscription, "Handled %d events", handled);
  } else if (handled < 0) {
    TLB_LOGF_EVENT(subscription, "Event handler failed with error: %s", strerror(errno));
    TLB_ASSERT(false);
  }
}

/**********************************************************************************************************************
 * Move/Remove                                                                                                        *
 **********************************************************************************************************************/

int tlb_evl_remove(struct tlb_event_loop *loop, tlb_handle subscription) {
  struct tlb_subscription *sub = subscription;
  int result = 0;

  TLB_LOG_EVENT(sub, "Unsubbing:");
  switch ((enum tlb_sub_state)sub->state) {
    case TLB_STATE_SUBBED:
      TLB_LOG_EVENT(sub, "  SUBBED, unsubbing and freeing");
      result = tlb_evl_impl_unsubscribe(loop, sub);
      tlb_free(loop->alloc, sub);
      break;

    case TLB_STATE_RUNNING:
      TLB_LOG_EVENT(sub, "  RUNNING, Setting state");
      sub->state = TLB_STATE_UNSUBBED;
      break;

    case TLB_STATE_UNSUBBED:
      /* no-op */
      break;
  }

  return result;
}
