#include "tlb/private/event_loop.h"

#include "tlb/allocator.h"
#include "tlb/event_loop.h"
#include "tlb/private/time.h"

#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

/**********************************************************************************************************************
 * Helpers                                                                                                            *
 **********************************************************************************************************************/

static int s_events_from_epoll(const struct epoll_event *event) {
  int tlb_events = 0;

  if (event->events & EPOLLIN) {
    tlb_events |= TLB_EV_READ;
  }

  if (event->events & EPOLLOUT) {
    tlb_events |= TLB_EV_WRITE;
  }

  if (event->events & EPOLLRDHUP) {
    tlb_events |= TLB_EV_CLOSE;
  }

  if (event->events & EPOLLHUP) {
    tlb_events |= TLB_EV_CLOSE;
  }

  if (event->events & EPOLLERR) {
    tlb_events |= TLB_EV_ERROR;
  }

  return tlb_events;
}

uint32_t s_events_to_epoll(struct tlb_subscription *sub) {
  uint32_t epoll_events = 0;

  if (sub->events & TLB_EV_READ) {
    epoll_events |= EPOLLIN;
  }
  if (sub->events & TLB_EV_WRITE) {
    epoll_events |= EPOLLOUT;
  }

  /* All subscriptions are "oneshot" subscriptions */
  epoll_events |= EPOLLONESHOT;
  if (sub->sub_mode & TLB_SUB_EDGE) {
    epoll_events |= EPOLLET;
  }

  return epoll_events;
}

int s_epoll_change(struct tlb_event_loop *loop, struct tlb_subscription *sub, int operation) {
  struct epoll_event change;

  /* Calculate flags */
  change.events = s_events_to_epoll(sub);
  change.data.ptr = sub;

  return epoll_ctl(loop->fd, operation, sub->ident.fd, &change);
}

/**********************************************************************************************************************
 * Event Loop                                                                                                         *
 **********************************************************************************************************************/

int tlb_evl_init(struct tlb_event_loop *loop, struct tlb_allocator *alloc) {
  loop->alloc = alloc;
  loop->fd = TLB_CHECK(-1 !=, epoll_create1(EPOLL_CLOEXEC));

  return 0;
}

void tlb_evl_cleanup(struct tlb_event_loop *loop) {
  if (loop->fd) {
    close(loop->fd);
    loop->fd = 0;
  }
}

/**********************************************************************************************************************
 * File Descriptors                                                                                                   *
 **********************************************************************************************************************/

void tlb_evl_impl_fd_init(struct tlb_subscription *sub) {
  (void)sub;
}

/**********************************************************************************************************************
 * Timers *
 **********************************************************************************************************************/

static tlb_on_event s_timer_on_event;

void tlb_evl_impl_timer_init(struct tlb_subscription *sub, int timeout) {
  int timerfd = TLB_CHECK_ASSERT(-1 !=, timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK));
  const struct itimerspec timeout_spec = {
      .it_value = tlb_timeout_to_timespec(timeout),
  };
  TLB_CHECK_ASSERT(-1 !=, timerfd_settime(timerfd, 0, &timeout_spec, NULL));

  sub->ident.fd = timerfd;
  sub->events = TLB_EV_READ;
  sub->sub_mode |= TLB_SUB_EDGE;

  sub->platform.epoll.close = true;
  sub->platform.epoll.on_event = sub->on_event;
  sub->on_event = s_timer_on_event;
}

static void s_timer_on_event(tlb_handle subscription, int events, void *userdata) {
  struct tlb_subscription *sub = subscription;

  sub->platform.epoll.on_event(subscription, events, userdata);

  /* Clean up after the timer */
  sub->state = TLB_STATE_UNSUBBED;
}

/**********************************************************************************************************************
 * Subscribe/Unsubscribe *
 **********************************************************************************************************************/

int tlb_evl_impl_subscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  return s_epoll_change(loop, sub, EPOLL_CTL_ADD);
}

int tlb_evl_impl_unsubscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  TLB_CHECK(0 ==, s_epoll_change(loop, sub, EPOLL_CTL_DEL));

  if (sub->platform.epoll.close) {
    TLB_CHECK(0 ==, close(sub->ident.fd));
  }

  return 0;
}

/**********************************************************************************************************************
 * Handle events *
 **********************************************************************************************************************/

int tlb_evl_handle_events(struct tlb_event_loop *loop, size_t budget, int timeout) {
  /* Zero budget means just keep truckin */
  if (budget == 0) {
    budget = SIZE_MAX;
  }

  /* Calculate the maximum number of events to run */
  int num_events = TLB_MIN(budget, TLB_EV_EVENT_BATCH);
  struct epoll_event eventlist[TLB_EV_EVENT_BATCH];

  num_events = TLB_CHECK(-1 !=, epoll_wait(loop->fd, eventlist, num_events, timeout));
  for (int ii = 0; ii < num_events; ii++) {
    const struct epoll_event *event = &eventlist[ii];
    struct tlb_subscription *sub = event->data.ptr;

    TLB_LOG_EVENT(sub, "Handling");

    /* Cache this off because sub can become invalid during on_event */
    sub->state = TLB_STATE_RUNNING;
    sub->on_event(sub, s_events_from_epoll(event), sub->userdata);

    switch ((enum tlb_sub_state)sub->state) {
      case TLB_STATE_SUBBED:
        /* Not possible */
        TLB_LOG_EVENT(sub, "In bad state!");
        TLB_ASSERT(false);
        break;

      case TLB_STATE_RUNNING:
        /* Resubscribe the event */
        sub->state = TLB_STATE_SUBBED;
        TLB_LOG_EVENT(sub, "Set to SUBBED");
        /* This line needs to be last here to prevent race conditions */
        s_epoll_change(loop, sub, EPOLL_CTL_MOD);
        break;

      case TLB_STATE_UNSUBBED:
        /* Force-remove the subscription */
        sub->state = TLB_STATE_SUBBED;
        tlb_evl_remove(loop, sub);
        break;
    }
  }

  return num_events;
}
