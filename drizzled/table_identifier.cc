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

#include <drizzled/table_identifier.h>

using namespace std;
using namespace drizzled;

// Prototypes, these will be removed.
size_t build_tmptable_filename(char *buff, size_t bufflen);
size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp);

const char *TableIdentifier::getPath()
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
    case TEMP_TABLE:
      path_length= build_tmptable_filename(path, sizeof(path));
      break;
    case SYSTEM_TMP_TABLE:
      assert(0);
    }
    path_inited= true;
    assert(path_length); // TODO throw exception, this is a possibility
  }

  return path;
}
