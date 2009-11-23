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

#include <string.h>

size_t build_tmptable_filename(char *buff, size_t bufflen);
size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp);


namespace drizzled {

class TableIdentifier 
{
private:
  bool path_inited;
  tmp_table_type type;
  char path[FN_REFLEN];
  const char *db;
  const char *table_name;

public:
  TableIdentifier( const char *db_arg,
                   const char *table_name_arg,
                   tmp_table_type tmp_arg) :
    path_inited(false),
    type(tmp_arg),
    db(db_arg),
    table_name(table_name_arg)
  { 
  }

  bool isTmp()
  {
    return type == NO_TMP_TABLE ? false  : true;
  }

  const char *getPath()
  {
    if (! path_inited)
    {
      size_t path_length= 0;

      switch (type) {
      case NO_TMP_TABLE:
        path_length= build_table_filename(path, sizeof(path),
                                          db, table_name,
                                          false);
        break;
      case INTERNAL_TMP_TABLE:
        path_length= build_table_filename(path, sizeof(path),
                                          db, table_name,
                                          true);
        break;
      case NON_TRANSACTIONAL_TMP_TABLE:
      case TRANSACTIONAL_TMP_TABLE:
        path_length= build_tmptable_filename(path, sizeof(path));
        break;
      case SYSTEM_TMP_TABLE:
        assert(0);
      }
      assert(path_length); // TODO throw exception, this is a possibility
    }

    return path;
  }

  const char *getDBName()
  {
    return db;
  }

  const char *getTableName()
  {
    return table_name;
  }
};

} /* namespace drizzled */

#endif /* DRIZZLED_TABLE_IDENTIFIER_H */
