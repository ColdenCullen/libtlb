#include <tlb/core.h>

#include <tlb/private/event_loop.h>

struct tlb_event_loop *tlb_event_loop_new(struct tlb_allocator *alloc) {
  struct tlb_event_loop *loop = TLB_CHECK(NULL !=, tlb_calloc(alloc, 1, sizeof(struct tlb_event_loop)));
  loop->alloc = alloc;

  TLB_CHECK_GOTO(0 ==, tlb_ev_init(loop), cleanup);

  return loop;

cleanup:
  tlb_event_loop_destroy(loop);
  return NULL;
}

void tlb_event_loop_destroy(struct tlb_event_loop *loop) {
  tlb_ev_cleanup(loop);

  tlb_free(loop->alloc, loop);
}

static struct tlb_subscription *s_sub_new(struct tlb_event_loop *loop, tlb_on_event *on_event, void *userdata) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, tlb_calloc(loop->alloc, 1, sizeof(struct tlb_subscription)));
  sub->on_event = on_event;
  sub->userdata = userdata;
  return sub;
}

tlb_handle tlb_event_loop_subscribe(struct tlb_event_loop *loop, int fd, int events, tlb_on_event *on_event,
                                    void *userdata) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, s_sub_new(loop, on_event, userdata));
  sub->ident = fd;
  sub->events = events;

  TLB_CHECK_GOTO(0 ==, tlb_fd_subscribe(loop, sub), sub_failed);

  return sub;

sub_failed:
  tlb_free(loop->alloc, sub);
  return NULL;
}

int tlb_event_loop_unsubscribe(struct tlb_event_loop *loop, tlb_handle subscription) {
  struct tlb_subscription *sub = subscription;

  int result = tlb_fd_unsubscribe(loop, sub);
  tlb_free(loop->alloc, sub);

  return result;
}

tlb_handle tlb_event_loop_trigger_add(struct tlb_event_loop *loop, tlb_on_event *trigger, void *userdata) {
  struct tlb_subscription *sub = TLB_CHECK(NULL !=, s_sub_new(loop, trigger, userdata));
  TLB_CHECK_GOTO(0 ==, tlb_trigger_add(loop, sub), sub_failed);
  return sub;

sub_failed:
  tlb_free(loop->alloc, sub);
  return NULL;
}

int tlb_event_loop_trigger_remove(struct tlb_event_loop *loop, tlb_handle trigger) {
  struct tlb_subscription *sub = trigger;

  int result = tlb_fd_unsubscribe(loop, sub);
  tlb_free(loop->alloc, sub);

  return result;
}
