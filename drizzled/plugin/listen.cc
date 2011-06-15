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

#include <drizzled/errmsg_print.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/null_client.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <poll.h>

namespace drizzled {
namespace plugin {

static std::vector<plugin::Listen*> listen_list;
std::vector<plugin::Listen*> listen_fd_list;
std::vector<pollfd> fd_list;
uint32_t fd_count= 0;
int wakeup_pipe[2];

ListenVector& Listen::getListenProtocols()
{
  return listen_list;
}

bool Listen::addPlugin(plugin::Listen *listen_obj)
{
  listen_list.push_back(listen_obj);
  return false;
}

void Listen::removePlugin(plugin::Listen *listen_obj)
{
  listen_list.erase(std::remove(listen_list.begin(), listen_list.end(), listen_obj), listen_list.end());
}

bool Listen::setup()
{
  BOOST_FOREACH(plugin::Listen* it, listen_list)
  {
    std::vector<int> fds;
    if (it->getFileDescriptors(fds))
    {
      errmsg_printf(error::ERROR, _("Error getting file descriptors"));
      return true;
    }

    fd_list.resize(fd_count + fds.size() + 1);
    
    BOOST_FOREACH(int fd, fds)
    {
      fd_list[fd_count].fd= fd;
      fd_list[fd_count].events= POLLIN | POLLERR;
      listen_fd_list.push_back(it);
      fd_count++;
    }
  }

  if (fd_count == 0)
  {
    errmsg_printf(error::ERROR, _("No sockets could be bound for listening"));
    return true;
  }

  /*
    We need a pipe to wakeup the listening thread since some operating systems
    are stupid. *cough* OSX *cough*
  */
  if (pipe(wakeup_pipe) == -1)
  {
    sql_perror("pipe()");
    return true;
  }

  fd_list.resize(fd_count + 1);

  fd_list[fd_count].fd= wakeup_pipe[0];
  fd_list[fd_count].events= POLLIN | POLLERR;
  fd_count++;

  return false;
}

Client *plugin::Listen::getClient()
{
  while (1)
  {
    int ready= poll(&fd_list[0], fd_count, -1);
    if (ready == -1)
    {
      if (errno != EINTR)
      {
        sql_perror("poll()");
      }
      continue;
    }
    else if (ready == 0)
      continue;

    for (uint32_t x= 0; x < fd_count; x++)
    {
      if (fd_list[x].revents != POLLIN)
        continue;

      /* Check to see if the wakeup_pipe was written to. */
      if (x == fd_count - 1)
      {
        /* Close all file descriptors now. */
        for (x= 0; x < fd_count; x++)
        {
          (void) ::shutdown(fd_list[x].fd, SHUT_RDWR);
          (void) close(fd_list[x].fd);
          fd_list[x].fd= -1;
        }

        /* wakeup_pipe[0] was closed in the for loop above. */
        (void) close(wakeup_pipe[1]);

        return NULL;
      }

      if (plugin::Client* client= listen_fd_list[x]->getClient(fd_list[x].fd))
        return client;
    }
  }
}

Client *plugin::Listen::getNullClient()
{
  return new plugin::NullClient();
}

void Listen::shutdown()
{
  ssize_t ret= write(wakeup_pipe[1], "\0", 1);
  assert(ret == 1);
}

} /* namespace plugin */
} /* namespace drizzled */
