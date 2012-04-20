/*    $Header: /cvsroot/wikipedia/willow/src/bin/willow/daemon.c,v 1.1 2005/05/02 19:15:21 kateturner Exp $    */
/*    $NetBSD: daemon.c,v 1.9 2003/08/07 16:42:46 agc Exp $    */
/*-
 * Copyright (c) 1990, 1993
 *    The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2010
 *    Stewart Smith
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#if defined __SUNPRO_C || defined __DECC || defined __HP_cc
# pragma ident "@(#)$Header: /cvsroot/wikipedia/willow/src/bin/willow/daemon.c,v 1.1 2005/05/02 19:15:21 kateturner Exp $"
# pragma ident "$NetBSD: daemon.c,v 1.9 2003/08/07 16:42:46 agc Exp $"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

#include <drizzled/daemon.h>

namespace drizzled
{

int parent_pipe_fds[2];

extern "C"
{

static void sigchld_handler(int sig)
{
  int status= -1;
  if (sig == SIGCHLD)
  {
    (void)wait(&status);
  }
  _exit(status);
}

}

void daemon_is_ready()
{
  int fd;
  ssize_t wbytes;
  while ((wbytes= write(parent_pipe_fds[1], "\0", sizeof("\0"))) == 0)
  {
    if (wbytes < 0)
    {
      perror("write");
      _exit(errno);
    }
  }
  if (close(parent_pipe_fds[1]))
  {
    perror("close");
    _exit(errno);
  }

  if ((fd = open("/dev/null", O_RDWR, 0)) != -1) 
  {
    if(dup2(fd, STDIN_FILENO) < 0)
    {
      perror("dup2 stdin");
      return;
    }

    if(dup2(fd, STDOUT_FILENO) < 0)
    {
      perror("dup2 stdout");
      return;
    }

    if(dup2(fd, STDERR_FILENO) < 0)
    {
      perror("dup2 stderr");
      return;
    }

    if (fd > STDERR_FILENO)
    {
      if (close(fd) < 0)
      {
        perror("close");
        return;
      }
    }
  }
}

bool daemonize()
{
  pid_t child= -1;

  if (pipe(parent_pipe_fds))
  {
      perror("pipe");
      _exit(errno);
  }

  child= fork();

  switch (child)
  {
  case -1:
    return true;

  case 0:
    break;

  default:
    {
      /* parent */
      char ready_byte[1];
      size_t rbytes;
      /* Register SIGCHLD handler for case where child exits before
         writing to the pipe */
      signal(SIGCHLD, sigchld_handler);

      if (close(parent_pipe_fds[1]))
      {
          perror("close");
          _exit(errno);
      }
      /* If the pipe is closed before a write, we exit -1, otherwise errno is used */
      if ((rbytes= read(parent_pipe_fds[0],ready_byte,sizeof(ready_byte))) < 1)
      {
          int estatus= -1;
          if (rbytes != 0)
          {
              estatus= errno;
              perror("read");
          }
          _exit(estatus);
      }
      if (close(parent_pipe_fds[0]))
      {
          perror("close");
          _exit(errno);
      }

      _exit(EXIT_SUCCESS);
    }
  }

  /* child */
  if (close(parent_pipe_fds[0]))
  {
      perror("close");
      _exit(errno);
  }
  if (setsid() == -1)
  {
    return true;
  }

  return false; 
}

} /* namespace drizzled */
