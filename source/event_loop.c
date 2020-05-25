#include "tlb/private/event_loop.h"

/**********************************************************************************************************************
 * Life cycle                                                                                                         *
 **********************************************************************************************************************/

struct tlb_event_loop *tlb_evl_new(struct tlb_allocator *alloc) {
  struct tlb_event_loop *loop = TLB_CHECK(NULL !=, tlb_calloc(alloc, 1, sizeof(struct tlb_event_loop)));
  loop->alloc = alloc;

  TLB_CHECK_GOTO(0 ==, tlb_evl_init(loop), cleanup);

  return loop;

cleanup:
  tlb_evl_destroy(loop);
  return NULL;
}

void tlb_evl_destroy(struct tlb_event_loop *loop) {
  tlb_evl_cleanup(loop);

  tlb_free(loop->alloc, loop);
}

static struct tlb_subscription *s_sub_new(struct tlb_event_loop *loop, tlb_on_event *on_event, void *userdata) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, tlb_calloc(loop->alloc, 1, sizeof(struct tlb_subscription)));
  sub->on_event = on_event;
  sub->userdata = userdata;
  return sub;
}

/**********************************************************************************************************************
 * File descriptor                                                                                                    *
 **********************************************************************************************************************/

tlb_handle tlb_evl_add_fd(struct tlb_event_loop *loop, int fd, int events, tlb_on_event *on_event, void *userdata) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, s_sub_new(loop, on_event, userdata));
  sub->ident.fd = fd;
  sub->events = events;
  sub->flags = TLB_SUB_EDGE;

  tlb_evl_impl_fd_init(sub);
  TLB_CHECK_GOTO(0 ==, tlb_evl_impl_subscribe(loop, sub), sub_failed);

  return sub;

sub_failed:
  tlb_free(loop->alloc, sub);
  return NULL;
}

/**********************************************************************************************************************
 * Trigger                                                                                                            *
 **********************************************************************************************************************/

tlb_handle tlb_evl_add_trigger(struct tlb_event_loop *loop, tlb_on_event *trigger, void *userdata) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, s_sub_new(loop, trigger, userdata));
  tlb_evl_impl_trigger_init(sub);
  TLB_CHECK_GOTO(0 ==, tlb_evl_impl_subscribe(loop, sub), sub_failed);
  return sub;

sub_failed:
  tlb_free(loop->alloc, sub);
  return NULL;
}

/**********************************************************************************************************************
 * Sub-loop                                                                                                           *
 **********************************************************************************************************************/

static tlb_on_event s_sub_loop_on_event;

tlb_handle tlb_evl_add_evl(struct tlb_event_loop *loop, struct tlb_event_loop *sub_loop) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, s_sub_new(loop, s_sub_loop_on_event, sub_loop));
  sub->ident.fd = sub_loop->fd;
  sub->events = TLB_EV_READ;
  sub->flags = TLB_SUB_ONESHOT;

  tlb_evl_impl_fd_init(sub);
  TLB_CHECK_GOTO(0 ==, tlb_evl_impl_subscribe(loop, sub), sub_failed);
  return sub;

sub_failed:
  tlb_free(loop->alloc, sub);
  return NULL;
}

static void s_sub_loop_on_event(tlb_handle subscription, int events, void *userdata) {
  (void)subscription;
  (void)events;
  struct tlb_event_loop *sub_loop = userdata;

  tlb_evl_handle_events(sub_loop, TLB_EV_EVENT_BATCH, 0);
}

/**********************************************************************************************************************
 * Move/Remove                                                                                                        *
 **********************************************************************************************************************/

int tlb_evl_move(struct tlb_event_loop *loop_from, tlb_handle subscription, struct tlb_event_loop *loop_to) {
  struct tlb_subscription *sub = subscription;
  TLB_CHECK(0 ==, tlb_evl_impl_unsubscribe(loop_from, sub));
  TLB_CHECK(0 ==, tlb_evl_impl_subscribe(loop_to, sub));

  return 0;
}

int tlb_evl_remove(struct tlb_event_loop *loop, tlb_handle subscription) {
  struct tlb_subscription *sub = subscription;
  int result = 0;

  switch (sub->state) {
    case TLB_STATE_SUBBED:
      result = tlb_evl_impl_unsubscribe(loop, sub);
      tlb_free(loop->alloc, sub);
      break;

    case TLB_STATE_RUNNING:
      sub->state = TLB_STATE_UNSUBBED;
      break;

    case TLB_STATE_UNSUBBED:
      /* no-op */
      break;
  }

  return result;
}
