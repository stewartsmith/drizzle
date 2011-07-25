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

#include <config.h>

#include <drizzled/error.h>
#include <plugin/schema_engine/schema.h>
#include <drizzled/schema.h>
#include <drizzled/sql_table.h>
#include <drizzled/charset.h>
#include <drizzled/cursor.h>
#include <drizzled/data_home.h>

#include <drizzled/pthread_globals.h>

#include <drizzled/execute.h>

#include <drizzled/internal/my_sys.h>
#include <drizzled/cached_directory.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <boost/foreach.hpp>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <iostream>
#include <fstream>
#include <string>

using namespace std;
using namespace drizzled;

const char* MY_DB_OPT_FILE= "db.opt";
const char* DEFAULT_FILE_EXTENSION= ".dfe"; // Deep Fried Elephant

static const char* g_schema_exts[] = 
{
  NULL
};

Schema::Schema() :
  drizzled::plugin::StorageEngine("schema",
                                  HTON_ALTER_NOT_SUPPORTED |
                                  HTON_HAS_SCHEMA_DICTIONARY |
                                  HTON_SKIP_STORE_LOCK |
                                  HTON_TEMPORARY_NOT_SUPPORTED),
  schema_cache_filled(false)
{
  table_definition_ext= DEFAULT_FILE_EXTENSION;
}

void Schema::prime()
{
  CachedDirectory directory(getDataHomeCatalog().file_string(), CachedDirectory::DIRECTORY);
  CachedDirectory::Entries files= directory.getEntries();
  boost::unique_lock<boost::shared_mutex> scopedLock(mutex);

  BOOST_FOREACH(CachedDirectory::Entries::reference entry, files)
  {
    if (not entry->filename.compare(GLOBAL_TEMPORARY_EXT))
      continue;
    message::Schema schema_message;
    if (readSchemaFile(entry->filename, schema_message))
    {
      identifier::Schema schema_identifier(schema_message.name());

      pair<SchemaCache::iterator, bool> ret=
        schema_cache.insert(make_pair(schema_identifier.getPath(), new message::Schema(schema_message)));

      assert(ret.second); // If this has happened, something really bad is going down.
    }
  }
}

void Schema::doGetSchemaIdentifiers(identifier::schema::vector &set_of_names)
{
  mutex.lock_shared();
  BOOST_FOREACH(SchemaCache::reference iter, schema_cache)
    set_of_names.push_back(iter.second->name());
  mutex.unlock_shared();
}

drizzled::message::schema::shared_ptr Schema::doGetSchemaDefinition(const identifier::Schema &schema_identifier)
{
  mutex.lock_shared();
  SchemaCache::iterator iter= schema_cache.find(schema_identifier.getPath());
  if (iter != schema_cache.end())
  {
    drizzled::message::schema::shared_ptr schema_message= iter->second;
    mutex.unlock_shared();
    return schema_message;
  }
  mutex.unlock_shared();
  return drizzled::message::schema::shared_ptr();
}


bool Schema::doCreateSchema(const drizzled::message::Schema &schema_message)
{
  identifier::Schema schema_identifier(schema_message.name());

  if (mkdir(schema_identifier.getPath().c_str(), 0777) == -1)
  {
    sql_perror(schema_identifier.getPath().c_str());
    return false;
  }

  if (not writeSchemaFile(schema_identifier, schema_message))
  {
    rmdir(schema_identifier.getPath().c_str());

    return false;
  }

  boost::unique_lock<boost::shared_mutex> scopedLock(mutex);
  pair<SchemaCache::iterator, bool> ret= 
    schema_cache.insert(make_pair(schema_identifier.getPath(), new message::Schema(schema_message)));

  assert(ret.second); // If this has happened, something really bad is going down.
  return true;
}

bool Schema::doDropSchema(const identifier::Schema &schema_identifier)
{
  string schema_file(schema_identifier.getPath());
  schema_file.append(1, FN_LIBCHAR);
  schema_file.append(MY_DB_OPT_FILE);

  if (not doGetSchemaDefinition(schema_identifier))
    return false;

  // No db.opt file, no love from us.
  if (access(schema_file.c_str(), F_OK))
  {
    sql_perror(schema_file.c_str());
    return false;
  }

  if (unlink(schema_file.c_str()))
  {
    sql_perror(schema_file.c_str());
    return false;
  }

  if (rmdir(schema_identifier.getPath().c_str()))
  {
    sql_perror(schema_identifier.getPath().c_str());
    //@todo If this happens, we want a report of it. For the moment I dump
    //to stderr so I can catch it in Hudson.
    CachedDirectory dir(schema_identifier.getPath());
    cerr << dir;
  }

  boost::unique_lock<boost::shared_mutex> scopedLock(mutex);
  schema_cache.erase(schema_identifier.getPath());

  return true;
}

