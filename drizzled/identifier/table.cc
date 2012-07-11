/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <cassert>
#include <boost/lexical_cast.hpp>
#include <drizzled/identifier.h>
#include <drizzled/internal/my_sys.h>

#include <drizzled/error.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>

#include <drizzled/table.h>

#include <drizzled/util/string.h>
#include <drizzled/util/tablename_to_filename.h>
#include <drizzled/catalog/local.h>

#include <algorithm>
#include <sstream>
#include <cstdio>

#include <boost/thread.hpp>

using namespace std;

namespace drizzled {

extern std::string drizzle_tmpdir;
extern pid_t current_pid;

namespace identifier {

static const char hexchars[]= "0123456789abcdef";

/*
  Translate a cursor name to a table name (WL #1324).

  SYNOPSIS
    filename_to_tablename()
      from                      The cursor name
      to                OUT     The table name
      to_length                 The size of the table name buffer.

  RETURN
    Table name length.
*/
uint32_t Table::filename_to_tablename(const char *from, char *to, uint32_t to_length)
{
  uint32_t length= 0;

  if (!memcmp(from, TMP_FILE_PREFIX, TMP_FILE_PREFIX_LENGTH))
  {
    /* Temporary table name. */
    length= strlen(strncpy(to, from, to_length));
  }
  else
  {
    for (; *from  && length < to_length; length++, from++)
    {
      if (*from != '@')
      {
        to[length]= *from;
        continue;
      }
      /* We've found an escaped char - skip the @ */
      from++;
      to[length]= 0;
      /* There will be a two-position hex-char version of the char */
      for (int x=1; x >= 0; x--)
      {
        if (*from >= '0' && *from <= '9')
        {
          to[length] += ((*from++ - '0') << (4 * x));
        }
        else if (*from >= 'a' && *from <= 'f')
        {
          to[length] += ((*from++ - 'a' + 10) << (4 * x));
        }
      }
      /* Backup because we advanced extra in the inner loop */
      from--;
    } 
  }

  return length;
}

/*
  Creates path to a cursor: drizzle_tmpdir/#sql1234_12_1.ext

  SYNOPSIS
   build_tmptable_filename()
     session                    The thread handle.
     buff                       Where to write result
     bufflen                    buff size

  NOTES

    Uses current_pid, thread_id, and tmp_table counter to create
    a cursor name in drizzle_tmpdir.

  RETURN
    path length on success, 0 on failure
*/

#ifdef _GLIBCXX_HAVE_TLS 
__thread uint32_t counter= 0;

static uint32_t get_counter()
{
  return ++counter;
}

#else
boost::mutex counter_mutex;
static uint32_t counter= 1;

static uint32_t get_counter()
{
  boost::mutex::scoped_lock lock(counter_mutex);
  return ++counter;
}

#endif

std::string Table::build_tmptable_filename()
{
  ostringstream os;
  os << "/" << TMP_FILE_PREFIX << current_pid << pthread_self() << "-" << get_counter();
  return drizzle_tmpdir + boost::to_lower_copy(os.str());
}

/*
  Creates path to a cursor: drizzle_data_dir/db/table.ext

  SYNOPSIS
   build_table_filename()
     buff                       Where to write result
                                This may be the same as table_name.
     bufflen                    buff size
     db                         Database name
     table_name                 Table name
     ext                        File extension.
     flags                      table_name is temporary, do not change.

  NOTES

    Uses database and table name, and extension to create
    a cursor name in drizzle_data_dir. Database and table
    names are converted from system_charset_info into "fscs".
    Unless flags indicate a temporary table name.
    'db' is always converted.
    'ext' is not converted.

    The conversion suppression is required for ALTER Table. This
    statement creates intermediate tables. These are regular
    (non-temporary) tables with a temporary name. Their path names must
    be derivable from the table name. So we cannot use
    build_tmptable_filename() for them.

  RETURN
    path length on success, 0 on failure
*/

std::string Table::build_table_filename(const identifier::Table &table_identifier, bool is_tmp)
{
  string in_path= table_identifier.getCatalog().getPath();
  in_path+= FN_LIBCHAR + util::tablename_to_filename(table_identifier.getSchemaName()) + FN_LIBCHAR;
  return in_path + (is_tmp ? table_identifier.getTableName() : util::tablename_to_filename(table_identifier.getTableName()));
}

Table::Table(const drizzled::Table &table) :
  identifier::Schema(table.getShare()->getTableIdentifier().getCatalog(),
                     str_ref(table.getShare()->getSchemaName())),
  type(table.getShare()->getTableType()),
  table_name(table.getShare()->getTableName())
{
  if (type == message::Table::TEMPORARY)
  {
    path= table.getShare()->getPath();
  }

  init();
}

Table::Table(const identifier::Schema &schema,
             const std::string &table_name_arg,
             Type tmp_arg) :
  Schema(schema),
  type(tmp_arg),
  table_name(table_name_arg)
{ 
  init();
}

Table::Table(const drizzled::identifier::Catalog &catalog,
             const std::string &db_arg,
             const std::string &table_name_arg,
             Type tmp_arg) :
  Schema(catalog, db_arg),
  type(tmp_arg),
  table_name(table_name_arg)
{ 
  init();
}

Table::Table(const drizzled::identifier::Catalog &catalog,
             const std::string &schema_name_arg,
             const std::string &table_name_arg,
             const std::string &path_arg ) :
  Schema(catalog, schema_name_arg),
  type(message::Table::TEMPORARY),
  path(path_arg),
  table_name(table_name_arg)
{ 
  init();
}

void Table::init()
{
  switch (type) 
  {
  case message::Table::FUNCTION:
  case message::Table::STANDARD:
    assert(path.empty());
    path= build_table_filename(*this, false);
    break;

  case message::Table::INTERNAL:
    assert(path.empty());
    path= build_table_filename(*this, true);
    break;

  case message::Table::TEMPORARY:
    if (path.empty())
    {
      path= build_tmptable_filename();
    }
    break;
  }

  if (type == message::Table::TEMPORARY)
  {
    size_t pos= path.find("tmp/#sql");
    if (pos != std::string::npos) 
    {
      key_path= path.substr(pos);
    }
  }

  hash_value= util::insensitive_hash()(path);
  key.set(getKeySize(), getCatalogName(), getCompareWithSchemaName(), boost::to_lower_copy(std::string(getTableName())));
}


const std::string &Table::getPath() const
{
  return path;
}

const std::string &Table::getKeyPath() const
{
  return key_path.empty() ? path : key_path;
}

std::string Table::getSQLPath() const  // @todo this is just used for errors, we should find a way to optimize it
{
  switch (type) 
	{
  case message::Table::FUNCTION:
  case message::Table::STANDARD:
		return getSchemaName() + "." + table_name;

  case message::Table::INTERNAL:
		return "temporary." + table_name;

  case message::Table::TEMPORARY:
    return getSchemaName() + ".#" + table_name;
  }
	assert(false);
	return "<no table>";
}

bool Table::isValid() const
{
  if (identifier::Schema::isValid() == false)
  {
    return false;
  }

  bool error= false;
  if (table_name.empty()
		|| table_name.size() > NAME_LEN
		|| table_name[table_name.length() - 1] == ' '
		|| table_name[0] == '.')
  {
    error= true;
  }
	else
  {
    const charset_info_st& cs= my_charset_utf8mb4_general_ci;
    int well_formed_error;
    uint32_t res= cs.cset->well_formed_len(cs, table_name, NAME_CHAR_LEN, &well_formed_error);
    if (well_formed_error or table_name.length() != res)
    {
      error= true;
    }
  }

  if (error == false)
  {
		return true;
  }
  my_error(ER_WRONG_TABLE_NAME, MYF(0), getSQLPath().c_str());

  return false;
}

void Table::copyToTableMessage(message::Table &message) const
{
  message.set_name(table_name);
  message.set_schema(getSchemaName());
}

void Table::Key::set(size_t resize_arg, const std::string &catalog_arg, const std::string &schema_arg, const std::string &table_arg)
{
  key_buffer.resize(resize_arg);

  schema_offset= catalog_arg.length() +1;
  table_offset= schema_offset +schema_arg.length() +1;

  std::copy(catalog_arg.begin(), catalog_arg.end(), key_buffer.begin());
  std::copy(schema_arg.begin(), schema_arg.end(), key_buffer.begin() +schema_offset);
  std::copy(table_arg.begin(), table_arg.end(), key_buffer.begin() +table_offset);

  util::sensitive_hash hasher;
  hash_value= hasher(key_buffer);
}

std::size_t hash_value(Table const& b)
{
  return b.getHashValue();
}

std::size_t hash_value(Table::Key const& b)
{
  return b.getHashValue();
}

std::ostream& operator<<(std::ostream& output, const Table::Key& arg)
{
  return output << "Key:(" <<  arg.schema_name() << ", " << arg.table_name() << ", " << arg.hash() << std::endl;
}

std::ostream& operator<<(std::ostream& output, const Table& identifier)
{
  return output << "Table:(" <<  identifier.getSchemaName() << ", " << identifier.getTableName() << ", " << message::type(identifier.getType()) << ", " << identifier.getPath() << ", " << identifier.getHashValue() << ")";
}

} /* namespace identifier */
} /* namespace drizzled */
