/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#ifndef DRIZZLED_TABLEOP_HOOKS_H
#define DRIZZLED_TABLEOP_HOOKS_H

#include CSTDINT_H

class Table;

/*
  Class for maintaining hooks used inside operations on tables such
  as: create table functions, delete table functions, and alter table
  functions.

  Class is using the Template Method pattern to separate the public
  usage interface from the private inheritance interface.  This
  imposes no overhead, since the public non-virtual function is small
  enough to be inlined.

  The hooks are usually used for functions that does several things,
  e.g., create_table_from_items(), which both create a table and lock
  it. */

class Tableop_hooks
{
public:
  Tableop_hooks() {}
  virtual ~Tableop_hooks() {}

  inline void prelock(Table **tables, uint32_t count)
  {
    do_prelock(tables, count);
  }

  inline int postlock(Table **tables, uint32_t count)
  {
    return do_postlock(tables, count);
  }
private:

  /* Function primitive that is called prior to locking tables */
  virtual void do_prelock(Table **tables, uint32_t count);

  /**
     Primitive called after tables are locked.

     If an error is returned, the tables will be unlocked and error
     handling start.

     @return Error code or zero.
   */
  virtual int do_postlock(Table **tables, uint32_t count);

};

#endif /* DRIZZLED_TABLEOP_HOOKS_H */