bool Schema::doAlterSchema(const drizzled::message::Schema &schema_message)
{
  identifier::Schema schema_identifier(schema_message.name());

  if (access(schema_identifier.getPath().c_str(), F_OK))
    return false;

  if (writeSchemaFile(schema_identifier, schema_message))
  {
    boost::unique_lock<boost::shared_mutex> scopedLock(mutex);
    schema_cache.erase(schema_identifier.getPath());

    pair<SchemaCache::iterator, bool> ret=
      schema_cache.insert(make_pair(schema_identifier.getPath(), new message::Schema(schema_message)));

    assert(ret.second); // If this has happened, something really bad is going down.
  }

  return true;
}

/**
  path is path to database, not schema file 

  @note we do the rename to make it crash safe.
*/
bool Schema::writeSchemaFile(const identifier::Schema &schema_identifier, const message::Schema &db)
{
  char schema_file_tmp[FN_REFLEN];
  string schema_file(schema_identifier.getPath());


  schema_file.append(1, FN_LIBCHAR);
  schema_file.append(MY_DB_OPT_FILE);

  snprintf(schema_file_tmp, FN_REFLEN, "%sXXXXXX", schema_file.c_str());

  int fd= mkstemp(schema_file_tmp);

  if (fd == -1)
  {
    sql_perror(schema_file_tmp);

    return false;
  }

  bool success;

  try {
    success= db.SerializeToFileDescriptor(fd);
  }
  catch (...)
  {
    success= false;
  }

  if (not success)
  {
    my_error(ER_CORRUPT_SCHEMA_DEFINITION, MYF(0), schema_file.c_str(),
             db.InitializationErrorString().empty() ? "unknown" :  db.InitializationErrorString().c_str());

    if (close(fd) == -1)
      sql_perror(schema_file_tmp);

    if (unlink(schema_file_tmp))
      sql_perror(schema_file_tmp);

    return false;
  }

  if (close(fd) == -1)
  {
    sql_perror(schema_file_tmp);

    if (unlink(schema_file_tmp))
      sql_perror(schema_file_tmp);

    return false;
  }

  if (rename(schema_file_tmp, schema_file.c_str()) == -1)
  {
    if (unlink(schema_file_tmp))
      sql_perror(schema_file_tmp);

    return false;
  }

  return true;
}


bool Schema::readSchemaFile(const drizzled::identifier::Schema &schema_identifier, drizzled::message::Schema &schema)
{
  return readSchemaFile(schema_identifier.getPath(), schema); 
}

bool Schema::readSchemaFile(std::string db_opt_path, drizzled::message::Schema &schema)
{
  /*
    Pass an empty file name, and the database options file name as extension
    to avoid table name to file name encoding.
  */
  db_opt_path.append(1, FN_LIBCHAR);
  db_opt_path.append(MY_DB_OPT_FILE);

  fstream input(db_opt_path.c_str(), ios::in | ios::binary);

  /**
    @note If parsing fails, either someone has done a "mkdir" or has deleted their opt file.
    So what do we do? We muddle through the adventure by generating 
    one with a name in it, and the charset set to the default.
  */
  if (input.good())
  {
    if (schema.ParseFromIstream(&input))
    {
      return true;
    }

    my_error(ER_CORRUPT_SCHEMA_DEFINITION, MYF(0), db_opt_path.c_str(),
             schema.InitializationErrorString().empty() ? "unknown" :  schema.InitializationErrorString().c_str());
  }
  else
  {
    sql_perror(db_opt_path.c_str());
  }

  return false;
}

void Schema::doGetTableIdentifiers(drizzled::CachedDirectory&,
                                   const drizzled::identifier::Schema&,
                                   drizzled::identifier::table::vector&)
{
}

const char** Schema::bas_ext() const
{
  return g_schema_exts;
}
