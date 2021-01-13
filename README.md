# libtlb

A library for safely balancing event loop handlers across threads.  

## Design

TLB is designed around a single event loop that is blocked on by multiple threads. Thread safety is achieved by setting
up all subscriptions in oneshot mode (`EPOLLONESHOT` on epoll, `EV_DISPATCH` on kqueue),
and automatically resubscribing the fd once the user's callback is complete.
This means that while one thread is processing an fd, new events will not trigger other threads to wake,
and thus only one thread may be processing an fd at a time.

Grouped FDs may be setup by subscribing them to a `tlb_evl` instance, and then adding that to the TLB's main loop.
In this case, events inside this "sub loop" will only be processed by a single thread at any given time.

## API

### TLB

#### `tlb_new`

Creates a new TLB instance.

#### `tlb_destroy`

Stops a TLB and throws it into the garbage.

#### `tlb_start`

Spin up the requested number of threads and starts them on waiting on the super loop.

#### `tlb_stop`

Stop processing events and spin down all of the threads.

#### `tlb_get_evl`

Get the super loop for the TLB. Use this loop to subscribe things that you would like to receive events for.

## Terminology

| Term | Definition |
| :--: | :--------- |
| TLB | The library's primary API. <sub><sup>Don't ask what TLB stands for.</sup></sub> |
| EVL | Event loop (wrapper around epoll or kqueue). |
| TLB | Main Loop: The event loop that the threads block on. |
| Sub Loop | An event loop that is not blocked on directly, but rather is subscribed to another event loop. |
| Super Loop | An event loop that has other event loops subscribed to it. |
