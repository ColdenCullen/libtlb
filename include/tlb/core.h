#ifndef TLB_CORE_H
#define TLB_CORE_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus /* C++ */
#  define TLB_EXTERN_C_BEGIN extern "C" {
#  define TLB_EXTERN_C_END }
#else /* C */
#  define TLB_EXTERN_C_BEGIN
#  define TLB_EXTERN_C_END

#  ifdef TLB_HAS_THREADS_H
#    include <threads.h>
#  else
#    include <tinycthread.h>
#  endif
#endif /* C/C++ */

enum {
  TLB_SUCCESS = 0,
  TLB_FAIL = -1,
};

#ifndef NDEBUG
#  define TLB_ASSERT assert
#else /* release */
#  define TLB_ASSERT(x) (void)(x)
#endif

#define TLB_BIT(b) (1ull << (b))
#define TLB_CONTAINER_OF(ptr, type, member) (void *)(((uint8_t *)(ptr)) - offsetof(type, member))
#define TLB_ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))
#define TLB_ZERO(object) memset(&(object), 0, sizeof(object))

#define TLB_CHECK_HANDLE(checker, expr, handle) \
  ({                                            \
    typeof(expr) _ = (expr);                    \
    if (!(checker _)) {                         \
      handle;                                   \
    }                                           \
    _;                                          \
  })
#define TLB_CHECK_RETURN(checker, expr, ret_value) TLB_CHECK_HANDLE(checker, (expr), return (ret_value))
#define TLB_CHECK_GOTO(checker, expr, label) \
  TLB_CHECK_HANDLE(checker, (expr), goto label) /* NOLINT(bugprone-macro-parentheses) */
#define TLB_CHECK_ASSERT(checker, expr) TLB_CHECK_HANDLE(checker, (expr), TLB_ASSERT(false))
#define TLB_CHECK(checker, expr) TLB_CHECK_RETURN(checker, (expr), _)

#define TLB_MIN(a, b)   \
  ({                    \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a < _b ? _a : _b;  \
  })
#define TLB_MAX(a, b)   \
  ({                    \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a > _b ? _a : _b;  \
  })

#define TLB_LOG(text) TLB_LOGF("%s", text)
#define TLB_LOGF(format, ...) \
  fprintf(stderr, "[%zu] %s:%d " format "\n", (size_t)thrd_current(), __FILE__, __LINE__, __VA_ARGS__);

#endif /* TLB_CORE_H */
