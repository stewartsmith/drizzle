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

#ifndef PLUGIN_TABLE_CACHE_DICTIONARY_TABLE_DEFINITION_CACHE_H
#define PLUGIN_TABLE_CACHE_DICTIONARY_TABLE_DEFINITION_CACHE_H

#include "drizzled/table_share.h"

namespace table_cache_dictionary {

class TableDefinitionCache : public  drizzled::plugin::TableFunction
{
public:
  TableDefinitionCache();

  class Generator : public drizzled::plugin::TableFunction::Generator 
  {
    bool is_primed;
    drizzled::TableSharePtr share;
    drizzled::TableDefinitionCache::const_iterator table_share_iterator;

    void fill();

    bool nextCore();
    bool next();

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

#endif /* PLUGIN_TABLE_CACHE_DICTIONARY_TABLE_DEFINITION_CACHE_H */
