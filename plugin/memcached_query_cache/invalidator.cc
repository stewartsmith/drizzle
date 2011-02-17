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

#include <config.h>
#include "invalidator.h"
#include "query_cache_service.h"
#include "memcached_qc.h"

#include <drizzled/session.h>
#include <drizzled/message/transaction.pb.h>
#include <drizzled/message/table.pb.h>
#include <drizzled/message/statement_transform.h>

#include <vector>
#include <string>
#include <algorithm>


using namespace drizzled;
using namespace std;

Invalidator::Invalidator(string name_arg)
  : 
    plugin::TransactionApplier(name_arg)
{
}

plugin::ReplicationReturnCode
Invalidator::apply(Session &in_session, 
                   const message::Transaction &to_apply)
{
  (void) in_session;
  string schema_name;
  string table_name;

  size_t stmt_size= to_apply.statement_size();

  for (size_t i= 0; i < stmt_size; i++)
  {
    schema_name.clear();
    table_name.clear();
    const message::Statement &stmt= to_apply.statement(i);

    /*
     * We don't handle raw SQL for now.
     */
    if (stmt.type() != message::Statement::RAW_SQL)
    {
      parseStatementTableMetadata(stmt, schema_name, table_name);
    }
    else
    {
      continue; /* go on to the next statement */
    }

    /* Now lets invalidate all the entries of the table */
    invalidateByTableName(schema_name, table_name);
  }
 return plugin::SUCCESS;
}

void Invalidator::parseStatementTableMetadata(const message::Statement &in_statement,
                                                  string &in_schema_name,
                                                  string &in_table_name) const
{
  switch (in_statement.type())
  {
    case message::Statement::INSERT:
    {
      const message::TableMetadata &metadata= in_statement.insert_header().table_metadata();
      in_schema_name.assign(metadata.schema_name());
      in_table_name.assign(metadata.table_name());
      break;
    }
    case message::Statement::UPDATE:
    {
      const message::TableMetadata &metadata= in_statement.update_header().table_metadata();
      in_schema_name.assign(metadata.schema_name());
      in_table_name.assign(metadata.table_name());
      break;
    }
    case message::Statement::DELETE:
    {
      const message::TableMetadata &metadata= in_statement.delete_header().table_metadata();
      in_schema_name.assign(metadata.schema_name());
      in_table_name.assign(metadata.table_name());
      break;
    }
    case message::Statement::CREATE_SCHEMA:
    {
      in_schema_name.assign(in_statement.create_schema_statement().schema().name());
      in_table_name.clear();
      break;
    }
    case message::Statement::ALTER_SCHEMA:
    {
      in_schema_name.assign(in_statement.alter_schema_statement().after().name());
      in_table_name.clear();
      break;
    }
    case message::Statement::DROP_SCHEMA:
    {
      in_schema_name.assign(in_statement.drop_schema_statement().schema_name());
      in_table_name.clear();
      break;
    }
    case message::Statement::CREATE_TABLE:
    {
      in_schema_name.assign(in_statement.create_table_statement().table().schema());
      in_table_name.assign(in_statement.create_table_statement().table().name());
      break;
    }
    case message::Statement::ALTER_TABLE:
    {
      in_schema_name.assign(in_statement.alter_table_statement().before().schema());
      in_table_name.assign(in_statement.alter_table_statement().before().name());
      break;
    }
    case message::Statement::DROP_TABLE:
    {
      const message::TableMetadata &metadata= in_statement.drop_table_statement().table_metadata();
      in_schema_name.assign(metadata.schema_name());
      in_table_name.assign(metadata.table_name());
      break;
    }
    default:
    {
      /* All other types have no schema and table information */
      in_schema_name.clear();
      in_table_name.clear();
      break;
    }
  }  
}
void Invalidator::invalidateByTableName(const std::string &in_schema_name,
                                        const std::string &in_table_name) const
{
  /* Reconstitute the schema+table key */
  string key= in_schema_name+in_table_name;
 
  /* Lookup for the invalidated table in the cached tables map */
  QueryCacheService::CachedTablesEntries::iterator itt= QueryCacheService::cachedTables.find(key);
  if (itt != QueryCacheService::cachedTables.end())
  {
    /* Extract the invloved hashes from the map and lookup the local Cache*/
    QueryCacheService::CachedTablesEntry &entry= *itt;
    vector<string>::iterator hash;
    for(hash= entry.second.begin(); hash != entry.second.end(); hash++)
    {
      QueryCacheService::CacheEntries::iterator it= QueryCacheService::cache.find(*hash);
      if (it != QueryCacheService::cache.end())
      {
        /* Remove the Query from the local Cache */
        QueryCacheService::cache.erase(*hash);
        /* Remove the Query from Memcached immediatly */
        MemcachedQueryCache::getClient()->remove(*hash, 0);
      }
    }
    /* finaly remove the table from the cached table list */
    QueryCacheService::cachedTables.erase(key);
  } 
}
