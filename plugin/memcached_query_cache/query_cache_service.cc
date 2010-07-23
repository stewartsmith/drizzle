/* 
 * Copyright (c) 2010, Djellel Eddine Difallah
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
 *   * Neither the name of Padraig O'Sullivan nor the names of its contributors
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

#include "config.h"
#include "drizzled/session.h"
#include "query_cache_service.h"
#include "drizzled/table_list.h"

using namespace std;

namespace drizzled
{

std::map<std::string, drizzled::message::Resultset> QueryCacheService::cache;

message::Resultset &QueryCacheService::getCurrentResultsetMessage(Session *in_session)
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
  return *resultset;
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
  } 

  /* Extract the returned fields 
   * and add the fieldeta to the SelectHeader
   */
  List_iterator_fast<Item> it(in_session->lex->select_lex.item_list);
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
  message::Resultset &resultset= *in_session->getResultsetMessage();
  
  if (&resultset != NULL)
  {
    message::SelectData *data= resultset.mutable_select_data();
    data->set_segment_id(1);
    data->set_end_segment(true);
    message::SelectRecord *record= data->add_record();

    List_iterator_fast<Item> li(list);

    String *string_value= new (in_session->mem_root) String(QueryCacheService::DEFAULT_RECORD_SIZE);
    string_value->set_charset(system_charset_info);
    Item *item;
    while ((item=li++))
    {
      string_value= item->val_str(string_value);
      if (string_value)
      {
        string s= String_to_std_string(*string_value); 
        record->add_record_value(s.c_str(), s.length());
        string_value->free();
      }
    }
    return false;
  }
  return true;
}

bool QueryCacheService::isCached(string query)
{
  Entries::iterator it= QueryCacheService::cache.find(query);
  if (it != QueryCacheService::cache.end())
     return true;
  return false;
}
} /* namespace drizzled */
