/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin/listen_tcp.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/constrained_value.h>

#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <cerrno>

#define MAX_ACCEPT_RETRY	10	// Test accept this many times

namespace drizzled
{
extern back_log_constraints back_log;
extern uint32_t drizzled_bind_timeout;


int plugin::ListenTcp::acceptTcp(int fd)
{
  int new_fd;
  uint32_t retry;

  for (retry= 0; retry < MAX_ACCEPT_RETRY; retry++)
  {
    new_fd= accept(fd, NULL, 0);
    if (new_fd != -1 || (errno != EINTR && errno != EAGAIN))
      break;
  }

  if (new_fd == -1)
  {
    if ((accept_error_count++ & 255) == 0)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("accept() failed with errno %d"),
                    errno);
    }

    if (errno == ENFILE || errno == EMFILE)
      sleep(1);

    return -1;
  }

  return new_fd;
}

bool plugin::ListenTcp::getFileDescriptors(std::vector<int> &fds)
{
  int ret;
  char host_buf[NI_MAXHOST];
  char port_buf[NI_MAXSERV];
  struct addrinfo hints;
  struct addrinfo *ai;
  struct addrinfo *ai_list;
  int fd= -1;
  uint32_t waited;
  uint32_t this_wait;
  uint32_t retry;
  struct linger ling= {0, 0};
  int flags= 1;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags= AI_PASSIVE;
  hints.ai_socktype= SOCK_STREAM;

  snprintf(port_buf, NI_MAXSERV, "%d", getPort());
  ret= getaddrinfo(getHost().empty() ? NULL : getHost().c_str(), port_buf, &hints, &ai_list);
  if (ret != 0)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("getaddrinfo() failed with error %s"),
                  gai_strerror(ret));
    return true;
  }

  for (ai= ai_list; ai != NULL; ai= ai->ai_next)
  {
    ret= getnameinfo(ai->ai_addr, ai->ai_addrlen, host_buf, NI_MAXHOST,
                     port_buf, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
    if (ret != 0)
    {
      strcpy(host_buf, "-");
      strcpy(port_buf, "-");
    }

    fd= socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1)
    {
      /*
        Call to socket() can fail for some getaddrinfo results, try another.
      */
      continue;
    }

#ifdef IPV6_V6ONLY
    if (ai->ai_family == AF_INET6)
    {
      flags= 1;
      ret= setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &flags, sizeof(flags));
      if (ret != 0)
      {
        errmsg_printf(ERRMSG_LVL_ERROR,
                      _("setsockopt(IPV6_V6ONLY) failed with errno %d"),
                      errno);
        return true;
      }
    }
#endif

    ret= fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (ret != 0 || !(fcntl(fd, F_GETFD, 0) & FD_CLOEXEC))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("fcntl(FD_CLOEXEC) failed with errno %d"),
                    errno);
      return true;
    }

    ret= setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
    if (ret != 0)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("setsockopt(SO_REUSEADDR) failed with errno %d"),
                    errno);
      return true;
    }

    ret= setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags));
    if (ret != 0)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("setsockopt(SO_KEEPALIVE) failed with errno %d"),
                    errno);
      return true;
    }

    ret= setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    if (ret != 0)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("setsockopt(SO_LINGER) failed with errno %d"),
                    errno);
      return true;
    }

    ret= setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));
    if (ret != 0)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("setsockopt(TCP_NODELAY) failed with errno %d"),
                    errno);
      return true;
    }

    /*
      Sometimes the port is not released fast enough when stopping and
      restarting the server. This happens quite often with the test suite
      on busy Linux systems. Retry to bind the address at these intervals:
      Sleep intervals: 1, 2, 4,  6,  9, 13, 17, 22, ...
      Retry at second: 1, 3, 7, 13, 22, 35, 52, 74, ...
      Limit the sequence by drizzled_bind_timeout.
    */
    for (waited= 0, retry= 1; ; retry++, waited+= this_wait)
    {
      if (((ret= ::bind(fd, ai->ai_addr, ai->ai_addrlen)) == 0) ||
          (errno != EADDRINUSE) || (waited >= drizzled_bind_timeout))
      {
        break;
      }

      errmsg_printf(ERRMSG_LVL_INFO, _("Retrying bind() on %u\n"), getPort());
      this_wait= retry * retry / 3 + 1;
      sleep(this_wait);
    }

    if (ret < 0)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("bind() failed with errno: %d\n"),
                    errno);
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Do you already have another drizzled running?\n"));
      return true;
    }

    if (listen(fd, (int) back_log) < 0)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("listen() failed with errno %d\n"), errno);
      return true;
    }

    fds.push_back(fd);

    errmsg_printf(ERRMSG_LVL_INFO, _("Listening on %s:%s\n"), host_buf,
                  port_buf);
  }

  freeaddrinfo(ai_list);

  return false;
}

const std::string plugin::ListenTcp::getHost(void) const
{
  return "";
}

} /* namespace drizzled */
