#ifndef TLB_PIPE_H
#define TLB_PIPE_H

#include <tlb/core.h>

struct tlb_pipe {
  union {
    struct {
      int fd_read;
      int fd_write;
    };
    int fds[2];
  };
};

TLB_EXTERN_C_BEGIN

int tlb_pipe_open(struct tlb_pipe *pipe);
void tlb_pipe_close(struct tlb_pipe *pipe);

TLB_EXTERN_C_END

#endif /* TLB_PIPE_H */
