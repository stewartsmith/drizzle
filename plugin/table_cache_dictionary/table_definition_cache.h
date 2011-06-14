/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#pragma once

#include <drizzled/generator.h>

namespace table_cache_dictionary {

class TableDefinitionCache : public  drizzled::plugin::TableFunction
{
public:
  TableDefinitionCache();

  class Generator : public drizzled::plugin::TableFunction::Generator 
  {
    drizzled::generator::TableDefinitionCache table_definition_cache_generator;

  public:
    bool populate();

    Generator(drizzled::Field **arg);
    ~Generator();
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

} /* namespace table_cache_dictionary */

