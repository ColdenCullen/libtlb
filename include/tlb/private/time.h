#ifndef TLB_PRIVATE_TIME_H
#define TLB_PRIVATE_TIME_H

#include "tlb/core.h"

#include "tlb/event_loop.h"

#include <time.h>

TLB_EXTERN_C_BEGIN

static inline struct timespec tlb_timeout_to_timespec(int timeout) {
  struct timespec timeout_spec = {};
  if (timeout == TLB_WAIT_NONE) {
    TLB_ZERO(timeout_spec);
  } else {
    static const int millis_per_second = 1000;
    static const int nanos_per_second = 1000000;
    timeout_spec.tv_sec = timeout / millis_per_second;
    timeout_spec.tv_nsec = (timeout % millis_per_second) * nanos_per_second;
  }
  return timeout_spec;
}

TLB_EXTERN_C_END

#endif /* TLB_PRIVATE_TIME_H */
