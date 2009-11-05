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
  Shared class for correct LOCK operation
  TODO -> Fix to never remove/etc. We could generate all of these in startup if we wanted to.
  Tracking count? I'm not sure that is needed at all. We could possibly make this a member of
  engine as well (should we just hide the share detail?)
*/

class InformationShare 
{

public:

  InformationShare() :
    count(1),
    i_s_table(NULL),
    lock()
  {
    thr_lock_init(&lock);
  };

  ~InformationShare() 
  {
    thr_lock_delete(&lock);
  }

  void incUseCount(void) 
  { 
    count++; 
  }

  uint32_t decUseCount(void) 
  { 
    return --count; 
  }

  uint32_t getUseCount() const
  {
    return count;
  }

  const std::string &getName() const
  {
    return i_s_table->getTableName();
  }

  void setInfoSchemaTable(const std::string &in_name)
  {
    i_s_table= drizzled::plugin::InfoSchemaTable::getTable(in_name.c_str());
  }

  drizzled::plugin::InfoSchemaTable *getInfoSchemaTable()
  {
    return i_s_table;
  }

  void initThreadLock()
  {
    thr_lock_init(&lock);
  }

  THR_LOCK *getThreadLock()
  {
    return &lock;
  }

private:

  uint32_t count;
  drizzled::plugin::InfoSchemaTable *i_s_table;
  THR_LOCK lock;

};

#endif /* PLUGIN_INFORMATION_ENGINE_INFORMATION_SHARE_H */
