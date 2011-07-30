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


#include <config.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/query_id.h>
#include <drizzled/session.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>
#include <algorithm>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <drizzled/module/option_map.h>

#include <sys/un.h>

#include <plugin/mysql_unix_socket_protocol/protocol.h>

#define DRIZZLE_UNIX_SOCKET_PATH "/tmp/mysql.socket"

namespace po= boost::program_options;
namespace fs= boost::filesystem;
using namespace drizzled;
using namespace std;

namespace drizzle_plugin {
namespace mysql_unix_socket_protocol {

static bool clobber= false;

ProtocolCounters Protocol::mysql_unix_counters;

Protocol::~Protocol()
{
  fs::remove(_unix_socket_path);
}

in_port_t Protocol::getPort() const
{
  return 0;
}

static int init(drizzled::module::Context &context)
{  
  const module::option_map &vm= context.getOptions();

  fs::path uds_path(vm["path"].as<fs::path>());
  if (not fs::exists(uds_path))
  {
    Protocol *listen_obj= new Protocol("mysql_unix_socket_protocol", uds_path);
    listen_obj->addCountersToTable();
    context.add(listen_obj);
    context.registerVariable(new sys_var_const_string_val("path", fs::system_complete(uds_path).file_string()));
    context.registerVariable(new sys_var_bool_ptr_readonly("clobber", &clobber));
    context.registerVariable(new sys_var_uint32_t_ptr("max-connections", &Protocol::mysql_unix_counters.max_connections));
  }
  else
  {
    cerr << uds_path << _(" exists already. Do you have another Drizzle or "
                          "MySQL running? Or perhaps the file is stale and "
                          "should be removed?") << std::endl;
    return 0;
  }

  return 0;
}

bool Protocol::getFileDescriptors(std::vector<int> &fds)
{
  int unix_sock= socket(AF_UNIX, SOCK_STREAM, 0);
  if (unix_sock < 0)
  {
    std::cerr << "Can't start server : UNIX Socket";
    return false;
  }

  // In case we restart and find something in our way we move it aside and
  // then attempt to remove it.
  if (clobber)
  {
    fs::path move_file(_unix_socket_path.file_string() + ".old");
    fs::rename(_unix_socket_path, move_file);
    unlink(move_file.file_string().c_str());
  }


  int arg= 1;

  (void) setsockopt(unix_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&arg, sizeof(arg));
  unlink(_unix_socket_path.file_string().c_str());

  sockaddr_un servAddr;
  memset(&servAddr, 0, sizeof(servAddr));

  servAddr.sun_family= AF_UNIX;
  if (_unix_socket_path.file_string().size() > sizeof(servAddr.sun_path))
  {
    std::cerr << "Unix Socket Path length too long. Must be under "
      << sizeof(servAddr.sun_path) << " bytes." << endl;
    return false;
  }
  memcpy(servAddr.sun_path, _unix_socket_path.file_string().c_str(), min(sizeof(servAddr.sun_path)-1,_unix_socket_path.file_string().size()));

  socklen_t addrlen= sizeof(servAddr);
  if (::bind(unix_sock, reinterpret_cast<sockaddr *>(&servAddr), addrlen) < 0)
  { 
    std::cerr << "Can't start server : Bind on unix socket." << std::endl;
    std::cerr << "Do you already have another of drizzled or mysqld running on socket: " << _unix_socket_path << "?" << std::endl;
    std::cerr << "Can't start server : UNIX Socket" << std::endl;

    return false;
  }

  if (listen(unix_sock, (int) 1000) < 0)
  {
    std::cerr << "listen() on Unix socket failed with error " << errno << "\n";
  }
  else
  {
    errmsg_printf(error::INFO, _("Listening on %s"), _unix_socket_path.file_string().c_str());
  }

  fds.push_back(unix_sock);

  return false;
}

plugin::Client *Protocol::getClient(int fd)
{
  int new_fd= acceptTcp(fd);
  return new_fd == -1 ? NULL : new ClientMySQLProtocol(new_fd, getCounters());
}

static void init_options(drizzled::module::option_context &context)
{
  context("path",
          po::value<fs::path>()->default_value(DRIZZLE_UNIX_SOCKET_PATH),
          _("Path used for MySQL UNIX Socket Protocol."));
  context("clobber",
          _("Clobber socket file if one is there already."));
  context("max-connections",
          po::value<uint32_t>(&Protocol::mysql_unix_counters.max_connections)->default_value(1000),
          _("Maximum simultaneous connections."));
}

} /* namespace mysql_unix_socket_protocol */
} /* namespace drizzle_plugin */

DRIZZLE_PLUGIN(drizzle_plugin::mysql_unix_socket_protocol::init, NULL, drizzle_plugin::mysql_unix_socket_protocol::init_options);
