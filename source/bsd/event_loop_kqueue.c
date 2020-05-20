#include <tlb/allocator.h>

#include <event_loop.h>
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

/**********************************************************************************************************************
 * Event Loop                                                                                                         *
 **********************************************************************************************************************/

int tlb_ev_init(struct tlb_event_loop *loop) {
  loop->ident = TLB_CHECK(-1 !=, kqueue());

  return 0;
}

void tlb_ev_cleanup(struct tlb_event_loop *loop) {
  if (loop->ident) {
    close(loop->ident);
    loop->ident = 0;
  }
}

/**********************************************************************************************************************
 * FDs                                                                                                                *
 **********************************************************************************************************************/

int tlb_fd_subscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  struct kevent cl[2];
  int num_changes = 0;

  if (sub->events & TLB_EV_READ) {
    EV_SET(&cl[num_changes++],  /* kev */
           sub->ident,          /* ident */
           EVFILT_READ,         /* filter */
           EV_ADD | EV_ONESHOT, /* flags */
           0,                   /* fflags */
           0,                   /* data */
           sub                  /* udata */
    );
  }
  if (sub->events & TLB_EV_WRITE) {
    EV_SET(&cl[num_changes++],  /* kev */
           sub->ident,          /* ident */
           EVFILT_WRITE,        /* filter */
           EV_ADD | EV_ONESHOT, /* flags */
           0,                   /* fflags */
           0,                   /* data */
           sub                  /* udata */
    );
  }

  TLB_CHECK(-1 !=, kevent(loop->ident, cl, num_changes, NULL, 0, 0));

  return 0;
}
int tlb_fd_unsubscribe(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  struct kevent cl[2];
  int num_changes = 0;

  if (sub->events & TLB_EV_READ) {
    EV_SET(&cl[num_changes++], /* kev */
           sub->ident,         /* ident */
           EVFILT_READ,        /* filter */
           EV_DELETE,          /* flags */
           0,                  /* fflags */
           0,                  /* data */
           sub                 /* udata */
    );
  }
  if (sub->events & TLB_EV_WRITE) {
    EV_SET(&cl[num_changes++], /* kev */
           sub->ident,         /* ident */
           EVFILT_WRITE,       /* filter */
           EV_DELETE,          /* flags */
           0,                  /* fflags */
           0,                  /* data */
           sub                 /* udata */
    );
  }

  TLB_CHECK(-1 !=, kevent(loop->ident, cl, num_changes, NULL, 0, NULL));

  return 0;
}

/**********************************************************************************************************************
 * Triggers                                                                                                           *
 **********************************************************************************************************************/

int tlb_trigger_add(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  sub->ident = (int)sub;

  struct kevent change;
  EV_SET(&change,           /* kev */
         sub->ident,        /* ident */
         EVFILT_USER,       /* filter */
         EV_ADD | EV_CLEAR, /* flags */
         0,                 /* fflags */
         0,                 /* data */
         sub                /* udata */
  );
  return kevent(loop->ident, &change, 1, NULL, 0, NULL);
}
int tlb_trigger_remove(struct tlb_event_loop *loop, struct tlb_subscription *sub) {
  struct kevent change;
  EV_SET(&change,     /* kev */
         sub->ident,  /* ident */
         EVFILT_USER, /* filter */
         EV_DELETE,   /* flags */
         0,           /* fflags */
         0,           /* data */
         sub          /* udata */
  );
  return kevent(loop->ident, &change, 1, NULL, 0, NULL);
}
int tlb_trigger_fire(struct tlb_event_loop *loop, tlb_handle trigger) {
  struct tlb_subscription *sub = trigger;
  struct kevent change;
  EV_SET(&change,      /* kev */
         sub->ident,   /* ident */
         EVFILT_USER,  /* filter */
         EV_ENABLE,    /* flags */
         NOTE_TRIGGER, /* fflags */
         0,            /* data */
         sub           /* udata */
  );
  return kevent(loop->ident, &change, 1, NULL, 0, NULL);
}

/**********************************************************************************************************************
 * Handle events                                                                                                      *
 **********************************************************************************************************************/

int tlb_event_loop_handle_events(struct tlb_event_loop *loop, size_t budget) {
  struct kevent eventlist[TLB_EV_EVENT_BATCH];
  int num_events;

  do {
    num_events = TLB_MIN(budget, TLB_EV_EVENT_BATCH);
    num_events = TLB_CHECK(-1 !=, kevent(loop->ident, NULL, 0, eventlist, num_events, NULL));
    for (int ii = 0; ii < num_events; ii++) {
      const struct kevent *ev = &eventlist[ii];
      struct tlb_subscription *sub = ev->udata;

      sub->on_event(s_events_from_kevent(ev), sub->userdata);

      /* Resubscribe the event */
      tlb_fd_subscribe(loop, sub);
    }
    budget -= num_events;
  } while (num_events == TLB_EV_EVENT_BATCH && budget > 0);

  return 0;
}
