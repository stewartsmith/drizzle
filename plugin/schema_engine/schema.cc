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
#include "drizzled/sql_table.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>
#include <fstream>
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
}

bool Schema::doGetSchemaDefinition(const std::string &schema_name, message::Schema &schema_message)
{
  char db_opt_path[FN_REFLEN];
  size_t length;

  /*
    Pass an empty file name, and the database options file name as extension
    to avoid table name to file name encoding.
  */
  length= build_table_filename(db_opt_path, sizeof(db_opt_path),
                               schema_name.c_str(), "", false);
  strcpy(db_opt_path + length, MY_DB_OPT_FILE);

  fstream input(db_opt_path, ios::in | ios::binary);

  /**
    @note If parsing fails, either someone has done a "mkdir" or has deleted their opt file.
    So what do we do? We muddle through the adventure by generating 
    one with a name in it, and the charset set to the default.
  */
  if (input.good())
  {
    if (schema_message.ParseFromIstream(&input))
    {
      return true;
    }
  }

  return false;
}

bool Schema::doCreateSchema(const drizzled::message::Schema &schema_message)
{
  char	 path[FN_REFLEN+16];
  uint32_t path_len;
  int error_erno;
  path_len= drizzled::build_table_filename(path, sizeof(path), schema_message.name().c_str(), "", false);
  path[path_len-1]= 0;                    // remove last '/' from path

  if (mkdir(path, 0777) == -1)
    return false;

  error_erno= write_schema_file(path, schema_message);
  if (error_erno && error_erno != EEXIST)
  {
    rmdir(path);

    return false;
  }

  return true;
}

bool Schema::doAlterSchema(const drizzled::message::Schema &schema_message)
{
  char	 path[FN_REFLEN+16];
  uint32_t path_len;
  int error_erno;
  path_len= drizzled::build_table_filename(path, sizeof(path), schema_message.name().c_str(), "", false);
  path[path_len-1]= 0;                    // remove last '/' from path

  error_erno= write_schema_file(path, schema_message);
  if (error_erno && error_erno != EEXIST)
  {
    return false;
  }

  return true;
}
