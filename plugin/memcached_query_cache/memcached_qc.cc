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



#include "config.h"

#include "drizzled/plugin.h"
#include "drizzled/session.h"
#include "drizzled/select_send.h"
#include "drizzled/item/null.h"

#include <gcrypt.h>
#include <string>
#include <iostream>
#include <vector>

#include "memcached_qc.h"
#include "query_cache_udf_tools.h"
#include "data_dictionary_schema.h"
#include "invalidator.h"
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>

using namespace drizzled;
using namespace std;
namespace po= boost::program_options;

static char* sysvar_memcached_servers= NULL;
static ulong expiry_time;

memcache::Memcache* MemcachedQueryCache::client;
std::string MemcachedQueryCache::memcached_servers;

static DRIZZLE_SessionVAR_BOOL(enable, 
                               PLUGIN_VAR_SessionLOCAL,
                               "Enable Memcached Query Cache",
                               /* check_func */ NULL, 
                               /* update_func */ NULL,
                               /* default */ false);

static int check_memc_servers(Session *,
                              drizzle_sys_var *,
                              void *save,
                              drizzle_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  int len= sizeof(buff);
  const char *input= value->val_str(value, buff, &len);

  if (input)
  {
    MemcachedQueryCache::setServers(input);
    *(bool *) save= (bool) true;
    return 0;
  }

  *(bool *) save= (bool) false;
  return 1;
}

static void set_memc_servers(Session *,
                             drizzle_sys_var *,
                             void *var_ptr,
                             const void *save)
{
  if (*(bool *) save != false)
  {
    *(const char **) var_ptr= MemcachedQueryCache::getServers();
  }
}

static DRIZZLE_SYSVAR_STR(servers,
                          sysvar_memcached_servers,
                          PLUGIN_VAR_OPCMDARG,
                          N_("List of memcached servers."),
                          check_memc_servers, /* check func */
                          set_memc_servers, /* update func */
                          "127.0.0.1:11211"); /* default value */

static DRIZZLE_SYSVAR_ULONG(expiry, 
                            expiry_time,
                            PLUGIN_VAR_OPCMDARG,
                            "Expiry time of memcached entries",
                             NULL, NULL, 1000, 0, ~0L, 0);

bool MemcachedQueryCache::isSelect(string query)
{
  uint i= 0;
  /*
   Skip '(' characters in queries like following:
   (select a from t1) union (select a from t1);
  */
  const char* sql= query.c_str();
  while (sql[i] == '(')
    i++;
  /*
   Test if the query is a SELECT
   (pre-space is removed in dispatch_command).
    First '/' looks like comment before command it is not
    frequently appeared in real life, consequently we can
    check all such queries, too.
  */
  if ((my_toupper(system_charset_info, sql[i])     != 'S' ||
       my_toupper(system_charset_info, sql[i + 1]) != 'E' ||
       my_toupper(system_charset_info, sql[i + 2]) != 'L') &&
      sql[i] != '/')
  {
    return false;
  }
  return true;
}

bool MemcachedQueryCache::doIsCached(Session *session)
{
  if (SessionVAR(session, enable) && isSelect(session->query))
  {
    /* ToDo: Check against the cache content */
    string query= session->query+session->db;
    char* key= md5_key(query.c_str());
    if(queryCacheService.isCached(key))
    {
     session->query_cache_key.assign(key);
     free(key);
     return true;
    }
    free(key);
  }
  return false;
}

bool MemcachedQueryCache::doSendCachedResultset(Session *session)
{
  /** TODO: pay attention to the case where the cache value is empty
  * ie: there is a session in the process of caching the query
  * and didn't finish the work
  */ 
  
  LEX *lex= session->lex;
  register Select_Lex *select_lex= &lex->select_lex;
  select_result *result=lex->result;
  if (not result && not (result= new select_send()))
    return true;
  result->prepare(select_lex->item_list, select_lex->master_unit());

  /* fetching the resultset from memcached */  
  vector<char> raw_resultset; 
  getClient()->get(session->query_cache_key, raw_resultset);
  if(raw_resultset.empty())
    return false;
  message::Resultset resultset_message;
  if (not resultset_message.ParseFromString(string(raw_resultset.begin(),raw_resultset.end())))
    return false;
  List<Item> item_list;

  /* Send the fields */
  message::SelectHeader header= resultset_message.select_header();
  size_t num_fields= header.field_meta_size();
  for (size_t y= 0; y < num_fields; y++)
  {
    message::FieldMeta field= header.field_meta(y);
    string value=field.field_alias();
    item_list.push_back(new Item_string(value.c_str(), value.length(), system_charset_info));
  }
  result->send_fields(item_list);
  item_list.empty();

  /* Send the Data */
  message::SelectData data= resultset_message.select_data();
  session->limit_found_rows= 0; 
  for (int j= 0; j < data.record_size(); j++)
  {
    message::SelectRecord record= data.record(j);
    for (size_t y= 0; y < num_fields; y++)
    {
      if(record.is_null(y))
      {
        item_list.push_back(new Item_null());
      }
      else
      {
        string value=record.record_value(y);
        item_list.push_back(new Item_string(value.c_str(), value.length(), system_charset_info));
      }
    }
    result->send_data(item_list);
    item_list.empty();
  }
  /* Send End of file */
  result->send_eof();
  /* reset the cache key at the session level */
  session->query_cache_key= "";
  return false;
}

