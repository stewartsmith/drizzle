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
                                  HTON_HAS_DATA_DICTIONARY |
                                  HTON_HAS_SCHEMA_DICTIONARY |
                                  HTON_SKIP_STORE_LOCK |
                                  HTON_TEMPORARY_NOT_SUPPORTED),
  schema_cache_filled(false)
{
  table_definition_ext= DEFAULT_FILE_EXTENSION;
  pthread_rwlock_init(&schema_lock, NULL);
  prime();
#if 0
  message::Schema schema_message;

  schema_message.set_name("temporary_tables");

  doCreateSchema(schema_message);
#endif
}

Schema::~Schema()
{
  pthread_rwlock_destroy(&schema_lock);
}

int Schema::doGetTableDefinition(Session &,
                                 const char *path,
                                 const char *,
                                 const char *,
                                 const bool,
                                 drizzled::TableIdentifier &,
                                 message::Table &table_proto)
{
  string proto_path(path);
  proto_path.append(DEFAULT_FILE_EXTENSION);

  if (access(proto_path.c_str(), F_OK))
  {
    return errno;
  }

  if (readTableFile(proto_path, table_proto))
    return EEXIST;

  return -1;
}

void Schema::doGetTableNames(CachedDirectory &directory, string&, set<string>& set_of_names)
{
  CachedDirectory::Entries entries= directory.getEntries();

  for (CachedDirectory::Entries::iterator entry_iter= entries.begin(); 
       entry_iter != entries.end(); ++entry_iter)
  {
    CachedDirectory::Entry *entry= *entry_iter;
    const string *filename= &entry->filename;

    assert(filename->size());

    const char *ext= strchr(filename->c_str(), '.');

    if (ext == NULL || my_strcasecmp(system_charset_info, ext, DEFAULT_FILE_EXTENSION) ||
        (filename->compare(0, strlen(TMP_FILE_PREFIX), TMP_FILE_PREFIX) == 0))
    { }
    else
    {
      char uname[NAME_LEN + 1];
      uint32_t file_name_len;

      file_name_len= filename_to_tablename(filename->c_str(), uname, sizeof(uname));
      // TODO: Remove need for memory copy here
      uname[file_name_len - sizeof(DEFAULT_FILE_EXTENSION) + 1]= '\0'; // Subtract ending, place NULL 
      set_of_names.insert(uname);
    }
  }
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
  char	 path[FN_REFLEN+16];
  uint32_t path_len;

  path_len= drizzled::build_table_filename(path, sizeof(path), schema_message.name().c_str(), "", false);
  path[path_len-1]= 0;                    // remove last '/' from path

  if (mkdir(path, 0777) == -1)
    return false;

  if (not writeSchemaFile(path, schema_message))
  {
    rmdir(path);

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
  char	 path[FN_REFLEN+16];
  uint32_t path_len;
  message::Schema schema_message;

  path_len= drizzled::build_table_filename(path, sizeof(path), schema_name.c_str(), "", false);
  path[path_len-1]= 0;                    // remove last '/' from path

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
    perror(schema_file.c_str());

  if (rmdir(path))
    perror(path);

  if (not pthread_rwlock_wrlock(&schema_lock))
  {
    schema_cache.erase(schema_message.name());
    pthread_rwlock_unlock(&schema_lock);
  }

  return true;
}

int Schema::doDropTable(Session&, TableIdentifier &, const string &table_path)
{
  string path(table_path);

  path.append(DEFAULT_FILE_EXTENSION);

  return internal::my_delete(path.c_str(), MYF(0));
}

bool Schema::doAlterSchema(const drizzled::message::Schema &schema_message)
{
  char	 path[FN_REFLEN+16];
  uint32_t path_len;
  path_len= drizzled::build_table_filename(path, sizeof(path), schema_message.name().c_str(), "", false);
  path[path_len-1]= 0;                    // remove last '/' from path

  if (access(path, F_OK))
    return false;

  if (writeSchemaFile(path, schema_message))
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

  snprintf(schema_file_tmp, FN_REFLEN, "%s%c%s.tmpXXXXXX", path, FN_LIBCHAR, MY_DB_OPT_FILE);

  schema_file.append(1, FN_LIBCHAR);
  schema_file.append(MY_DB_OPT_FILE);

  int fd= mkstemp(schema_file_tmp);

  if (fd == -1)
    return false;

  if (not db.SerializeToFileDescriptor(fd))
  {
    close(fd);
    unlink(schema_file_tmp);

    return false;
  }

  if (rename(schema_file_tmp, schema_file.c_str()) == -1)
  {
    close(fd);

    return false;
  }
  close(fd);

  return true;
}


bool Schema::readTableFile(const std::string &path, message::Table &table_message)
{
  fstream input(path.c_str(), ios::in | ios::binary);

  if (input.good())
  {
    if (table_message.ParseFromIstream(&input))
    {
      return true;
    }
  }
  else
  {
    perror(path.c_str());
  }

  return false;
}


bool Schema::readSchemaFile(const std::string &schema_name, drizzled::message::Schema &schema_message)
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
  else
  {
    perror(db_opt_path);
  }

  return false;
}

bool Schema::doCanCreateTable(const drizzled::TableIdentifier &identifier)
{
  if (not strcasecmp(identifier.getSchemaName(), "temporary_tables"))
  {
    return false;
  }

  return true;
}

bool Schema::doDoesTableExist(Session&, TableIdentifier &identifier)
{
  string proto_path(identifier.getPath());
  proto_path.append(DEFAULT_FILE_EXTENSION);

  if (access(proto_path.c_str(), F_OK))
  {
    return false;
  }

  return true;
}
