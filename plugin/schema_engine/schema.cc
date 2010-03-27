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
#include "drizzled/global_charset_info.h"
#include "drizzled/charset.h"
#include "drizzled/charset_info.h"
#include "drizzled/cursor.h"

#include "drizzled/internal/my_sys.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <iostream>
#include <fstream>
#include <string>

using namespace std;
using namespace drizzled;

#define MY_DB_OPT_FILE "db.opt"
#define DEFAULT_FILE_EXTENSION ".dfe" // Deep Fried Elephant

Schema::Schema():
  drizzled::plugin::StorageEngine("schema",
                                  HTON_ALTER_NOT_SUPPORTED |
                                  HTON_HAS_SCHEMA_DICTIONARY |
                                  HTON_SKIP_STORE_LOCK |
                                  HTON_TEMPORARY_NOT_SUPPORTED),
  schema_cache_filled(false)
{
  table_definition_ext= DEFAULT_FILE_EXTENSION;
  pthread_rwlock_init(&schema_lock, NULL);
  prime();
}

Schema::~Schema()
{
  pthread_rwlock_destroy(&schema_lock);
}

void Schema::prime()
{
  CachedDirectory directory(drizzle_data_home, CachedDirectory::DIRECTORY);
  CachedDirectory::Entries files= directory.getEntries();

  pthread_rwlock_wrlock(&schema_lock);

  for (CachedDirectory::Entries::iterator fileIter= files.begin();
       fileIter != files.end(); fileIter++)
  {
    CachedDirectory::Entry *entry= *fileIter;
    message::Schema schema_message;

    if (readSchemaFile(entry->filename, schema_message))
    {
      pair<SchemaCache::iterator, bool> ret=
        schema_cache.insert(make_pair(schema_message.name(), schema_message));

      if (ret.second == false)
      {
        abort(); // If this has happened, something really bad is going down.
      }
    }
  }
  pthread_rwlock_unlock(&schema_lock);
}

void Schema::doGetSchemaNames(std::set<std::string>& set_of_names)
{
  if (not pthread_rwlock_rdlock(&schema_lock))
  {
    for (SchemaCache::iterator iter= schema_cache.begin();
         iter != schema_cache.end();
         iter++)
    {
      set_of_names.insert((*iter).first);
    }
    pthread_rwlock_unlock(&schema_lock);

    return;
  }

  // If for some reason getting a lock should fail, we resort to disk

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
  if (not pthread_rwlock_rdlock(&schema_lock))
  {
    SchemaCache::iterator iter= schema_cache.find(schema_name);
    if (iter != schema_cache.end())
    {
      schema_message.CopyFrom(((*iter).second));
      pthread_rwlock_unlock(&schema_lock);
      return true;
    }
    pthread_rwlock_unlock(&schema_lock);

    return false;
  }

  // Fail to disk based means
  return readSchemaFile(schema_name, schema_message);
}

bool Schema::doCreateSchema(const drizzled::message::Schema &schema_message)
{
  std::string path;
  drizzled::build_table_filename(path, schema_message.name().c_str(), "", false);

  path.erase(path.length()-1);

  if (mkdir(path.c_str(), 0777) == -1)
    return false;

  if (not writeSchemaFile(path.c_str(), schema_message))
  {
    rmdir(path.c_str());

    return false;
  }

  if (not pthread_rwlock_wrlock(&schema_lock))
  {
      pair<SchemaCache::iterator, bool> ret=
        schema_cache.insert(make_pair(schema_message.name(), schema_message));


      if (ret.second == false)
      {
        abort(); // If this has happened, something really bad is going down.
      }
    pthread_rwlock_unlock(&schema_lock);
  }

  return true;
}

bool Schema::doDropSchema(const std::string &schema_name)
{
  string path;
  message::Schema schema_message;

  drizzled::build_table_filename(path, schema_name.c_str(), "", false);
  path.erase(path.length()-1);

  string schema_file(path);
  schema_file.append(1, FN_LIBCHAR);
  schema_file.append(MY_DB_OPT_FILE);

  if (not doGetSchemaDefinition(schema_name, schema_message))
    return false;

  // No db.opt file, no love from us.
  if (access(schema_file.c_str(), F_OK))
  {
    perror(schema_file.c_str());
    return false;
  }

  if (unlink(schema_file.c_str()))
  {
    perror(schema_file.c_str());
    return false;
  }

  if (rmdir(path.c_str()))
  {
    perror(path.c_str());
    CachedDirectory dir(path);
    cerr << dir;
  }

  if (not pthread_rwlock_wrlock(&schema_lock))
  {
    schema_cache.erase(schema_message.name());
    pthread_rwlock_unlock(&schema_lock);
  }

  return true;
}

bool Schema::doAlterSchema(const drizzled::message::Schema &schema_message)
{
  string path;
  drizzled::build_table_filename(path, schema_message.name().c_str(), "", false);
  path.erase(path.length()-1);

  if (access(path.c_str(), F_OK))
    return false;

  if (writeSchemaFile(path.c_str(), schema_message))
  {
    if (not pthread_rwlock_wrlock(&schema_lock))
    {
      schema_cache.erase(schema_message.name());

      pair<SchemaCache::iterator, bool> ret=
        schema_cache.insert(make_pair(schema_message.name(), schema_message));

      if (ret.second == false)
      {
        abort(); // If this has happened, something really bad is going down.
      }

      pthread_rwlock_unlock(&schema_lock);
    }
    else
    {
      abort(); // This would leave us out of sync, suck.
    }
  }

  return true;
}

/**
  path is path to database, not schema file 

  @note we do the rename to make it crash safe.
*/
bool Schema::writeSchemaFile(const char *path, const message::Schema &db)
{
  char schema_file_tmp[FN_REFLEN];
  string schema_file(path);


  schema_file.append(1, FN_LIBCHAR);
  schema_file.append(MY_DB_OPT_FILE);

  snprintf(schema_file_tmp, FN_REFLEN, "%sXXXXXX", schema_file.c_str());

  int fd= mkstemp(schema_file_tmp);

  if (fd == -1)
  {
    perror(schema_file_tmp);

    return false;
  }

  if (not db.SerializeToFileDescriptor(fd))
  {
    cerr << "Couldn't write " << path << "\n";

    if (close(fd) == -1)
      perror(schema_file_tmp);

    if (unlink(schema_file_tmp))
      perror(schema_file_tmp);

    return false;
  }

  if (close(fd) == -1)
  {
    perror(schema_file_tmp);

    if (unlink(schema_file_tmp))
      perror(schema_file_tmp);

    return false;
  }

  if (rename(schema_file_tmp, schema_file.c_str()) == -1)
  {
    if (unlink(schema_file_tmp))
      perror(schema_file_tmp);

    return false;
  }

  return true;
}


bool Schema::readSchemaFile(const std::string &schema_name, drizzled::message::Schema &schema_message)
{
  string db_opt_path;

  /*
    Pass an empty file name, and the database options file name as extension
    to avoid table name to file name encoding.
  */
  build_table_filename(db_opt_path, schema_name.c_str(), "", false);
  db_opt_path.append(MY_DB_OPT_FILE);

  fstream input(db_opt_path.c_str(), ios::in | ios::binary);

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
  else
  {
    perror(db_opt_path.c_str());
  }

  return false;
}

bool Schema::doCanCreateTable(const drizzled::TableIdentifier &identifier)
{
  if (not strcasecmp(identifier.getSchemaName().c_str(), "temporary"))
  {
    return false;
  }

  return true;
}
