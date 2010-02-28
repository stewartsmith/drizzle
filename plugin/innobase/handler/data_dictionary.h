/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

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
    void populateINNODB_TRX();
    void populateINNODB_LOCKS();
    void populateINNODB_LOCK_WAITS();

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
