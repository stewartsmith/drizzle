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
#include <drizzled/session.h>
#include "query_cache_service.h"
#include <drizzled/table_list.h>

using namespace std;

namespace drizzled
{

QueryCacheService::CacheEntries QueryCacheService::cache;
QueryCacheService::CachedTablesEntries QueryCacheService::cachedTables;

message::Resultset *QueryCacheService::setCurrentResultsetMessage(Session *in_session)
{
  message::Resultset *resultset= in_session->getResultsetMessage();

  if (unlikely(resultset == NULL))
  {
    /* Allocate and initialize a new Resultset message 
     * for this Session object.  Session is responsible for
     * deleting Resultset message when done with it.
     */
    resultset= new (nothrow) message::Resultset();
    in_session->setResultsetMessage(resultset);

    /* for Atomic purpose, reserve an entry for the following query
     */
    cache[in_session->query_cache_key]= message::Resultset();
  }
  return resultset;
}

void QueryCacheService::setResultsetHeader(message::Resultset &resultset,
                                          Session *in_session,
                                          TableList *in_table)
{
  /* Set up the Select header */
  message::SelectHeader *header= resultset.mutable_select_header();
  
  (void) in_session;

  /* Extract all the tables mentioned in the query and 
   * add the metadata to the SelectHeader
   */

  for (TableList* tmp_table= in_table; tmp_table; tmp_table= tmp_table->next_global)
  {
    message::TableMeta *table_meta= header->add_table_meta();
    table_meta->set_schema_name(tmp_table->db, strlen(tmp_table->db));
    table_meta->set_table_name(tmp_table->table_name, strlen(tmp_table->table_name));
    /* Populate the cached tables hash */
    QueryCacheService::cachedTables[strcat(tmp_table->db, tmp_table->table_name)].push_back(in_session->query_cache_key);
  } 

  /* Extract the returned fields 
   * and add the field data to the SelectHeader
   */
  List<Item>::iterator it(in_session->lex->select_lex.item_list);
  Item *item;
  while ((item=it++))
  {
    SendField field;
    item->make_field(&field);
    
    message::FieldMeta *field_meta= header->add_field_meta();
    field_meta->set_field_name(field.col_name, strlen(field.col_name));    
    field_meta->set_schema_name(field.db_name, strlen(field.db_name));
    field_meta->set_table_name(field.table_name, strlen(field.table_name));
    field_meta->set_field_alias(field.org_col_name, strlen(field.org_col_name));
    field_meta->set_table_alias(field.org_table_name, strlen(field.org_table_name));
   }
}

bool QueryCacheService::addRecord(Session *in_session, List<Item> &list)
{
  message::Resultset *resultset= in_session->getResultsetMessage();
  
  if (resultset != NULL)
  {
    message::SelectData *data= resultset->mutable_select_data();
    data->set_segment_id(1);
    data->set_end_segment(true);
    message::SelectRecord *record= data->add_record();

    List<Item>::iterator li(list);
    
    char buff[MAX_FIELD_WIDTH];
    String buffer(buff, sizeof(buff), &my_charset_bin);
    
    Item *current_field;
    while ((current_field= li++))
    {
      if (current_field->is_null())
      {
        record->add_is_null(true);
        record->add_record_value("", 0);
      } 
      else 
      {
        String *string_value= current_field->val_str(&buffer);
        record->add_is_null(false);
        record->add_record_value(string_value->c_ptr(), string_value->length());
        string_value->free();
      }
    }
    return false;
  }
  return true;
}

bool QueryCacheService::isCached(string query)
{
  CacheEntries::iterator it= QueryCacheService::cache.find(query);
  if (it != QueryCacheService::cache.end())
     return true;
  return false;
}
} /* namespace drizzled */
