/* 
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef PLUGIN_MEMCACHED_STATS_SYSVAR_HOLDER_H
#define PLUGIN_MEMCACHED_STATS_SYSVAR_HOLDER_H

#include <string>

class SysvarHolder
{
public:

  static SysvarHolder &singleton()
  {
    static SysvarHolder holder;
    return holder;
  }

  void setServersString(const std::string &in_servers)
  {
    servers_string.assign(in_servers);
  }

  void setServersStringVar(const std::string &in_servers)
  {
    pthread_mutex_lock(&mutex);
    servers_string.assign(in_servers);
  }

  void updateServersSysvar(const char **var_ptr)
  {
    *var_ptr= servers_string.c_str();
    pthread_mutex_unlock(&mutex);
  }

  const std::string getServersString() const
  {
    return servers_string;
  }

private:

  pthread_mutex_t mutex;

  std::string servers_string;

  SysvarHolder()
    :
      mutex(),
      servers_string()
  {
    pthread_mutex_init(&mutex, NULL);
  }

  ~SysvarHolder()
  {
    pthread_mutex_destroy(&mutex);
  }

  SysvarHolder(const SysvarHolder&);
};

#endif /* PLUGIN_MEMCACHED_STATS_SYSVAR_HOLDER_H */