/* Check if the tables in the query do not contain
 * Data_dictionary
 */
void MemcachedQueryCache::checkTables(Session *session, TableList* in_table)
{
  for (TableList* tmp_table= in_table; tmp_table; tmp_table= tmp_table->next_global)
  {
    if (strcasecmp(tmp_table->db, "DATA_DICTIONARY") == 0)
    {
      session->lex->setCacheable(false);
      break;
    }
  } 
}

/* init the current resultset in the session
 * set the header message (hashkey= sql + schema)
 */
bool MemcachedQueryCache::doPrepareResultset(Session *session)
{		
  checkTables(session, session->lex->query_tables);
  if (SessionVAR(session, enable) && session->lex->isCacheable())
  {
    /* Prepare and set the key for the session */
    string query= session->query+session->db;
    char* key= md5_key(query.c_str());

    /* make sure only one thread will cache the query 
     * if executed concurently
     */
    pthread_mutex_lock(&mutex);

    if(not queryCacheService.isCached(key))
    {
      session->query_cache_key.assign(key);
      free(key);
    
      /* create the Resultset */
      message::Resultset *resultset= queryCacheService.setCurrentResultsetMessage(session);
  
      /* setting the resultset infos */
      resultset->set_key(session->query_cache_key);
      resultset->set_schema(session->db);
      resultset->set_sql(session->query);
      pthread_mutex_unlock(&mutex);
      
      return true;
    }
    pthread_mutex_unlock(&mutex);
    free(key);
  }
  return false;
}

/* Send the current resultset to memcached
 * Reset the current resultset of the session
 */
bool MemcachedQueryCache::doSetResultset(Session *session)
{		
  message::Resultset *resultset= session->getResultsetMessage();
  if (SessionVAR(session, enable) && (not session->is_error()) && resultset != NULL && session->lex->isCacheable())
  {
    /* Generate the final Header */
    queryCacheService.setResultsetHeader(*resultset, session, session->lex->query_tables);
    /* serialize the Resultset Message */
    std::string output;
    resultset->SerializeToString(&output);

    /* setting to memecahced */
    time_t expiry= expiry_time;  // ToDo: add a user defined expiry
    uint32_t flags= 0;
    std::vector<char> raw(output.size());
    memcpy(&raw[0], output.c_str(), output.size());
    if(not client->set(session->query_cache_key, raw, expiry, flags))
    {
      delete resultset;
      session->resetResultsetMessage();
      return false;
    }
    
    /* Clear the Selectdata from the Resultset to be localy cached
     * Comment if Keeping the data in the header is needed
     */
    resultset->clear_select_data();

    /* add the Resultset (including the header) to the hash 
     * This is done after the memcached set
     */
    queryCacheService.cache[session->query_cache_key]= *resultset;

    /* endup the current statement */
    delete resultset;
    session->resetResultsetMessage();
    return true;
  }
  return false;
}

/* Adds a record (List<Item>) to the current Resultset.SelectData
 */
bool MemcachedQueryCache::doInsertRecord(Session *session, List<Item> &list)
{		
  if(SessionVAR(session, enable))
  {
    queryCacheService.addRecord(session, list);
    return true;
  }
  return false;
}

