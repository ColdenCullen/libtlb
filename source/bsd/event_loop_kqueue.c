#include "tlb/private/event_loop.h"

#include "tlb/allocator.h"
#include "tlb/event_loop.h"

#include <sys/event.h>
#include <time.h>
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
  int num_changes = 0;

  /* Calculate flags */
  if (flags == EV_ADD) {
    if (sub->flags & TLB_SUB_ONESHOT) {
      flags |= EV_DISPATCH;
    }
    if (sub->flags & TLB_SUB_EDGE) {
      flags |= EV_CLEAR;
    }
  }

  for (size_t ii = 0; ii < TLB_ARRAY_LENGTH(sub->platform.kqueue); ++ii) {
    const int16_t filter = sub->platform.kqueue[ii];
    if (filter) {
      EV_SET(&cl[num_changes++], /* kev */
             sub->ident.ident,   /* ident */
             filter,             /* filter */
             flags,              /* flags */
             0,                  /* fflags */
             0,                  /* data */
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
  size_t num_filters = 0;
  if (sub->events & TLB_EV_READ) {
    sub->platform.kqueue[num_filters++] = EVFILT_READ;
  }
  if (sub->events & TLB_EV_WRITE) {
    sub->platform.kqueue[num_filters++] = EVFILT_WRITE;
  }
}

/**********************************************************************************************************************
 * Triggers                                                                                                           *
 **********************************************************************************************************************/

void tlb_evl_impl_trigger_init(struct tlb_subscription *sub) {
  sub->ident.ident = (uintptr_t)sub;
  sub->flags = TLB_SUB_EDGE;
  sub->platform.kqueue[0] = EVFILT_USER;
}

int tlb_evl_trigger_fire(struct tlb_event_loop *loop, tlb_handle trigger) {
  struct tlb_subscription *sub = trigger;
  struct kevent change;
  EV_SET(&change,          /* kev */
         sub->ident.ident, /* ident */
         EVFILT_USER,      /* filter */
         EV_ENABLE,        /* flags */
         NOTE_TRIGGER,     /* fflags */
         0,                /* data */
         sub               /* udata */
  );
  return kevent(loop->fd, &change, 1, NULL, 0, NULL);
}

/**********************************************************************************************************************
 * Subscribe/Unsubscribe                                                                                              *
 **********************************************************************************************************************/

int tlb_evl_impl_subscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  return s_kqueue_change(loop, sub, EV_ADD);
}

int tlb_evl_impl_unsubscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  return s_kqueue_change(loop, sub, EV_DELETE);
}

/**********************************************************************************************************************
 * Handle events                                                                                                      *
 **********************************************************************************************************************/

int tlb_evl_handle_events(struct tlb_event_loop *loop, size_t budget, int timeout) {
  struct kevent eventlist[TLB_EV_EVENT_BATCH];
  int events_handled = 0;
  int num_events;

  /* Zero budget means just keep truckin */
  if (budget == 0) {
    budget = SIZE_MAX;
  }

  do {
    /* Calculate the maximum number of events to run */
    num_events = TLB_MIN(budget, TLB_EV_EVENT_BATCH);

    struct timespec timeout_spec = {};
    if (timeout == TLB_WAIT_NONE) {
      memset_s(&timeout_spec, sizeof(timeout_spec), 0, sizeof(timeout_spec));
    } else {
      static const int millis_per_second = 1000;
      static const int nanos_per_second = 1000000;
      timeout_spec.tv_sec = timeout / millis_per_second;
      timeout_spec.tv_nsec = (timeout % millis_per_second) * nanos_per_second;
    }
    struct timespec *timeout_ptr = timeout == TLB_WAIT_INDEFINITE ? NULL : &timeout_spec;

    num_events = TLB_CHECK(-1 !=, kevent(loop->fd, NULL, 0, eventlist, num_events, timeout_ptr));
    for (int ii = 0; ii < num_events; ii++) {
      const struct kevent *ev = &eventlist[ii];
      struct tlb_subscription *sub = ev->udata;

      sub->state = TLB_STATE_RUNNING;
      sub->on_event(sub, s_events_from_kevent(ev), sub->userdata);

      switch (sub->state) {
        case TLB_STATE_SUBBED:
          /* Not possible */
          TLB_ASSERT(false);
          break;

        case TLB_STATE_RUNNING:
          /* Resubscribe the event */
          if (sub->flags & TLB_SUB_ONESHOT) {
            s_kqueue_change(loop, sub, EV_ENABLE);
          }
          sub->state = TLB_STATE_SUBBED;
          break;

        case TLB_STATE_UNSUBBED:
          /* Force-remove the subscription */
          sub->state = TLB_STATE_SUBBED;
          tlb_evl_remove(loop, sub);
          break;
      }
    }
    events_handled += num_events;
    /* Run as long as we're still doing full batch runs and we haven't hit budget */
  } while (num_events == TLB_EV_EVENT_BATCH && (size_t)events_handled < budget);

  return events_handled;
}
