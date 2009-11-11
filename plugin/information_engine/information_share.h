/* Copyright (C) 2009 Sun Microsystems

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#ifndef PLUGIN_INFORMATION_ENGINE_INFORMATION_SHARE_H
#define PLUGIN_INFORMATION_ENGINE_INFORMATION_SHARE_H

#include <drizzled/server_includes.h>
#include <drizzled/plugin/info_schema_table.h>

#include <string>

/*
*/

class InformationShare 
{
private:
  uint32_t count;
  drizzled::plugin::InfoSchemaTable *table;
  THR_LOCK lock;


public:

  InformationShare(const std::string &in_name) :
    count(1)
  {
    thr_lock_init(&lock);
    table= drizzled::plugin::InfoSchemaTable::getTable(in_name.c_str());
  }

  ~InformationShare() 
  {
    thr_lock_delete(&lock);
  }

  /**
   * Increment the counter which tracks how many instances of this share are
   * currently open.
   * @return the new counter value
   */
  uint32_t incUseCount(void) 
  { 
    return ++count; 
  }

  /**
   * Decrement the count which tracks how many instances of this share are
   * currently open.
   * @return the new counter value
   */
  uint32_t decUseCount(void) 
  { 
    return --count; 
  }

  /**
   * @ return the value of the use counter for this share
   */
  uint32_t getUseCount() const
  {
    return count;
  }

  /**
   * @return the table name associated with this share.
   */
  const std::string &getName() const
  {
    return table->getTableName();
  }

  /**
   * @return the I_S table associated with this share.
   */
  drizzled::plugin::InfoSchemaTable *getInfoSchemaTable()
  {
    return table;
  }

  /**
   * @return the thread lock for this share.
   */
  THR_LOCK *getThreadLock()
  {
    return &lock;
  }
};

#endif /* PLUGIN_INFORMATION_ENGINE_INFORMATION_SHARE_H */
