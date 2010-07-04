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
#include "memcached_qc.h"
#include "drizzled/session.h"
#include "drizzled/select_send.h"

#include <gcrypt.h>
#include <iostream>
#include <vector>

using namespace drizzled;
using namespace std;

bool Memcached_Qc::isSelect(string query)
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

bool Memcached_Qc::doIsCached(Session *session)
{
  if (isSelect(session->query))
  {
    /* ToDo: Check against the cache content */
    string query= session->query+session->db;
    char* key= md5_key(query.c_str());
    if(queryCacheService.isCached(key))
    {
     return true;
    }
  }
  return false;
}

bool Memcached_Qc::doSendCachedResultset(Session *session)
{
  /* TODO implement the send resultset functionality */
  (void) session;
  return true;
}

/* init the current resultset in the session
 * set the header message (hashkey= sql + schema)
 */
bool Memcached_Qc::doPrepareResultset(Session *session)
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
    session->query_cache_key= key;

    /* create the Resultset */
    message::Resultset &resultset= queryCacheService.getCurrentResultsetMessage(session);
  
    /* setting the resultset infos */
    resultset.set_key(key);
    resultset.set_schema(session->db);
    resultset.set_sql(session->query);
    pthread_mutex_unlock(&mutex);
    return true;
  }
  pthread_mutex_unlock(&mutex);
  return false;
}

/* Send the current resultset to memcached
 * Reset the current resultset of the session
 */
bool Memcached_Qc::doSetResultset(Session *session)
{		
  message::Resultset *resultset= session->getResultsetMessage();
  if (resultset != NULL)
  {
    /* serialize the Resultset Message */
    std::string output;
    resultset->SerializeToString(&output);

    /* setting to memecahced */
    time_t expiry= 0;  // ToDo: add a user defined expiry
    uint32_t flags= 0;
    std::vector<char> raw(output.size());
    memcpy(&raw[0], &output, output.size());
    client->set(session->query_cache_key, raw, expiry, flags);

    /* Generate the final Header 
     * and clear the Selectdata from the Resultset to be localy cached
     */
    queryCacheService.setResultsetHeader(*resultset, session, session->lex->query_tables);
    resultset->clear_select_data();

    /* add the Resultset (including the header) to the hash 
     * This is done before the final set (this can be a problem)
     * ToDo: fix that
     */
    queryCacheService.cache[session->query_cache_key]= *resultset;

    /* endup the current statement */
    session->setResultsetMessage(NULL);
    return true;
  }
  return false;
}

/* Adds a record (List<Item>) to the current Resultset.SelectData
 */
bool Memcached_Qc::doInsertRecord(Session *session, List<Item> &list)
{		
  queryCacheService.addRecord(session, list);
  return true;
}

char* Memcached_Qc::md5_key(const char *str)
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
  return out;
}

static int init(module::Context &context)
{
  Memcached_Qc* memc= new Memcached_Qc("memcached_qc");
  context.add(memc);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "memcached_qc",
  "0.2",
  "Djellel Eddine Difallah",
  "Cache of Select Data into memcached",
  PLUGIN_LICENSE_BSD,
  init,   /* Plugin Init      */
  NULL, /* system variables */
  NULL    /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
