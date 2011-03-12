/* 
 * Copyright (C) 2010 Djellel Eddine Difallah
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of Djellel Eddine Difallah nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PLUGIN_MEMCACHED_QUERY_CACHE_DATA_DICTIONARY_SCHEMA_H
#define PLUGIN_MEMCACHED_QUERY_CACHE_DATA_DICTIONARY_SCHEMA_H

#include <drizzled/plugin/table_function.h>
#include <drizzled/field.h>

namespace drizzled
{
class QueryCacheService;

class QueryCacheTool : public drizzled::plugin::TableFunction
{
public:

  QueryCacheTool();

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);
    bool populate();
  private:
    QueryCacheService::CacheEntries::iterator it;
    QueryCacheService::CacheEntries::iterator end;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class CachedTables : public drizzled::plugin::TableFunction
{
public:

  CachedTables();

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);
    bool populate();
  private:
    QueryCacheService::CachedTablesEntries::iterator it;
    QueryCacheService::CachedTablesEntries::iterator end;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class QueryCacheStatusTool : public drizzled::plugin::TableFunction
{
public:
  QueryCacheStatusTool() :
    plugin::TableFunction("DATA_DICTIONARY", "QUERY_CACHE_STATUS")
  {
    add_field("VARIABLE_NAME");
    add_field("VARIABLE_VALUE");
  }

  class Generator : public drizzled::plugin::TableFunction::Generator 
  {
    drizzled::drizzle_sys_var **status_var_ptr;

  public:
    Generator(drizzled::Field **fields);

    bool populate();
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};
}
#endif /* PLUGIN_MEMCACHED_QUERY_CACHE_DATA_DICTIONARY_SCHEMA_H */
