/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C)
 *  
 *  Authors:
 *  Jay Pipes <joinfu@sun.com> 
 *  Joseph Daly <skinny.moey@gmail.com>
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

#include <drizzled/plugin/table_function.h>
#include <drizzled/field.h>

#include "transaction_log.h"

class TransactionLogTool : public drizzled::plugin::TableFunction
{
public:

  TransactionLogTool();

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    bool is_done;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class TransactionLogEntriesTool : public drizzled::plugin::TableFunction
{
public:

  TransactionLogEntriesTool();

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    TransactionLog::Entries::iterator it;
    TransactionLog::Entries::iterator end;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class TransactionLogTransactionsTool : public drizzled::plugin::TableFunction
{
public:

  TransactionLogTransactionsTool();

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    TransactionLog::TransactionEntries::iterator it;
    TransactionLog::TransactionEntries::iterator end;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

