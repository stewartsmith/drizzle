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


/**
 * @file
 *
 * Implements the DATA_DICTIONARY views which allows querying the
 * state of the Query Cache
 *
 * CREATE TABLE DATA_DICTIONARY.QUERY_CACHE_ENTRIES (
 *   KEY VARCHAR NOT NULL
 * , SCHEMA VARCHAR NOT NULL
 * , SQL VARCHAR NOT NULL
 * );
 */

#include <config.h>

#include "query_cache_service.h"
#include "data_dictionary_schema.h"

#include <fcntl.h>
#include <sys/stat.h>

using namespace std;
using namespace drizzled;

/*
 *
 * Query_Cache_Meta_ENTRIES view
 *
 */

QueryCacheTool::QueryCacheTool() :
  plugin::TableFunction("DATA_DICTIONARY", "QUERY_CACHE_ENTRIES")
{
  add_field("key");
  add_field("schema");
  add_field("sql");
}

QueryCacheTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  it= QueryCacheService::cache.begin();
  end= QueryCacheService::cache.end(); 
}

bool QueryCacheTool::Generator::populate()
{
  if (it == end)
  { 
    return false;
  } 

  QueryCacheService::CacheEntry &entry= *it;

  push(entry.first);
  push(entry.second.schema());
  push(entry.second.sql());

  it++;

  return true;
}

/*
 *
 * Query_Cache_Cached_Tables view
 *
 */

CachedTables::CachedTables() :
  plugin::TableFunction("DATA_DICTIONARY", "QUERY_CACHED_TABLES")
{
  add_field("Table");
  add_field("Cache_Keys");
}

CachedTables::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  it= QueryCacheService::cachedTables.begin();
  end= QueryCacheService::cachedTables.end(); 
}

bool CachedTables::Generator::populate()
{
  if (it == end)
  { 
    return false;
  } 

  QueryCacheService::CachedTablesEntry &entry= *it;

  push(entry.first);
  string list_keys;
  vector<string>::iterator tmp;
  for(tmp= entry.second.begin(); tmp != entry.second.end(); tmp++)
  {
    list_keys+= "::"+ *tmp;
  }
  push(list_keys);
  it++;
  return true;
}
