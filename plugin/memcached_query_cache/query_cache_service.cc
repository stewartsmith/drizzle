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

using namespace drizzled;
using namespace std;

map<string, message::Resultset> QueryCacheService::cache;

message::Resultset &QueryCacheService::getCurrentResultsetMessage(Session *in_session,
                                            TableList *in_table)
{
  message::Resultset *resultset= in_session->getResultsetMessage();

  if (unlikely(resultset == NULL))
  {
    /* Allocate and initialize a new Resultset message 
     * for this Session object.  Session is responsible for
     * deleting Resultset message when done with it.
     */
    resultset= new (nothrow) message::Resultset();
    setResultsetHeader(*resultset, in_session, in_table);
    in_session->setResultsetMessage(resultset);

    /* add the Resultset (including the header) to the hash 
     * This is done before the final set (this can be a problem)
     * ToDo: fix that
     */
    cout << cache.size() << endl;
    cache[in_session->query_cache_key]= *resultset;
  }
  return *resultset;
}
void QueryCacheService::setResultsetHeader(message::Resultset &resultset,
                                          Session *in_session,
                                          TableList *in_table)
{
  /* Set up the Select header */
  message::SelectHeader *header= resultset.mutable_select_header();
  message::TableMetadata *table_metadata= header->add_table_metadata();

  /* I could extract only the first table in the From clause
   * ToDo: generate the list of all tables
   */
  string table_name(in_table->table_name);
  string schema_name(in_table->db);

  table_metadata->set_schema_name(schema_name.c_str(), schema_name.length());
  table_metadata->set_table_name(table_name.c_str(), table_name.length());

  /* try to extract the fields */
  List_iterator_fast<Item> li(in_session->lex->select_lex.item_list);
  String *string_value= new (in_session->mem_root) String(QueryCacheService::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);
  Item *item;
  while ((item=li++))
  {
    cout << item << endl;
    string_value->free();
  }
}

bool QueryCacheService::addRecord(Session *in_session, List<Item> &list)
{
  message::Resultset &resultset= getCurrentResultsetMessage(in_session, in_session->lex->query_tables);

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
    record->add_record_value(string_value->c_ptr(), string_value->length());
    string_value->free();
  }
  return false;
}

bool QueryCacheService::isCached(string query)
{
  map<string, message::Resultset>::iterator it= QueryCacheService::cache.find(query);
  if (it != QueryCacheService::cache.end())
     return true;
  return false;
}
