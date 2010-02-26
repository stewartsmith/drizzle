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

/**************************************************//**
@file handler/i_s.h
InnoDB INFORMATION SCHEMA tables interface to MySQL.

Created July 18, 2007 Vasil Dimov
*******************************************************/

#ifndef PLUGIN_INNOBASE_HANDLER_DATA_DICTIONARY_H
#define PLUGIN_INNOBASE_HANDLER_DATA_DICTIONARY_H 

#include "drizzled/plugin/table_function.h"
#include "drizzled/field.h"

class CmpTool : public drizzled::plugin::TableFunction
{
public:

  CmpTool();

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);
                        
    bool populate();
  private:
    uint32_t record_number;    
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }                     
};

#endif /* PLUGIN_INNOBASE_HANDLER_DATA_DICTIONARY_H */
