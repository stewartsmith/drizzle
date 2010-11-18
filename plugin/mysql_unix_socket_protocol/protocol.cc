/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <drizzled/query_id.h>
#include <drizzled/sql_state.h>
#include <drizzled/session.h>
#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/m_string.h"
#include <algorithm>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <drizzled/module/option_map.h>

#include <sys/un.h>

#include "plugin/mysql_unix_socket_protocol/protocol.h"

#define DRIZZLE_UNIX_SOCKET_PATH "/tmp/mysql.socket"

namespace po= boost::program_options;
namespace fs= boost::filesystem;
using namespace drizzled;
using namespace std;

namespace mysql_unix_socket_protocol
{

Protocol::~Protocol()
{
  fs::remove(unix_socket_path);
}

const char* Protocol::getHost(void) const
{
  return unix_socket_path.file_string().c_str();
}

in_port_t Protocol::getPort(void) const
{
  return 0;
}

static int init(drizzled::module::Context &context)
{  
  const module::option_map &vm= context.getOptions();

  fs::path uds_path(vm["path"].as<fs::path>());
  if (not fs::exists(uds_path))
  {
    context.add(new Protocol("mysql_unix_socket_protocol",
                             true,
                             uds_path));
    context.registerVariable(new sys_var_fs_path("path", uds_path));
  }
  else
  {
    cerr << uds_path << _(" exists already. Do you have another Drizzle or "
                          "MySQL running? Or perhaps the file is stale and "
                          "should be removed?") << std::endl;
    return 1;
  }

  return 0;
}

bool Protocol::getFileDescriptors(std::vector<int> &fds)
{
  struct sockaddr_un servAddr;
  socklen_t addrlen;
  int unix_sock;

  if ((unix_sock= socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
  {
    std::cerr << "Can't start server : UNIX Socket";
    return false;
  }

  memset(&servAddr, 0, sizeof(servAddr));

  servAddr.sun_family= AF_UNIX;
  strcpy(servAddr.sun_path, unix_socket_path.file_string().c_str());

  // In case we restart and find something in our way we move it aside and
  // then attempt to remove it.
  {
    fs::path move_file(unix_socket_path.file_string() + ".old");
    fs::rename(unix_socket_path, move_file);
    unlink(move_file.file_string().c_str());
  }


  int arg= 1;

  (void) setsockopt(unix_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&arg, sizeof(arg));
  unlink(unix_socket_path.file_string().c_str());

  addrlen= sizeof(servAddr);
  if (::bind(unix_sock, reinterpret_cast<sockaddr *>(&servAddr), addrlen) < 0)
  { 
    std::cerr << "Can't start server : Bind on unix socket." << std::endl;
    std::cerr << "Do you already have another of drizzled or mysqld running on socket: " << unix_socket_path << "?" << std::endl;
    std::cerr << "Can't start server : UNIX Socket" << std::endl;

    return false;
  }

  if (listen(unix_sock, (int) 1000) < 0)
  {
    std::cerr << "listen() on Unix socket failed with error " << errno << "\n";
  }
  else
  {
    std::cerr << "Listening on " << unix_socket_path << "\n";
  }
  (void) unlink(unix_socket_path.c_str());

  fds.push_back(unix_sock);

  return false;
}


static void init_options(drizzled::module::option_context &context)
{
  context("path",
          po::value<fs::path>()->default_value(DRIZZLE_UNIX_SOCKET_PATH),
          N_("Path used for MySQL UNIX Socket Protocol."));
}

} /* namespace mysql_unix_socket_protocol */

DRIZZLE_PLUGIN(mysql_unix_socket_protocol::init, NULL, mysql_unix_socket_protocol::init_options);
