/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>
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

namespace drizzled {

extern back_log_constraints back_log;
extern uint32_t drizzled_bind_timeout;

int plugin::ListenTcp::acceptTcp(int fd)
{
  for (int retry= 0; retry < 10; retry++)
  {
    int new_fd= accept(fd, NULL, 0);
    if (new_fd != -1)
      return new_fd;
    if (errno != EINTR && errno != EAGAIN)
      break;
  }
  if ((accept_error_count++ & 255) == 0)
  {
    sql_perror(_("accept() failed with errno %d"));
  }
  if (errno == ENFILE || errno == EMFILE)
    sleep(1);
  return -1;
}

bool plugin::ListenTcp::getFileDescriptors(std::vector<int> &fds)
{
  int ret;
  char host_buf[NI_MAXHOST];
  char port_buf[NI_MAXSERV];
  addrinfo hints;
  addrinfo *ai_list;
  uint32_t this_wait;
  int flags= 1;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags= AI_PASSIVE;
  hints.ai_socktype= SOCK_STREAM;

  snprintf(port_buf, NI_MAXSERV, "%d", getPort());
  ret= getaddrinfo(getHost().empty() ? NULL : getHost().c_str(), port_buf, &hints, &ai_list);
  if (ret != 0)
  {
    errmsg_printf(error::ERROR, _("getaddrinfo() failed with error %s"),
                  gai_strerror(ret));
    return true;
  }

  for (addrinfo* ai= ai_list; ai != NULL; ai= ai->ai_next)
  {
    ret= getnameinfo(ai->ai_addr, ai->ai_addrlen, host_buf, NI_MAXHOST,
                     port_buf, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
    if (ret != 0)
    {
      strcpy(host_buf, "-");
      strcpy(port_buf, "-");
    }

    int fd= socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
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
        sql_perror(_("setsockopt(IPV6_V6ONLY)"));
        return true;
      }
    }
#endif

    ret= fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (ret != 0 || !(fcntl(fd, F_GETFD, 0) & FD_CLOEXEC))
    {
      sql_perror(_("fcntl(FD_CLOEXEC)"));
      return true;
    }

    ret= setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
    if (ret != 0)
    {
      sql_perror(_("setsockopt(SO_REUSEADDR)"));
      return true;
    }

    ret= setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags));
    if (ret != 0)
    {
      sql_perror(_("setsockopt(SO_KEEPALIVE)"));
      return true;
    }

    linger ling= {0, 0};
    ret= setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    if (ret != 0)
    {
      sql_perror(_("setsockopt(SO_LINGER)"));
      return true;
    }

    ret= setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));
    if (ret != 0)
    {
      sql_perror(_("setsockopt(TCP_NODELAY)"));
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
    for (uint32_t waited= 0, retry= 1; ; retry++, waited+= this_wait)
    {
      if (((ret= ::bind(fd, ai->ai_addr, ai->ai_addrlen)) == 0) ||
          (errno != EADDRINUSE) || (waited >= drizzled_bind_timeout))
      {
        break;
      }

      errmsg_printf(error::INFO, _("Retrying bind() on %u"), getPort());
      this_wait= retry * retry / 3 + 1;
      sleep(this_wait);
    }

    if (ret < 0)
    {
      std::string error_message;

      error_message+= host_buf;
      error_message+= ":";
      error_message+= port_buf;
      error_message+= _(" failed to bind");
      sql_perror(error_message);

      return true;
    }

    if (listen(fd, (int) back_log) < 0)
    {
      sql_perror("listen()");
      return true;
    }

    fds.push_back(fd);

    errmsg_printf(error::INFO, _("Listening on %s:%s"), host_buf, port_buf);
  }

  freeaddrinfo(ai_list);

  return false;
}

const std::string plugin::ListenTcp::getHost() const
{
  return "";
}

} /* namespace drizzled */
