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

#ifndef PLUGIN_MEMCACHED_STATS_STATS_TABLE_H
#define PLUGIN_MEMCACHED_STATS_STATS_TABLE_H

#include "drizzled/info_schema.h"

#include <vector>

class MemcachedStatsISMethods : public InfoSchemaMethods
{
public:
  MemcachedStatsISMethods(const std::string &in_servers)
    :
      InfoSchemaMethods(),
      servers_string(in_servers)
  {}

  virtual int fillTable(Session *session,
                        TableList *tables,
                        COND *cond);

  void setServersString(const std::string &in_servers)
  {
    servers_string.assign(in_servers);
  }

private:
  std::string servers_string;
};

bool createMemcachedStatsColumns(std::vector<const ColumnInfo *> &cols);

void clearMemcachedColumns(std::vector<const ColumnInfo *> &cols);

#endif /* PLUGIN_MEMCACHED_STATS_STATS_TABLE_H */
