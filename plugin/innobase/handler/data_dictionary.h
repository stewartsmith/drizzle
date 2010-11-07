/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Joseph Daly <skinny.moey@gmail.com> 
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

#ifndef PLUGIN_INNOBASE_HANDLER_DATA_DICTIONARY_H
#define PLUGIN_INNOBASE_HANDLER_DATA_DICTIONARY_H 

#include "drizzled/plugin/table_function.h"
#include "drizzled/field.h"

class CmpTool : public drizzled::plugin::TableFunction
{
public:

  CmpTool(bool reset);

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg, bool outer_reset);
                        
    bool populate();
  private:
    uint32_t record_number;
    bool inner_reset;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, outer_reset);
  }
private:
  bool outer_reset; 
};

class CmpmemTool : public drizzled::plugin::TableFunction
{
public:

  CmpmemTool(bool reset);

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg, bool outer_reset);

    ~Generator();

    bool populate();
  private:
    uint32_t record_number;
    bool inner_reset;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, outer_reset);
  }
private:
  bool outer_reset;
};

class InnodbTrxTool : public drizzled::plugin::TableFunction
{
public:

  InnodbTrxTool(const char* in_table_name);

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg, const char* in_table_name);

    ~Generator();

    bool populate();
  private:
    void populate_innodb_trx();
    void populate_innodb_locks();
    void populate_innodb_lock_waits();

  private:
    uint32_t record_number;
    uint32_t number_rows;
    const char* table_name;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, table_name);
  }
private:
  const char* table_name;
};

#endif /* PLUGIN_INNOBASE_HANDLER_DATA_DICTIONARY_H */
