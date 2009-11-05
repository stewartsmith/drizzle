/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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

#ifndef DRIZZLE_PLUGIN_INFORMATION_ENGINE_OPEN_TABLES_H
#define DRIZZLE_PLUGIN_INFORMATION_ENGINE_OPEN_TABLES_H

#include "information_cursor.h"
#include "information_share.h"

#include <string>
#include <vector>
#include <map>

/**
 * @class OpenTables
 *
 * Class which tracks all the open tables for the I_S storage engine.
 * Encapsulating this functionality in a class will make it easier for us to
 * change things such as whether a std::map or HASH is used to lookup the
 * open tables.
 */
class OpenTables
{
public:

  /**
   * Instantiate the singleton object.
   */
  static OpenTables &singleton()
  {
    static OpenTables open_tabs;
    return open_tabs;
  }

  /**
   * @param[in] name the name of the table share to retrieve
   * @return the corresponding table share
   */
  InformationShare *getShare(const std::string &name);

  /**
   * Free the share that is currently open.
   */
  void freeShare();

private:

  pthread_mutex_t mutex;

  std::map<const std::string, InformationShare *>
    open_tables;

  InformationShare *share;

  OpenTables()
    :
      mutex(),
      open_tables(),
      share(NULL)
  {
    pthread_mutex_init(&mutex, MY_MUTEX_INIT_FAST);
  }

  ~OpenTables()
  {
    pthread_mutex_destroy(&mutex);
  }

  OpenTables(const OpenTables&);
};

#endif /* DRIZZLE_PLUGIN_INFORMATION_ENGINE_OPEN_TABLES_H */
