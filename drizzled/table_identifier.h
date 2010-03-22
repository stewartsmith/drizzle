/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

/* 
  This is a "work in progress". The concept needs to be replicated throughout
  the code, but we will start with baby steps for the moment. To not incur
  cost until we are complete, for the moment it will do no allocation.

  This is mainly here so that it can be used in the SE interface for
  the time being.

  This will replace Table_ident.
  */

#ifndef DRIZZLED_TABLE_IDENTIFIER_H
#define DRIZZLED_TABLE_IDENTIFIER_H

#include <drizzled/enum.h>
#include "drizzled/definitions.h"
#include <string.h>

#include <assert.h>

#include <ostream>
#include <set>
#include <algorithm>
#include <functional>

namespace drizzled {

uint32_t filename_to_tablename(const char *from, char *to, uint32_t to_length);
size_t build_table_filename(std::string &buff, const char *db, const char *table_name, bool is_tmp);


class TableIdentifier
{
  tmp_table_type type;
  std::string path;
  std::string db;
  std::string table_name;
  std::string lower_db;
  std::string lower_table_name;
  std::string sql_path;

public:
  TableIdentifier( const std::string &db_arg,
                   const std::string &table_name_arg,
                   tmp_table_type tmp_arg= STANDARD_TABLE) :
    type(tmp_arg),
    db(db_arg),
    table_name(table_name_arg),
    lower_db(db_arg),
    lower_table_name(table_name_arg),
    sql_path(db_arg)
  { 
    std::transform(lower_table_name.begin(), lower_table_name.end(),
                   lower_table_name.begin(), ::tolower);

    std::transform(lower_db.begin(), lower_db.end(),
                   lower_db.begin(), ::tolower);

    sql_path.append(".");
    sql_path.append(table_name);
  }

  /**
    This is only used in scavenging lost tables. Once the temp schema engine goes in, this should go away.
  */
  TableIdentifier( const char *path_arg ) :
    type(TEMP_TABLE),
    path(path_arg),
    db(path_arg),
    table_name(path_arg),
    sql_path(db)
  { 
    sql_path.append(".");
    sql_path.append(table_name);
  }

  bool isTmp() const
  {
    return type == STANDARD_TABLE ? false  : true;
  }

  const std::string &getSQLPath()
  {
    return sql_path;
  }

  const std::string &getPath();

  const std::string &getDBName() const
  {
    return db;
  }

  const std::string &getSchemaName() const
  {
    return db;
  }

  const std::string getTableName() const
  {
    return table_name;
  }

  friend std::ostream& operator<<(std::ostream& output, const TableIdentifier &identifier)
  {
    const char *type_str;

    output << "TableIdentifier:(";
    output <<  identifier.getDBName();
    output << ", ";
    output << identifier.getTableName();
    output << ", ";

    switch (identifier.type) {
    case STANDARD_TABLE:
      type_str= "standard";
      break;
    case INTERNAL_TMP_TABLE:
      type_str= "internal";
      break;
    case TEMP_TABLE:
      type_str= "temporary";
      break;
    case SYSTEM_TMP_TABLE:
      type_str= "system";
    }

    output << type_str;
    output << ")";

    return output;  // for multiple << operators.
  }

  friend bool operator==(const TableIdentifier &left, const TableIdentifier &right)
  {
    if (left.type == right.type)
    {
      if (left.db == right.db)
      {
        if (left.table_name == right.table_name)
        {
          return true;
        }
      }
    }

    return false;
  }

};

typedef std::set <TableIdentifier> TableIdentifierList;

} /* namespace drizzled */

#endif /* DRIZZLED_TABLE_IDENTIFIER_H */
