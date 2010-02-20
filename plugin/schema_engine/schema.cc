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

#include "plugin/schema_engine/schema.h"
#include "drizzled/db.h"

#include <string>

using namespace std;
using namespace drizzled;

Schema::Schema():
  drizzled::plugin::StorageEngine("schema",
                                  HTON_ALTER_NOT_SUPPORTED |
                                  HTON_HAS_DATA_DICTIONARY |
                                  HTON_SKIP_STORE_LOCK |
                                  HTON_TEMPORARY_NOT_SUPPORTED)
{
}

void Schema::doGetSchemaNames(std::set<std::string>& set_of_names)
{
  CachedDirectory directory(drizzle_data_home, CachedDirectory::DIRECTORY);

  CachedDirectory::Entries files= directory.getEntries();

  for (CachedDirectory::Entries::iterator fileIter= files.begin();
       fileIter != files.end(); fileIter++)
  {
    CachedDirectory::Entry *entry= *fileIter;
    set_of_names.insert(entry->filename);
  }

  set_of_names.insert("information_schema"); // special cases suck
}

bool Schema::doGetSchemaDefinition(const std::string &schema_name, message::Schema &proto)
{
  return not get_database_metadata(schema_name.c_str(), proto);
}