char* MemcachedQueryCache::md5_key(const char *str)
{
  int msg_len= strlen(str);
  /* Length of resulting sha1 hash - gcry_md_get_algo_dlen
  * returns digest lenght for an algo */
  int hash_len= gcry_md_get_algo_dlen( GCRY_MD_MD5 );
  /* output sha1 hash - this will be binary data */
  unsigned char* hash= (unsigned char*) malloc(hash_len);
  /* output sha1 hash - converted to hex representation
  * 2 hex digits for every byte + 1 for trailing \0 */
  char *out= (char *) malloc( sizeof(char) * ((hash_len*2)+1) );
  char *p= out;
  /* calculate the SHA1 digest. This is a bit of a shortcut function
  * most gcrypt operations require the creation of a handle, etc. */
  gcry_md_hash_buffer( GCRY_MD_MD5, hash, str , msg_len );
  /* Convert each byte to its 2 digit ascii
  * hex representation and place in out */
  int i;
  for ( i = 0; i < hash_len; i++, p += 2 )
  {
    snprintf ( p, 3, "%02x", hash[i] );
  }
  free(hash);
  return out;
}

/** User Defined Function print_query_cache_meta **/
extern plugin::Create_function<PrintQueryCacheMetaFunction> *print_query_cache_meta_func_factory;
plugin::Create_function<QueryCacheFlushFunction> *query_cache_flush_func= NULL;

/** DATA_DICTIONARY views */
static QueryCacheTool *query_cache_tool;
static QueryCacheStatusTool *query_cache_status;
static CachedTables *query_cached_tables;

static int init(module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  if (vm.count("expiry"))
  { 
    if (expiry_time > (ulong)~0L)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of expiry\n"));
      exit(-1);
    }
  }

  if (vm.count("servers"))
  {
    sysvar_memcached_servers= const_cast<char *>(vm["servers"].as<string>().c_str());
  }

  if (vm.count("enable"))
  {
    (SessionVAR(NULL,enable))= vm["enable"].as<bool>();
  }

  MemcachedQueryCache* memc= new MemcachedQueryCache("Memcached_Query_Cache", sysvar_memcached_servers);
  context.add(memc);

  Invalidator* invalidator= new Invalidator("Memcached_Query_Cache_Invalidator");
  context.add(invalidator);
  ReplicationServices &replication_services= ReplicationServices::singleton();
  string replicator_name("default_replicator");
  replication_services.attachApplier(invalidator, replicator_name);
  
  /* Setup the module's UDFs */
  print_query_cache_meta_func_factory=
    new plugin::Create_function<PrintQueryCacheMetaFunction>("print_query_cache_meta");
  context.add(print_query_cache_meta_func_factory);
  
  query_cache_flush_func= new plugin::Create_function<QueryCacheFlushFunction>("query_cache_flush");
  context.add(query_cache_flush_func);

  /* Setup the module Data dict and status infos */
  query_cache_tool= new (nothrow) QueryCacheTool();
  context.add(query_cache_tool);
  query_cache_status= new (nothrow) QueryCacheStatusTool();
  context.add(query_cache_status);
  query_cached_tables= new (nothrow) CachedTables();
  context.add(query_cached_tables);
  
  return 0;
}

static drizzle_sys_var* vars[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(servers),
  DRIZZLE_SYSVAR(expiry),
  NULL
};

QueryCacheStatusTool::Generator::Generator(drizzled::Field **fields) :
  plugin::TableFunction::Generator(fields)
{ 
  status_var_ptr= vars;
}

bool QueryCacheStatusTool::Generator::populate()
{
  if (*status_var_ptr)
  {
    std::ostringstream oss;
    string return_value;

    /* VARIABLE_NAME */
    push((*status_var_ptr)->name);
    if (strcmp((**status_var_ptr).name, "enable") == 0)
      return_value= SessionVAR(&(getSession()), enable) ? "ON" : "OFF";
    if (strcmp((**status_var_ptr).name, "servers") == 0) 
      return_value= MemcachedQueryCache::getServers();
    if (strcmp((**status_var_ptr).name, "expiry") == 0)
    {
      oss << expiry_time;
      return_value= oss.str();
    }
    /* VARIABLE_VALUE */
    if (return_value.length())
      push(return_value);
    else 
      push(" ");

    status_var_ptr++;

    return true;
  }
  return false;
}

static void init_options(drizzled::module::option_context &context)
{
  context("servers",
          po::value<string>()->default_value("127.0.0.1:11211"),
          N_("List of memcached servers."));
  context("expiry",
          po::value<ulong>(&expiry_time)->default_value(1000),
          N_("Expiry time of memcached entries"));
  context("enable",
          po::value<bool>()->default_value(false)->zero_tokens(),
          N_("Enable Memcached Query Cache"));
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "Query_Cache",
  "0.3",
  "Djellel Eddine Difallah",
  "Caches Select resultsets in Memcached",
  PLUGIN_LICENSE_BSD,
  init,   /* Plugin Init      */
  vars, /* system variables */
  init_options    /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
