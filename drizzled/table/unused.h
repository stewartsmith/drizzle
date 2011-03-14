/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *  Copyright (C) 2010 Sun Microsystems, Inc.
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

#pragma once

namespace drizzled {
namespace table {

class UnusedTables {
  Concurrent *tables;				/* Used by mysql_test */

  Concurrent *getTable() const
  {
    return tables;
  }

  Concurrent *setTable(Table *arg)
  {
    return tables= static_cast<Concurrent *>(arg);
  }

public:

  void cull();

  void cullByVersion();
  
  void link(Concurrent *table);

  void unlink(Concurrent *table);

/* move table first in unused links */

  void relink(Concurrent *table);

  void clear();

  UnusedTables():
    tables(NULL)
  { }

  ~UnusedTables()
  { 
  }
};

UnusedTables &getUnused(void);


} /* namepsace table */
} /* namepsace drizzled */

