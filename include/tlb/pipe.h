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

ssize_t tlb_pipe_read(struct tlb_pipe *pipe, void *buf, size_t count);
ssize_t tlb_pipe_write(struct tlb_pipe *pipe, const void *buf, size_t count);

TLB_EXTERN_C_END

#endif /* TLB_PIPE_H */
