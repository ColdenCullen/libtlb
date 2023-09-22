#include "tlb/private/event_loop.h"

#include "tlb/allocator.h"
#include "tlb/event_loop.h"
#include "tlb/private/time.h"

#include <sys/event.h>
#include <unistd.h>

/**********************************************************************************************************************
 * Helpers                                                                                                            *
 **********************************************************************************************************************/

static int s_events_from_kevent(const struct kevent *kevent) {
  int ev = 0;

  if (kevent->flags & EV_ERROR) {
    ev |= TLB_EV_ERROR;
  } else if (kevent->filter == EVFILT_READ) {
    if (kevent->data != 0) {
      ev |= TLB_EV_READ;
    }

    if (kevent->flags & EV_EOF) {
      ev |= TLB_EV_CLOSE;
    }
  } else if (kevent->filter == EVFILT_WRITE) {
    if (kevent->data != 0) {
      ev |= TLB_EV_WRITE;
    }

    if (kevent->flags & EV_EOF) {
      ev |= TLB_EV_CLOSE;
    }
  }

  return ev;
}

int s_kqueue_change(struct tlb_event_loop *loop, struct tlb_subscription *sub, uint16_t flags) {
  struct kevent cl[2];
  size_t num_changes = 0;

  /* Calculate flags */
  if (flags == EV_ADD) {
    if (sub->sub_mode & TLB_SUB_EDGE) {
      flags |= EV_CLEAR;
    }

    if (sub->sub_mode & TLB_SUB_ONESHOT) {
      flags |= EV_DISPATCH;
    }
  }

  struct tlb_evl_kqueue *kq = &sub->platform.kqueue;
  for (size_t ii = 0; ii < TLB_ARRAY_LENGTH(kq->filters); ++ii) {
    const int16_t filter = kq->filters[ii];
    if (filter) {
      EV_SET(&cl[num_changes++], /* kev */
             sub->ident.ident,   /* ident */
             filter,             /* filter */
             flags,              /* flags */
             0,                  /* fflags */
             kq->data,           /* data */
             sub                 /* udata */
      );
    }
  }

  return kevent(loop->fd, cl, num_changes, NULL, 0, NULL);
}

/**********************************************************************************************************************
 * Event Loop                                                                                                         *
 **********************************************************************************************************************/

int tlb_evl_init(struct tlb_event_loop *loop, struct tlb_allocator *alloc) {
  loop->alloc = alloc;
  loop->fd = TLB_CHECK(-1 !=, kqueue());

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
  /* kqueue always wants the uintptr_t version, so force a cast. */
  sub->ident.ident = sub->ident.fd;
  size_t num_filters = 0;
  if (sub->events & TLB_EV_READ) {
    sub->platform.kqueue.filters[num_filters++] = EVFILT_READ;
  }
  if (sub->events & TLB_EV_WRITE) {
    sub->platform.kqueue.filters[num_filters++] = EVFILT_WRITE;
  }
}

/**********************************************************************************************************************
 * Timers *
 **********************************************************************************************************************/

void tlb_evl_impl_timer_init(struct tlb_subscription *sub, int timeout) {
  sub->ident.ident = (uintptr_t)sub;
  sub->sub_mode |= TLB_SUB_ONESHOT;
  sub->platform.kqueue.filters[0] = EVFILT_TIMER;
  sub->platform.kqueue.data = timeout;
}

/**********************************************************************************************************************
 * Subscribe/Unsubscribe *
 **********************************************************************************************************************/

int tlb_evl_impl_subscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  return s_kqueue_change(loop, sub, EV_ADD);
}

int tlb_evl_impl_unsubscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  return s_kqueue_change(loop, sub, EV_DELETE);
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
  struct kevent eventlist[TLB_EV_EVENT_BATCH];

  struct timespec timeout_spec = tlb_timeout_to_timespec(timeout);
  struct timespec *timeout_ptr = timeout == TLB_WAIT_INDEFINITE ? NULL : &timeout_spec;

  num_events = TLB_CHECK(-1 !=, kevent(loop->fd, NULL, 0, eventlist, num_events, timeout_ptr));
  for (int ii = 0; ii < num_events; ii++) {
    const struct kevent *ev = &eventlist[ii];
    struct tlb_subscription *sub = ev->udata;

    TLB_LOG_EVENT(sub, "Handling");

    /* Cache this off because sub can become invalid during on_event */
    const bool is_oneshot = sub->sub_mode & TLB_SUB_ONESHOT;
    sub->oneshot_state = TLB_STATE_RUNNING;
    sub->on_event(sub, s_events_from_kevent(ev), sub->userdata);

    if (is_oneshot) {
      switch (sub->oneshot_state) {
        case TLB_STATE_SUBBED:
          /* Not possible */
          TLB_LOG_EVENT(sub, "In bad state!");
          TLB_ASSERT(false);
          break;

        case TLB_STATE_RUNNING:
          /* Resubscribe the event */
          sub->oneshot_state = TLB_STATE_SUBBED;
          s_kqueue_change(loop, sub, EV_ENABLE);
          TLB_LOG_EVENT(sub, "Set to SUBBED");
          break;

        case TLB_STATE_UNSUBBED:
          /* Force-remove the subscription */
          sub->oneshot_state = TLB_STATE_SUBBED;
          tlb_evl_remove(loop, sub);
          break;
      }
    }
  }

  return num_events;
}
