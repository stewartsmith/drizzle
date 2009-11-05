/* Copyright (C) 2005 MySQL AB

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


#ifndef PLUGIN_INFORMATION_ENGINE_INFORMATION_ENGINE_H
#define PLUGIN_INFORMATION_ENGINE_INFORMATION_ENGINE_H

#include <drizzled/server_includes.h>
#include <drizzled/cursor.h>
#include <mysys/thr_lock.h>

#include "information_share.h"
#include "information_cursor.h"

class InformationEngine : public drizzled::plugin::StorageEngine
{
public:
  InformationEngine(const std::string &name_arg)
   : drizzled::plugin::StorageEngine(name_arg,
                                     HTON_FILE_BASED
                                      | HTON_HAS_DATA_DICTIONARY) 
  {}

  int doCreateTable(Session *,
                    const char *,
                    Table&,
                    HA_CREATE_INFO&,
                    drizzled::message::Table&)
  {
    return 1;
  }

  int doDropTable(Session&, const std::string) 
  { 
    return 0; 
  }

  virtual Cursor *create(TableShare *table, MEM_ROOT *mem_root)
  {
    return new (mem_root) InformationCursor(this, table);
  }

  const char **bas_ext() const 
  {
    return NULL;
  }

  void doGetTableNames(CachedDirectory&, 
                       std::string &db, 
                       std::set<std::string> &set_of_names);

  int doGetTableDefinition(Session &session,
                           const char *path,
                           const char *db,
                           const char *table_name,
                           const bool is_tmp,
                           drizzled::message::Table *table_proto);

};

#endif /* PLUGIN_INFORMATION_ENGINE_INFORMATION_ENGINE_H */
