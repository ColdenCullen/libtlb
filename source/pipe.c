#include <tlb/pipe.h>

#include <fcntl.h>
#include <unistd.h>

int tlb_pipe_open(struct tlb_pipe *pip) {
  TLB_CHECK(0 ==, pipe(pip->fds));

  for (size_t i = 0; i < TLB_ARRAY_LENGTH(pip->fds); ++i) {
    int flags = TLB_CHECK_GOTO(-1 !=, fcntl(pip->fds[i], F_GETFL), error);
    flags |= O_NONBLOCK | O_CLOEXEC;
    TLB_CHECK_GOTO(-1 !=, fcntl(pip->fds[i], F_SETFL, flags), error);
  }

  return 0;

error:
  tlb_pipe_close(pip);
  return -1;
}

void tlb_pipe_close(struct tlb_pipe *pipe) {
  close(pipe->fd_read);
  close(pipe->fd_write);
}
