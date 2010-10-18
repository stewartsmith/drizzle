/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Vijay Samuel
 *  Copyright (C) 2008 MySQL
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

#ifndef CLIENT_THREAD_CONTEXT_H
#define CLIENT_THREAD_CONTEXT_H

#include "client_priv.h"
#include "statement.h"
#include <string>
#include <iostream>

class ThreadContext 
{
public:
  ThreadContext(Statement *in_stmt,
                uint64_t in_limit) :
    stmt(in_stmt),
    limit(in_limit)
  { }

  ThreadContext() :
    stmt(),
    limit(0)
  { }

  Statement *getStmt() const
  {
    return stmt;
  }

  uint64_t getLimit() const
  {
    return limit;
  }

  void setStmt(Statement *in_stmt)
  {
    stmt= in_stmt;
  }

  void setLimit(uint64_t in_limit)
  {
    limit= in_limit;
  }  

private:
  Statement *stmt;
  uint64_t limit;
};

#endif /* CLIENT_THREAD_CONTEXT_H */
